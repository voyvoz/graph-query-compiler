#include "codegen.hpp"

void codegen_visitor::visit(std::shared_ptr<scan_op> op) {
    Function *df_finish = Function::Create(ctx.finishFctTy, Function::ExternalLinkage, "default_finish",
                                           ctx.getModule());

    BasicBlock *df_finish_bb = BasicBlock::Create(ctx.getContext(), "entry", df_finish);
    ctx.getBuilder().SetInsertPoint(df_finish_bb);
    ctx.getBuilder().CreateRetVoid();

    int cnt = 0;
    auto func_name = op->name_;
    while(ctx.gen_funcs.find(func_name) != ctx.gen_funcs.end()) {
        func_name = op->name_+std::to_string(++cnt);
    }

    op->name_ = func_name;

    Function *fct = Function::Create(ctx.startFctTy, Function::ExternalLinkage, func_name, ctx.getModule());

    FunctionCallee get_valid_node = ctx.extern_func("get_valid_node");

    FunctionCallee dict_lookup_label = ctx.extern_func("dict_lookup_label");
    FunctionCallee gdb_get_nodes = ctx.extern_func("gdb_get_nodes");
    FunctionCallee gdb_get_node_from_it = ctx.extern_func("get_node_from_it");

    FunctionCallee get_begin = ctx.extern_func("get_vec_begin");
    FunctionCallee get_next = ctx.extern_func("get_vec_next");
    FunctionCallee is_end = ctx.extern_func("vec_end_reached");

    BasicBlock *bb = BasicBlock::Create(ctx.getModule().getContext(), "entry", fct);
    BasicBlock *scan_nodes_end = BasicBlock::Create(ctx.getModule().getContext(), "scan_nodes_end", fct);
    BasicBlock *consumeBB = BasicBlock::Create(ctx.getModule().getContext(), "consume_node", fct);

    ctx.getBuilder().SetInsertPoint(bb);

    auto * resAlloc = ctx.getBuilder().CreateAlloca(ctx.res_arr_type);
    if (op->label_.empty()) {
        // rship
    } else {
        // node
        // TODO: label empty
        auto strAlloc = ctx.getBuilder().CreateAlloca(ctx.int8PtrTy);
        auto cur_node_alloca = ctx.getBuilder().CreateAlloca(ctx.nodePtrTy);
        auto loop_it_alloca = ctx.getBuilder().CreateAlloca(ctx.nodeItPtrTy);
        auto transformed_result = ctx.getBuilder().CreateAlloca(ctx.qrResultTy);
        auto resPos = ConstantInt::get(ctx.int64Ty, op->op_id_);

        // function arguments
        auto gdb = fct->args().begin();
        auto first = fct->args().begin() + 1;
        auto last = fct->args().begin() + 2;
        auto tx_ptr = fct->args().begin() + 3;
        auto oid = fct->args().begin() + 4;
        auto ty = fct->args().begin() + 5;
        auto rs = fct->args().begin() + 6;
        auto call_map_arg = fct->args().begin() + 7;
        auto finish = fct->args().begin() + 8;
        auto offset = fct->args().begin() + 9;
        auto call_map = ctx.getBuilder().CreateBitCast(call_map_arg, ctx.callMapPtrTy);

        // 1 get nodes vec from gdb -> extern func
        auto nodes_vec = ctx.getBuilder().CreateCall(gdb_get_nodes, {gdb});

        // 2 lookup label in dict -> extern func
        //auto label_code = ctx.getBuilder().CreateCall(dict_lookup_label, {str});
        auto label_code = ConstantInt::get(ctx.int32Ty, APInt(32, ctx.get_dcode(op->label_)));
        // 3 iterate through nodes vec
        auto node_begin_it = ctx.getBuilder().CreateCall(get_begin, {nodes_vec, first, last});

        // set size field or qr to 1
        auto size_field = ctx.getBuilder().CreateInBoundsGEP(resAlloc, {ctx.LLVM_ZERO, ctx.LLVM_ZERO});
        auto sz = ctx.getBuilder().CreateAlloca(ctx.int64Ty);

        /*auto node_msize = ConstantExpr::getSizeOf(ctx.int64Ty);
        node_msize = ConstantExpr::getTruncOrBitCast(node_msize, ctx.int64Ty);
        auto sz = CallInst::CreateMalloc(ctx.getBuilder().GetInsertBlock(), ctx.int64Ty, ctx.int64Ty,
                                                     node_msize, nullptr, nullptr, "qr_size");
        ctx.getBuilder().Insert(sz);*/



        auto insert_pos = ctx.getBuilder().CreateAdd(oid, offset);

        auto res = ctx.getBuilder().CreateInBoundsGEP(resAlloc, {ctx.LLVM_ZERO, insert_pos});

        // extract fct ptr from call map
        auto fct_ptr = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateInBoundsGEP(call_map, {ctx.LLVM_ZERO, ctx.LLVM_ZERO}));

        // increment oid for next operator
        auto next_oid = ctx.getBuilder().CreateAdd(oid, ctx.LLVM_ONE);

        ctx.getBuilder().CreateStore(node_begin_it, loop_it_alloca);
        BasicBlock *curBB = ctx.while_loop(fct, get_begin, get_next, is_end, loop_it_alloca, nodes_vec,
                                           scan_nodes_end, [&](BasicBlock *curBB, BasicBlock *epilog) {
                    // 4 if node label == label -> consume fct
                    auto node_it = ctx.getBuilder().CreateLoad(loop_it_alloca);
                    auto node_raw = ctx.getBuilder().CreateCall(gdb_get_node_from_it, {node_it});

                    auto node = ctx.getBuilder().CreateCall(get_valid_node, {gdb, node_raw, tx_ptr});

                    ctx.getBuilder().CreateStore(node, cur_node_alloca);
                    // 1 get node GEP -> label
                    auto lc_ptr = ctx.getBuilder().CreateStructGEP(node, 5);

                    auto lc = ctx.getBuilder().CreateLoad(lc_ptr);

                    // 2 cmp with label_code
                    auto cond = ctx.getBuilder().CreateICmpEQ(lc, label_code);

                    // 3 if equal -> call consumer
                    ctx.getBuilder().CreateCondBr(cond, consumeBB, epilog);
                }, consumeBB);

        // consume node
        ctx.getBuilder().SetInsertPoint(consumeBB);
        {
            ctx.getBuilder().CreateStore(ctx.LLVM_ONE, sz);
            ctx.getBuilder().CreateStore(sz, size_field);
            auto node_ptr = ctx.getBuilder().CreateLoad(cur_node_alloca);
            auto node = ctx.getBuilder().CreateBitCast(node_ptr, ctx.int64PtrTy);
            ctx.getBuilder().CreateStore(node, res);
            auto forward = ctx.getBuilder().CreateBitCast(resAlloc, ctx.int64PtrTy);

            // 3 consume next operator
            ctx.getBuilder().CreateCall(fct_ptr, {gdb, oid, forward, rs, ctx.LLVM_ONE, ty, call_map_arg, offset});


            ctx.getBuilder().CreateBr(curBB);
        }

        ctx.getBuilder().SetInsertPoint(scan_nodes_end);
        {
            ctx.getBuilder().CreateCall(finish, {rs});
            ctx.getBuilder().CreateRet(nullptr);
        }
    }

    ctx.gen_funcs[op->name_] = fct;
}

