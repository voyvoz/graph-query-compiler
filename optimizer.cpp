#include "optimizer.hpp"
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Transforms/Utils.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/AggressiveInstCombine/AggressiveInstCombine.h>
#include <llvm/Transforms/Vectorize.h>
#include <llvm/CodeGen/Passes.h>
#include <llvm/Analysis/CallGraphSCCPass.h>

using namespace llvm;
using namespace llvm::orc;

Expected<ThreadSafeModule>
Optimizer::operator()(ThreadSafeModule TSM,
                      const MaterializationResponsibility &) {
    Module &M = *TSM.getModuleUnlocked();


    legacy::FunctionPassManager FPM(&M);
    //FPM.add(createPromoteMemoryToRegisterPass());
    /*FPM.add(createCFGSimplificationPass());
    FPM.add(createLCSSAPass());
    FPM.add(createLoopDeletionPass());*/
    //FPM.add(createInstructionCombiningPass());
    B.populateFunctionPassManager(FPM);
    FPM.doInitialization();

    for (Function &F : M)
        FPM.run(F);
    FPM.doFinalization();
    //M.dump();
    legacy::PassManager MPM;
    //B.populateModulePassManager(MPM);
    //MPM.run(M);
    //M.dump();
    return std::move(TSM);
}
