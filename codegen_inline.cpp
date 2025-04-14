#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include "codegen_inline.hpp"
#include "filter_expression_inline.hpp"

/*
* Function initialisation at first access path
*/
void codegen_inline_visitor::init_function(BasicBlock *entry) {

    // create function entry block
    ctx.getBuilder().SetInsertPoint(entry);

    // allocate result counter and initialize with 0
    res_counter = ctx.getBuilder().CreateAlloca(ctx.int64Ty);
    ctx.getBuilder().CreateStore(ctx.LLVM_ZERO, res_counter);
    resAlloc = ctx.getBuilder().CreateAlloca(ctx.res_arr_type);

    // forward parameters
    qr_size = ConstantInt::get(ctx.int64Ty, 1);
    sizes.push_back(qr_size);

    // rhs loop counter, used in while_loop
    // intialize with UNKNOWN ID
    rhs_alloca = ctx.getBuilder().CreateAlloca(ctx.int64Ty);
    ctx.getBuilder().CreateStore(ctx.UNKNOWN_ID, rhs_alloca);
    strAlloc = ctx.getBuilder().CreateAlloca(ctx.int8PtrTy);

    // allocate space for lhs, used in while_loop
    lhs_alloca = ctx.getBuilder().CreateAlloca(ctx.int64Ty);

    // allocate space for pitem iteration
    cur_item_arr = ctx.getBuilder().CreateAlloca(ctx.pSetRawArrTy);
    cur_item = ctx.getBuilder().CreateAlloca(ctx.pitemTy);
    cur_pset = ctx.getBuilder().CreateAlloca(ctx.propertySetPtrTy);
    pitem = ctx.getBuilder().CreateAlloca(ctx.pitemTy);
    plist_id = ctx.getBuilder().CreateAlloca(ctx.int64Ty);

    new_res_arr_type = ArrayType::get(ctx.int64PtrTy, new_type_vec.size() + 1);
    newResAlloc = ctx.getBuilder().CreateAlloca(new_res_arr_type);

    // allocate space for each projection variable
    for(auto & i : pv)
        i = ctx.getBuilder().CreateAlloca(ctx.int64Ty);

    // create vector of projection keys
    project_keys.resize(project_string.size());
    int i = 0;
    for(auto & s : project_string) {
        auto prj_alloc = ctx.getBuilder().CreateAlloca(ctx.int8PtrTy);
        GlobalVariable *label = ctx.getBuilder().CreateGlobalString(StringRef(s), "pkey_"+s);
        auto str_ptr = ctx.getBuilder().CreateBitCast(label, ctx.int8PtrTy);
        ctx.getBuilder().CreateStore(str_ptr, prj_alloc);
        project_keys[i] = ctx.getBuilder().CreateLoad(prj_alloc);
        i++;
    }

    // allocate space for iteration variables
    max_idx = ctx.getBuilder().CreateAlloca(ctx.int64Ty);
    max_cnt = ctx.getBuilder().CreateAlloca(ctx.int64Ty);
    ctx.getBuilder().CreateStore(ctx.MAX_PITEM_CNT, max_cnt);
    cur_idx = ctx.getBuilder().CreateAlloca(ctx.int64Ty);
    loop_cnt = ctx.getBuilder().CreateAlloca(ctx.int64Ty);

    cross_join_idx.push_back(ctx.getBuilder().CreateAlloca(ctx.int64Ty));
    //cross_join_rship = ctx.getBuilder().CreateAlloca(ctx.rshipPtrTy);
    dangling = ctx.getBuilder().CreateAlloca(ctx.int64Ty);    
}

