#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Utils.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include "qoperator.hpp"

std::string expand_str(EXPAND exp) {
    switch (exp) {
        case EXPAND::IN:
            return "in";
        case EXPAND::OUT:
            return "out";
    }
}

std::string join_op_str(JOIN_OP jop) {
    switch (jop) {
        case JOIN_OP::CROSS:
            return "cross";
        case JOIN_OP::LEFT_OUTER:
            return "left_outer";
        case JOIN_OP::NESTED_LOOP:
            return "nested_loop";
        case JOIN_OP::HASH_JOIN:
            return "hash_join";
    }
}


std::map<int, std::vector<int>> pipeline_types;
std::map<int, GlobalValue*> limits_;
std::map<int, Value*> limit_values_;

int query_id = 0;

void scan_op::codegen(op_visitor & vis, unsigned & op_id, bool interpreted) {
    pipeline_types[query_id].push_back(0);
    op_id_ = op_id;
    auto next_offset = indexed_ ? 3 : 1;

    vis.visit(shared_from_this());

    for(auto & inp : inputs_) {
        inp->codegen(vis, op_id+=next_offset,interpreted);
    }
}



void foreach_rship_op::codegen(op_visitor & vis, unsigned & op_id, bool interpreted) {
    pipeline_types[query_id].push_back(1);
    op_id_ = op_id;

    vis.visit(shared_from_this());

    for(auto & inp : inputs_) {
        inp->codegen(vis, op_id+=1,interpreted);
    }
}



void filter_op::codegen(op_visitor & vis, unsigned & op_id, bool interpreted) {
    op_id_ = op_id;


    vis.visit(shared_from_this());

    for(auto & inp : inputs_) {
        inp->codegen(vis, op_id+=1,interpreted);
    }
}

void project::codegen(op_visitor & vis, unsigned & op_id, bool interpreted) {
    op_id_ = op_id;

    vis.visit(shared_from_this());

    for(auto & inp : inputs_) {
        inp->codegen(vis, op_id,interpreted);
    }
}

void expand_op::codegen(op_visitor & vis, unsigned & op_id, bool interpreted) {
    pipeline_types[query_id].push_back(0);
    op_id_ = op_id;


    vis.visit(shared_from_this());

    for(auto & inp : inputs_) {
        inp->codegen(vis, op_id+=1,interpreted);
    }
}

int get_nopid(int & start, std::vector<algebra_optr> & ops, join_op endop) {
    auto cur = ops.back();
    ops.pop_back();
    while(cur->op_id_ != endop.op_id_) {
        start++;
        if(cur->type_ == qop_type::left_join || cur->type_ == qop_type::cross_join) {
            ops.push_back(cur->inputs_[0]);
            cur = cur->inputs_[1];
        } else {
            if(cur->type_ == qop_type::none) {
                if(ops.empty()) {
                    break;
                } else {
                    cur = ops.back();
                    ops.pop_back();
                }
            } else {
                cur = cur->inputs_[0];
            }
        }

    }
    return start;
}

void join_op::codegen(op_visitor & vis, unsigned & op_id, bool interpreted) {
    op_id_ = op_id;

    auto lhs_op_id = op_id_+1;
    auto cur_op = inputs_[1];

    if(!interpreted) {
        inputs_[1]->codegen(vis, op_id, false);
    }

    vis.visit(shared_from_this());

    if(interpreted) {
        for(auto & inp : inputs_) {
            inp->codegen(vis, op_id,true);
        }
    } else {
        inputs_[0]->codegen(vis, op_id, false);
    }

}

