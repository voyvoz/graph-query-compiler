#include "p_context.hpp"
#include <llvm/Support/TargetSelect.h>

dcode_t PContext::get_dcode(std::string &key) {
    return gdb_->get_code(key);
}

DataLayout PContext::get_data_layout() {
    return module_->getDataLayout();
}

FunctionCallee PContext::extern_func(std::string fct_name) {
    return module_->getOrInsertFunction(fct_name, function_types[fct_name]);
}

LLVMContext &PContext::getContext() {
    return *ctx_;
}

IRBuilder<> &PContext::getBuilder() {
    return *Builder;
}

Module &PContext::getModule() {
    return *module_;
}

std::unique_ptr<Module> PContext::moveModule() {
    return std::move(module_);
}

void PContext::createNewModule() {
    module_ = std::make_unique<Module>("QOP", *ctx_);
}

PContext::PContext(graph_db_ptr gdb) : gdb_(gdb) {
    ctx_ = std::make_unique<LLVMContext>();
    module_ = std::make_unique<Module>("QOP", *ctx_);
    Builder = std::make_unique<IRBuilder<>>(*ctx_);

    voidTy = Type::getVoidTy(*ctx_);
    boolTy = Type::getInt1Ty(*ctx_);
    int8Ty = Type::getInt8Ty(*ctx_);
    int8PtrTy = Type::getInt8PtrTy(*ctx_);
    int24Ty = IntegerType::get(*ctx_, 24);
    int24PtrTy = PointerType::getUnqual(int24Ty);
    int32Ty = Type::getInt32Ty(*ctx_);
    int32PtrTy = Type::getInt32PtrTy(*ctx_);
    int64Ty = Type::getInt64Ty(*ctx_);
    int64PtrTy = Type::getInt64PtrTy(*ctx_);
    doubleTy = Type::getDoubleTy(*ctx_);
    doublePtrTy = Type::getDoublePtrTy(*ctx_);

//++++++++++++++++++ CONSTANT TYPES ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    UNKNOWN_ID = ConstantInt::get(int64Ty, std::numeric_limits<uint64_t>::max());
    LLVM_TWO = ConstantInt::get(int64Ty, 2);
    LLVM_ONE = ConstantInt::get(int64Ty, 1);
    LLVM_ZERO = ConstantInt::get(int64Ty, 0);
    MAX_PITEM_CNT = ConstantInt::get(int64Ty, 3);

//++++++++++++++++++ DATA_STRUCTURES +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++ QR_LIST +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    queryResultNodeTy = StructType::create(*ctx_, "query_result_node");
    queryResultNodePtrTy = PointerType::getUnqual(queryResultNodeTy);
    queryResultList = StructType::create(*ctx_, "query_result_list");
    queryResultListPtrTy = PointerType::getUnqual(queryResultList);

    list_size = FunctionType::get(Type::getVoidTy(*ctx_), {queryResultListPtrTy}, false);

//++++++++++++++++++ NODE TYPE +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    opaqueListTy = StructType::create(*ctx_, "opaque_list");
    nodeAtomicIdTy = StructType::create(*ctx_, "node_atomic_id");
    nodeTxnBaseTy = StructType::create(*ctx_, "node_txn_base");
    typecodeArrTy = ArrayType::get(int8Ty, 7);
    nodeTy = StructType::create(*ctx_,
                                "node"); // {int8PtrTy, int8PtrTy, int64Ty, int64Ty, int64Ty, int64Ty, int64Ty}
    nodePtrTy = PointerType::get(nodeTy, 0);
    nodeItTy = StructType::create(*ctx_, {nodeTy}, "node_iterator");
    nodeItPtrTy = PointerType::get(nodeItTy, 0);

    qrResultTy = StructType::create(*ctx_, "query_result");
    qrResultPtrTy = PointerType::get(qrResultTy, 0);

    queryArgTy = ArrayType::get(int64PtrTy, 64);
//++++++++++++++++++ RSHIP TYPE ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    rshipTy = StructType::create(*ctx_, "relationship");
    rshipPtrTy = PointerType::getUnqual(rshipTy);
    rshipAtomicIdTy = StructType::create(*ctx_, "rship_atomic_id");
    rshipTxnBaseTy = StructType::create(*ctx_, "rship_txn_base");

//++++++++++++++++++ PITEM TYPE ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    pitemValueArrTy = ArrayType::get(int8Ty, 8);
    pitemTy = StructType::create(*ctx_, "p_item");
    pitemPtrTy = PointerType::get(pitemTy, 0);

    pSetRawArrTy = ArrayType::get(pitemTy, 3);
    pItemListTy = StructType::create(*ctx_, "p_item_list");
    propertySetTy = StructType::create(*ctx_, "property_set");
    propertySetPtrTy = PointerType::get(propertySetTy, 0);

//++++++++++++++++++ GRAPH DB TYPE +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    graphDbTy = StructType::create(*ctx_, "graph_db"); // TODO: set body
    graphDbSharedTy = StructType::create(*ctx_, "graph_db_shared_ptr");
    graphDbSharedPtrTy = PointerType::get(graphDbSharedTy, 0);
    graphDbPtrTy = PointerType::get(graphDbTy, 0);

    graphDbCVecTy = StructType::create(*ctx_, "chunk_vec_iter");

//++++++++++++++++++ NODE SCAN FCT +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    nodeConsumerFctTy = FunctionType::get(Type::getVoidTy(*ctx_), {nodePtrTy}, false);
    nodeConsumerFctPtrTy = PointerType::getUnqual(nodeConsumerFctTy);
    scanNodesByLabelTy = FunctionType::get(Type::getVoidTy(*ctx_),
                                           {int8PtrTy, int8PtrTy, nodeConsumerFctPtrTy}, false);
    gdb_get_node_by_id_type = FunctionType::get(nodePtrTy, {int8PtrTy, int64Ty}, false);

    queryResultConsumerFctTy = FunctionType::get(Type::getVoidTy(*ctx_), {qrResultPtrTy}, false);

    consumerDummyCall = FunctionType::get(Type::getVoidTy(*ctx_), {qrResultPtrTy}, false);

//++++++++++++++++++ RELATIONSHIP SCAN FCT +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    rshipConsumerFctTy = FunctionType::get(Type::getVoidTy(*ctx_), {rshipPtrTy}, false);
    rshipConsumerFctPtrTy = PointerType::getUnqual(rshipConsumerFctTy);

    consumerFctTy = FunctionType::get(Type::getVoidTy(*ctx_), {int8PtrTy, int64Ty, int64PtrTy, int64PtrTy, int64Ty, int64PtrTy, int64PtrTy, int64Ty}, false);
    callMapTy = ArrayType::get(consumerFctTy->getPointerTo(), 32);
    callMapPtrTy = callMapTy->getPointerTo();

    foreachRshipFctTy = FunctionType::get(Type::getVoidTy(*ctx_), {int8PtrTy, int64Ty, int64PtrTy, int64PtrTy, int64Ty, int64PtrTy, callMapPtrTy},
                                          false);
    countPotentialOHopFctTy = FunctionType::get(int64Ty, {int8PtrTy, int64Ty}, false);
    retrieveFEVqueryFctTy = FunctionType::get(int8PtrTy, {}, false);
    fevQueueEmptyFctTy = FunctionType::get(boolTy, {int8PtrTy}, false);
    insertInFEVQueueFctTy = FunctionType::get(voidTy, {int8PtrTy, int64Ty, int64Ty}, false);

    feFromVarFctTy = FunctionType::get(voidTy, {int8PtrTy, int32Ty, nodePtrTy, int64Ty, int64Ty}, false);
    getNextRshipFctTy = FunctionType::get(rshipPtrTy, {}, false);
    fevListEndFctTy = FunctionType::get(boolTy, {}, false);
//++++++++++++++++++ FILTER FCT ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    filterConsumerFctTy = FunctionType::get(Type::getVoidTy(*ctx_), {int8PtrTy, int64Ty, int64PtrTy, int64PtrTy, int64Ty, int64PtrTy},
                                            false);