/*
* Generates code for a scan operator.
*/
void codegen_inline_visitor::visit(std::shared_ptr<scan_op> op) {
    // clear register mapping of previous access path
    reg_query_results.clear();
    // process the appropriate query length
    query_length(op);

    // check if this operator is the first access path
    if(!main_function) {
        // if it is the first access path -> intialize finish function
        Function *df_finish = Function::Create(ctx.finishFctTy, Function::ExternalLinkage, "default_finish_"+qid_,
                                               ctx.getModule());

        BasicBlock *df_finish_bb = BasicBlock::Create(ctx.getContext(), "entry", df_finish);
        ctx.getBuilder().SetInsertPoint(df_finish_bb);
        ctx.getBuilder().CreateRetVoid();

        // create unique function name for the JIT instance
        int cnt = 0;
        auto func_name = op->name_;
        while(ctx.gen_funcs.find(func_name) != ctx.gen_funcs.end()) {
            func_name = op->name_+std::to_string(++cnt);
        }

        op->name_ = func_name+qid_;
    }

    // check for previous access
    bool access = false;
    if(!main_function) {
        // create IR function if no previous access path exists
        access = true;
        main_function = Function::Create(ctx.startFctTy, Function::ExternalLinkage, op->name_, ctx.getModule());
    }

    // obtain all relevant function callees for the scan operator
    FunctionCallee dict_lookup_label = ctx.extern_func("dict_lookup_label");
    FunctionCallee get_valid_node = ctx.extern_func("get_valid_node");
    FunctionCallee gdb_get_nodes = ctx.extern_func("gdb_get_nodes");
    FunctionCallee gdb_get_node_from_it = ctx.extern_func("get_node_from_it");
    FunctionCallee get_begin = ctx.extern_func("get_vec_begin");
    FunctionCallee get_next = ctx.extern_func("get_vec_next");
    FunctionCallee is_end = ctx.extern_func("vec_end_reached");

    /*
    * When a previous access path already exists, push the new entry block as the first block into the
    * function block list and link it as the finish block
    */
    BasicBlock *bb;
    BasicBlock *next_op;
    if(!access) {
        next_op = &main_function->getEntryBlock();
        next_op->setName("next_op");
        op->name_ = "";
    } else {
        bb = BasicBlock::Create(ctx.getModule().getContext(), "entry", main_function);
    }

    if(!access) {
        bb = BasicBlock::Create(ctx.getModule().getContext(), "entry");
        main_function->getBasicBlockList().push_front(bb);
    }
    scan_nodes_end = BasicBlock::Create(ctx.getModule().getContext(), "scan_nodes_end", main_function);
    BasicBlock *consumeBB = BasicBlock::Create(ctx.getModule().getContext(), "consume_node", main_function);

    // initialize all relevant functions
    init_function(bb);

        // node

    // obtain the query function arguments
    gdb = main_function->args().begin();
    auto first = main_function->args().begin() + 1;
    auto last = main_function->args().begin() + 2;
    auto tx_ptr = main_function->args().begin() + 3;
    auto oid = main_function->args().begin() + 4;
    ty = main_function->args().begin() + 5;
    rs = main_function->args().begin() + 6;
    finish = main_function->args().begin() + 8;
    offset = main_function->args().begin() + 9;
    queryArgs = main_function->args().begin() + 10;

    BasicBlock *curBB;

    // register for the current node
    Value *node;

    // scan all
    if(!op->indexed_) {
        // SCAN NODES
        // 1 get nodes vector from graph db
        auto nodes_vec = ctx.getBuilder().CreateCall(gdb_get_nodes, {gdb});

        // 2 lookup label in dict
        auto label_code = ctx.extract_arg_label(op->op_id_, gdb, queryArgs);

        // 3 obtain iterator from nodes vector
        auto node_begin_it = ctx.getBuilder().CreateCall(get_begin, {nodes_vec, first, last});

        // 4 iterate through node list
        curBB = ctx.while_loop(main_function, get_begin, get_next, is_end, node_begin_it, nodes_vec,
                                           scan_nodes_end, [&](BasicBlock *curBB, BasicBlock *epilog) {
                    // 5 obtain node from iterator
                    auto node_raw = ctx.getBuilder().CreateCall(gdb_get_node_from_it, {node_begin_it});

                    // 6 obtain valid node
                    node = ctx.getBuilder().CreateCall(get_valid_node, {gdb, node_raw, tx_ptr});

                    // 7 compare labels
                    auto cond = ctx.node_cmp_label(node, label_code);

                    // 8 if equal -> consume
                    ctx.getBuilder().CreateCondBr(cond, consumeBB, epilog);
                }, consumeBB);

    } else { // index scan

        // for index scan call extern gdb function and branch to consumer
        auto get_index_node = ctx.extern_func("index_get_node");

        // get label and property key from parameter descriptor
        auto opid = ConstantInt::get(ctx.int64Ty, op->op_id_);
        auto prop_opid = ConstantInt::get(ctx.int64Ty, op->op_id_+1);
        auto val_opid = ConstantInt::get(ctx.int64Ty, op->op_id_+2);
        auto label = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateInBoundsGEP(queryArgs, {ctx.LLVM_ZERO, opid}));
        auto prop = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateInBoundsGEP(queryArgs, {ctx.LLVM_ZERO, prop_opid}));
        auto val = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateInBoundsGEP(queryArgs, {ctx.LLVM_ZERO, val_opid})));

        // call extern function to obtain node
        node = ctx.getBuilder().CreateCall(get_index_node, {gdb,
                                                            ctx.getBuilder().CreateBitCast(label, ctx.int8PtrTy),
                                                            ctx.getBuilder().CreateBitCast(prop, ctx.int8PtrTy), val});

        // check if node ptr is null
        auto nullptr_cmp = ctx.getBuilder().CreateIsNotNull(node);

    	// consume or goto finish
        ctx.getBuilder().CreateCondBr(nullptr_cmp, consumeBB, scan_nodes_end);
        curBB = scan_nodes_end;
    }

    // branch to the next oeprator
    ctx.getBuilder().SetInsertPoint(consumeBB);
    {
        // add current node to register
        reg_query_results.push_back({node, 0});
        main_return = curBB;
        // next operator fills this basic block and adds terminator
    }

    // branch to the finish function
    ctx.getBuilder().SetInsertPoint(scan_nodes_end);
    {
        if(!access) {
            ctx.getBuilder().CreateBr(next_op);
        } else {
            global_end = scan_nodes_end;
            ctx.getBuilder().CreateCall(ctx.finishFctTy, finish, {rs});
            ctx.getBuilder().CreateRet(nullptr);
        }
    }

    // set appropriate backflow
    prev_bb = consumeBB;
    ctx.gen_funcs[op->name_] = main_function;
}

/**
 * Generates code for the foreach relationship operator.
 */