void codegen_visitor::visit(std::shared_ptr<foreach_rship_op> op) {
    int node_rship_idx;
    int next_rship_idx;
    switch (op->dir_) {
        case RSHIP_DIR::FROM:
            node_rship_idx = 2;
            next_rship_idx = 4;
            break;
        case RSHIP_DIR::TO:
            node_rship_idx = 3;
            next_rship_idx = 5;
    }

    int cnt = 0;
    auto func_name = op->name_;
    while(ctx.gen_funcs.find(func_name) != ctx.gen_funcs.end()) {
        func_name = op->name_+std::to_string(++cnt)+std::to_string(op->op_id_);
    }
    std::string hops;
    if(op->hops_.second > 0) {
        hops = std::to_string(op->hops_.first) + std::to_string(op->hops_.first);
    }
    op->name_ = func_name + op->dir_str(op->dir_) + hops + op->label_;

    Function *fct = Function::Create(ctx.consumerFctTy, Function::ExternalLinkage,
                                     op->name_, ctx.getModule());


    FunctionCallee dict_lookup_label = ctx.extern_func("dict_lookup_label");
    FunctionCallee gdb_get_rships = ctx.extern_func("gdb_get_rships");
    FunctionCallee gdb_get_rship_from_it = ctx.extern_func("get_rship_from_it");

    FunctionCallee get_begin = ctx.extern_func("get_vec_begin_r");
    FunctionCallee get_next = ctx.extern_func("get_vec_next_r");
    FunctionCallee is_end = ctx.extern_func("vec_end_reached_r");

    FunctionCallee rship_by_id = ctx.extern_func("rship_by_id");


    BasicBlock *entry = BasicBlock::Create(ctx.getContext(), "entry", fct);
    BasicBlock *foreach_rship_end = BasicBlock::Create(ctx.getModule().getContext(), "foreach_rel_end", fct);
    BasicBlock *nextBB = BasicBlock::Create(ctx.getModule().getContext(), "next_rship", fct);
    BasicBlock *consumeBB = BasicBlock::Create(ctx.getModule().getContext(), "consume_rship", fct);


    ctx.getBuilder().SetInsertPoint(entry);
    auto gdb = fct->args().begin();
    auto oid = fct->args().begin() + 1;
    auto qr_tuple_list = fct->args().begin() + 2;
    auto rs = fct->args().begin() + 3;
    auto prev_size = fct->args().begin() + 4;
    auto ty = fct->args().begin() + 5;
    auto call_map_arg = fct->args().begin() + 6;
    auto offset = fct->args().begin() + 7;
    auto call_map = ctx.getBuilder().CreateBitCast(fct->args().begin() + 6, ctx.callMapPtrTy);

    // extract fct ptr from call map
    auto fct_ptr = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateInBoundsGEP(call_map, {ctx.LLVM_ZERO, oid}));

    // 2 lookup label in dict -> extern func
    //auto label_code = ctx.getBuilder().CreateCall(dict_lookup_label, {str});
    auto label_code = ConstantInt::get(ctx.int32Ty, APInt(32, ctx.get_dcode(op->label_)));

    if(op->hops_.second != 0) {
        auto foreach_rship_from = ctx.extern_func("foreach_variable_from");
        auto min = ConstantInt::get(ctx.int64Ty, op->hops_.first);
        auto max = ConstantInt::get(ctx.int64Ty, op->hops_.second);

        ctx.getBuilder().CreateCall(foreach_rship_from, {gdb, label_code, min, max, fct_ptr,
                                                         oid, qr_tuple_list, rs, prev_size, ty, call_map_arg, offset});
        ctx.getBuilder().CreateBr(foreach_rship_end);
    } else {
        auto qr_size = ctx.getBuilder().CreateAdd(prev_size, ctx.LLVM_ONE);

        auto qrl = ctx.getBuilder().CreateBitCast(qr_tuple_list, ctx.res_arr_type->getPointerTo());

        auto prev_pos = ctx.getBuilder().CreateAdd(prev_size, offset);
        auto insert_pos = ctx.getBuilder().CreateAdd(prev_pos, ctx.LLVM_ONE);
        auto pres = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateInBoundsGEP(qrl, {ctx.LLVM_ZERO, prev_pos}));


        auto rhs_alloca = ctx.getBuilder().CreateAlloca(ctx.int64Ty);
        auto strAlloc = ctx.getBuilder().CreateAlloca(ctx.int8PtrTy);
        auto cur_rship_alloca = ctx.getBuilder().CreateAlloca(ctx.nodePtrTy);
        auto lhs_alloca = ctx.getBuilder().CreateAlloca(ctx.int64Ty);
        auto transformed_result = ctx.getBuilder().CreateAlloca(ctx.qrResultTy);

        auto UNKNOWN_REL_ID = ConstantInt::get(ctx.int64Ty,
                                               std::numeric_limits<int32_t>::max()); // TODO: UNKNOWN VALUE = std::numeric_limits<int64_t>::max()
        ctx.getBuilder().CreateStore(UNKNOWN_REL_ID, rhs_alloca);
        auto rhs = ctx.getBuilder().CreateLoad(rhs_alloca);

        GlobalVariable *label = ctx.getBuilder().CreateGlobalString(StringRef(op->label_), "rship_label");

        auto strPtr = ctx.getBuilder().CreateBitCast(label, ctx.int8PtrTy);
        ctx.getBuilder().CreateStore(strPtr, strAlloc);
        auto str = ctx.getBuilder().CreateLoad(strAlloc);

        // get node from back of qr_list
        /*auto qr_list_node = ctx.getBuilder().CreateCall(get_vec_back, {qr_tuple_list});
        auto qr = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateStructGEP(qr_list_node, 3));
        auto node_ptr = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateStructGEP(qr, 0));*/
        auto node = ctx.getBuilder().CreateBitCast(pres, ctx.nodePtrTy);

        // 1 get rship vec from gdb -> extern func
        auto rship_vec = ctx.getBuilder().CreateCall(gdb_get_rships, {gdb});

        // 3 set rhs alloca for loop

        //ctx.getBuilder().CreateBr(foreach_rship_end);
        // 4 set init lhs alloca for loop
        auto node_rship_id = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateStructGEP(node, node_rship_idx));
        ctx.getBuilder().CreateStore(node_rship_id, lhs_alloca);

        auto rship_it_alloca = ctx.getBuilder().CreateAlloca(ctx.rshipPtrTy);

        auto loop_body = ctx.while_loop_condition(fct, lhs_alloca, rhs_alloca, PContext::WHILE_COND::LT, foreach_rship_end,
                                                  [&](BasicBlock *, BasicBlock *) {
                                                      auto lhs = ctx.getBuilder().CreateLoad(lhs_alloca);

                                                      auto rship = ctx.getBuilder().CreateCall(rship_by_id, {gdb, lhs});
                                                      ctx.getBuilder().CreateStore(rship, rship_it_alloca);
                                                      auto rship_lcode = ctx.getBuilder().CreateLoad(
                                                              ctx.getBuilder().CreateStructGEP(rship, 7));

                                                      auto consume_cond = ctx.getBuilder().CreateICmpEQ(rship_lcode,
                                                                                                        label_code);
                                                      ctx.getBuilder().CreateCondBr(consume_cond, consumeBB, nextBB);
                                                  });

        ctx.getBuilder().SetInsertPoint(nextBB);
        {
            auto rship = ctx.getBuilder().CreateLoad(rship_it_alloca);
            LoadInst *next_rship = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateStructGEP(rship, next_rship_idx));
            ctx.getBuilder().CreateStore(next_rship, lhs_alloca);
            ctx.getBuilder().CreateBr(loop_body);
        }

        ctx.getBuilder().SetInsertPoint(consumeBB);
        {
            // set size field or qr to 1
            auto size_field = ctx.getBuilder().CreateInBoundsGEP(qrl, {ctx.LLVM_ZERO, ctx.LLVM_ZERO});
            auto sz = ctx.getBuilder().CreateLoad(size_field);
            auto sz_val = ctx.getBuilder().CreateAdd(ctx.getBuilder().CreateLoad(sz), ctx.LLVM_ONE);
            ctx.getBuilder().CreateStore(sz_val, sz);
            ctx.getBuilder().CreateStore(sz, size_field);

            auto src_i = ctx.getBuilder().CreateInBoundsGEP(qrl, {ctx.LLVM_ZERO, insert_pos});
            auto rship_ptr = ctx.getBuilder().CreateLoad(rship_it_alloca);
            auto rship = ctx.getBuilder().CreateBitCast(rship_ptr, ctx.int64PtrTy);
            ctx.getBuilder().CreateStore(rship, src_i);
            auto forward = ctx.getBuilder().CreateBitCast(qrl, ctx.int64PtrTy);

            // increment oid for next operator
            auto next_oid = ctx.getBuilder().CreateAdd(oid, ctx.LLVM_ONE);

            // call consumer
            ctx.getBuilder().CreateCall(fct_ptr, {gdb, next_oid, forward, rs, qr_size, ty, call_map_arg, offset});

            ctx.getBuilder().CreateBr(nextBB);
        }
    }



    ctx.getBuilder().SetInsertPoint(foreach_rship_end);
    {
        ctx.getBuilder().CreateRet(nullptr);
    }

    ctx.gen_funcs[op->name_] = fct;

    for (auto &inp : op->inputs_) {
        inp->set_consumer(fct);
    }
}