//++++++++++++++++++ EXPAND FCT +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    expandConsumerFctTy = FunctionType::get(Type::getVoidTy(*ctx_), {int8PtrTy, int64Ty, int64PtrTy, int64PtrTy, int64Ty, int64PtrTy},
                                            false);
//++++++++++++++++++ JOIN FCT ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    joinConsumerFctTy = FunctionType::get(Type::getVoidTy(*ctx_), {int8PtrTy, int64Ty, queryResultListPtrTy},
                                          false);
    joinInsertLeftFctTy = FunctionType::get(Type::getVoidTy(*ctx_), {int64PtrTy, int64PtrTy},
                                            false); // TODO: insert name argument to identify correct join table
    joinConsumeLeftFctTy = FunctionType::get(queryResultListPtrTy, {},
                                             false); // TODO: insert name argument to identify correct join table

    projectConsumerFctTy = FunctionType::get(Type::getVoidTy(*ctx_), {int8PtrTy, int64Ty, int64PtrTy, int64PtrTy, int64Ty, int64PtrTy},
                                             false);

    collectConsumerFctTy = FunctionType::get(Type::getVoidTy(*ctx_), {int8PtrTy, int64Ty, int64PtrTy, int64PtrTy, int64Ty, int64PtrTy, callMapPtrTy}, false);

    applyNodeProjectionFctTy = FunctionType::get(Type::getVoidTy(*ctx_), {int8PtrTy, int8PtrTy, int64Ty, int64PtrTy, int64PtrTy}, false);
    applyRshipProjectionFctTy = FunctionType::get(Type::getVoidTy(*ctx_), {int8PtrTy, int8PtrTy, int64Ty, int64PtrTy, int64PtrTy}, false);
//++++++++++++++++++ START FCT +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    finishFctTy = FunctionType::get(Type::getVoidTy(*ctx_), {int64PtrTy}, false);

    // gdb, first, last, tx, oid, typevec, resultset, callmap, finish, result_offset
    startFctTy = FunctionType::get(Type::getVoidTy(*ctx_), {int8PtrTy, int64Ty, int64Ty, int8PtrTy, int64Ty, int64PtrTy, int64PtrTy, int64PtrTy, finishFctTy->getPointerTo(), int64Ty, queryArgTy->getPointerTo()}, false);
    collectFctTy = FunctionType::get(Type::getVoidTy(*ctx_), {int8PtrTy, int64PtrTy, int64PtrTy, int64Ty, int64PtrTy}, false);
    limitFctTy = FunctionType::get(Type::getVoidTy(*ctx_), {int8PtrTy, int64PtrTy, int64PtrTy, int64Ty, int64PtrTy}, false);


    call_consumer_ty = FunctionType::get(Type::getVoidTy(*ctx_), {int64PtrTy, int8PtrTy, int64Ty, int64PtrTy, int64PtrTy, int64Ty, int64PtrTy, int64PtrTy}, false);

    indexGetNodeTy = FunctionType::get(nodePtrTy, {int8PtrTy, int8PtrTy, int8PtrTy, int64Ty}, false);

//++++++++++++++++++ CONSTANTS +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    constZero = ConstantInt::get(int8Ty, 0, false);
    constOne = ConstantInt::get(int8Ty, 1, false);

//++++++++++++++++++ DATA_STRUCTURES_FUNCTIONS +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++ QR_LIST +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    add_qr_list_end = FunctionType::get(Type::getVoidTy(*ctx_), {queryResultListPtrTy, qrResultPtrTy},
                                        false);
    add_qr_list_front = FunctionType::get(Type::getVoidTy(*ctx_),
                                          {queryResultListPtrTy, qrResultPtrTy}, false);
    get_front_qr_list = FunctionType::get(queryResultNodePtrTy, {queryResultListPtrTy}, false);
    get_back_qr_list = FunctionType::get(queryResultNodePtrTy, {queryResultListPtrTy}, false);
    pop_front_qr_list = FunctionType::get(queryResultNodePtrTy, {queryResultListPtrTy}, false);
    pop_back_qr_list = FunctionType::get(queryResultNodePtrTy, {queryResultListPtrTy}, false);
    append_qr_list = FunctionType::get(queryResultListPtrTy, {queryResultListPtrTy, qrResultPtrTy},
                                       false);
    at_qr_list = FunctionType::get(queryResultNodePtrTy, {queryResultListPtrTy, int64Ty},false);
    concat_qr_list = FunctionType::get(queryResultListPtrTy, {queryResultListPtrTy, queryResultListPtrTy, qrResultPtrTy},false);

//++++++++++++++++++ UTILITY +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    vec_end_reached = FunctionType::get(boolTy, {int8PtrTy, nodeItPtrTy}, false);
    vec_get_begin = FunctionType::get(nodeItPtrTy, {int8PtrTy, int64Ty, int64Ty}, false);
    vec_get_next = FunctionType::get(nodeItPtrTy, {nodeItPtrTy}, false);

    apply_pexpr_ty = FunctionType::get(int64Ty, {int8PtrTy, int8PtrTy, int64Ty, int64PtrTy, int64Ty, int64PtrTy, int64PtrTy}, false);

    res_arr_type = ArrayType::get(int64PtrTy, 64);

    get_join_vec_arr_ty = FunctionType::get(int64PtrTy, {int64PtrTy, int64Ty}, false);
    get_join_vec_size_ty = FunctionType::get(int64Ty, {int64PtrTy}, false);

    mat_reg_val_ty = FunctionType::get(voidTy, {int8PtrTy, int64PtrTy, int64Ty}, false);
    collect_reg_ty = FunctionType::get(voidTy, {int64PtrTy, boolTy}, false);

    obtain_mat_tuple_ty = FunctionType::get(int8PtrTy, {}, false);
    mat_node_ty = FunctionType::get(voidTy, {nodePtrTy, int8PtrTy}, false);
    mat_rship_ty = FunctionType::get(voidTy, {rshipPtrTy, int8PtrTy}, false);
    collect_tuple_join_ty = FunctionType::get(voidTy, {int64Ty, int8PtrTy}, false);


    get_join_tp_at_ty = FunctionType::get(int8PtrTy, {int64Ty, int64Ty}, false);
    get_node_res_at_ty = FunctionType::get(nodePtrTy, {int8PtrTy, int64Ty}, false);
    get_rship_res_at_ty = FunctionType::get(rshipPtrTy, {int8PtrTy, int64Ty}, false);
    get_mat_res_size_ty = FunctionType::get(int64Ty, {int64Ty}, false);