void codegen_inline_visitor::visit(std::shared_ptr<foreach_rship_op> op) {
    op->name_ = "";

    // obtain the relationship field ids from the direction
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

    // obtain the FunctionCallees for the operator processing
    FunctionCallee rship_by_id = ctx.extern_func("rship_by_id");
    FunctionCallee gdb_get_rships = ctx.extern_func("gdb_get_rships");
    FunctionCallee ohop_count = ctx.extern_func("count_potential_o_hop");
    FunctionCallee get_rship_queue = ctx.extern_func("retrieve_fev_queue");
    FunctionCallee insert_to_queue = ctx.extern_func("insert_fev_rship");

    FunctionCallee retrieve_rships = ctx.extern_func("foreach_from_variable_rship");
    FunctionCallee get_next_rship = ctx.extern_func("get_next_rship_fev");
    FunctionCallee fev_list_end = ctx.extern_func("fev_list_end");

    // create the basic blocks for the operator processing
    BasicBlock *fe_entry = BasicBlock::Create(ctx.getContext(), "fe_entry", main_function);
    BasicBlock *foreach_rship_end = BasicBlock::Create(ctx.getModule().getContext(), "foreach_rel_end", main_function);
    BasicBlock *nextBB = BasicBlock::Create(ctx.getModule().getContext(), "next_rship", main_function);
    BasicBlock *consumeBB = BasicBlock::Create(ctx.getModule().getContext(), "consume_rship", main_function);

    // basic blocks for the fev operator
    BasicBlock *loop_head = BasicBlock::Create(ctx.getContext(), "fev_loop_head", main_function);
    BasicBlock *loop_body = BasicBlock::Create(ctx.getContext(), "fev_loop_body", main_function);
    BasicBlock *loop_next = BasicBlock::Create(ctx.getContext(), "fev_loop_next", main_function);

    // link the entry point with the block of the previous operator
    ctx.getBuilder().SetInsertPoint(prev_bb);
    ctx.getBuilder().CreateBr(fe_entry);
    ctx.getBuilder().SetInsertPoint(fe_entry);

    // get the previous tuple result
    auto node = reg_query_results.back().reg_val;

    auto UNKNOWN_REL_ID = ConstantInt::get(ctx.int64Ty,
                                           std::numeric_limits<int64_t>::max()); // TODO: UNKNOWN VALUE = std::numeric_limits<int64_t>::max()
    ctx.getBuilder().CreateStore(UNKNOWN_REL_ID, rhs_alloca);

    GlobalVariable *label = ctx.getBuilder().CreateGlobalString(StringRef(op->label_), "rship_label");

    auto strPtr = ctx.getBuilder().CreateBitCast(label, ctx.int8PtrTy);
    ctx.getBuilder().CreateStore(strPtr, strAlloc);

    //auto label_code = ConstantInt::get(ctx.int32Ty, APInt(32, ctx.get_dcode(op->label_)));

    // extract label from arguments and obtain label code
    auto label_code = ctx.extract_arg_label(op->op_id_, gdb, queryArgs);

    // extract the first relationship id from the node
    auto node_rship_id = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateStructGEP(node, node_rship_idx));
    ctx.getBuilder().CreateStore(node_rship_id, lhs_alloca);

    // register value for the current relationship
    Value *rship;

    Value *hop_cnt;
    // generate code depending on variable traversion or single
    if(op->is_variable()) {
        // for variable traversion
        // transform min and max hop into constants
        auto min_hop = ConstantInt::get(ctx.int64Ty, op->hops_.first);
        auto max_hop = ConstantInt::get(ctx.int64Ty, op->hops_.second);

        // retrieve all relationships and store it into a intermediate vector (externally)
        ctx.getBuilder().CreateCall(retrieve_rships, {gdb, label_code, node, min_hop, max_hop});        

        // simple loop in order to traverse the previous filled vector
        ctx.getBuilder().CreateBr(loop_head);
        // the loop header
        ctx.getBuilder().SetInsertPoint(loop_head);
        // check if the end of the vector is reached (externally) and branch to body or end
        auto is_end = ctx.getBuilder().CreateCall(fev_list_end, {});
        ctx.getBuilder().CreateCondBr(is_end, foreach_rship_end, loop_body);

        // in the body, simply obtain the relationship (externally) and move the address to a register value
        ctx.getBuilder().SetInsertPoint(loop_body);
        rship = ctx.getBuilder().CreateCall(get_next_rship, {});

        // branch to the next operator
        ctx.getBuilder().CreateBr(consumeBB);

    } else {
        // iterate through the relationship list of the node
        auto loop_body = ctx.while_loop_condition(main_function, lhs_alloca, rhs_alloca, PContext::WHILE_COND::LT, foreach_rship_end,
                                                [&](BasicBlock *, BasicBlock *) {
                                                    // extract relationship id from alloca
                                                    auto lhs = ctx.getBuilder().CreateLoad(lhs_alloca);

                                                    // obtain the relationship through an extern function call
                                                    rship = ctx.getBuilder().CreateCall(rship_by_id, {gdb, lhs});

                                                    // check for the given label
                                                    auto consume_cond = ctx.rship_cmp_label(rship, label_code);

                                                    // branch if relationship label is equal to the given label
                                                    ctx.getBuilder().CreateCondBr(consume_cond, consumeBB, nextBB);
                                                });

        // obtain the next relationship 
        ctx.getBuilder().SetInsertPoint(nextBB);
        {
            auto next_rship = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateStructGEP(rship, next_rship_idx));
            ctx.getBuilder().CreateStore(next_rship, lhs_alloca);
            ctx.getBuilder().CreateBr(loop_body);
        }
    }

    // consume the given relationship
    ctx.getBuilder().SetInsertPoint(consumeBB);
    {
        reg_query_results.push_back({rship, 1});
        prev_bb = consumeBB;
    }

    // backflow after complete iteration to the previous operator
    ctx.getBuilder().SetInsertPoint(foreach_rship_end);
    {
        // return to main loop of scan
        ctx.getBuilder().CreateBr(main_return);
    }

    if(op->is_variable()) {
        main_return = loop_head;
    } else {
        // set backflow to the iteration loop
        main_return = nextBB;
    }

}

/**
 * Generates code for a projection operator
 */ 