void codegen_visitor::visit(std::shared_ptr<expand_op> op) {
    unsigned exp_id;
    switch (op->exp_) {
        case EXPAND::OUT:
            exp_id = 3;
            break;
        case EXPAND::IN:
            exp_id = 2;
            break;
    }
    int cnt = 0;
    auto func_name = op->name_+std::to_string(op->op_id_);
    while(ctx.gen_funcs.find(func_name) != ctx.gen_funcs.end()) {
        func_name = op->name_+std::to_string(++cnt);
    }
    op->name_ = func_name+expand_str(op->exp_);

    Function *fct = Function::Create(ctx.consumerFctTy, Function::ExternalLinkage, func_name + expand_str(op->exp_),
                                     ctx.getModule());
    FunctionCallee ls_fct = ctx.extern_func("list_size");


    BasicBlock *bb = BasicBlock::Create(ctx.getContext(), "entry", fct);
    BasicBlock *consume = BasicBlock::Create(ctx.getContext(), "consume", fct);
    BasicBlock *check_label = BasicBlock::Create(ctx.getContext(), "check_label", fct);
    BasicBlock *null = BasicBlock::Create(ctx.getContext(), "false", fct);



    auto gdb = fct->args().begin();
    auto oid = fct->args().begin() + 1;
    auto qr_tuple_list = fct->args().begin() + 2;
    auto rs = fct->args().begin() + 3;
    auto prev_size = fct->args().begin() + 4;
    auto ty = fct->args().begin() + 5;
    auto call_map_arg = fct->args().begin() + 6;
    auto offset = fct->args().begin() + 7;
    auto get_node_by_id = ctx.extern_func("node_by_id");

    ctx.getBuilder().SetInsertPoint(bb);
    auto call_map = ctx.getBuilder().CreateBitCast(fct->args().begin() + 6, ctx.callMapPtrTy);

    auto qr_size = ctx.getBuilder().CreateAdd(prev_size, ctx.LLVM_ONE);

    auto qrl = ctx.getBuilder().CreateBitCast(qr_tuple_list, ctx.res_arr_type->getPointerTo());

    auto prev_pos = ctx.getBuilder().CreateAdd(prev_size, offset);
    auto insert_pos = ctx.getBuilder().CreateAdd(prev_pos, ctx.LLVM_ONE);
    auto pres = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateInBoundsGEP(qrl, {ctx.LLVM_ZERO, prev_pos}));

    auto rship = ctx.getBuilder().CreateBitCast(pres, ctx.rshipPtrTy);

    auto exp_rship = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateStructGEP(rship, exp_id));

    //get node by id
    auto node = ctx.getBuilder().CreateCall(get_node_by_id, {gdb, exp_rship});

    auto null_node = Constant::getNullValue(ctx.int8PtrTy);
    auto cmp_node = ctx.getBuilder().CreateBitCast(node, ctx.int8PtrTy);
    auto cmp = ctx.getBuilder().CreateICmpEQ(null_node, cmp_node);

    if(op->label_.empty())
        ctx.getBuilder().CreateCondBr(cmp, null, consume);
    else
        ctx.getBuilder().CreateCondBr(cmp, null, check_label);


    if(!op->label_.empty())
    {
        ctx.getBuilder().SetInsertPoint(check_label);
        {
            auto label_code = ConstantInt::get(ctx.int32Ty, APInt(32, ctx.get_dcode(op->label_)));
            auto lc_ptr = ctx.getBuilder().CreateStructGEP(node, 5);
            auto lc = ctx.getBuilder().CreateLoad(lc_ptr);
            auto cond = ctx.getBuilder().CreateICmpEQ(lc, label_code);
            ctx.getBuilder().CreateCondBr(cond, consume, null);
        }
    }


    ctx.getBuilder().SetInsertPoint(consume);
    {
        // set size field or qr to 1
        auto size_field = ctx.getBuilder().CreateInBoundsGEP(qrl, {ctx.LLVM_ZERO, ctx.LLVM_ZERO});
        auto sz = ctx.getBuilder().CreateLoad(size_field);
        auto sz_val = ctx.getBuilder().CreateAdd(ctx.getBuilder().CreateLoad(sz), ctx.LLVM_ONE);
        ctx.getBuilder().CreateStore(sz_val, sz);
        ctx.getBuilder().CreateStore(sz, size_field);

        auto src_i = ctx.getBuilder().CreateInBoundsGEP(qrl, {ctx.LLVM_ZERO, insert_pos});
        auto n = ctx.getBuilder().CreateBitCast(node, ctx.int64PtrTy);
        ctx.getBuilder().CreateStore(n, src_i);
        auto forward = ctx.getBuilder().CreateBitCast(qrl, ctx.int64PtrTy);

        // extract fct ptr from call map
        auto fct_ptr = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateInBoundsGEP(call_map, {ctx.LLVM_ZERO, oid}));

        // increment oid for next operator
        auto next_oid = ctx.getBuilder().CreateAdd(oid, ctx.LLVM_ONE);

        // call consumer
        ctx.getBuilder().CreateCall(fct_ptr, {gdb, next_oid, forward, rs, qr_size, ty, call_map_arg, offset});

        ctx.getBuilder().CreateRet(nullptr);
    }

    ctx.getBuilder().SetInsertPoint(null);
    {
        ctx.getBuilder().CreateRet(nullptr);
    }

    ctx.gen_funcs[op->name_] = fct;
    for (auto &inp : op->inputs_) {
        inp->set_consumer(fct);
    }

}