//++++++++++++++++++ DICT FUNCTIONS ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    lookup_label_type = FunctionType::get(int32Ty, {int8PtrTy, int8PtrTy}, false);
    lookup_dcode_type = FunctionType::get(int8PtrTy, {int8PtrTy, int32Ty}, false);

//++++++++++++++++++ GDB FUNCTIONS +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    gdb_get_nodes_type = FunctionType::get(int8PtrTy, {int8PtrTy}, false);
    get_node_from_it_type = FunctionType::get(nodePtrTy, {nodeItPtrTy}, false);
    gdb_get_rship_by_id_type = FunctionType::get(rshipPtrTy, {int8PtrTy, int64Ty}, false);
    gdb_get_dcode_type = FunctionType::get(int64Ty, {int8PtrTy, int8PtrTy}, false);

    create_node_type = FunctionType::get(nodePtrTy, {int8PtrTy, int8PtrTy, int64PtrTy}, false);
    create_rship_type = FunctionType::get(rshipPtrTy, {int8PtrTy, int8PtrTy, nodePtrTy, nodePtrTy, int64PtrTy}, false);


    foreach_variable_from_type = FunctionType::get(voidTy, {int8PtrTy, int32Ty, int64Ty, int64Ty, consumerFctTy->getPointerTo(),
                                                            int64Ty, int64PtrTy, int64PtrTy, int64Ty, int64PtrTy, int64PtrTy, int64Ty}, false);

//++++++++++++++++++ PROPERTY FUNCTIONS ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    pset_get_item_at_type = FunctionType::get(propertySetPtrTy, {int8PtrTy, int64Ty}, false);


//++++++++++++++++++ TX FUNCTIONS ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    getTxFctTy = FunctionType::get(int64Ty, {int8PtrTy}, false);
    getValidNodeFctTy = FunctionType::get(nodePtrTy, {int8PtrTy, nodePtrTy, int8PtrTy}, false);

//++++++++++++++++++ BODY DEFINITIONS + ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    nodeAtomicIdTy->setBody({int64Ty});
    rshipAtomicIdTy->setBody({int64Ty});
    nodeTxnBaseTy->setBody({nodeAtomicIdTy, int8PtrTy});
    nodeTy->setBody({nodeTxnBaseTy, int64Ty, int64Ty, int64Ty, int64Ty, int32Ty}); // TODO: check type size

    rshipTxnBaseTy->setBody({rshipAtomicIdTy, int8PtrTy});
    rshipTy->setBody({rshipTxnBaseTy, int64Ty, int64Ty, int64Ty, int64Ty, int64Ty, int64Ty, int32Ty});

    pitemTy->setBody({pitemValueArrTy, int32Ty, int8Ty}); // value, key, flags
    pItemListTy->setBody({pSetRawArrTy});
    propertySetTy->setBody({int64Ty, int64Ty, pItemListTy, int8Ty}); // next, owner, items, flags

    qrResultTy->setBody({int8PtrTy, int64Ty, int8Ty}); // actual result, type, is null
    queryResultNodeTy->setBody({queryResultNodePtrTy, queryResultNodePtrTy, int64Ty,
                                qrResultPtrTy}); //actual qr result, next, prev
    queryResultList->setBody({queryResultNodePtrTy, queryResultNodePtrTy, int64Ty}); // head, tail, size

    graphDbTy->setBody({int8PtrTy, int8PtrTy, int8PtrTy});
    graphDbSharedTy->setBody({int8PtrTy, int8PtrTy});

    graphDbCVecTy->setBody({int8PtrTy, int64Ty});


    function_types["call_consumer_function"] = call_consumer_ty;

    function_types["pset_get_item_at"] = pset_get_item_at_type;
    function_types["gdb_get_dcode"] = gdb_get_dcode_type;
    function_types["rship_by_id"] = gdb_get_rship_by_id_type;
    function_types["get_node_from_it"] = get_node_from_it_type;
    function_types["gdb_get_nodes"] = gdb_get_nodes_type;

    function_types["gdb_get_rships"] = gdb_get_nodes_type;
    function_types["get_rship_from_it"] = get_node_from_it_type;
    function_types["dict_lookup_label"] = lookup_label_type;
    function_types["dict_lookup_dcode"] = lookup_dcode_type;
    function_types["get_vec_next"] = vec_get_next;
    function_types["get_vec_begin"] = vec_get_begin;
    function_types["vec_end_reached"] = vec_end_reached;
    function_types["get_vec_next_r"] = vec_get_next;
    function_types["get_vec_begin_r"] = vec_get_begin;
    function_types["vec_end_reached_r"] = vec_end_reached;
    function_types["list_size"] = list_size;
    function_types["scan_nodes_by_label"] = scanNodesByLabelTy;
    function_types["pset_get_item_at"] = pset_get_item_at_type;
    function_types["node_by_id"] = gdb_get_node_by_id_type;
    function_types["collect"] = collectFctTy;
    function_types["join_insert_left"] = joinInsertLeftFctTy;
    function_types["join_consume_left"] = joinConsumeLeftFctTy;
    function_types["get_tx"] = getTxFctTy;
    function_types["get_valid_node"] = getValidNodeFctTy;
    function_types["apply_pexpr"] = apply_pexpr_ty;
    function_types["print_int"] = FunctionType::get(Type::getVoidTy(*ctx_), {int64Ty}, false);
    function_types["check_qr"] = FunctionType::get(Type::getVoidTy(*ctx_), {int64PtrTy}, false);
    function_types["get_join_vec_size"] = get_join_vec_size_ty;
    function_types["get_join_vec_arr"] = get_join_vec_arr_ty;
    function_types["create_node"] = create_node_type;
    function_types["create_rship"] = create_rship_type;
    function_types["foreach_variable_from"] = foreach_variable_from_type;
    function_types["mat_reg_value"] = mat_reg_val_ty;
    function_types["collect_tuple"] = collect_reg_ty;
    function_types["obtain_mat_tuple"] = obtain_mat_tuple_ty;
    function_types["mat_node"] = mat_node_ty;
    function_types["mat_rship"] = mat_rship_ty;
    function_types["collect_tuple_join"] = collect_tuple_join_ty;

    function_types["get_join_tp_at"] = get_join_tp_at_ty;
    function_types["get_node_res_at"] = get_node_res_at_ty;
    function_types["get_rship_res_at"] = get_rship_res_at_ty;
    function_types["get_mat_res_size"] = get_mat_res_size_ty;

    function_types["index_get_node"] = indexGetNodeTy;

    function_types["apply_pexpr_node"] = applyNodeProjectionFctTy;
    function_types["apply_pexpr_rship"] = applyRshipProjectionFctTy;

    function_types["count_potential_o_hop"] = countPotentialOHopFctTy;
    function_types["retrieve_fev_queue"] = retrieveFEVqueryFctTy;
    function_types["fev_queue_empty"] = fevQueueEmptyFctTy;
    function_types["insert_fev_rship"] = insertInFEVQueueFctTy;

    function_types["foreach_from_variable_rship"] = feFromVarFctTy;
    function_types["get_next_rship_fev"] = getNextRshipFctTy;
    function_types["fev_list_end"] = fevListEndFctTy;
}