void codegen_inline_visitor::visit(std::shared_ptr<project> op) {
    op->name_ = "";

    // obtain FunctionCallee for apply_pexpr
    auto apply_pexpr = ctx.extern_func("apply_pexpr");
    auto apply_pexpr_node = ctx.extern_func("apply_pexpr_node");
    auto apply_pexpr_rship = ctx.extern_func("apply_pexpr_rship");

    // create entry block and link with the previous operator
    BasicBlock *entry = BasicBlock::Create(ctx.getContext(), "project_entry", main_function);
    ctx.getBuilder().SetInsertPoint(prev_bb);
    ctx.getBuilder().CreateBr(entry);
    ctx.getBuilder().SetInsertPoint(entry);
    std::map<std::size_t, Value*> vmap;

    auto i = 1;

    // add new projection variables to registers
    std::vector<QR_VALUE> nqrv;
    for(auto & pe : op->prexpr_) {
        if(pe.type == FTYPE::BOOLEAN) {
            nqrv.push_back(reg_query_results[pe.id]);
        } else {
            // get id of tuple for projection
            auto id = ConstantInt::get(ctx.int64Ty, APInt(64, pe.id));
            auto idx = ConstantInt::get(ctx.int64Ty, APInt(64, pe.id+1));
            //auto qrp = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateInBoundsGEP(qrl, {ctx.LLVM_ZERO, idx}));

            // get register value of tuple for projection
            auto r = reg_query_results[pe.id];
            auto qrp = ctx.getBuilder().CreateBitCast(r.reg_val, ctx.int64PtrTy);

            // get the appropriate type of the tuple
            auto type = ConstantInt::get(ctx.int64Ty, APInt(64, static_cast<int>(pe.type)));

            auto nidx = ConstantInt::get(ctx.int64Ty, i);
            auto dest = ctx.getBuilder().CreateInBoundsGEP(newResAlloc, {ctx.LLVM_ZERO, nidx});

            // apply the projection in an external function
            if(r.type == 0) {
                ctx.getBuilder().CreateCall(apply_pexpr_node, {gdb, project_keys[i-1], type, qrp, pv[i]});
            } else {
                ctx.getBuilder().CreateCall(apply_pexpr_rship, {gdb, project_keys[i-1], type, qrp, pv[i]});
            }

            //ctx.getBuilder().CreateCall(apply_pexpr, {gdb, project_keys[i-1], type, qrp, id, ty, pv[i]});

            // add new register value to global list
            nqrv.push_back({pv[i], static_cast<int>(pe.type)+2});

        }
        i++;
    }

    // switch old register list with the new one
    reg_query_results = nqrv;

    prev_bb = entry;
}

/**
 * Generates code for the expand operator
 */ 
void codegen_inline_visitor::visit(std::shared_ptr<expand_op> op) {
    // get the id of the appropriate expand operation (to or from rship)
    unsigned exp_id;
    switch (op->exp_) {
        case EXPAND::OUT:
            exp_id = 3;
            break;
        case EXPAND::IN:
            exp_id = 2;
            break;
    }
    op->name_ = "";

    // create the basic blocks of the expand operator
    BasicBlock *entry = BasicBlock::Create(ctx.getContext(), "expand_entry", main_function);
    BasicBlock *consume = BasicBlock::Create(ctx.getContext(), "expand_consume", main_function);
    BasicBlock *null = BasicBlock::Create(ctx.getContext(), "expand_false", main_function);
    BasicBlock *check_label = BasicBlock::Create(ctx.getContext(), "check_label", main_function);

    Value *label_code;

    // get the relevant FunctionCallee
    auto get_node_by_id = ctx.extern_func("node_by_id");

    // link with the previous operator
    ctx.getBuilder().SetInsertPoint(prev_bb);
    ctx.getBuilder().CreateBr(entry);
    ctx.getBuilder().SetInsertPoint(entry);

    // obtain the last query result from register -> relationship
    auto rship = reg_query_results.back().reg_val;

    // extract the node id from the relationship
    auto exp_rship = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateStructGEP(rship, exp_id));

    //get node by id
    auto node = ctx.getBuilder().CreateCall(get_node_by_id, {gdb, exp_rship});
    qr.push_back(node);

    // test if node is null
    auto null_node = Constant::getNullValue(ctx.int8PtrTy);
    auto cmp_node = ctx.getBuilder().CreateBitCast(node, ctx.int8PtrTy);
    auto cmp = ctx.getBuilder().CreateICmpEQ(null_node, cmp_node);

    // branch to different positions, when node label is given
    if(op->label_.empty())
        ctx.getBuilder().CreateCondBr(cmp, null, consume);
    else {
        label_code = ctx.extract_arg_label(op->op_id_, gdb, queryArgs);
        ctx.getBuilder().CreateCondBr(cmp, null, check_label);
    }

    // node label is given
    if(!op->label_.empty())
    {
        ctx.getBuilder().SetInsertPoint(check_label);
        {
            //auto label_code = ConstantInt::get(ctx.int32Ty, APInt(32, ctx.get_dcode(op->label_)));
            // compare labels 
            auto cond = ctx.node_cmp_label(node, label_code);

            // branch to appropriate block
            ctx.getBuilder().CreateCondBr(cond, consume, null);
        }
    }

    // add result to register list 
    ctx.getBuilder().SetInsertPoint(consume);
    {
        reg_query_results.push_back({node, 0});

        prev_bb = consume;
        // next operator
    }

    ctx.getBuilder().SetInsertPoint(null);
    {
        ctx.getBuilder().CreateBr(main_return);
    }
}

/**
 * Generates code for the filter operator
 */ 