void codegen_visitor::visit(std::shared_ptr<filter_op> op) {
    int cnt = 0;
    auto func_name = op->name_;
    while(ctx.gen_funcs.find(func_name) != ctx.gen_funcs.end()) {
        func_name = op->name_+std::to_string(++cnt);
    }
    op->name_ = func_name;

    Function *fct = Function::Create(ctx.consumerFctTy, Function::ExternalLinkage, func_name + op->op_name_,
                                     ctx.getModule());
    FunctionCallee ls_fct = ctx.extern_func("list_size");

    BasicBlock *entry = BasicBlock::Create(ctx.getContext(), "entry", fct);
    BasicBlock *consume = BasicBlock::Create(ctx.getContext(), "consume", fct);
    BasicBlock *false_pred = BasicBlock::Create(ctx.getContext(), "false", fct);

    ctx.getBuilder().SetInsertPoint(entry);
    auto gdb = fct->args().begin();
    auto oid = fct->args().begin() + 1;
    auto qr_tuple_list = fct->args().begin() + 2;
    auto rs = fct->args().begin() + 3;
    auto qr_size = fct->args().begin() + 4;
    auto ty = fct->args().begin() + 5;
    auto call_map_arg = fct->args().begin() + 6;
    auto offset = fct->args().begin() + 7;
    auto call_map = ctx.getBuilder().CreateBitCast(fct->args().begin() + 6, ctx.callMapPtrTy);

    auto qrl = ctx.getBuilder().CreateBitCast(qr_tuple_list, ctx.res_arr_type->getPointerTo());

    auto prev_pos = ctx.getBuilder().CreateAdd(qr_size, offset);
    auto pres = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateInBoundsGEP(qrl, {ctx.LLVM_ZERO, prev_pos}));

    auto vis = fep_visitor(&ctx, fct, pres, consume, false_pred);
    op->fexpr_->accept(0, vis);

    ctx.getBuilder().SetInsertPoint(consume);
    {
        // extract fct ptr from call map
        auto fct_ptr = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateInBoundsGEP(call_map, {ctx.LLVM_ZERO, oid}));

        // increment oid for next operator
        auto next_oid = ctx.getBuilder().CreateAdd(oid, ctx.LLVM_ONE);

        // call consumer
        ctx.getBuilder().CreateCall(fct_ptr, {gdb, next_oid, qr_tuple_list, rs, qr_size, ty, call_map_arg, offset});

        ctx.getBuilder().CreateRet(nullptr);
    }

    ctx.getBuilder().SetInsertPoint(false_pred);
    {
        //ctx.getBuilder().CreateCall(ls_fct, {qrl});
        ctx.getBuilder().CreateRet(nullptr);
    }

    ctx.gen_funcs[op->name_] = fct;
    for (auto &inp : op->inputs_) {
        inp->set_consumer(fct);
    }

}