BasicBlock *
PContext::while_loop_condition(Function *parent, Value *cond_lhs, Value *cond_rhs, WHILE_COND cond_op,
                     BasicBlock *endBB, const std::function<void(BasicBlock *, BasicBlock *)> &loop_body) {
    BasicBlock *condition = BasicBlock::Create(getContext(), "while_condition");
    BasicBlock *body = BasicBlock::Create(getContext(), "while_body");
    BasicBlock *body_epilog = BasicBlock::Create(getContext(), "while_body_epilog");
    BasicBlock *end = BasicBlock::Create(getContext(), "while_end");

    // branch to the loop header for condition evaluation
    Builder->CreateBr(condition);

    // insert the condition header to the block list of the function
    parent->getBasicBlockList().push_back(condition);
    Builder->SetInsertPoint(condition);
    {
        // load both operands to evaluate the condition
        auto lhs = Builder->CreateLoad(cond_lhs);
        auto rhs = Builder->CreateLoad(cond_rhs);

        // choose evaluation instruction based on WHILE_COND and branch to the next block
        // branch to end when condition is not fulfilled
        switch (cond_op) {
            case EQ: {
                auto cnd = Builder->CreateICmpEQ(lhs, rhs, "cond_equal");
                Builder->CreateCondBr(cnd, body, endBB);
                break;
            }

            case UEQ: {
                auto cnd = Builder->CreateICmpEQ(lhs, rhs, "cond_unequal");
                Builder->CreateCondBr(cnd, endBB, body);
                break;
            }

            case LT: {
                auto cnd = Builder->CreateICmpULT(lhs, rhs, "cond_less_than");
                Builder->CreateCondBr(cnd, body, endBB);
                break;
            }

            case LE: {
                auto cnd = Builder->CreateICmpULE(lhs, rhs, "cond_less_equal");
                Builder->CreateCondBr(cnd, body, endBB);
                break;
            }

            case GT: {
                auto cnd = Builder->CreateICmpUGT(lhs, rhs, "cond_greater_than");
                Builder->CreateCondBr(cnd, body, endBB);
                break;
            }

            case GE: {
                auto cnd = Builder->CreateICmpUGE(lhs, rhs, "cond_greater_equal");
                Builder->CreateCondBr(cnd, body, endBB);
                break;
            }

        }
    }

    // insert loop body block to block list of function
    parent->getBasicBlockList().push_back(body);
    Builder->SetInsertPoint(body);
    {
        // process the actual loop body
        loop_body(body, body_epilog);

        //Builder->CreateBr(body_epilog);
    }

    // branch, from outside to epilog, if body leaved
    parent->getBasicBlockList().push_back(body_epilog);
    Builder->SetInsertPoint(body_epilog);
    {
        Builder->CreateBr(condition);
    }

    return condition;
}


BasicBlock *PContext::while_loop(Function *parent,
                       FunctionCallee get_begin, FunctionCallee get_next, FunctionCallee reached_end,
                       Value *it_alloca, Value *cond_param,
                       BasicBlock *nextBB, const std::function<void(BasicBlock *, BasicBlock *)> &loop_body,
                       BasicBlock *loop_body_condition) {

    BasicBlock *condition = BasicBlock::Create(*ctx_, "while_condition");
    BasicBlock *body = BasicBlock::Create(*ctx_, "while_body");
    BasicBlock *body_epilog = BasicBlock::Create(*ctx_, "while_body_epilog");
    BasicBlock *end = BasicBlock::Create(*ctx_, "while_end");

    auto cmp_false = ConstantInt::get(boolTy, 0);
    Builder->CreateBr(condition);

    parent->getBasicBlockList().push_back(condition);
    Builder->SetInsertPoint(condition);
    {
        //auto it = Builder->CreateLoad(it_alloca);
        auto end_cond = Builder->CreateCall(reached_end, {cond_param, it_alloca});
        auto cond = Builder->CreateICmpEQ(end_cond, cmp_false);
        Builder->CreateCondBr(cond, body, end);
    }

    parent->getBasicBlockList().push_back(body);
    Builder->SetInsertPoint(body);
    {
        loop_body(body, body_epilog);

        if (cond_param == nullptr)
            Builder->CreateBr(body_epilog);
    }

    parent->getBasicBlockList().push_back(body_epilog);
    Builder->SetInsertPoint(body_epilog);
    {
        //auto it = Builder->CreateLoad(it_alloca);
        it_alloca = Builder->CreateCall(get_next, {it_alloca});
        //Builder->CreateStore(nit, it_alloca);

        auto end_cond = Builder->CreateCall(reached_end, {cond_param, it_alloca});
        auto cond = Builder->CreateICmpEQ(end_cond, cmp_false);
        Builder->CreateCondBr(cond, body, end);
    }

    parent->getBasicBlockList().push_back(end);
    Builder->SetInsertPoint(end);
    {
        Builder->CreateBr(nextBB);
    }

    return body_epilog;
}