void codegen_inline_visitor::visit(std::shared_ptr<filter_op> op) {
    op->name_ = "";

    // create all relevant basic blocks for the operator processing
    BasicBlock *entry = BasicBlock::Create(ctx.getContext(), "filter_entry", main_function);
    BasicBlock *consume = BasicBlock::Create(ctx.getContext(), "filter_consume", main_function);
    BasicBlock *false_pred = BasicBlock::Create(ctx.getContext(), "filter_false", main_function);

    // link with the previous operator
    ctx.getBuilder().SetInsertPoint(prev_bb);
    ctx.getBuilder().CreateBr(entry);
    ctx.getBuilder().SetInsertPoint(entry);

    // extract the argument from the argument desc
    auto opid = ConstantInt::get(ctx.int64Ty, op->op_id_);
    auto arg = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateInBoundsGEP(queryArgs, {ctx.LLVM_ZERO, opid})));
    
    // obtain the last query result
    auto pres = reg_query_results.back().reg_val;
    
    // generate code for each filter expression
    auto vis = fep_visitor_inline(&ctx, main_function, reg_query_results, consume, 
            false_pred, cur_item_arr, cur_item, cur_pset, 
            rhs_alloca, plist_id, 
            loop_cnt, max_cnt, pitem, arg);
    op->fexpr_->accept(0, vis);


    ctx.getBuilder().SetInsertPoint(consume);
    {
        // next operator
        prev_bb = consume;
    }

    ctx.getBuilder().SetInsertPoint(false_pred);
    {
        //return to main loop
        ctx.getBuilder().CreateBr(main_return);
    }
}

/**
 * Generates code for the collection
 */ 
void codegen_inline_visitor::visit(std::shared_ptr<collect_op> op) {
    cur_size = op->op_id_;
    op->name_ = "";

    // obtain all relevant FunctionCallees
    FunctionCallee mat_reg = ctx.extern_func("mat_reg_value");
    FunctionCallee collect_regs = ctx.extern_func("collect_tuple");

    // link with previous operator
    BasicBlock *entry = BasicBlock::Create(ctx.getContext(), "collect_entry", main_function);
    ctx.getBuilder().SetInsertPoint(prev_bb);
    ctx.getBuilder().CreateBr(entry);
    ctx.getBuilder().SetInsertPoint(entry);

    auto print_tuple = ConstantInt::get(ctx.boolTy, op->print_on_collect_);

    // create a single materialization call for each register value
    // each register will be materialized into thread local storage
    for(auto & res : reg_query_results) {
        // value type 7 => boolean, already transformed into integer
        auto type_id = (res.type == 7 ? 2 : res.type);
        auto type = ConstantInt::get(ctx.int64Ty, type_id);
        auto cv_reg = ctx.getBuilder().CreateBitCast(res.reg_val, ctx.int64PtrTy);
        ctx.getBuilder().CreateCall(mat_reg, {gdb, cv_reg, type});
    }

    // materialize the complete thread local storage into a global list
    ctx.getBuilder().CreateCall(collect_regs, {rs, print_tuple});

    ctx.getBuilder().CreateBr(main_return);
}

// process recursively the type vector of the rhs of a join
void get_rhs_type(std::shared_ptr<base_op>  &qop, std::vector<int> &typv) {
    auto op = qop;
    while(op->inputs_.size() > 0) {
        if(op->type_ == qop_type::scan)
            typv.push_back(0);
        else if(op->type_ == qop_type::foreach_rship)
            typv.push_back(1);
        else if(op->type_ == qop_type::expand)
            typv.push_back(0);
        else if(op->type_ == qop_type::cross_join) {
            auto jop = std::dynamic_pointer_cast<join_op>(op);
            get_rhs_type(jop->inputs_[1], typv);
        } else if(op->type_ == qop_type::left_join) {
            auto jop = std::dynamic_pointer_cast<join_op>(op);
            get_rhs_type(jop->inputs_[1], typv);
        }
        op = op->inputs_[0];
    }
}

/**
 * Generate code for the join operator
 */
