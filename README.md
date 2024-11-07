# bf-compiler
A compiler for the brainf**k language.

## BF Compiler in CPP
### How to build:
- on ubuntu on x86-64:
    1. Install llvm version 16:
        - `wget https://apt.llvm.org/llvm.sh`
        - `chmod +x llvm.sh`
        - `sudo ./llvm.sh 16`
        - `sudo apt install llvm-16-dev`
    2. Compile the compiler:
        - `clang++-16 -std=c++11 bf2llvm.cpp 'llvm-config --cxxflags --ldflags --system-libs --libs core' -o bf2llvm`
- On mac with aarm64:
    1. Install llvm version 16:
        - `brew install llvm@16`
        - `export PATH="/opt/homebrew/opt/llvm@16/bin:$PATH"`
        - `export LIBRARY_PATH="/opt/homebrew/opt/llvm@16/lib:$LIBRARY_PATH"`
        - `export CPLUS_INCLUDE_PATH="/opt/homebrew/opt/llvm@16/include:$CPLUS_INCLUDE_PATH"`
    2. Compile the compiler:
        - `clang++ -std=c++11 bf2llvm.cpp -I/opt/homebrew/opt/llvm@16/include -L/opt/homebrew/opt/llvm@16/lib '"/opt/homebrew/opt/llvm@16/bin/llvm-config" --cxxflags --ldflags --system-libs --libs core' -lunwind -o bf2llvm`

### How to compile and run:
1. `clang -c bf_wrapper.c -o bf_wrapper.o`
2. `./bf2llvm <bf_program_src.bf>`
3. `clang -c program.ll -o program.o`
4. `llc -filetype=obj program.ll -o program.o`
5. `clang bf_wrapper.o program.o -o program`
6. `./program`

### Test environment:
- Both Intel 13th gen x86-64 machine and Apple M3 aarch64 chip
- On both Linux Ubuntu 22.04 and MacOS 15.0.1
- LLVM Version 16




## BF Compiler in Java
I rewrote the compiler in Java so that the partial evaluation phase could be fast.

### How to build:
1. Install JDK:
    - Download dmg from [TEMURIN by ADOPTIUM](https://adoptium.net/temurin/releases/?os=any&arch=any)
    - or run `brew install --cask temurin` in terminal with homebrew installed (not tested)
2. Run `javac BFCompiler.java` or `javac BFCompilerPE.java` in terminal
3. Run `java BFCompiler <bf-src-filepath> [-O | --o-simple-loops | --o-scanloops]` in terminal to compile the bf program _without_ partial evaluation
4. Run `java BFCompilerPE <bf-src-filepath> [-O | --o-simple-loops | --o-scanloops]` in terminal to compile the bf program _with_ partial evaluation

### How to compile and run:
1. First compile the C wrapper which calls the BF main after allocating the tape:
    - `gcc -c bf_wrapper.c -o bf_wrapper.o`
2. Compile a BF source file using the java compiler:
    - example: `java BFCompilerPE benches/hello.b`
3. Assemble the BF program to object code:
    - `as -o program.o program.s`
4. Use gcc to link the wrapper and the assembly BF main:
    - `gcc -o bf_program bf_wrapper.o program.o -lc`
5. Execute the program:
    - `./bf_program`

The `measure_winp.py` script can be used to time the execution of benchmarks with and without partially evaluated compilation.

### Test environment:
- JDK 21 from [TEMURIN by ADOPTIUM](https://adoptium.net/temurin/releases/?os=any&arch=any)
- Intel 13th gen x86-64 machine
- On Linux Ubuntu 22.04




## BF Compiler in Python
### How to use optimizations:
- Use `python bf2x86-64opt.py ../benches/hello.b` for no optimizations
- Use `python bf2x86-64opt.py ../benches/hello.b --o-simpleloops` for only simple loops optimizations
- Use `python bf2x86-64opt.py ../benches/hello.b --o-scanloops` for scan loops optimizations
- Use `python bf2x86-64opt.py ../benches/hello.b -O` for both optimizations



### How to compile and run:
1. First compile the C wrapper which calls the BF main after allocating the tape:
    - `gcc -c bf_wrapper.c -o bf_wrapper.o`
2. Compile a BF source file using the python compiler script:
    - example: `python bf2x86-64.py ../benches/hello.b`
3. Assemble the BF program to object code:
    - `as -o program.o program.s`
4. Use gcc to link the wrapper and the assembly BF main:
    - `gcc -o bf_program bf_wrapper.o program.o -lc`
5. Execute the program:
    - `./bf_program`

### Test environment:
- Intel 13th gen x86-64 machine
- On Linux Ubuntu 22.04