BasicBlock * PContext::while_rship_exist(Function *parent, Value *gdb, Value *node, BasicBlock *nextBB, int nodeGEPidx, int rshipGEPidx, BasicBlock *endBB, const std::function<void (BasicBlock *, BasicBlock *, Value *)> &loop_body) {
    BasicBlock *condition = BasicBlock::Create(*ctx_, "while_condition");
    BasicBlock *body = BasicBlock::Create(*ctx_, "while_body");
    BasicBlock *body_epilog = BasicBlock::Create(*ctx_, "while_body_epilog");
    BasicBlock *init_rship = BasicBlock::Create(*ctx_, "init_rship");
    BasicBlock *next_rship = BasicBlock::Create(*ctx_, "next_rship");
    BasicBlock *end = BasicBlock::Create(*ctx_, "while_end");

    FunctionCallee rship_by_id = extern_func("rship_by_id");
    Value *rship_id = Builder->CreateLoad(Builder->CreateStructGEP(node, nodeGEPidx));
    auto UNKNOWN_REL_ID = ConstantInt::get(int64Ty, std::numeric_limits<int64_t>::max());
    Value *rship;
    auto cmp_false = ConstantInt::get(boolTy, 0);
    Builder->CreateBr(condition);

    parent->getBasicBlockList().push_back(condition);
    Builder->SetInsertPoint(condition);
    {
        //auto it = Builder->CreateLoad(it_alloca);
        auto cond = Builder->CreateICmpULT(rship_id, UNKNOWN_REL_ID);
        Builder->CreateCondBr(cond, init_rship, end);
    }

    parent->getBasicBlockList().push_back(init_rship);
    Builder->SetInsertPoint(init_rship);
    {
        //auto it = Builder->CreateLoad(it_alloca);
        rship = Builder->CreateCall(rship_by_id, {gdb, rship_id});
        Builder->CreateBr(body);
    }

    parent->getBasicBlockList().push_back(body);
    Builder->SetInsertPoint(body);
    {
        loop_body(body, body_epilog, rship);

        //Builder->CreateBr(body_epilog);
    }

    parent->getBasicBlockList().push_back(body_epilog);
    Builder->SetInsertPoint(body_epilog);
    {
        //auto it = Builder->CreateLoad(it_alloca);
        rship_id = Builder->CreateLoad(Builder->CreateStructGEP(rship, rshipGEPidx));
        //Builder->CreateStore(nit, it_alloca);

        auto end_cond = Builder->CreateICmpULT(rship_id, UNKNOWN_REL_ID);
        //auto cond = Builder->CreateICmpEQ(end_cond, cmp_false);
        Builder->CreateCondBr(end_cond, next_rship, end);
    }

    parent->getBasicBlockList().push_back(next_rship);
    Builder->SetInsertPoint(next_rship);
    {
        auto nrship = Builder->CreateLoad(Builder->CreateCall(rship_by_id, {gdb, rship_id}));
        Builder->CreateStore(nrship, rship);
        Builder->CreateBr(body);
    }

    parent->getBasicBlockList().push_back(end);
    Builder->SetInsertPoint(end);
    {
        Builder->CreateBr(nextBB);
    }

    return body_epilog;
}

Value * PContext::create_qr_node(Value *qr_element, Value *qr_prev,
                       Value *qr_next) {
    assert(qr_element->getType() == qrResultPtrTy && "qr_element is qrResultPtrTy");

    auto node_msize = ConstantExpr::getSizeOf(queryResultNodeTy);
    node_msize = ConstantExpr::getTruncOrBitCast(node_msize, int64Ty);
    auto qr_node_alloca = CallInst::CreateMalloc(Builder->GetInsertBlock(), int64Ty, queryResultNodeTy,
                                                 node_msize, nullptr, nullptr, "qr_list_node");
    Builder->Insert(qr_node_alloca);

    auto qr_field_next = Builder->CreateStructGEP(qr_node_alloca, 0); // **
    auto qr_field_prev = Builder->CreateStructGEP(qr_node_alloca, 1); // **
    auto check = Builder->CreateStructGEP(qr_node_alloca, 2);
    auto qr_field = Builder->CreateStructGEP(qr_node_alloca, 3);
    Builder->CreateStore(qr_element, qr_field);


    if (qr_prev) {
        Builder->CreateStore(qr_prev, qr_field_prev);
    } else {
        auto null = Constant::getNullValue(queryResultNodePtrTy);
        auto x = Builder->CreateStore(null, qr_field_prev);
    }


    if (qr_next) {
        Builder->CreateStore(qr_next, qr_field_next);
    } else {
        auto null = Constant::getNullValue(queryResultNodePtrTy);
        auto x = Builder->CreateStore(null, qr_field_next);
    }
    Builder->CreateStore(ConstantInt::get(int64Ty, 22401), check);
    return qr_node_alloca;
}

Value *PContext::create_qr_list(Value *firstElement) {
    auto list_msize = ConstantExpr::getSizeOf(queryResultList);
    list_msize = ConstantExpr::getTruncOrBitCast(list_msize, int64Ty);
    auto list_alloca = CallInst::CreateMalloc(Builder->GetInsertBlock(), int64Ty, queryResultList,
                                              list_msize, nullptr, nullptr, "qr_list");
    Builder->Insert(list_alloca);
    //alloc_stack.push(Builder->CreateAlloca(queryResultList));
    //auto list_alloca = alloc_stack.top();

    auto list_size = Builder->CreateStructGEP(list_alloca, 2);
    auto list_head = Builder->CreateStructGEP(list_alloca, 0);
    auto list_tail = Builder->CreateStructGEP(list_alloca, 1);

    if (firstElement) {
        assert(firstElement->getType() == qrResultPtrTy && "qr_element is qrResultPtrTy");
        auto qr_node_alloca = create_qr_node(firstElement);
        Builder->CreateStore(qr_node_alloca, list_head);
        Builder->CreateStore(qr_node_alloca, list_tail);
        auto one_size = ConstantInt::get(int64Ty, 1);
        Builder->CreateStore(one_size, list_size);

    } else {
        auto null = Constant::getNullValue(queryResultNodePtrTy);
        auto zero_size = ConstantInt::get(int64Ty, 0);
        Builder->CreateStore(null, list_head);
        Builder->CreateStore(null, list_tail);
        Builder->CreateStore(zero_size, list_size);
    }
    return list_alloca;
}

Function *PContext::qr_list_add_end() {
    auto fct = Function::Create(add_qr_list_end, Function::InternalLinkage, "qr_list_add_end", getModule());
    FunctionCallee ls_fct = getModule().getOrInsertFunction("list_size", list_size);
    gen_funcs["qr_list_add_end"] = fct;
    auto entry = BasicBlock::Create(getContext(), "entry", fct);
    auto qr_list = fct->arg_begin(); //qr_list*
    auto qr_element = fct->arg_begin() + 1;
    Builder->SetInsertPoint(entry);
    Builder->CreateRet(nullptr);
    return fct;
    /*auto list_size = Builder->CreateStructGEP(qr_list, 2);
    auto list_size_val = Builder->CreateLoad(list_size);
    auto *thenBB = BasicBlock::Create(getContext(), "then_qr_list_add", fct);
    auto *elseBB = BasicBlock::Create(getContext(), "else_qr_list_add");
    auto *endBB = BasicBlock::Create(getContext(), "end_qr_list_add");
    auto empty_size = ConstantInt::get(int64Ty, 0);
    auto one_size = ConstantInt::get(int64Ty, 1);
    auto condition = Builder->CreateICmpEQ(list_size_val, empty_size);
    Builder->CreateCondBr(condition, thenBB, elseBB);

    Builder->SetInsertPoint(thenBB);
    // size == 0
    // set only tail & head
    {
        auto list_head = Builder->CreateStructGEP(qr_list, 0, "list_head"); // qr_node **
        auto qr_node_alloca_if = create_qr_node(qr_element);
        auto list_tail = Builder->CreateStructGEP(qr_list, 1, "list_tail"); // qr_node **
        Builder->CreateStore(qr_node_alloca_if, list_head);
        Builder->CreateStore(qr_node_alloca_if, list_tail);
    }

    Builder->CreateBr(endBB);

    fct->getBasicBlockList().push_back(elseBB);
    Builder->SetInsertPoint(elseBB);
    // size > 1
    {
        auto list_tail_ptr = Builder->CreateStructGEP(qr_list, 1, "list_tail"); //qr_node**
        auto list_tail = Builder->CreateLoad(list_tail_ptr); // qr_node*
        auto qr_node_alloca = create_qr_node(qr_element, list_tail);
        auto old_tail_next = Builder->CreateStructGEP(list_tail, 0); //qr_node**
        Builder->CreateStore(qr_node_alloca, old_tail_next);
        Builder->CreateStore(qr_node_alloca, list_tail_ptr);
    }


    Builder->CreateBr(endBB);

    fct->getBasicBlockList().push_back(endBB);
    Builder->SetInsertPoint(endBB);
    auto inc_size = Builder->CreateAdd(list_size_val, one_size, "inc_list_size");
    Builder->CreateStore(inc_size, list_size);
    Builder->CreateRet(nullptr);
    return fct;*/
}

