#ifndef PJIT_OPTIMIZER_HPP
#define PJIT_OPTIMIZER_HPP
#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/Support/Error.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>

/**
 * Helper class to manage the IR code optimization passes and optimization level
 */
class Optimizer {
public:
    Optimizer(unsigned OptLevel) { B.OptLevel = OptLevel; }

    /**
     * Process the given optimization passes
     */
    llvm::Expected<llvm::orc::ThreadSafeModule>
    operator()(llvm::orc::ThreadSafeModule TSM,
               const llvm::orc::MaterializationResponsibility &);

private:
    llvm::PassManagerBuilder B;
};;


#endif //PJIT_OPTIMIZER_HPP
