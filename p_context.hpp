#pragma once
#ifndef ART_P_CONTEXT_HPP
#define ART_P_CONTEXT_HPP

#include <graph_db.hpp>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/Support/InitLLVM.h>
#include <stack>


using namespace llvm;

/**
 * The PContext class provides all relevant types and abstractions
 * for code generation, like fixed-width integers, function types or loop abstraction.
 * For this purpose, a pointer to the current graph database must be passed.
 * This class generates a new LLVM Module, as well as a new LLVM Context.
 */
class PContext {
    std::unique_ptr<LLVMContext> ctx_;
    std::unique_ptr<Module> module_;
    std::unique_ptr<IRBuilder<>> Builder;

public:
    graph_db_ptr gdb_;
    PContext(graph_db_ptr gdb);

    dcode_t get_dcode(std::string &key);

    DataLayout get_data_layout();

    FunctionCallee extern_func(std::string fct_name);

    LLVMContext &getContext();

    IRBuilder<> &getBuilder();

    Module &getModule();

    std::unique_ptr<Module> moveModule();

    void createNewModule();

    enum QR_TYPE {
        NODE = 0,
        RSHIP = 1,
        PROP = 2
    };
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++ LLVM TYPES ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    Type *voidTy;
    Type *boolTy;
    Type *int8Ty;
    Type *int8PtrTy;
    Type *int24Ty;
    Type *int24PtrTy;
    Type *int32Ty;
    Type *int64Ty;
    Type *int32PtrTy;
    Type *int64PtrTy;
    Type *doubleTy;
    Type *doublePtrTy;

//++++++++++++++++++ CONSTANT TYPES ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    Value *UNKNOWN_ID;
    Value *LLVM_TWO;
    Value *LLVM_ONE;
    Value *LLVM_ZERO;
    Value *MAX_PITEM_CNT;
    Value *MAX_CVEC_ENTRIES;

//++++++++++++++++++ DATA_STRUCTURES +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++ QR_LIST +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    StructType *queryResultNodeTy;
    PointerType *queryResultNodePtrTy;
    StructType *queryResultList;
    PointerType *queryResultListPtrTy;

    FunctionType *list_size;

    ArrayType *callMapTy;
    PointerType *callMapPtrTy;
//++++++++++++++++++ NODE TYPE +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    StructType *opaqueListTy;
    StructType *nodeAtomicIdTy;
    StructType *nodeTxnBaseTy;
    ArrayType *typecodeArrTy;
    StructType *nodeTy;
    PointerType *nodePtrTy;
    StructType *nodeItTy;
    PointerType *nodeItPtrTy;

    StructType *qrResultTy;
    PointerType *qrResultPtrTy;

//++++++++++++++++++ RSHIP TYPE ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    StructType *rshipTy;
    PointerType *rshipPtrTy;
    StructType *rshipAtomicIdTy;
    StructType *rshipTxnBaseTy;

//++++++++++++++++++ PITEM TYPE ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ArrayType *pitemValueArrTy;
    StructType *pitemTy;
    PointerType *pitemPtrTy;

    ArrayType *pSetRawArrTy;
    StructType *pItemListTy;
    StructType *propertySetTy;
    PointerType *propertySetPtrTy;

//++++++++++++++++++ GRAPH DB TYPE +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    StructType *graphDbTy; // TODO: set body
    StructType *graphDbSharedTy;
    PointerType *graphDbSharedPtrTy;
    PointerType *graphDbPtrTy;

    StructType *graphDbCVecTy;

    ArrayType *queryArgTy;
//++++++++++++++++++ NODE SCAN FCT +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    FunctionType *nodeConsumerFctTy;
    PointerType *nodeConsumerFctPtrTy;
    FunctionType *scanNodesByLabelTy;
    FunctionType *gdb_get_node_by_id_type;

    FunctionType *queryResultConsumerFctTy;

    FunctionType *consumerDummyCall;

//++++++++++++++++++ RELATIONSHIP SCAN FCT +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    FunctionType *rshipConsumerFctTy;
    PointerType *rshipConsumerFctPtrTy;

    FunctionType *foreachRshipFctTy;

    FunctionType *countPotentialOHopFctTy;
    FunctionType *retrieveFEVqueryFctTy;
    FunctionType *fevQueueEmptyFctTy;
    FunctionType *insertInFEVQueueFctTy;

    FunctionType *feFromVarFctTy;
    FunctionType *getNextRshipFctTy;
    FunctionType *fevListEndFctTy;
//++++++++++++++++++ FILTER FCT ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    FunctionType *filterConsumerFctTy;

//++++++++++++++++++ EXPAND FCT +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    FunctionType *expandConsumerFctTy;
    FunctionType *collectConsumerFctTy;
    FunctionType *finishFctTy;
    FunctionType *limitFctTy;
//++++++++++++++++++ EXPAND FCT +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    FunctionType *joinConsumerFctTy;
    FunctionType *joinInsertLeftFctTy; // TODO: insert name argument to identify correct join table
    FunctionType *joinConsumeLeftFctTy; // TODO: insert name argument to identify correct join table

//++++++++++++++++++ PROJECT FCT +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    FunctionType *projectConsumerFctTy;
    FunctionType *applyNodeProjectionFctTy;
    FunctionType *applyRshipProjectionFctTy;
//++++++++++++++++++ INDEX +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    FunctionType *indexGetNodeTy;

//++++++++++++++++++ START FCT +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    FunctionType *startFctTy;
    FunctionType *collectFctTy;

