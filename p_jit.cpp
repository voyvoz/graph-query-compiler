#include <llvm/ExecutionEngine/Orc/CompileUtils.h>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/IR/Mangler.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include "p_jit.hpp"
#include "optimizer.hpp"
#include <cstdio>

#include "global_definitions.hpp"

using namespace llvm;
using namespace llvm::orc;

p_jit::p_jit(ExitOnError ExitOnErr)
        : ctx(std::make_unique<LLVMContext>()),
          ES(std::make_unique<ExecutionSession>()),
          TM(createTargetMachine(ExitOnErr)),
          E_ERR(ExitOnErr),
          //GDBListener(JITEventListener::createGDBRegistrationListener()),
          ObjLinkingLayer(*ES, createMemoryManagerFtor()),
#if USE_PMDK
          ObjCache(std::make_unique<PJitObjectCache>("/mnt/pmem0/jit_cache")),
#else
        ObjCache(std::make_unique<PJitObjectCache>()),
#endif
          CompileLayer(*ES, ObjLinkingLayer, std::make_unique<SimpleCompiler>(*TM)),
          OptimizeLayer(*ES, CompileLayer) {
    //ObjLinkingLayer.setNotifyLoaded(createNotifyLoadedFtor());
    auto exp_jit_dylib = ES->createJITDylib("Main");
    if(exp_jit_dylib) {
        if(auto R = createHostProcessResolver())
                ES->getJITDylibByName("Main")->addGenerator(std::move(R));
        SymbolMap M;
        MangleAndInterner Mangle(*ES, getDataLayout());
        // Register every symbol that can be accessed from the JIT'ed code.
        M[Mangle("vec_end_reached")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&vec_end_reached), JITSymbolFlags::Exported);
        M[Mangle("get_vec_begin")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&get_vec_begin), JITSymbolFlags::Exported);
        M[Mangle("get_vec_next")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&get_vec_next), JITSymbolFlags::Exported);
        M[Mangle("dict_lookup_label")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&dict_lookup_label), JITSymbolFlags::Exported);
        M[Mangle("gdb_get_nodes")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&gdb_get_nodes), JITSymbolFlags::Exported);
        M[Mangle("node_by_id")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&node_by_id), JITSymbolFlags::Absolute);
        M[Mangle("get_node_from_it")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&get_node_from_it), JITSymbolFlags::Exported);
        M[Mangle("gdb_get_rships")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&gdb_get_rships), JITSymbolFlags::Exported);
        M[Mangle("get_rship_from_it")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&get_rship_from_it), JITSymbolFlags::Exported);
        M[Mangle("get_vec_begin_r")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&get_vec_begin_r), JITSymbolFlags::Exported);
        M[Mangle("get_vec_next_r")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&get_vec_next_r), JITSymbolFlags::Exported);
        M[Mangle("vec_end_reached_r")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&vec_end_reached_r), JITSymbolFlags::Exported);
        M[Mangle("rship_by_id")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&rship_by_id), JITSymbolFlags::Exported);
        M[Mangle("gdb_get_dcode")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&gdb_get_dcode), JITSymbolFlags::Exported);
        M[Mangle("pset_get_item_at")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&pset_get_item_at), JITSymbolFlags::Exported);
        M[Mangle("get_tx")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&get_tx), JITSymbolFlags::Exported);
        M[Mangle("get_valid_node")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&get_valid_node), JITSymbolFlags::Exported);
        M[Mangle("apply_pexpr")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&apply_pexpr), JITSymbolFlags::Exported);
        M[Mangle("dict_lookup_dcode")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&lookup_dc), JITSymbolFlags::Exported);
        M[Mangle("create_node")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&create_node), JITSymbolFlags::Exported);
        M[Mangle("create_ship")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&create_rship), JITSymbolFlags::Exported);
        M[Mangle("foreach_variable_from")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&foreach_variable_from), JITSymbolFlags::Exported);
        M[Mangle("foreach_variable_from")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&foreach_variable_from), JITSymbolFlags::Exported);
        M[Mangle("mat_reg_value")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&mat_reg_value), JITSymbolFlags::Exported);
        M[Mangle("collect_tuple")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&collect_tuple), JITSymbolFlags::Exported);
        M[Mangle("obtain_mat_tuple")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&obtain_mat_tuple), JITSymbolFlags::Exported);
        M[Mangle("mat_node")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&mat_node), JITSymbolFlags::Exported);
        M[Mangle("mat_rship")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&mat_rship), JITSymbolFlags::Exported);
        M[Mangle("collect_tuple_join")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&collect_tuple_join), JITSymbolFlags::Exported);
        M[Mangle("get_join_tp_at")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&get_join_tp_at), JITSymbolFlags::Exported);
        M[Mangle("get_node_res_at")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&get_node_res_at), JITSymbolFlags::Exported);
        M[Mangle("get_rship_res_at")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&get_rship_res_at), JITSymbolFlags::Exported);
        M[Mangle("get_mat_res_size")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&get_mat_res_size), JITSymbolFlags::Exported);
        M[Mangle("index_get_node")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&index_get_node), JITSymbolFlags::Exported);
        M[Mangle("apply_pexpr_node")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&apply_pexpr_node), JITSymbolFlags::Exported);
        M[Mangle("apply_pexpr_rship")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&apply_pexpr_rship), JITSymbolFlags::Exported);
        M[Mangle("retrieve_fev_queue")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&retrieve_fev_queue), JITSymbolFlags::Exported);            
        M[Mangle("insert_fev_rship")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&insert_fev_rship), JITSymbolFlags::Exported);            
        M[Mangle("foreach_from_variable_rship")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&foreach_from_variable_rship), JITSymbolFlags::Exported);            
        M[Mangle("get_next_rship_fev")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&get_next_rship_fev), JITSymbolFlags::Exported);            
        M[Mangle("fev_list_end")] = JITEvaluatedSymbol(
                pointerToJITTargetAddress(&fev_list_end), JITSymbolFlags::Exported);            

        ExitOnErr(ES->getJITDylibByName("Main")->define(absoluteSymbols(M)));
    }
}


