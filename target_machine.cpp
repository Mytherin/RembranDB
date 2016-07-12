
// For some strange reason, the LLVM C API does not expose target-machine specific optimizations
// This is annoying; because without target-machine specific optimizations we cannot be very fast
// because we it means LLVM will not generate SIMD instructions (it doesn't know if your CPU supports them)
// This small library exposes target machine specific optimizations to the C API so we can be fast

#include "llvm-c/TargetMachine.h"

#include "target_machine.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/IR/LegacyPassManager.h"
#include <llvm/Analysis/TargetTransformInfo.h>

// compile: clang++ -std=c++11 `llvm-config --cxxflags` -c target_machine.cpp  -O3 -o target_machine.o
// library: ar rs libLLVMTargetMachineExtra.a  target_machine.o
// both: clang++ -std=c++11 `llvm-config --cxxflags` -c target_machine.cpp  -O3 -o target_machine.o && ar rs libLLVMTargetMachineExtra.a target_machine.o

using namespace llvm;

inline Module *unwrap_mod(LLVMModuleRef P) {
	return reinterpret_cast<Module *>(P);
}

inline legacy::FunctionPassManager *unwrap_pm(LLVMPassManagerRef P) {
	return reinterpret_cast<legacy::FunctionPassManager *>(P);
}

TargetMachine *tm = NULL;

void LLVMInitializeTargetOptimizer() {
	if (tm) return;
	tm = EngineBuilder().selectTarget();
	tm->setOptLevel(CodeGenOpt::Aggressive);
}

void LLVMOptimizeModuleForTarget(LLVMModuleRef module) {
	if (!tm) LLVMInitializeTargetOptimizer();
	unwrap_mod(module)->setDataLayout(tm->createDataLayout());
}

void LLVMAddTargetMachinePasses(LLVMPassManagerRef passManager) {
	if (!tm) LLVMInitializeTargetOptimizer();
	unwrap_pm(passManager)->add(createTargetTransformInfoWrapperPass(tm->getTargetIRAnalysis()));
}