# bf-compiler
A compiler for the brainf**k language.


## How to use optimizations:
- Use `python bf2x86-64opt.py ../benches/hello.b` for no optimizations
- Use `python bf2x86-64opt.py ../benches/hello.b --o-simpleloops` for only simple loops optimizations
- Use `python bf2x86-64opt.py ../benches/hello.b --o-scanloops` for scan loops optimizations
- Use `python bf2x86-64opt.py ../benches/hello.b -O` for both optimizations



## How to compile and run:
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


