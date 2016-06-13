
#include "llvm/ADT/STLExtras.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Vectorize.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include <cctype>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "jit.hpp"
#include "table.hpp"
#include "parser.hpp"

using namespace llvm;
using namespace llvm::orc;

static LLVMContext context;
static IRBuilder<> builder(context);
static std::unique_ptr<legacy::FunctionPassManager> fpm;
static std::unique_ptr<JIT> jit;
static std::unique_ptr<Module> module;

static void Initialize(void);
static void ResetJIT(void);
static std::string ReadQuery(void);
static Table *ExecuteQuery(Query *query);
static void Cleanup(void); 

static bool enable_optimizations = false;
static bool print_result = true;
static bool print_llvm = true;
static bool execute_statement = false;
static std::string statement = "";

// compile: clang++ -g database.cpp `llvm-config --cxxflags --ldflags --system-libs --libs` -O0 -o database

// explain how to do this:
// SELECT (x + 5) * 2 FROM table;
// exercise/benchmark: 
// SELECT * FROM table WHERE x > 5 AND (y < 10 OR z > 20);

int main(int argc, char** argv) {
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();

    Initialize();

    for(int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg.find("--help") != std::string::npos) {
            std::cout << "RembranDB Options." << std::endl;
            std::cout << "  -opt              Enable  LLVM optimizations." << std::endl;
            std::cout << "  -no-print         Do not print query results." << std::endl;
            std::cout << "  -no-llvm          Do not print LLVM instructions." << std::endl;
            std::cout << "  -s \"stmnt\"        Execute \"stmnt\" and exit." << std::endl;
            return 0;
        } else if (arg.find("-opt") != std::string::npos) {
            std::cout << "Optimizations enabled." << std::endl;
            enable_optimizations = true;
        } else if (arg.find("-no-print") != std::string::npos) {
            std::cout << "Printing output disabled." << std::endl;
            print_result = false;
        } else if (arg.find("-no-llvm") != std::string::npos) {
            std::cout << "Printing LLVM disabled." << std::endl;
            print_llvm = false;
        } else if (arg.find("-s") != std::string::npos) {
            execute_statement = true;
        } else if (execute_statement) {
            statement = std::string(arg);
        }
    }

    if (!execute_statement) {
        std::cout << "# RembranDB server v0.0.0.1" << std::endl;
        std::cout << "# Serving table \"demo\", with no support for multithreading" << std::endl;
        std::cout << "# Did not find any available memory (didn't look for any either)" << std::endl;
        std::cout << "# Not listening to any connection requests." << std::endl;
        std::cout << "# RembranDB/SQL module loaded" << std::endl;
    }

    while(true) {
        ResetJIT();
        std::string query_string;
        if (!execute_statement) {
            query_string = ReadQuery();
        } else {
            query_string = statement;
        }

        if (query_string == "\\q" || (query_string.size() > 0 && query_string[0] == '^')) break;
        if (query_string == "\\d") {
            PrintTables();
            continue;
        }
        Query *query = ParseQuery(query_string);
        
        if (query) {
            clock_t tic = clock();
            Table *tbl = ExecuteQuery(query);
            clock_t toc = clock();

            printf("Total Runtime: %f seconds\n", (double)(toc - tic) / CLOCKS_PER_SEC);

            if (print_result) {
                PrintTable(tbl);
            }
        }
        if (execute_statement) break;
    }

    Cleanup();
}

static void EnableOptimizations(void) {
    // set optimizations that we want LLVM to perform
    // Do simple "peephole" optimizations and bit-twiddling optzns.
    fpm->add(createInstructionCombiningPass());
    // Reassociate expressions.
    fpm->add(createReassociatePass());
    // Eliminate Common SubExpressions.
    fpm->add(createGVNPass());
    // Simplify the control flow graph (deleting unreachable blocks, etc).
    fpm->add(createCFGSimplificationPass());
    // Vectorization Transformations
    fpm->add(createBBVectorizePass());
    fpm->add(createLoopVectorizePass());
    fpm->add(createSLPVectorizerPass());
}

static void ResetJIT(void) {
    module = llvm::make_unique<Module>("RembranDB", context);
    module->setDataLayout(jit->getTargetMachine().createDataLayout());

    // Create a new pass manager attached to it.
    fpm = llvm::make_unique<legacy::FunctionPassManager>(module.get());

    if (enable_optimizations) {
        EnableOptimizations();
    }
    fpm->doInitialization();
}


