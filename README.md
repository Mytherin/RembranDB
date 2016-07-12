# RembranDB
Simple database that is supposed to have an LLVM execution engine. However, the execution engine is not implemented yet! The execution engine can be found in `database.c`. The `ExecuteQuery()` function is responsible for 

# Usage
You can generate tables for RembranDB by running `python gentbl.py`. By default the script will generate the table `demo` with three `DOUBLE` columns (`x`, `y`, `z`).

You can run queries either in interactive mode by launching `rembrandb`, or execute individual queries by running `rembrandb -s '{query}'`.

# Building
Run `make`. Note that `llvm-config` must be in your path for RembranDB to build. It requires LLVM 3.7 or higher (older versions have a different API). Many package managers only have older LLVM versions; you can build the latest version from source by following the instructions [here](http://clang.llvm.org/get_started.html).

Note that if you want to run performance experiments, you should compile LLVM with `-DCMAKE_BUILD_TYPE=Release` (CMake parameter).