void codegen_inline_visitor::visit(std::shared_ptr<join_op> op) {
    // obtain types of rhs
    std::vector<int> types;
    get_rhs_type(op->inputs_[1], types);

    op->name_ = "";

    // create all basic blocks for the join processing
    BasicBlock *join_lhs_entry = BasicBlock::Create(ctx.getContext(), "entry_join_lhs", main_function);
    BasicBlock *left_outer = BasicBlock::Create(ctx.getContext(), "left_outer", main_function);
    //BasicBlock *nested_loop = BasicBlock::Create(ctx.getContext(), "nested_loop", main_function);
    //BasicBlock *hash_join = BasicBlock::Create(ctx.getContext(), "hash_join_probe", main_function);

    BasicBlock *for_each_next = BasicBlock::Create(ctx.getContext(), "for_each_next_rship", main_function);
    BasicBlock *concat_qrl = BasicBlock::Create(ctx.getContext(), "concat_qrl", main_function);
    BasicBlock *incr_loop = BasicBlock::Create(ctx.getContext(), "incr_loop", main_function);
    BasicBlock *undang = BasicBlock::Create(ctx.getContext(), "undang", main_function);
    BasicBlock *return_handle = BasicBlock::Create(ctx.getContext(), "handle_ret", main_function);
    BasicBlock *consume = BasicBlock::Create(ctx.getContext(), "consume", main_function);
    BasicBlock *end = BasicBlock::Create(ctx.getContext(), "end", main_function);

    // obtain all FunctionCallees
    auto get_join_tp_at = ctx.extern_func("get_join_tp_at");
    auto node_reg = ctx.extern_func("get_node_res_at");
    auto rship_reg = ctx.extern_func("get_rship_res_at");
    auto rship_by_id = ctx.extern_func("rship_by_id");
    auto get_size = ctx.extern_func("get_mat_res_size");
    // link with previous operator
    ctx.getBuilder().SetInsertPoint(prev_bb);
    ctx.getBuilder().CreateBr(join_lhs_entry);
    ctx.getBuilder().SetInsertPoint(join_lhs_entry);

    // init idx and dangling flag
    ctx.getBuilder().CreateStore(ctx.LLVM_ONE, dangling);
    ctx.getBuilder().CreateStore(ctx.LLVM_ZERO, cur_idx);

    Value *tp;

    // get the join identifier and the appropriate rhs tuple list
    auto jid = ConstantInt::get(ctx.int64Ty, query_id_inline);
    auto ma = ctx.getBuilder().CreateCall(get_size, {jid});
    ctx.getBuilder().CreateStore(ma, max_idx);
    jids.push_back(query_id_inline);
    query_id_inline++;

    // iterate through the join list
    auto loop_body = ctx.while_loop_condition(main_function, cur_idx, max_idx, PContext::WHILE_COND::LT, end,
                                              [&](BasicBlock *body, BasicBlock *epilog) {
                                                  // get current index for join vector
                                                  auto idx = ctx.getBuilder().CreateLoad(cur_idx);
                                                  tp = ctx.getBuilder().CreateCall(get_join_tp_at, {jid, idx});


                                                  if(op->jop_ == JOIN_OP::CROSS) {
                                                      // process cross join
                                                      ctx.getBuilder().CreateBr(concat_qrl);
                                                  } else if(op->jop_ == JOIN_OP::LEFT_OUTER) {
                                                      // process cross join
                                                      ctx.getBuilder().CreateBr(left_outer);
                                                  }

                                              });


    BasicBlock *loop_rship;
    if(op->jop_ == JOIN_OP::LEFT_OUTER) {
        // for left outer join
        ctx.getBuilder().SetInsertPoint(left_outer);

        // get lhs tuple at id -> direct register value
        auto lhs = reg_query_results.at(op->join_pos_.first).reg_val;

        // rhs is materialized, call extern function to obtain tuple at idx
        auto idx = ConstantInt::get(ctx.int64Ty, op->join_pos_.second);
        auto rhs = ctx.getBuilder().CreateCall(node_reg, {tp, idx});

        // get the appropriate fields to compare, rship ids
        auto rhs_id = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateStructGEP(rhs, 1));
        auto node_rship_id = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateStructGEP(lhs, 2));
        ctx.getBuilder().CreateStore(node_rship_id, cross_join_idx.back());

        // create space for storage
        auto ra = ctx.getBuilder().CreateAlloca(ctx.rshipPtrTy);
        auto ridx = ctx.getBuilder().CreateAlloca(ctx.int64Ty);
        ctx.getBuilder().CreateStore(node_rship_id, ridx);

        //Value *rship; //TODO: fix workaround for LLVM 11 bug 

        // iterate through relationship list of lhs node
        loop_rship = ctx.while_loop_condition(main_function, ridx, rhs_alloca, PContext::WHILE_COND::LT, incr_loop,
                                                  [&](BasicBlock *, BasicBlock *) {
                                                      auto cur_id = ctx.getBuilder().CreateLoad(ridx);

                                                      // get the current rship
                                                      auto rship = ctx.getBuilder().CreateCall(rship_by_id, {gdb, cur_id});
                                                      ctx.getBuilder().CreateStore(rship, ra);
                                                      // get the dest node of the rship
                                                      auto to_node = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateStructGEP(rship, 3));
                                                      
                                                      // check if the dest node is the rhs node
                                                      auto concat_cond = ctx.getBuilder().CreateICmpEQ(rhs_id, to_node);

                                                      // handle dangling node
                                                      ctx.getBuilder().CreateCondBr(concat_cond, undang, for_each_next);
                                                  });

        // get next rship from list
        ctx.getBuilder().SetInsertPoint(for_each_next);
        {
            auto rship = ctx.getBuilder().CreateLoad(ra);
            auto next_rship = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateStructGEP(rship, 4));
            ctx.getBuilder().CreateStore(next_rship, ridx);
            ctx.getBuilder().CreateBr(loop_rship);
        }

        // handle danling flag
        ctx.getBuilder().SetInsertPoint(undang);
        ctx.getBuilder().CreateStore(ctx.LLVM_ZERO, dangling);
        ctx.getBuilder().CreateBr(concat_qrl);

        // handling backflow
        ctx.getBuilder().SetInsertPoint(return_handle);
        auto lh = ctx.getBuilder().CreateLoad(ridx);
        auto is_reached = ctx.getBuilder().CreateICmpULT(lh, ctx.UNKNOWN_ID);
        ctx.getBuilder().CreateCondBr(is_reached, for_each_next, loop_body);

    } else if(op->jop_ == JOIN_OP::NESTED_LOOP) {
        /*// for nested loop join
        ctx.getBuilder().SetInsertPoint(nested_loop);

        // get lhs tuple at id -> direct register value
        auto lhs = reg_query_results.at(op->join_pos_.first).reg_val;
        // extract the lhs id with GEP instruction
        auto lhs_id = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateStructGEP(lhs, 1));

        // rhs is materialized, call extern function to obtain tuple at idx
        auto idx = ConstantInt::get(ctx.int64Ty, op->join_pos_.second);
        auto rhs = ctx.getBuilder().CreateCall(node_reg, {tp, idx});
        // extract the rhs id with GEP instruction
        auto rhs_id = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateStructGEP(rhs, 1));

        // compare the node ids and branch 
        auto id_cmp = ctx.getBuilder().CreateICmpEQ(lhs_id, rhs_id);
        ctx.getBuilder().CreateCondBr(id_cmp, concat_qrl, incr_loop);*/

    } else if(op->jop_ == JOIN_OP::HASH_JOIN) {
        /*// for hash join
        ctx.getBuilder().SetInsertPoint(hash_join);
        
        // get lhs tuple at id -> direct register value
        auto lhs = reg_query_results.at(op->join_pos_.first).reg_val;
        // extract the lhs id with GEP instruction
        auto lhs_id = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateStructGEP(lhs, 1));

        // calculate the bucket id 
        auto bucket_size = ConstantInt::get(ctx.int64Ty, 10);
        auto bucket_id = ctx.getBuilder().CreateSRem(lhs_id, bucket_size);

        // iterate through buckets to find matches
        auto cur_rhs_idx = ctx.getBuilder().CreateAlloca(ctx.int64Ty);
        ctx.getBuilder().CreateStore(ctx.LLVM_ZERO, cur_rhs_idx);*/

        // iteration 
        // TODO:
        
    }

    // increment the outer loop counter for next query result
    ctx.getBuilder().SetInsertPoint(incr_loop);
    {
        auto i = ctx.getBuilder().CreateLoad(cur_idx);
        ctx.getBuilder().CreateStore(ctx.getBuilder().CreateAdd(i, ctx.LLVM_ONE), cur_idx);

        auto dang = ctx.getBuilder().CreateLoad(dangling);
        auto is_dangling = ctx.getBuilder().CreateICmpEQ(dang, ctx.LLVM_ONE);
        ctx.getBuilder().CreateCondBr(is_dangling, concat_qrl, loop_body);
    }

    // merge the lhs and rhs    
    ctx.getBuilder().SetInsertPoint(concat_qrl);

    for(auto i = 0u; i < types.size(); i++) {
        if(types.at(i) == 0) {
            auto idx = ConstantInt::get(ctx.int64Ty, i);
            Value *n = ctx.getBuilder().CreateCall(node_reg, {tp, idx});
            reg_query_results.push_back({n, 0});
        } else {
            auto idx = ConstantInt::get(ctx.int64Ty, i);
            Value *r = ctx.getBuilder().CreateCall(rship_reg, {tp, idx});
            reg_query_results.push_back({r, 1});
        }
    }
    // add the dangling flag to the tuple result
    if(op->jop_ == JOIN_OP::LEFT_OUTER)
        reg_query_results.push_back({dangling, 7});

    if(op->jop_ == JOIN_OP::CROSS || op->jop_ == JOIN_OP::NESTED_LOOP) {
        auto i = ctx.getBuilder().CreateLoad(cur_idx);
        ctx.getBuilder().CreateStore(ctx.getBuilder().CreateAdd(i, ctx.LLVM_ONE), cur_idx);
    }

    ctx.getBuilder().CreateBr(consume);

    ctx.getBuilder().SetInsertPoint(consume);
    prev_bb = consume;

    ctx.getBuilder().SetInsertPoint(end);
    {
        ctx.getBuilder().CreateBr(main_return);
    }

    if(op->jop_ == JOIN_OP::CROSS)
        main_return = loop_body;
    else if(op->jop_ == JOIN_OP::LEFT_OUTER)
        main_return = return_handle;
}

