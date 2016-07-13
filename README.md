# RembranDB
Simple database that is supposed to have an LLVM execution engine. However, the execution engine is not implemented yet! The execution engine can be found in `database.c`. The `ExecuteQuery()` function is responsible for executing queries. It takes a Query object as input, and should produce a result table.

# Usage
You can generate tables for RembranDB by running `python gentbl.py`. By default the script will generate the table `demo` with three `DOUBLE` columns (`x`, `y`, `z`).

* You can run queries either in interactive mode by launching `rembrandb`
* You can execute individual queries by running `rembrandb -s [query]`

# Building
Run `make`. Note that `llvm-config` must be in your path for RembranDB to build. It requires LLVM 3.8 or higher (older versions have a different API). Many package managers only have older LLVM versions; you can build the latest version from source by following the instructions [here](http://clang.llvm.org/get_started.html). 

Note that if you want to run performance experiments, you should compile LLVM with `-DCMAKE_BUILD_TYPE=Release` (CMake parameter).

LLVM is already installed on a few scilens machines in `/scratch/llvm` (`stones04`, `stones05`, `stones06`), feel free to use those installations. Run `export PATH=/scratch/llvm/bin:$PATH` to add `llvm-config` to your path and you should be able to compile RembranDB. Alternatively, there is a shell script [here](https://gist.github.com/Mytherin/3b6ef566dee90bb27a815a860bd1a03f) for building LLVM from source that should work on all the cluster machines. 

# Tips
* You can compile C code to LLVM IR using `clang`: `clang file.c -S -emit-llvm -o -`
* When generating LLVM IR using the C API, you can use the `LLVMDumpModule(module)` function to write the generated IR to the console.