Function *join_op::codegen_rhs(PContext &ctx, Function *consumer) {
    int cnt = 0;
    auto func_name = name_;
    while(ctx.gen_funcs.find(func_name) != ctx.gen_funcs.end()) {
        func_name = name_+std::to_string(++cnt);
    }

    func_name = func_name + join_op_str(jop_) + "lhs";

    name_ = func_name;

    FunctionCallee rship_by_id = ctx.extern_func("rship_by_id");

    Function *fct = Function::Create(ctx.consumerFctTy, Function::ExternalLinkage,
                                     name_, ctx.getModule());
    BasicBlock *entry = BasicBlock::Create(ctx.getContext(), "entry", fct);
    BasicBlock *concat_qrl = BasicBlock::Create(ctx.getContext(), "concat_qrl", fct);
    BasicBlock *incr_loop = BasicBlock::Create(ctx.getContext(), "incr_loop", fct);
    BasicBlock *consume = BasicBlock::Create(ctx.getContext(), "consume", fct);
    BasicBlock *foreach_rship = BasicBlock::Create(ctx.getContext(), "foreach_rship", fct);
    BasicBlock *next_rship = BasicBlock::Create(ctx.getContext(), "next_rship", fct);
    BasicBlock *consume_next = BasicBlock::Create(ctx.getContext(), "consume_next_left", fct);
    BasicBlock *for_each_rship = BasicBlock::Create(ctx.getContext(), "for_each_rship", fct);
    BasicBlock *for_each_next = BasicBlock::Create(ctx.getContext(), "for_each_next_rship", fct);
    BasicBlock *end = BasicBlock::Create(ctx.getContext(), "end", fct);

    Value *left_pos = nullptr;
    Value *right_pos = nullptr;

    auto get_join_vec = ctx.extern_func("get_join_vec_arr");
    auto get_join_vec_size = ctx.extern_func("get_join_vec_size");

    ctx.getBuilder().SetInsertPoint(entry);
    if(jop_ == JOIN_OP::LEFT_OUTER) {
        left_pos = ConstantInt::get(ctx.int64Ty, join_pos_.first);
        right_pos = ConstantInt::get(ctx.int64Ty, join_pos_.second);
    }

    auto gdb = fct->args().begin();
    auto oid = fct->args().begin() + 1;
    auto qr_tuple_list = fct->args().begin() + 2;
    auto rs = fct->args().begin() + 3;
    auto prev_size = fct->args().begin() + 4;
    auto ty = fct->args().begin() + 5;
    auto call_map_arg = fct->args().begin() + 6;
    auto offset = fct->args().begin() + 7;
    auto call_map = ctx.getBuilder().CreateBitCast(fct->args().begin() + 6, ctx.callMapPtrTy);

    auto lhs_qr_arr = ctx.getBuilder().CreateBitCast(qr_tuple_list, ctx.res_arr_type->getPointerTo());
    auto lhs_size_field = ctx.getBuilder().CreateInBoundsGEP(lhs_qr_arr, {ctx.LLVM_ZERO, ctx.LLVM_ZERO});
    auto lhs_size = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateLoad(lhs_size_field));
    auto insert_pos = ctx.getBuilder().CreateAdd(prev_size, ctx.LLVM_ONE);

    auto lhs_alloca = ctx.getBuilder().CreateAlloca(ctx.int64Ty);
    auto rhs_alloca = ctx.getBuilder().CreateAlloca(ctx.int64Ty);
    auto rhs_id_alloca = ctx.getBuilder().CreateAlloca(ctx.int64Ty);

    auto max_idx = ctx.getBuilder().CreateAlloca(ctx.int64Ty);
    auto cur_idx = ctx.getBuilder().CreateAlloca(ctx.int64Ty);
    ctx.getBuilder().CreateStore(ctx.LLVM_ZERO, cur_idx);

    //get input vector from join obj
    auto inputs_vec_raw = ConstantInt::get(ctx.int64Ty, (int64_t )&join_inputs_);
    auto inputs_vec = ctx.getBuilder().CreateIntToPtr(inputs_vec_raw, ctx.int64PtrTy);

    auto qrl_size = ctx.getBuilder().CreateCall(get_join_vec_size, {inputs_vec});
    ctx.getBuilder().CreateStore(qrl_size, max_idx);

    auto cur_qr = ctx.getBuilder().CreateAlloca(ctx.int64PtrTy);

    auto cur_lhs_pos = ctx.getBuilder().CreateAlloca(ctx.int64Ty);
    auto cur_rhs_pos = ctx.getBuilder().CreateAlloca(ctx.int64Ty);

    auto cpy_size = ctx.getBuilder().CreateAlloca(ctx.int64Ty);

    auto noid = ctx.getBuilder().CreateAdd(oid, ctx.LLVM_ONE);
    auto fct_ptr = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateInBoundsGEP(call_map, {ctx.LLVM_ZERO, oid}));

    //auto ql = ctx.getBuilder().CreateCall(get_join_vec, {inputs_vec, ctx.LLVM_TWO});
    auto loop_body = ctx.while_loop_condition(fct, cur_idx, max_idx, PContext::WHILE_COND::LT, end,
                                              [&](BasicBlock *body, BasicBlock *epilog) {
        // get current index for join vector
        auto idx = ctx.getBuilder().CreateLoad(cur_idx);

        // get tuple at index from vector
        //auto qrlp = ctx.getBuilder().CreateInBoundsGEP(ql, {ctx.LLVM_ZERO});
        //auto qrl = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateInBoundsGEP(qrlp, {idx}));
        auto qrl = ctx.getBuilder().CreateCall(get_join_vec, {inputs_vec, idx});

        //qrl->getType()->dump();
        // store current tuple to stack
        ctx.getBuilder().CreateStore(qrl, cur_qr);

        // process join
        if(jop_ == JOIN_OP::CROSS)
            ctx.getBuilder().CreateBr(concat_qrl);
        else
            ctx.getBuilder().CreateBr(foreach_rship);
                                              });

    ctx.getBuilder().SetInsertPoint(foreach_rship);
    auto rhs_qr_ptr_lj = ctx.getBuilder().CreateLoad(cur_qr);
    auto rhs_qr_arr_lj = ctx.getBuilder().CreateBitCast(rhs_qr_ptr_lj, ctx.res_arr_type->getPointerTo());
    auto rhs_node_lj = ctx.getBuilder().CreateBitCast(ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateInBoundsGEP(rhs_qr_arr_lj, {ctx.LLVM_ZERO, right_pos})), ctx.nodePtrTy);
    auto lhs_node_lj = ctx.getBuilder().CreateBitCast(ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateInBoundsGEP(qr_tuple_list, {ctx.LLVM_ZERO, left_pos})), ctx.nodePtrTy);

    auto rhs_node_id = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateStructGEP(rhs_node_lj, 1));


    auto lhs_node_rship_id = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateStructGEP(lhs_node_lj, 2));
    ctx.getBuilder().CreateStore(lhs_node_rship_id, lhs_alloca);

    ctx.getBuilder().CreateStore(ctx.UNKNOWN_ID, rhs_alloca);

    Value *rship;

    auto loop_body_lf = ctx.while_loop_condition(fct, lhs_alloca, rhs_alloca, PContext::WHILE_COND::LT, end,
                                              [&](BasicBlock *, BasicBlock *) {
                                                  auto lhs = ctx.getBuilder().CreateLoad(lhs_alloca);

                                                  rship = ctx.getBuilder().CreateCall(rship_by_id, {gdb, lhs});
                                                  auto to_nod_id = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateStructGEP(rship, 3));

                                                  auto id_eq = ctx.getBuilder().CreateICmpEQ(to_nod_id, rhs_node_id);
                                                  ctx.getBuilder().CreateCondBr(id_eq, concat_qrl, next_rship);
                                              });

    ctx.getBuilder().SetInsertPoint(next_rship);
    auto *nrship = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateStructGEP(rship, 4));
    ctx.getBuilder().CreateStore(nrship, lhs_alloca);
    ctx.getBuilder().CreateBr(loop_body_lf);

    ctx.getBuilder().SetInsertPoint(concat_qrl);
    // increment index and store
    auto idx = ctx.getBuilder().CreateLoad(cur_idx);
    auto nidx = ctx.getBuilder().CreateAdd(idx, ctx.LLVM_ONE);
    ctx.getBuilder().CreateStore(nidx, cur_idx);

    auto rhs_qr_ptr = ctx.getBuilder().CreateLoad(cur_qr);
    auto rhs_qr_arr = ctx.getBuilder().CreateBitCast(rhs_qr_ptr, ctx.res_arr_type->getPointerTo());
    auto rhs_size_field = ctx.getBuilder().CreateInBoundsGEP(rhs_qr_arr, {ctx.LLVM_ZERO, ctx.LLVM_ZERO});

    // get size of tuples from rhs
    auto rhs_size_ptr = ctx.getBuilder().CreateLoad(rhs_size_field);
    auto rhs_size = ctx.getBuilder().CreateLoad(rhs_size_ptr);

    // get insert position of rhs -> size + 1
    auto rhs_pos_max = ctx.getBuilder().CreateAlloca(ctx.int64Ty);
    ctx.getBuilder().CreateStore(rhs_size, rhs_pos_max);

    // store intitial lhs & rhs index to stack
    ctx.getBuilder().CreateStore(ctx.LLVM_ONE, cur_lhs_pos);

    ctx.getBuilder().CreateStore(ctx.LLVM_ONE, cur_rhs_pos);

    ctx.getBuilder().CreateStore(prev_size, cpy_size);

    auto offset_rhs = ctx.getBuilder().CreateAdd(rhs_size, ctx.LLVM_ONE);
    auto nsize = ctx.getBuilder().CreateAdd(prev_size, offset_rhs);

    // copy each element from rhs to lhs
    auto copy_body = ctx.while_loop_condition(fct, cur_lhs_pos, rhs_pos_max, PContext::WHILE_COND::LE, consume,
                                              [&](BasicBlock *body, BasicBlock *epilog) {
      auto lhs_pos = ctx.getBuilder().CreateLoad(cur_lhs_pos);
      auto lhs_dst = ctx.getBuilder().CreateInBoundsGEP(lhs_qr_arr, {ctx.LLVM_ZERO, lhs_pos});

      auto rhs_pos = ctx.getBuilder().CreateLoad(cur_rhs_pos);
      auto rhs_src = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateInBoundsGEP(rhs_qr_arr, {ctx.LLVM_ZERO, rhs_pos}));

      ctx.getBuilder().CreateStore(rhs_src, lhs_dst);
      ctx.getBuilder().CreateBr(incr_loop);
    });

    ctx.getBuilder().SetInsertPoint(incr_loop);
    auto lhs_pos = ctx.getBuilder().CreateLoad(cur_lhs_pos);
    auto incr_lhs = ctx.getBuilder().CreateAdd(lhs_pos, ctx.LLVM_ONE);
    ctx.getBuilder().CreateStore(incr_lhs, cur_lhs_pos);
    auto rhs_pos = ctx.getBuilder().CreateLoad(cur_rhs_pos);
    auto incr_rhs = ctx.getBuilder().CreateAdd(rhs_pos, ctx.LLVM_ONE);
    ctx.getBuilder().CreateStore(incr_rhs, cur_rhs_pos);
    ctx.getBuilder().CreateBr(copy_body);

    ctx.getBuilder().SetInsertPoint(consume);
    auto forward = ctx.getBuilder().CreateBitCast(lhs_qr_arr, ctx.int64PtrTy);
    //ctx.getBuilder().CreateCall(fct_ptr, {gdb, noid, forward, rs, nsize, ty, call_map_arg, offset});
    ctx.getBuilder().CreateBr(loop_body);


    ctx.getBuilder().SetInsertPoint(end);
    {
        ctx.getBuilder().CreateRetVoid();
    }


    ctx.gen_funcs[name_] = fct;
    return fct;
}



void collect_op::codegen(op_visitor & vis, unsigned & op_id, bool interpreted) {
    op_id_ = op_id;

    vis.visit(shared_from_this());
}



void sort_op::codegen(op_visitor & vis, unsigned & op_id, bool interpreted) {
    op_id_ = op_id;

    vis.visit(shared_from_this());

    for(auto & inp : inputs_) {
        inp->codegen(vis, op_id+=1,true);
    }
}

std::function<bool(const qr_tuple &, const qr_tuple &)> sort_op::cmp_ = 0;



void limit_op::codegen(op_visitor &vis, unsigned int & op_id, bool interpreted) {
    op_id_ = op_id;
    vis.visit(shared_from_this());

    for(auto & inp : inputs_) {
        inp->codegen(vis, op_id+=1,true);
    }
}



void end_op::codegen(op_visitor &vis, unsigned int & op_id, bool interpreted) {
    vis.visit(shared_from_this());
}

void create_op::codegen(op_visitor &vis, unsigned int & op_id, bool interpreted) {
    op_id_ = op_id;

    vis.visit(shared_from_this());

    for(auto & inp : inputs_) {
        inp->codegen(vis, op_id+=2,true);
    }
}