Function *PContext::qr_list_add_front() {
    auto fct = Function::Create(add_qr_list_front, Function::InternalLinkage, "qr_list_add_front", getModule());
    gen_funcs["qr_list_add_front"] = fct;
    auto entry = BasicBlock::Create(getContext(), "entry", fct);
    auto qr_list = fct->arg_begin(); //qr_list*
    auto qr_element = fct->arg_begin() + 1;
    Builder->SetInsertPoint(entry);
    auto list_size = Builder->CreateStructGEP(qr_list, 2);
    auto list_size_val = Builder->CreateLoad(list_size);
    auto *thenBB = BasicBlock::Create(getContext(), "then_qr_list_add", fct);
    auto *elseBB = BasicBlock::Create(getContext(), "else_qr_list_add");
    auto *endBB = BasicBlock::Create(getContext(), "end_qr_list_add");
    auto empty_size = ConstantInt::get(int64Ty, 0);
    auto one_size = ConstantInt::get(int64Ty, 1);
    auto condition = Builder->CreateICmpEQ(list_size_val, empty_size);
    Builder->CreateCondBr(condition, thenBB, elseBB);

    Builder->SetInsertPoint(thenBB);
    // size == 0
    // set only tail & head
    {
        auto list_head = Builder->CreateStructGEP(qr_list, 0, "list_head"); // qr_node **
        auto qr_node_alloca_if = create_qr_node(qr_element);
        auto list_tail = Builder->CreateStructGEP(qr_list, 1, "list_tail"); // qr_node **
        Builder->CreateStore(qr_node_alloca_if, list_head);
        Builder->CreateStore(qr_node_alloca_if, list_tail);
    }

    Builder->CreateBr(endBB);

    fct->getBasicBlockList().push_back(elseBB);
    Builder->SetInsertPoint(elseBB);
    // size > 1
    {
        auto list_head_ptr = Builder->CreateStructGEP(qr_list, 0, "list_head"); //qr_node**
        auto list_head = Builder->CreateLoad(list_head_ptr); // qr_node*
        auto qr_node_alloca = create_qr_node(qr_element, nullptr, list_head);
        auto old_head_prev = Builder->CreateStructGEP(list_head, 1); //qr_node**
        Builder->CreateStore(qr_node_alloca, old_head_prev);
        Builder->CreateStore(qr_node_alloca, list_head_ptr);
    }


    Builder->CreateBr(endBB);

    fct->getBasicBlockList().push_back(endBB);
    Builder->SetInsertPoint(endBB);
    auto inc_size = Builder->CreateAdd(list_size_val, one_size, "inc_list_size");
    Builder->CreateStore(inc_size, list_size);
    Builder->CreateRet(nullptr);
    return nullptr;
}

Function *PContext::qr_list_get_front() {
    auto fct = Function::Create(get_front_qr_list, Function::InternalLinkage, "qr_list_get_front", getModule());
    gen_funcs["qr_list_get_front"] = fct;
    auto entry = BasicBlock::Create(getContext(), "entry", fct);
    auto qr_list = fct->arg_begin(); //qr_list*
    Builder->SetInsertPoint(entry);
    auto list_size = Builder->CreateStructGEP(qr_list, 2);
    auto list_size_val = Builder->CreateLoad(list_size);
    auto *thenBB = BasicBlock::Create(getContext(), "then_qr_list_add", fct);
    auto *elseBB = BasicBlock::Create(getContext(), "else_qr_list_add");
    auto empty_size = ConstantInt::get(int64Ty, 0);
    auto condition = Builder->CreateICmpEQ(list_size_val, empty_size);
    Builder->CreateCondBr(condition, thenBB, elseBB);

    Builder->SetInsertPoint(thenBB);
    // size == 0
    {
        auto null = Constant::getNullValue(queryResultNodePtrTy);
        Builder->CreateRet(null);
    }

    fct->getBasicBlockList().push_back(elseBB);
    Builder->SetInsertPoint(elseBB);
    // size > 0
    {
        auto list_head_ptr = Builder->CreateStructGEP(qr_list, 0, "list_head"); //qr_node**
        auto list_head = Builder->CreateLoad(list_head_ptr); // qr_node*
        Builder->CreateRet(list_head);
    }

    return nullptr;
}

Function *PContext::qr_list_get_back() {
    auto fct = Function::Create(get_back_qr_list, Function::InternalLinkage, "qr_list_get_back", getModule());
    gen_funcs["qr_list_get_back"] = fct;
    auto entry = BasicBlock::Create(getContext(), "entry", fct);
    auto qr_list = fct->arg_begin(); //qr_list*
    Builder->SetInsertPoint(entry);
    auto list_size = Builder->CreateStructGEP(qr_list, 2);
    auto list_size_val = Builder->CreateLoad(list_size);
    auto *thenBB = BasicBlock::Create(getContext(), "then_qr_list_add", fct);
    auto *elseBB = BasicBlock::Create(getContext(), "else_qr_list_add");
    auto empty_size = ConstantInt::get(int64Ty, 0);
    auto condition = Builder->CreateICmpEQ(list_size_val, empty_size);
    Builder->CreateCondBr(condition, thenBB, elseBB);

    Builder->SetInsertPoint(thenBB);
    // size == 0
    {
        auto null = Constant::getNullValue(queryResultNodePtrTy);
        Builder->CreateRet(null);
    }

    fct->getBasicBlockList().push_back(elseBB);
    Builder->SetInsertPoint(elseBB);
    // size > 0
    {
        auto list_tail_ptr = Builder->CreateStructGEP(qr_list, 1, "list_tail"); //qr_node**
        auto list_tail = Builder->CreateLoad(list_tail_ptr); // qr_node*
        Builder->CreateRet(list_tail);
    }

    return fct;
}