void codegen_visitor::visit(std::shared_ptr<project> op) {
    op->new_types;
    for(auto & e : op->prexpr_) {
        op->name_ = op->name_ + e.key;
        switch(e.type) {
            case FTYPE::INT:
                op->new_types.push_back(2);
                break;
            case FTYPE::DOUBLE:
                op->new_types.push_back(3);
                break;
            case FTYPE::STRING:
                op->new_types.push_back(4);
                break;
            case FTYPE::TIME:
                op->new_types.push_back(5);
                break;
            case FTYPE::DATE:
                op->new_types.push_back(6);
                break;
        }
    }

    int cnt = 0;
    auto func_name = op->name_;
    while(ctx.gen_funcs.find(func_name) != ctx.gen_funcs.end()) {
        func_name = op->name_+std::to_string(++cnt);
    }
    op->name_ = func_name;

    auto max_ =
            std::max_element(op->prexpr_.begin(), op->prexpr_.end(),
                             [](pr_expr &e1, pr_expr &e2) { return e1.id < e2.id; });

    auto apply_pexpr = ctx.extern_func("apply_pexpr");
    Function *fct = Function::Create(ctx.consumerFctTy, Function::ExternalLinkage, func_name,ctx.getModule());
    BasicBlock *bb = BasicBlock::Create(ctx.getContext(), "entry", fct);


    auto new_res_arr_type = ArrayType::get(ctx.int64PtrTy, op->prexpr_.size() + 1);

    ctx.getBuilder().SetInsertPoint(bb);
    auto gdb = fct->args().begin();
    auto oid = fct->args().begin() + 1;
    auto qr_tuple_list = fct->args().begin() + 2;
    auto rs = fct->args().begin() + 3;
    auto qr_size = fct->args().begin() + 4;
    auto ty = fct->args().begin() + 5;
    auto call_map_arg = fct->args().begin() + 6;
    auto offset = fct->args().begin() + 7;
    auto call_map = ctx.getBuilder().CreateBitCast(fct->args().begin() + 6, ctx.callMapPtrTy);
    std::map<std::size_t, Value*> vmap;

    auto qrl = ctx.getBuilder().CreateBitCast(qr_tuple_list, ctx.res_arr_type->getPointerTo());

    // pointer to new type vector
    auto types_raw = ConstantInt::get(ctx.int64Ty, (int64_t )&op->new_types);

    // new forward parameters
    auto nqr_size = ConstantInt::get(ctx.int64Ty, op->prexpr_.size() + 1);
    auto nty = ctx.getBuilder().CreateIntToPtr(types_raw, ctx.int64PtrTy);
    auto qr_sz = ConstantInt::get(ctx.int64Ty, op->prexpr_.size());

    auto * resAlloc = ctx.getBuilder().CreateAlloca(new_res_arr_type);
    auto sz_alloc = ctx.getBuilder().CreateAlloca(ctx.int64Ty);
    ctx.getBuilder().CreateStore(qr_sz, sz_alloc);

    auto i = 1;
    for(auto pe : op->prexpr_) {
        GlobalVariable *key = ctx.getBuilder().CreateGlobalString(StringRef(pe.key), "pkey_"+pe.key);

        auto k = ctx.getBuilder().CreateBitCast(key, ctx.int8PtrTy);
        auto id = ConstantInt::get(ctx.int64Ty, APInt(64, pe.id));

        auto idx = ConstantInt::get(ctx.int64Ty, APInt(64, pe.id+1));
        auto qr = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateInBoundsGEP(qrl, {ctx.LLVM_ZERO, idx}));
        auto type = ConstantInt::get(ctx.int64Ty, APInt(64, static_cast<int>(pe.type)));

        auto nidx = ConstantInt::get(ctx.int64Ty, i);
        auto dest = ctx.getBuilder().CreateInBoundsGEP(resAlloc, {ctx.LLVM_ZERO, nidx});

        auto pv = ctx.getBuilder().CreateAlloca(ctx.int64Ty);

        ctx.getBuilder().CreateCall(apply_pexpr, {gdb, k, type, qr, id, ty, pv});
        ctx.getBuilder().CreateStore(pv, dest);
        i++;
    }

    // set size field or qr to 1
    auto size_field = ctx.getBuilder().CreateInBoundsGEP(resAlloc, {ctx.LLVM_ZERO, ctx.LLVM_ZERO});
    ctx.getBuilder().CreateStore(sz_alloc, size_field);


    // extract fct ptr from call map
    auto fct_ptr = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateInBoundsGEP(call_map, {ctx.LLVM_ZERO, oid}));

    // increment oid for next operator
    auto next_oid = ctx.getBuilder().CreateAdd(oid, ctx.LLVM_ONE);

    // call consumer
    auto forward = ctx.getBuilder().CreateBitCast(resAlloc, ctx.int64PtrTy);
    ctx.getBuilder().CreateCall(fct_ptr, {gdb, next_oid, forward, rs, nqr_size, nty, call_map_arg, offset});

    ctx.getBuilder().CreateRet(nullptr);

    ctx.gen_funcs[op->name_] = fct;
    for (auto &inp : op->inputs_) {
        inp->set_consumer(fct);
    }
}