    FunctionType *getTxFctTy;
    FunctionType *getValidNodeFctTy;

    FunctionType *consumerFctTy;

//++++++++++++++++++ CONSTANTS +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    Value *constZero;
    Value *constOne;

    ArrayType *res_arr_type;

//++++++++++++++++++ DATA_STRUCTURES_FUNCTIONS +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++ QR_LIST +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    FunctionType *add_qr_list_end;
    FunctionType *add_qr_list_front;
    FunctionType *get_front_qr_list;
    FunctionType *get_back_qr_list;
    FunctionType *pop_front_qr_list;
    FunctionType *pop_back_qr_list;
    FunctionType *append_qr_list;
    FunctionType *at_qr_list;
    FunctionType *concat_qr_list;

//++++++++++++++++++ UTILITY +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    FunctionType *vec_end_reached;
    FunctionType *vec_get_begin;
    FunctionType *vec_get_next;

    FunctionType *apply_pexpr_ty;
    FunctionType *call_consumer_ty;

    FunctionType *get_join_vec_size_ty;
    FunctionType *get_join_vec_arr_ty;

    FunctionType *mat_reg_val_ty;
    FunctionType *collect_reg_ty;

    FunctionType *obtain_mat_tuple_ty;
    FunctionType *mat_node_ty;
    FunctionType *mat_rship_ty;
    FunctionType *collect_tuple_join_ty;

    FunctionType *get_join_tp_at_ty;
    FunctionType *get_node_res_at_ty;
    FunctionType *get_rship_res_at_ty;
    FunctionType *get_mat_res_size_ty;

//++++++++++++++++++ DICT FUNCTIONS ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    FunctionType *lookup_label_type;
    FunctionType *lookup_dcode_type;

//++++++++++++++++++ GDB FUNCTIONS +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    FunctionType *gdb_get_nodes_type;
    FunctionType *get_node_from_it_type;
    FunctionType *gdb_get_rship_by_id_type;
    FunctionType *gdb_get_dcode_type;

    FunctionType *create_node_type;
    FunctionType *create_rship_type;

    FunctionType *foreach_variable_from_type;

//++++++++++++++++++ PROPERTY FUNCTIONS ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    FunctionType *pset_get_item_at_type;

//++++++++++++++++++ LOOP FUNCTIONS ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    enum WHILE_COND {
        EQ = 0,
        UEQ = 1,
        LT = 2,
        LE = 3,
        GT = 4,
        GE = 5
    };

    BasicBlock *
    while_loop_condition(Function *parent, Value *cond_lhs, Value *cond_rhs, WHILE_COND cond_op,
                         BasicBlock *endBB, const std::function<void(BasicBlock *, BasicBlock *)> &loop_body);

    BasicBlock *while_loop(Function *parent,
                           FunctionCallee get_begin, FunctionCallee get_next, FunctionCallee reached_end,
                           Value *it_alloca, Value *cond_param,
                           BasicBlock *nextBB, const std::function<void(BasicBlock *, BasicBlock *)> &loop_body,
                           BasicBlock *loop_body_condition = nullptr);

    BasicBlock *while_rship_exist(Function *parent, Value *gdb, Value *node, BasicBlock *nextBB, int nodeGEPidx, int rshipGEPidx,
                                  BasicBlock *endBB, const std::function<void(BasicBlock *, BasicBlock *, Value *rship)> &loop_body);

    Value * create_qr_node(Value *qr_element, Value *qr_prev = nullptr,
                           Value *qr_next = nullptr);

    Value *create_qr_list(Value *firstElement = nullptr);

    Function *qr_list_add_end();

    Function *qr_list_add_front();

    Function *qr_list_get_front();

    Function *qr_list_get_back();

    Function *qr_list_pop_back();

    Function *qr_list_pop_front(Module &module);

    BasicBlock *while_qr_list(Function *parent, Value *qr_list, AllocaInst *cur_node, BasicBlock *next,
                              const std::function<void(BasicBlock *, BasicBlock *)> &loop_body);

    Function *qr_list_append();
    Function *qr_list_at();

    Function *qr_list_concat();

    Value *node_cmp_label(Value *node, Value *label_code);
    Value *rship_cmp_label(Value *rship, Value *label_code);
    Value *extract_arg_label(int op_id, Value *gdb, Value *arg_desc);

    std::stack<Value*> alloc_stack;
    std::map<std::string, Value*> globals_;
    std::map<std::string, FunctionType *> function_types;
    std::map<std::string, Value *> gen_funcs;
};

#endif //ART_P_CONTEXT_HPP