Function *PContext::qr_list_pop_back() {
    auto fct = Function::Create(pop_back_qr_list, Function::InternalLinkage, "qr_list_pop_back", getModule());
    gen_funcs["qr_list_pop_back"] = fct;
    auto entry = BasicBlock::Create(getContext(), "entry", fct);
    auto qr_list = fct->arg_begin(); //qr_list*
    Builder->SetInsertPoint(entry);
    auto list_size = Builder->CreateStructGEP(qr_list, 2);
    auto list_size_val = Builder->CreateLoad(list_size);
    auto *thenBB = BasicBlock::Create(getContext(), "then_qr_list_add", fct);
    auto *elseBB = BasicBlock::Create(getContext(), "else_qr_list_add");
    auto empty_size = ConstantInt::get(int64Ty, 0);
    auto one_size = ConstantInt::get(int64Ty, 1);
    auto condition = Builder->CreateICmpEQ(list_size_val, empty_size);
    Builder->CreateCondBr(condition, thenBB, elseBB);
    auto null = Constant::getNullValue(queryResultNodePtrTy);

    Builder->SetInsertPoint(thenBB);
    // size == 0
    {
        Builder->CreateRet(null);
    }

    fct->getBasicBlockList().push_back(elseBB);
    Builder->SetInsertPoint(elseBB);
    // size > 0
    {
        auto inc_size = Builder->CreateSub(list_size_val, one_size, "sub_list_size");
        Builder->CreateStore(inc_size, list_size);
        auto *then_size_BB = BasicBlock::Create(getContext(), "then_qr_list_pop_size", fct);
        auto *else_size_BB = BasicBlock::Create(getContext(), "else_qr_list_pop_size");
        condition = Builder->CreateICmpEQ(list_size_val, one_size);
        Builder->CreateCondBr(condition, then_size_BB, else_size_BB);
        Builder->SetInsertPoint(then_size_BB); //size == 1
        {
            auto list_head_ptr = Builder->CreateStructGEP(qr_list, 0, "list_tail"); //qr_node**
            auto list_tail_ptr = Builder->CreateStructGEP(qr_list, 1, "list_tail"); //qr_node**
            auto list_tail = Builder->CreateLoad(list_tail_ptr); // qr_node*
            Builder->CreateStore(null, list_tail_ptr);
            Builder->CreateStore(null, list_head_ptr);
            Builder->CreateRet(list_tail);
        }

        fct->getBasicBlockList().push_back(else_size_BB);
        Builder->SetInsertPoint(else_size_BB); // size > 1
        {
            auto list_tail_ptr = Builder->CreateStructGEP(qr_list, 1, "list_tail"); //qr_node**
            auto list_tail = Builder->CreateLoad(list_tail_ptr); // qr_node*
            auto prev_ptr = Builder->CreateStructGEP(list_tail, 1);
            auto prev = Builder->CreateLoad(prev_ptr);
            auto prev_next = Builder->CreateStructGEP(prev, 0);
            Builder->CreateStore(null, prev_next);
            Builder->CreateStore(prev, list_tail_ptr);
            Builder->CreateRet(list_tail);
        }

    }

    return fct;
}

Function *PContext::qr_list_pop_front(Module &module) {
    auto fct = Function::Create(pop_front_qr_list, Function::InternalLinkage, "qr_list_pop_front", module);
    gen_funcs["qr_list_pop_front"] = fct;
    auto entry = BasicBlock::Create(module.getContext(), "entry", fct);
    auto qr_list = fct->arg_begin(); //qr_list*
    Builder->SetInsertPoint(entry);
    auto list_size = Builder->CreateStructGEP(qr_list, 2);
    auto list_size_val = Builder->CreateLoad(list_size);
    auto *thenBB = BasicBlock::Create(module.getContext(), "then_qr_list_add", fct);
    auto *elseBB = BasicBlock::Create(module.getContext(), "else_qr_list_add");
    auto empty_size = ConstantInt::get(int64Ty, 0);
    auto one_size = ConstantInt::get(int64Ty, 1);
    auto condition = Builder->CreateICmpEQ(list_size_val, empty_size);
    Builder->CreateCondBr(condition, thenBB, elseBB);
    auto null = Constant::getNullValue(queryResultNodePtrTy);

    Builder->SetInsertPoint(thenBB);
    // size == 0
    {
        Builder->CreateRet(null);
    }

    fct->getBasicBlockList().push_back(elseBB);
    Builder->SetInsertPoint(elseBB);
    // size > 0
    {
        auto inc_size = Builder->CreateSub(list_size_val, one_size, "sub_list_size");
        Builder->CreateStore(inc_size, list_size);
        auto *then_size_BB = BasicBlock::Create(module.getContext(), "then_qr_list_pop_size", fct);
        auto *else_size_BB = BasicBlock::Create(module.getContext(), "else_qr_list_pop_size");
        condition = Builder->CreateICmpEQ(list_size_val, one_size);
        Builder->CreateCondBr(condition, then_size_BB, else_size_BB);
        Builder->SetInsertPoint(then_size_BB); //size == 1
        {
            auto list_head_ptr = Builder->CreateStructGEP(qr_list, 0, "list_tail"); //qr_node**
            auto list_tail_ptr = Builder->CreateStructGEP(qr_list, 1, "list_tail"); //qr_node**
            auto list_tail = Builder->CreateLoad(list_tail_ptr); // qr_node*
            Builder->CreateStore(null, list_tail_ptr);
            Builder->CreateStore(null, list_head_ptr);
            Builder->CreateRet(list_tail);
        }

        fct->getBasicBlockList().push_back(else_size_BB);
        Builder->SetInsertPoint(else_size_BB); // size > 1
        {
            auto list_head_ptr = Builder->CreateStructGEP(qr_list, 0, "list_head"); //qr_node**
            auto list_head = Builder->CreateLoad(list_head_ptr); // qr_node*
            auto next_ptr = Builder->CreateStructGEP(list_head, 0);
            auto next = Builder->CreateLoad(next_ptr);
            auto prev_next = Builder->CreateStructGEP(next, 1);
            Builder->CreateStore(null, prev_next);
            Builder->CreateStore(next, list_head_ptr);
            Builder->CreateRet(list_head);
        }

    }

    return fct;
}