void codegen_visitor::visit(std::shared_ptr<join_op> op) {
    int cnt = 0;
    auto func_name = op->name_+std::to_string(op->op_id_);
    while(ctx.gen_funcs.find(func_name) != ctx.gen_funcs.end()) {
        func_name = op->name_+std::to_string(++cnt);
    }

    func_name = func_name + join_op_str(op->jop_) + "lhs";

    op->name_ = func_name;

    Function *fct = Function::Create(ctx.consumerFctTy, Function::ExternalLinkage,
                                     op->name_, ctx.getModule());
    BasicBlock *entry = BasicBlock::Create(ctx.getContext(), "entry", fct);
    BasicBlock *concat_qrl = BasicBlock::Create(ctx.getContext(), "concat_qrl", fct);
    BasicBlock *incr_loop = BasicBlock::Create(ctx.getContext(), "incr_loop", fct);
    BasicBlock *consume = BasicBlock::Create(ctx.getContext(), "consume", fct);
    BasicBlock *consume_rship = BasicBlock::Create(ctx.getContext(), "consume_rship", fct);
    BasicBlock *consume_next = BasicBlock::Create(ctx.getContext(), "consume_next_left", fct);
    BasicBlock *for_each_rship = BasicBlock::Create(ctx.getContext(), "for_each_rship", fct);
    BasicBlock *for_each_next = BasicBlock::Create(ctx.getContext(), "for_each_next_rship", fct);
    BasicBlock *end = BasicBlock::Create(ctx.getContext(), "end", fct);

    auto get_join_vec = ctx.extern_func("get_join_vec_arr");
    auto get_join_vec_size = ctx.extern_func("get_join_vec_size");

    ctx.getBuilder().SetInsertPoint(entry);
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

    auto max_idx = ctx.getBuilder().CreateAlloca(ctx.int64Ty);
    auto cur_idx = ctx.getBuilder().CreateAlloca(ctx.int64Ty);
    ctx.getBuilder().CreateStore(ctx.LLVM_ZERO, cur_idx);

    //get input vector from join obj
    auto inputs_vec_raw = ConstantInt::get(ctx.int64Ty, (int64_t )&op->join_inputs_);
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

    if(op->jop_ == JOIN_OP::CROSS) {
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
                                                      ctx.getBuilder().CreateBr(concat_qrl);
                                                  });

        ctx.getBuilder().SetInsertPoint(concat_qrl);
        // increment index and store
        auto idx = ctx.getBuilder().CreateLoad(cur_idx);
        auto nidx = ctx.getBuilder().CreateAdd(idx, ctx.LLVM_ONE);
        ctx.getBuilder().CreateStore(nidx, cur_idx);

        // load rhs tuple from stack
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
        ctx.getBuilder().CreateCall(fct_ptr, {gdb, noid, forward, rs, nsize, ty, call_map_arg, offset});
        ctx.getBuilder().CreateBr(loop_body);
    } else {

    }

    ctx.getBuilder().SetInsertPoint(end);
    {
        ctx.getBuilder().CreateRetVoid();
    }


    ctx.gen_funcs[op->name_] = fct;
}

