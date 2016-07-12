

#ifndef LLVM_TARGET_MACHINE_H
#define LLVM_TARGET_MACHINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "llvm-c/Types.h"

void LLVMInitializeTargetOptimizer();
void LLVMOptimizeModuleForTarget(LLVMModuleRef module);
void LLVMAddTargetMachinePasses(LLVMPassManagerRef passManager);

#ifdef __cplusplus
}
#endif /* defined(__cplusplus) */

#endif