BasicBlock *PContext::while_qr_list(Function *parent, Value *qr_list, AllocaInst *cur_node, BasicBlock *next,
                          const std::function<void(BasicBlock *, BasicBlock *)> &loop_body) {
    BasicBlock *condition = BasicBlock::Create(getContext(), "while_condition");
    BasicBlock *body = BasicBlock::Create(getContext(), "while_body");
    BasicBlock *body_epilog = BasicBlock::Create(getContext(), "while_body_epilog");
    BasicBlock *end = BasicBlock::Create(getContext(), "while_end");

    auto null_ptr = Constant::getNullValue(queryResultNodePtrTy);
    auto list_head = Builder->CreateLoad(Builder->CreateStructGEP(qr_list, 0));
    Builder->CreateStore(list_head, cur_node);

    Builder->CreateBr(condition);

    parent->getBasicBlockList().push_back(condition);
    Builder->SetInsertPoint(condition);
    {
        auto node = Builder->CreateLoad(cur_node);
        auto cond = Builder->CreateICmpEQ(node, null_ptr);
        Builder->CreateCondBr(cond, end, body);
    }

    parent->getBasicBlockList().push_back(body);
    Builder->SetInsertPoint(body);
    {
        loop_body(body, body_epilog);

    }

    parent->getBasicBlockList().push_back(body_epilog);
    Builder->SetInsertPoint(body_epilog);
    {
        auto node = Builder->CreateLoad(cur_node);
        auto nextn = Builder->CreateLoad(Builder->CreateStructGEP(node, 0));
        Builder->CreateStore(nextn, cur_node);
        Builder->CreateBr(condition);
    }

    parent->getBasicBlockList().push_back(end);
    Builder->SetInsertPoint(end);
    {
        Builder->CreateBr(next);
    }

    return body_epilog;
}

Function *PContext::qr_list_append() {
    auto fct = Function::Create(append_qr_list, Function::InternalLinkage, "qr_list_append", getModule());
    gen_funcs["qr_list_append"] = fct;
    auto add_end = gen_funcs["qr_list_add_end"];
    auto llist = fct->args().begin();
    auto qr_el = fct->args().begin() + 1;

    BasicBlock *entry = BasicBlock::Create(getContext(), "entry", fct);
    BasicBlock *end = BasicBlock::Create(getContext(), "end", fct);

    Builder->SetInsertPoint(entry);
    auto nlist = create_qr_list();

    auto cur_node = Builder->CreateAlloca(queryResultNodePtrTy);
    auto loop = while_qr_list(fct, llist, cur_node, end, [&](BasicBlock *loop, BasicBlock *epilog) {
        // 1 extract qr from node
        auto lnode = Builder->CreateLoad(cur_node);
        auto qr = Builder->CreateLoad(Builder->CreateStructGEP(lnode, 3));

        // 2 create new node
        auto nnode = create_qr_node(qr);

        // 3 add to new list
        //Builder->CreateCall(add_end, {nlist, qr});
        Builder->CreateBr(epilog);
    });

    Builder->SetInsertPoint(end);
    {
        //Builder->CreateCall(add_end, {nlist, qr_el});
        Builder->CreateRet(nlist);
    }

    return fct;
}

Function *PContext::qr_list_at() {
    // TODO null pointer handling?
    auto fct = Function::Create(at_qr_list, Function::InternalLinkage, "qr_list_at", getModule());
    auto llist = fct->args().begin();
    auto at = fct->args().begin() + 1;

    BasicBlock *entry = BasicBlock::Create(getContext(), "entry", fct);
    BasicBlock *end = BasicBlock::Create(getContext(), "end", fct);

    Builder->SetInsertPoint(entry);

    auto cur_pos_alloca = Builder->CreateAlloca(int64Ty);
    Builder->CreateStore(LLVM_ZERO, cur_pos_alloca);
    auto cur_node = Builder->CreateAlloca(queryResultNodePtrTy);
    auto loop = while_qr_list(fct, llist, cur_node, end, [&](BasicBlock *loop, BasicBlock *epilog) {
        // load current it pos
        auto cur_pos = Builder->CreateLoad(cur_pos_alloca);

        // cmp is position reached
        auto cmp = Builder->CreateICmpEQ(cur_pos, at);

        // increment & store next it before branching
        auto npos = Builder->CreateAdd(cur_pos, LLVM_ONE);
        Builder->CreateStore(npos, cur_pos_alloca);

        // branch if pos reached
        Builder->CreateCondBr(cmp, end, epilog);
    });

    Builder->SetInsertPoint(end);
    auto ret = Builder->CreateLoad(cur_node);
    Builder->CreateRet(ret);
    return fct;
}

Function *PContext::qr_list_concat() {
    auto fct = Function::Create(concat_qr_list, Function::InternalLinkage, "qr_list_concat", getModule());
    auto qrl1 = fct->args().begin();
    auto qrl2 = fct->args().begin() + 1;
    auto qr_el = fct->args().begin() + 2;

    auto add_end = gen_funcs["qr_list_add_end"];
    BasicBlock *entry = BasicBlock::Create(getContext(), "entry", fct);
    BasicBlock *r_list = BasicBlock::Create(getContext(), "right_list", fct);
    BasicBlock *end = BasicBlock::Create(getContext(), "end", fct);

    Builder->SetInsertPoint(entry);
    auto nlist = create_qr_list();

    auto cur_node = Builder->CreateAlloca(queryResultNodePtrTy);
    auto l_loop = while_qr_list(fct, qrl1, cur_node, r_list, [&](BasicBlock *loop, BasicBlock *epilog) {
        // 1 extract qr from node
        auto lnode = Builder->CreateLoad(cur_node);
        auto qr = Builder->CreateLoad(Builder->CreateStructGEP(lnode, 3));

        // 2 create new node
        auto nnode = create_qr_node(qr);

        // 3 add to new list
        //Builder->CreateCall(add_end, {nlist, qr});
        Builder->CreateBr(epilog);
    });


    Builder->SetInsertPoint(r_list);
    auto r_loop = while_qr_list(fct, qrl2, cur_node, r_list, [&](BasicBlock *loop, BasicBlock *epilog) {
        // 1 extract qr from node
        auto lnode = Builder->CreateLoad(cur_node);
        auto qr = Builder->CreateLoad(Builder->CreateStructGEP(lnode, 3));

        // 2 create new node
        auto nnode = create_qr_node(qr);

        // 3 add to new list
        //Builder->CreateCall(add_end, {nlist, qr});
        Builder->CreateBr(epilog);
    });

    Builder->SetInsertPoint(end);
    {
        //Builder->CreateCall(add_end, {nlist, qr_el});
        Builder->CreateRet(nlist);
    }

    return fct;
}

Value *PContext::node_cmp_label(Value *node, Value *label_code) {
    auto node_lc = getBuilder().CreateLoad(
            getBuilder().CreateStructGEP(node, 5));
    return getBuilder().CreateICmpEQ(node_lc, label_code);
};

Value *PContext::rship_cmp_label(Value *rship, Value *label_code) {
    auto rship_lcode = getBuilder().CreateLoad(
            getBuilder().CreateStructGEP(rship, 7));
    return getBuilder().CreateICmpEQ(rship_lcode, label_code);
}

Value *PContext::extract_arg_label(int op_id, Value *gdb, Value *arg_desc) {
    FunctionCallee dict_lookup_label = extern_func("dict_lookup_label");
    auto opid = ConstantInt::get(int64Ty, op_id);
    auto str = getBuilder().CreateLoad(Builder->CreateInBoundsGEP(arg_desc, {LLVM_ZERO, opid}));
    // 2.2 call extern function
    return Builder->CreateCall(dict_lookup_label, {gdb, Builder->CreateBitCast(str, int8PtrTy)});
}