static void Initialize(void) {
    jit = llvm::make_unique<JIT>();

    std::vector<std::string> tables(2);
    tables[0] = "demo";
    tables[1] = "benchmark";

    // Load data, demo table = small table (20 entries per column), benchmark table = big table (1GB per column)
    ReadTables(tables);
}

static std::string 
ReadQuery(void) {
    std::string input = "";
    std::string line;
    std::cout << "> ";
    while(std::cin) {
        getline(std::cin, line);
        if (!std::cin) break;
        if (line.size() > 0 && line[0] == '\\') return line;
        input += line + " ";
        if (line.find(';') != std::string::npos) {
            return input;
        }
        std::cout << "> ";
    }
    std::cout << std::endl;
    return "\\q";
}

static Value *PerformOperation(Operation *op, Value *index) {
    if (op->Type() == OPTYPE_const) {
        return ConstantFP::get(Type::getDoubleTy(context), ((ConstantOperation*) op)->value);
    } else if (op->Type() == OPTYPE_colmn) {
        ColumnOperation *colop = (ColumnOperation *) op;
        AllocaInst *address = colop->column->address;
        LoadInst *value_address = builder.CreateLoad(address);
        Type *column_type = GetLLVMType(context, colop->column->type);
        Value *colptr = builder.CreateGEP(column_type, value_address, index, "column[i]");
        Value *value = builder.CreateLoad(colptr);
        if (colop->column->type == TYPE_dbl) return value;
        if (colop->column->type == TYPE_flt) {
            return builder.CreateFPCast(value, GetLLVMType(context, TYPE_dbl));
        }
        return builder.CreateSIToFP(value, GetLLVMType(context, TYPE_dbl));
    } else if (op->Type() == OPTYPE_binop) {
        BinaryOperation *binop = (BinaryOperation *) op;
        Value *lhs = PerformOperation(binop->LHS, index);
        Value *rhs = PerformOperation(binop->RHS, index);
        switch(binop->optype) {
            case OPTYPE_mul:
                return builder.CreateFMul(lhs, rhs);
            case OPTYPE_div:
                return builder.CreateFDiv(lhs, rhs);
            case OPTYPE_add:
                return builder.CreateFAdd(lhs, rhs);
            case OPTYPE_sub:
                return builder.CreateFSub(lhs, rhs);
            case OPTYPE_lt:
                return builder.CreateFCmpOLT(lhs, rhs);
            case OPTYPE_le:
                return builder.CreateFCmpOLE(lhs, rhs);
            case OPTYPE_eq:
                return builder.CreateFCmpOEQ(lhs, rhs);
            case OPTYPE_ne:
                return builder.CreateFCmpONE(lhs, rhs);
            case OPTYPE_gt:
                return builder.CreateFCmpOGT(lhs, rhs);
            case OPTYPE_ge:
                return builder.CreateFCmpOGE(lhs, rhs);
            case OPTYPE_and:
                return builder.CreateAnd(lhs, rhs);
            case OPTYPE_or:
                return builder.CreateOr(lhs, rhs);
        }
    }
    std::cout << "Unrecognized operation" << std::endl;
    return NULL;
}

typedef void (*functionptr)(double* result, size_t size, void** inputs);

