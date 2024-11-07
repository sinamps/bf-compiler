clang-16 -c bf_wrapper.c -o bf_wrapper.o
clang-16 -c program.ll -o program.o
llc-16 -filetype=obj program.ll -o program.o
clang-16 bf_wrapper.o program.o -o program
./program