/**
 * Generate code for the sort operator
 * For now it execute the given sorting predicate
 */ 
void codegen_inline_visitor::visit(std::shared_ptr<sort_op> op) {
    op->name_ = "sort_finish_"+qid_;
    Function *df_finish = Function::Create(ctx.finishFctTy, Function::ExternalLinkage, op->name_,
                                           ctx.getModule());

    BasicBlock *sort_entry = BasicBlock::Create(ctx.getContext(), "entry", df_finish);

    ctx.getBuilder().SetInsertPoint(sort_entry);

    auto res = df_finish->args().begin();

    auto sort_fc_raw = ConstantInt::get(ctx.int64Ty, (int64_t)sort_op::sort);
    auto sort_fc_ptr = ctx.getBuilder().CreateIntToPtr(sort_fc_raw, ctx.int64PtrTy);
    auto sort_fc = ctx.getBuilder().CreateBitCast(sort_fc_ptr, ctx.finishFctTy->getPointerTo());

    ctx.getBuilder().CreateCall(ctx.finishFctTy, sort_fc, {res});

    ctx.getBuilder().CreateRetVoid();
}

/**
 * Generates code for the limit operator
 */ 
void codegen_inline_visitor::visit(std::shared_ptr<limit_op> op) {
    op->name_ = "";
    BasicBlock *limit = BasicBlock::Create(ctx.getContext(), "limit_entry", main_function);
    BasicBlock *limit_reached = BasicBlock::Create(ctx.getContext(), "limit_entry", main_function);
    BasicBlock *limit_cont = BasicBlock::Create(ctx.getContext(), "limit_entry", main_function);

    // link with previous operator
    ctx.getBuilder().SetInsertPoint(prev_bb);
    ctx.getBuilder().CreateBr(limit);
    ctx.getBuilder().SetInsertPoint(limit);

    // get the given limit
    auto res_limit = ConstantInt::get(ctx.int64Ty, op->limit_);

    // load the value from the global result counter
    auto cur_cnt = ctx.getBuilder().CreateLoad(res_counter);

    // check if the limit is reached
    auto reached = ctx.getBuilder().CreateICmpEQ(cur_cnt, res_limit);
    ctx.getBuilder().CreateCondBr(reached, limit_reached, limit_cont);

    // if the limit is reached branch to the global end block
    ctx.getBuilder().SetInsertPoint(limit_reached);
    ctx.getBuilder().CreateBr(scan_nodes_end);


    // increment the global result counter
    ctx.getBuilder().SetInsertPoint(limit_cont);
    auto incr = ctx.getBuilder().CreateAdd(cur_cnt, ctx.LLVM_ONE);
    ctx.getBuilder().CreateStore(incr, res_counter);
    prev_bb = limit_cont;

}