Error p_jit::addModule(std::unique_ptr<Module> M) {
    auto K = ES->allocateVModule();
    ModuleKeys.push_back(K);

    /*auto obj = ObjCache->getCachedObject(*M);
    if(!obj) {
        M.~unique_ptr();
        return obj.takeError();
    }


    if(obj->hasValue()) {
        M.~unique_ptr();
        return ObjLinkingLayer.add(*ES->getJITDylibByName("Main"), std::move(obj->getValue()));
    }*/

    OptimizeLayer.setTransform(Optimizer(3));

    return OptimizeLayer.add(*ES->getJITDylibByName("Main"), ThreadSafeModule(std::move(M), ctx), K);
    //cantFail(CompileLayer.add(*ES->getJITDylibByName("Main"), ThreadSafeModule(std::move(M), ctx)));
}


std::unique_ptr<TargetMachine> p_jit::createTargetMachine(llvm::ExitOnError ExitOnErr) {
    auto JTMP = ExitOnErr(JITTargetMachineBuilder::detectHost());
    return ExitOnErr(JTMP.createTargetMachine());
}

using GetMemoryManagerFunction =
RTDyldObjectLinkingLayer::GetMemoryManagerFunction;

GetMemoryManagerFunction p_jit::createMemoryManagerFtor() {
    return []() -> GetMemoryManagerFunction::result_type {
        return std::make_unique<SectionMemoryManager>();
    };
}

std::string p_jit::mangle(llvm::StringRef UnmangledName) {
    std::string MangledName;
    {
        DataLayout DL = getDataLayout();
        raw_string_ostream MangledNameStream(MangledName);
        Mangler::getNameWithPrefix(MangledNameStream, UnmangledName, DL);
    }
    return MangledName;
}

Error p_jit::applyDataLayout(llvm::Module &M) {
    DataLayout DL = TM->createDataLayout();
    if(M.getDataLayout().isDefault())
        M.setDataLayout(DL);

    if(M.getDataLayout() != DL)
        return make_error<StringError>(
                "Added modules have incompatible data layouts",
                inconvertibleErrorCode());
    return Error::success();
}

Expected<JITTargetAddress> p_jit::getFunctionAddr(llvm::StringRef Name) {
    SymbolStringPtr NamePtr = ES->intern(mangle(Name));
    JITDylibSearchOrder JDs{{ES->getJITDylibByName("Main"), JITDylibLookupFlags::MatchAllSymbols}};
    Expected<JITEvaluatedSymbol> S = ES->lookup(JDs, NamePtr);
    if(!S)
        return S.takeError();
    JITTargetAddress A = S->getAddress();
    if(!A)
        printf("Error!!!\n");
    return A;
}

std::unique_ptr<DynamicLibrarySearchGenerator> p_jit::createHostProcessResolver() {
    char Prefix = TM->createDataLayout().getGlobalPrefix();
    auto R = DynamicLibrarySearchGenerator::GetForCurrentProcess(Prefix);
    if(!R) {
        ES->reportError(R.takeError());
        return nullptr;
    }

    if(!*R) {
        return nullptr;
    }
    return std::move(*R);
}
