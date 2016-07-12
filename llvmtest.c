/**
 * LLVM equivalent of:
 *
 * void loop(double *result, double *a, double *b, double *c, size_t length) {
 *     for(size_t i = 0; i < length; i++) {
 *	       result[i] = a[i] * b[i] * c[i];
 *     }
 * }
 */

#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/Transforms/IPO.h>
#include <llvm-c/Transforms/Scalar.h>
#include <llvm-c/Transforms/Vectorize.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "target_machine.h"

// compile: clang `llvm-config --cflags` -c llvmtest.c -o llvmtest.o 
// link: clang++ `llvm-config --cxxflags --ldflags` llvmtest.o `llvm-config --libs --system-libs` -L`pwd` -lLLVMTargetMachineExtra -o llvmtest
// both: clang `llvm-config --cflags` -c llvmtest.c -o llvmtest.o && clang++ `llvm-config --cxxflags --ldflags` llvmtest.o `llvm-config --libs --system-libs` -L`pwd` -lLLVMTargetMachineExtra -o llvmtest

static void print_array(const char *name, double* ptr, size_t elements) {
    printf("%s: [", name);
    for(size_t i = 0; i < elements; i++) {
        printf("%lf", ptr[i]);
        if (i != elements - 1) {
            printf(", ");
        }
    }
    printf("]\n");
}

static void 
GenData(double **values, long long count) {
    *values = malloc(sizeof(double) * count);
    if (*values == NULL) {
        fprintf(stderr, "Failed to allocate memory.\n");
        exit(EXIT_FAILURE);
    }
    for(long long i = 0; i < count; i++) {
        values[0][i] = rand() % 100;
    }
}

LLVMPassManagerRef InitializePassManager(LLVMModuleRef module);

int main(int argc, char const *argv[]) {
    size_t elements = 125000000;
    int seed = 37;
    // generate input data
    srand(seed);
    double *x;
    double *y;
    double *z;
    GenData(&x, elements);
    GenData(&y, elements);
    GenData(&z, elements);

    if (!x || !y || !z) {
        fprintf(stderr, "Failed to load data.\n");
        exit(EXIT_FAILURE);
    }

    // boilerplate initialization code
    LLVMLinkInMCJIT();
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    struct LLVMMCJITCompilerOptions options;
    LLVMInitializeMCJITCompilerOptions(&options, sizeof(options));

    clock_t tic = clock();

    // module that holds our function
    LLVMModuleRef module = LLVMModuleCreateWithName("LoopModule");
    LLVMOptimizeModuleForTarget(module);

    // LLVM types that we will use
    LLVMTypeRef void_type = LLVMVoidType();
	LLVMTypeRef double_type = LLVMDoubleType();
	LLVMTypeRef doubleptr_type = LLVMPointerType(double_type, 0);
	LLVMTypeRef int64_type = LLVMInt64Type();

    // function prototype information
    LLVMTypeRef return_type = void_type;
    LLVMTypeRef param_types[] = { doubleptr_type, doubleptr_type, doubleptr_type, 
        doubleptr_type, int64_type};
    size_t arg_count = 5;

    // create function prototype
    LLVMTypeRef prototype = LLVMFunctionType(return_type, param_types, arg_count, 0);
    // create function from prototype
    LLVMValueRef function = LLVMAddFunction(module, "loop", prototype);

    // basic blocks
    LLVMBasicBlockRef entry = LLVMAppendBasicBlock(function, "entry");
    LLVMBasicBlockRef condition = LLVMAppendBasicBlock(function, "condition");
    LLVMBasicBlockRef body = LLVMAppendBasicBlock(function, "body");
    LLVMBasicBlockRef increment = LLVMAppendBasicBlock(function, "increment");
    LLVMBasicBlockRef end = LLVMAppendBasicBlock(function, "end");

    // create LLVM IR builder
    LLVMBuilderRef builder = LLVMCreateBuilder();

    // entry point of function
    LLVMValueRef index_addr;
    LLVMPositionBuilderAtEnd(builder, entry);
    {
        // create index and initialize to zero
        // this will be the variable we loop over in the for loop
        index_addr = LLVMBuildAlloca(builder, int64_type, "index");
        LLVMBuildStore(builder, LLVMConstInt(int64_type, 0, 1), index_addr);
        // goto condition
        LLVMBuildBr(builder, condition);
    }

    // for loop condition
    LLVMPositionBuilderAtEnd(builder, condition);
    {   
        // check if index < size
    	LLVMValueRef index = LLVMBuildLoad(builder, index_addr, "[index]");
    	LLVMValueRef cond = LLVMBuildICmp(builder, LLVMIntSLT, index, LLVMGetParam(function, 4), "index < size");
        // conditional branch, if index < size then go to the body, otherwise go to the end
    	LLVMBuildCondBr(builder, cond, body, end);
    }
    // for loop body
	LLVMPositionBuilderAtEnd(builder, body);
	{  
        // obtain index for the current multiplication
	    LLVMValueRef index = LLVMBuildLoad(builder, index_addr, "[index]");
        // load x[index]
	    LLVMValueRef x_addr = LLVMBuildInBoundsGEP(builder, LLVMGetParam(function, 1), &index, 1, "&x[index]");
	    LLVMValueRef xindex = LLVMBuildLoad(builder, x_addr, "x[index]");
        // load y[index]
	    LLVMValueRef y_addr = LLVMBuildInBoundsGEP(builder, LLVMGetParam(function, 2), &index, 1, "&y[index]");
	    LLVMValueRef yindex = LLVMBuildLoad(builder, y_addr, "y[index]");
        // compute x[index] * y[index]
	    LLVMValueRef xmuly = LLVMBuildFMul(builder, xindex, yindex, "tempval");
        // load z[index]
        LLVMValueRef z_addr = LLVMBuildInBoundsGEP(builder, LLVMGetParam(function, 3), &index, 1, "&z[index]");
        LLVMValueRef zindex = LLVMBuildLoad(builder, z_addr, "z[index]");
        // compute xmuly * z[index]
        LLVMValueRef finalres = LLVMBuildFMul(builder, xmuly, zindex, "tempval * z[index]");
        // get the address of result[index]
	    LLVMValueRef result_addr = LLVMBuildInBoundsGEP(builder, LLVMGetParam(function, 0), &index, 1, "result[index]");
        // store the computation result in result[index]
	    LLVMBuildStore(builder, finalres, result_addr);
        // go to increment
	    LLVMBuildBr(builder, increment);
	}
    // for loop increment
    LLVMPositionBuilderAtEnd(builder, increment);
    {
        // load index
        LLVMValueRef index = LLVMBuildLoad(builder, index_addr, "[index]");
        // compute index + 1
        LLVMValueRef indexpp = LLVMBuildAdd(builder, index, LLVMConstInt(int64_type, 1, 1), "index++");
        // store index + 1 in index
        LLVMBuildStore(builder, indexpp, index_addr);
        LLVMBuildBr(builder, condition);
    }
    // end point
    LLVMPositionBuilderAtEnd(builder, end);
    {
        // simply exit the function (return;)
        LLVMBuildRetVoid(builder);
    }

    // LLVMDumpModule(module);

    // optimize function
    LLVMPassManagerRef passManager = InitializePassManager(module);
    LLVMRunFunctionPassManager(passManager, function);
    LLVMDisposePassManager(passManager);

    LLVMDumpModule(module);

    // create JIT and compile function
    LLVMExecutionEngineRef engine;
    char *error = NULL;
    if (LLVMCreateMCJITCompilerForModule(&engine, module, &options, sizeof(options), &error) != 0) {
        fprintf(stderr, "failed to create execution engine\n");
        abort();
    }
    if (error) {
        fprintf(stderr, "error: %s\n", error);
        LLVMDisposeMessage(error);
        exit(EXIT_FAILURE);
    }

    typedef void (*fptr)(double*,double*,double*,double*,long long);
    // get pointer to compiled function
    fptr func = (fptr) LLVMGetFunctionAddress(engine, "loop");
    if (!func) {
        fprintf(stderr, "Failed to get function pointer.\n");
        exit(EXIT_FAILURE);
    }
    clock_t compile = clock();

    double *result = malloc(sizeof(double) * elements);
    if (!result) {
        fprintf(stderr, "Malloc fail.\n");
        exit(EXIT_FAILURE);
    }
    // call function
    func(result, x, y, z, elements);

    clock_t toc = clock();

    printf("Compile: %f seconds\n", (double)(compile - tic) / CLOCKS_PER_SEC);
    printf("Elapsed: %f seconds\n", (double)(toc - tic) / CLOCKS_PER_SEC);

    print_array("x", x, 5);
    print_array("y", y, 5);
    print_array("z", z, 5);
    print_array("result", result, 5);

    LLVMDisposeBuilder(builder);
    LLVMDisposeExecutionEngine(engine);
}