/**
 * Generates code for the materialization of the rhs of the join
 */
void codegen_inline_visitor::visit(std::shared_ptr<end_op> op) {
    op->name_ = "";
    auto join_insert_left = ctx.extern_func("join_insert_left");

    auto obtain_mat_tuple = ctx.extern_func("obtain_mat_tuple");
    auto mat_node = ctx.extern_func("mat_node");
    auto mat_rship = ctx.extern_func("mat_rship");
    auto collect_tuple_join = ctx.extern_func("collect_tuple_join");

    FunctionCallee mat_reg = ctx.extern_func("mat_reg_value");
    BasicBlock *bb = BasicBlock::Create(ctx.getContext(), "entry_join_rhs", main_function);

    ctx.getBuilder().SetInsertPoint(prev_bb);
    ctx.getBuilder().CreateBr(bb);
    ctx.getBuilder().SetInsertPoint(bb);

    // obtain tuple object for materialization -> joiner mat_tuple
    auto tp = ctx.getBuilder().CreateCall(obtain_mat_tuple, {});

    // materialize each result from registers
    for(auto & res : reg_query_results) {
        auto type = ConstantInt::get(ctx.int64Ty, res.type);

        if(res.type == 0) {
            ctx.getBuilder().CreateCall(mat_node, {res.reg_val, tp});
        } else {
            ctx.getBuilder().CreateCall(mat_rship, {res.reg_val, tp});
        }
    }

    // collect materialized tuple -> joiner rhs_input @ join id
    auto last_jid = jids.front();
    jids.pop_front();
    auto jid = ConstantInt::get(ctx.int64Ty, last_jid);
    ctx.getBuilder().CreateCall(collect_tuple_join, {jid, tp});

    ctx.getBuilder().CreateBr(main_return);
}

/**
 * Generates code for the create operation
 */
void codegen_inline_visitor::visit(std::shared_ptr<create_op> op) {
    op->name_ = "";
    BasicBlock *bb;
    bool access = false;
    // if the create is a access path -> init function
    if(!main_function) {
        op->name_ = "query_start";
        access = true;
        Function *df_finish = Function::Create(ctx.finishFctTy, Function::ExternalLinkage, "default_finish_"+qid_,
                                               ctx.getModule());

        BasicBlock *df_finish_bb = BasicBlock::Create(ctx.getContext(), "entry", df_finish);
        ctx.getBuilder().SetInsertPoint(df_finish_bb);
        ctx.getBuilder().CreateRetVoid();

        main_function = Function::Create(ctx.startFctTy, Function::ExternalLinkage, op->name_, ctx.getModule());
        bb = BasicBlock::Create(ctx.getModule().getContext(), "entry", main_function);
        main_return = BasicBlock::Create(ctx.getModule().getContext(), "end", main_function);
        init_function(bb);
    } else { // else link with previous operator
        bb = BasicBlock::Create(ctx.getModule().getContext(), "entry_create", main_function);
        ctx.getBuilder().SetInsertPoint(prev_bb);
        ctx.getBuilder().CreateBr(bb);
        ctx.getBuilder().SetInsertPoint(bb);

    }
    auto consume = BasicBlock::Create(ctx.getModule().getContext(), "entry", main_function);

    gdb = main_function->args().begin();
    ty = main_function->args().begin() + 5;
    rs = main_function->args().begin() + 6;
    finish = main_function->args().begin() + 8;
    offset = main_function->args().begin() + 9;
    queryArgs = main_function->args().begin() + 10;

    //create node by calling external function
    if(op->produced_type_ == 0) {
        auto create_node = ctx.extern_func("create_node");
        auto opid = ConstantInt::get(ctx.int64Ty, op->op_id_);
        auto sec_opid = ConstantInt::get(ctx.int64Ty, op->op_id_+1);
        auto label = ctx.getBuilder().CreateBitCast(ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateInBoundsGEP(queryArgs, {ctx.LLVM_ZERO, opid})), ctx.int8PtrTy);
        auto prop_ptr = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateInBoundsGEP(queryArgs, {ctx.LLVM_ZERO, sec_opid}));
        auto node = ctx.getBuilder().CreateCall(create_node, {gdb, label, prop_ptr});
        reg_query_results.push_back({node, 0});
        ctx.getBuilder().CreateBr(consume);
    } else { // create rship
        auto create_rship = ctx.extern_func("create_rship");
        auto opid = ConstantInt::get(ctx.int64Ty, op->op_id_);
        auto sec_opid = ConstantInt::get(ctx.int64Ty, op->op_id_+1);
        auto label = ctx.getBuilder().CreateBitCast(ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateInBoundsGEP(queryArgs, {ctx.LLVM_ZERO, opid})), ctx.int8PtrTy);
        auto prop_ptr = ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateInBoundsGEP(queryArgs, {ctx.LLVM_ZERO, sec_opid}));

        auto n1 = reg_query_results[op->src_des_.first].reg_val;
        auto n2 = reg_query_results[op->src_des_.second].reg_val;

        auto rship = ctx.getBuilder().CreateCall(create_rship, {gdb, label, n1, n2, prop_ptr});
        reg_query_results.push_back({rship, 1});

        ctx.getBuilder().CreateBr(consume);
    }


    ctx.getBuilder().SetInsertPoint(consume);
    prev_bb = consume;

    if(access) {
        ctx.getBuilder().SetInsertPoint(main_return);
        ctx.getBuilder().CreateRetVoid();
    }


}