static Table*
ExecuteQuery(Query *query) {
    clock_t tic, toc;
    double compile = 0;
    double execute = 0;
    Table *table = (Table*) malloc(sizeof(Table));
    table->name = strdup("Result Table");
    Column *current_column = NULL;

    OperationList *collection;
    for(collection = query->selection; collection; collection = collection->next) {
        tic = clock();
        // Generate Code
        Operation *op = collection->operation;
        ColumnList *columns = collection->columns;

        size_t column_count = GetColCount(columns);

        Type *double_tpe = Type::getDoubleTy(context);
        Type *doubleptr_tpe = PointerType::get(double_tpe, 0);
        Type *int64_tpe = Type::getInt64Ty(context);
        Type *void_tpe = Type::getVoidTy(context);
        Type *voidptr_tpe = PointerType::get(void_tpe, 0);
        Type *voidptrptr_tpe = PointerType::get(voidptr_tpe, 0);

        std::vector<Type *> arguments(3);
        arguments[0] = doubleptr_tpe;
        arguments[1] = int64_tpe;
        arguments[2] = voidptrptr_tpe;

        FunctionType *prototype = FunctionType::get(void_tpe, arguments, false);

        Function *function = Function::Create(prototype, Function::ExternalLinkage, "TestFunction", module.get());
        function->setCallingConv(CallingConv::C);

        BasicBlock *entry = BasicBlock::Create(context, "entry", function, 0);
        BasicBlock *cond = BasicBlock::Create(context, "for.cond", function, 0);
        BasicBlock *body = BasicBlock::Create(context, "for.body", function, 0);
        BasicBlock *inc = BasicBlock::Create(context, "for.inc", function, 0);
        BasicBlock *end = BasicBlock::Create(context, "for.end", function, 0);

        std::vector<AllocaInst*> argument_addresses(3);
        std::vector<AllocaInst*> input_addresses(column_count);
        AllocaInst *index_addr;
        builder.SetInsertPoint(entry);
        {
            Function::arg_iterator args;
            size_t i = 0;
            for(args = function->arg_begin(); args != function->arg_end(); args++, i++) {
                argument_addresses[i] = builder.CreateAlloca(arguments[i], nullptr, "input_args");
                builder.CreateStore(&*args, argument_addresses[i]);
            }
            
            LoadInst *input_ptrs = builder.CreateLoad(argument_addresses[2]);
            i = 0;
            for(ColumnList *col = columns; i < column_count; col = col->next, i++) {
                input_addresses[i] = builder.CreateAlloca(voidptr_tpe, nullptr, "input_arrays[i]");
                Value *input_ptrptr = builder.CreateGEP(voidptr_tpe, input_ptrs, ConstantInt::get(int64_tpe, i, true), "inputs[i]");
                Value *input_ptr = builder.CreateLoad(input_ptrptr);
                builder.CreateStore(input_ptr, input_addresses[i]);
                col->column->address = input_addresses[i];
            }

            index_addr = builder.CreateAlloca(Type::getInt64Ty(context), nullptr, "index");
            builder.CreateStore(ConstantInt::get(Type::getInt64Ty(context), 0, true), index_addr);

            builder.CreateBr(cond);
        }

        builder.SetInsertPoint(cond);
        {
            LoadInst *index = builder.CreateLoad(index_addr, "index");
            LoadInst *size = builder.CreateLoad(argument_addresses[1], "size");
            Value *condition = builder.CreateICmpSLT(index, size, "index < size");
            builder.CreateCondBr(condition, body, end);
        }

        builder.SetInsertPoint(body);
        {
            LoadInst *index = builder.CreateLoad(index_addr, "index");

            Value *computation_result =  PerformOperation(op, index);

            LoadInst *result = builder.CreateLoad(argument_addresses[0], "resultptr");
            Value *resultptr = builder.CreateGEP(double_tpe, result, index, "result[index]");
            builder.CreateStore(computation_result, resultptr, "result[index] = computation_result;");

            builder.CreateBr(inc);
        }

        builder.SetInsertPoint(inc);
        {
            LoadInst *index = builder.CreateLoad(index_addr, "index");
            Value *incremented_index = builder.CreateAdd(index, ConstantInt::get(int64_tpe, 1, true), "index++");
            builder.CreateStore(incremented_index, index_addr);

            builder.CreateBr(cond);
        }

        builder.SetInsertPoint(end);
        {
            builder.CreateRetVoid();
        }
        verifyFunction(*function);
        fpm->run(*function);

        if (print_llvm) {
            module->dump();
        }
        jit->addModule(std::move(module));
        toc = clock();
        compile += (double) (toc - tic)  / CLOCKS_PER_SEC;

        tic = clock();
        auto expression = jit->findSymbol("TestFunction");
        assert(expression);

        functionptr fp = (functionptr) (expression.getAddress());

        columns = collection->columns;
        size_t elcount = (size_t) columns->column->size;
        double *result = (double*) malloc(sizeof(double) * elcount);
        void **inputs = (void**) malloc(sizeof(void*) * column_count);
        for(size_t i = 0; i < column_count; i++) {
            inputs[i] = columns->column->data;
            columns = columns->next;
        }
        fp(result, elcount, inputs);

        free(inputs);

        Column *column = (Column*) malloc(sizeof(Column));
        column->name = strdup("Result");
        column->type = TYPE_dbl;
        column->elsize = sizeof(double);
        column->base_oid = 0;
        column->size = elcount;
        column->data = (void*) result;
        column->next = NULL;
        column->address = NULL;

        if (current_column == NULL) {
            table->columns = column;
        } else {
            current_column->next = column;
        }
        current_column = column;

        toc = clock();

        execute += (double) (toc - tic) / CLOCKS_PER_SEC;

        ResetJIT();
    }
    printf("Compile: %f seconds\n", (double)(compile));
    printf("Runtime: %f seconds\n", (double)(execute));
    return table;
}

static void 
Cleanup(void) {
    module = NULL;
}