LLVMPassManagerRef 
InitializePassManager(LLVMModuleRef module) {
    LLVMPassManagerRef passManager = LLVMCreateFunctionPassManagerForModule(module);
    // This set of passes was copied from the Julia people (who probably know what they're doing)
    // Julia Passes: https://github.com/JuliaLang/julia/blob/master/src/jitlayers.cpp

    LLVMAddTargetMachinePasses(passManager);
    LLVMAddCFGSimplificationPass(passManager);
    LLVMAddPromoteMemoryToRegisterPass(passManager);
    LLVMAddInstructionCombiningPass(passManager);
    LLVMAddScalarReplAggregatesPass(passManager);
    LLVMAddScalarReplAggregatesPassSSA(passManager);
    LLVMAddInstructionCombiningPass(passManager);
    LLVMAddJumpThreadingPass(passManager);
    LLVMAddInstructionCombiningPass(passManager);
    LLVMAddReassociatePass(passManager);
    LLVMAddEarlyCSEPass(passManager);
    LLVMAddLoopIdiomPass(passManager);
    LLVMAddLoopRotatePass(passManager);
    LLVMAddLICMPass(passManager);
    LLVMAddLoopUnswitchPass(passManager);
    LLVMAddInstructionCombiningPass(passManager);
    LLVMAddIndVarSimplifyPass(passManager);
    LLVMAddLoopDeletionPass(passManager);
    LLVMAddLoopUnrollPass(passManager);
    LLVMAddLoopVectorizePass(passManager);
    LLVMAddInstructionCombiningPass(passManager);
    LLVMAddGVNPass(passManager);
    LLVMAddMemCpyOptPass(passManager);
    LLVMAddSCCPPass(passManager);
    LLVMAddInstructionCombiningPass(passManager);
    LLVMAddSLPVectorizePass(passManager);
    LLVMAddAggressiveDCEPass(passManager);
    LLVMAddInstructionCombiningPass(passManager);

    LLVMInitializeFunctionPassManager(passManager);
    return passManager;
}