void codegen_visitor::visit(std::shared_ptr<collect_op> op) {
    cur_size = op->op_id_;
    int cnt = 0;
    auto func_name = op->name_;
    while(ctx.gen_funcs.find(func_name) != ctx.gen_funcs.end()) {
        func_name = op->name_+std::to_string(++cnt);
    }
    op->name_ = func_name;

    Function *fct = Function::Create(ctx.consumerFctTy, Function::ExternalLinkage, func_name,
                                     ctx.getModule());
    FunctionCallee collect = ctx.extern_func("collect");

    BasicBlock *bb = BasicBlock::Create(ctx.getContext(), "entry", fct);
    ctx.getBuilder().SetInsertPoint(bb);

    auto gdb = fct->args().begin();
    auto tid = fct->args().begin() + 1;
    auto qr_tuple_list = fct->args().begin() + 2;
    auto rs = fct->args().begin() + 3;
    auto qr_size = fct->args().begin() + 4;
    auto ty = fct->args().begin() + 5;

    ctx.getBuilder().CreateCall(collect, {gdb, qr_tuple_list, rs, qr_size, ty});

    ctx.getBuilder().CreateRet(nullptr);

    ctx.gen_funcs[op->name_] = fct;
    for (auto &inp : op->inputs_) {
        inp->set_consumer(fct);
    }
}

void codegen_visitor::visit(std::shared_ptr<sort_op> op) {
    cur_size = op->op_id_;
    int cnt = 0;
    auto func_name = op->name_;
    while(ctx.gen_funcs.find(func_name) != ctx.gen_funcs.end()) {
        func_name = op->name_+std::to_string(++cnt);
    }
    op->name_ = func_name;

    Function *fct = Function::Create(ctx.finishFctTy, Function::ExternalLinkage, func_name,
                                     ctx.getModule());

    BasicBlock *bb = BasicBlock::Create(ctx.getContext(), "entry", fct);
    ctx.getBuilder().SetInsertPoint(bb);

    auto rs = fct->args().begin();

    auto sort_fc_raw = ConstantInt::get(ctx.int64Ty, (int64_t)sort_op::sort);
    auto sort_fc_ptr = ctx.getBuilder().CreateIntToPtr(sort_fc_raw, ctx.int64PtrTy);
    auto sort_fc = ctx.getBuilder().CreateBitCast(sort_fc_ptr, ctx.finishFctTy->getPointerTo());

    ctx.getBuilder().CreateCall(sort_fc, {rs});


    ctx.getBuilder().CreateRet(nullptr);

    ctx.gen_funcs[op->name_] = fct;
    for (auto &inp : op->inputs_) {
        inp->set_consumer(fct);
    }
}

void codegen_visitor::visit(std::shared_ptr<limit_op> op) {


    Function *fct = Function::Create(ctx.consumerFctTy, Function::ExternalLinkage, "Limit",
                                     ctx.getModule());


    BasicBlock *bb = BasicBlock::Create(ctx.getContext(), "entry", fct);
    BasicBlock *consume = BasicBlock::Create(ctx.getContext(), "consume", fct);
    BasicBlock *end = BasicBlock::Create(ctx.getContext(), "end", fct);

    ctx.getBuilder().SetInsertPoint(bb);
    ctx.getModule().getOrInsertGlobal("limit", ctx.int64Ty);
    GlobalVariable *global_count = ctx.getModule().getGlobalVariable("global_count");
    global_count->setInitializer(ConstantInt::get(ctx.int64Ty, 0));
    //global_count->setConstant(false);

    auto gdb = fct->args().begin();
    auto oid = fct->args().begin() + 1;
    auto qr_tuple_list = fct->args().begin() + 2;
    auto rs = fct->args().begin() + 3;
    auto prev_size = fct->args().begin() + 4;
    auto ty = fct->args().begin() + 5;
    auto call_map_arg = fct->args().begin() + 6;
    auto offset = fct->args().begin() + 7;
    auto call_map = ctx.getBuilder().CreateBitCast(fct->args().begin() + 6, ctx.callMapPtrTy);
    auto noid = ctx.getBuilder().CreateAdd(oid, ctx.LLVM_ONE);
    auto fct_ptr = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateInBoundsGEP(call_map, {ctx.LLVM_ZERO, oid}));

    auto limit = ConstantInt::get(ctx.int64Ty, op->limit_);
    auto count = ctx.getBuilder().CreateLoad(global_count);
    auto incr_count = ctx.getBuilder().CreateAdd(count, ctx.LLVM_ONE);
    ctx.getBuilder().CreateStore(incr_count, global_count);
    auto cmp_limt = ctx.getBuilder().CreateICmpEQ(limit, incr_count);
    ctx.getBuilder().CreateCondBr(cmp_limt, consume, end);

    ctx.getBuilder().SetInsertPoint(consume);
    ctx.getBuilder().CreateCall(fct_ptr, {gdb, noid, qr_tuple_list, rs, prev_size, ty, call_map_arg, offset});
    ctx.getBuilder().CreateRetVoid();

    ctx.getBuilder().SetInsertPoint(end);
    ctx.getBuilder().CreateRetVoid();

}

void codegen_visitor::visit(std::shared_ptr<end_op> op) {
    int cnt = 0;
    std::string func_name = "joininsertrhs";
    while(ctx.gen_funcs.find(func_name) != ctx.gen_funcs.end()) {
        func_name = op->name_+std::to_string(++cnt);
    }
    op->name_ = func_name;

    Function *fct = Function::Create(ctx.consumerFctTy, Function::ExternalLinkage,
                                     func_name, ctx.getModule());
    auto join_insert_left = ctx.extern_func("join_insert_left");
    BasicBlock *bb = BasicBlock::Create(ctx.getContext(), "entry", fct);
    ctx.getBuilder().SetInsertPoint(bb);

    //get input vector from join obj
    auto inputs_vec_raw = ConstantInt::get(ctx.int64Ty, (int64_t )op->join_inputs_);
    auto inputs_vec = ctx.getBuilder().CreateIntToPtr(inputs_vec_raw, ctx.int64PtrTy);

    auto qrl = fct->args().begin() + 2;

    // insert left results in global list -> extern function
    ctx.getBuilder().CreateCall(join_insert_left, {inputs_vec, qrl});

    ctx.getBuilder().CreateRet(nullptr);
    ctx.gen_funcs[op->name_] = fct;
}

void codegen_visitor::visit(std::shared_ptr<create_op> op) {
    int cnt = 0;
    std::string func_name = op->name_;
    while(ctx.gen_funcs.find(func_name) != ctx.gen_funcs.end()) {
        func_name = op->name_+std::to_string(++cnt);
    }
    op->name_ = func_name;

    auto create_node = ctx.extern_func("create_node");
    auto create_rship = ctx.extern_func("create_rship");

    Function *fct = Function::Create(ctx.consumerFctTy, Function::ExternalLinkage,
                                     func_name, ctx.getModule());

    BasicBlock *bb = BasicBlock::Create(ctx.getContext(), "entry", fct);
    BasicBlock *consume = BasicBlock::Create(ctx.getContext(), "consume", fct);

    ctx.getBuilder().SetInsertPoint(bb);
    auto gdb = fct->args().begin();
    auto oid = fct->args().begin() + 1;
    auto qr_tuple_list = fct->args().begin() + 2;
    auto rs = fct->args().begin() + 3;
    auto prev_size = fct->args().begin() + 4;
    auto ty = fct->args().begin() + 5;
    auto call_map_arg = fct->args().begin() + 6;
    auto offset = fct->args().begin() + 7;
    auto call_map = ctx.getBuilder().CreateBitCast(fct->args().begin() + 6, ctx.callMapPtrTy);
    auto noid = ctx.getBuilder().CreateAdd(oid, ctx.LLVM_ONE);
    auto fct_ptr = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateInBoundsGEP(call_map, {ctx.LLVM_ZERO, oid}));

    auto qr_size = ctx.getBuilder().CreateAdd(prev_size, ctx.LLVM_ONE);
    auto qrl = ctx.getBuilder().CreateBitCast(qr_tuple_list, ctx.res_arr_type->getPointerTo());
    auto prev_pos = ctx.getBuilder().CreateAdd(prev_size, offset);
    auto insert_pos = ctx.getBuilder().CreateAdd(prev_pos, ctx.LLVM_ONE);
    auto pres = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateInBoundsGEP(qrl, {ctx.LLVM_ZERO, prev_pos}));

    GlobalVariable *label = ctx.getBuilder().CreateGlobalString(op->label_, "create_label");
    auto label_ptr = ctx.getBuilder().CreateBitCast(label, ctx.int8PtrTy);

    //get input vector from join obj
    auto props_raw = ConstantInt::get(ctx.int64Ty, (int64_t )&op->props_);
    auto props = ctx.getBuilder().CreateIntToPtr(props_raw, ctx.int64PtrTy);

    Value *res;

    if(op->ctype_ == create_type::node) {

        res = ctx.getBuilder().CreateCall(create_node, {gdb, label_ptr, props});

    } else {
        auto id1 = ConstantInt::get(ctx.int64Ty, op->src_des_.first);
        auto id2 = ConstantInt::get(ctx.int64Ty, op->src_des_.second);
        auto pn1 = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateInBoundsGEP(qrl, {ctx.LLVM_ZERO, id1}));
        auto pn2 = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateInBoundsGEP(qrl, {ctx.LLVM_ZERO, id2}));
        auto n1 = ctx.getBuilder().CreateBitCast(pn1, ctx.nodePtrTy);
        auto n2 = ctx.getBuilder().CreateBitCast(pn2, ctx.nodePtrTy);

        res = ctx.getBuilder().CreateCall(create_rship, {gdb, label_ptr, n1, n2, props});
    }

    auto size_field = ctx.getBuilder().CreateInBoundsGEP(qrl, {ctx.LLVM_ZERO, ctx.LLVM_ZERO});
    auto sz = ctx.getBuilder().CreateLoad(size_field);
    auto sz_val = ctx.getBuilder().CreateAdd(ctx.getBuilder().CreateLoad(sz), ctx.LLVM_ONE);
    ctx.getBuilder().CreateStore(sz_val, sz);
    ctx.getBuilder().CreateStore(sz, size_field);

    auto src_i = ctx.getBuilder().CreateInBoundsGEP(qrl, {ctx.LLVM_ZERO, insert_pos});
    auto ires = ctx.getBuilder().CreateBitCast(res, ctx.int64PtrTy);
    ctx.getBuilder().CreateStore(ires, src_i);
    auto forward = ctx.getBuilder().CreateBitCast(qrl, ctx.int64PtrTy);

    // increment oid for next operator
    auto next_oid = ctx.getBuilder().CreateAdd(oid, ctx.LLVM_ONE);

    // call consumer
    ctx.getBuilder().CreateCall(fct_ptr, {gdb, next_oid, forward, rs, qr_size, ty, call_map_arg, offset});

    ctx.getBuilder().CreateRetVoid();
}
