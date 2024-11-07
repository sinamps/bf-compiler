gcc -c bf_wrapper.c -o bf_wrapper.o
as -o program.o program.s
gcc -o bf_program bf_wrapper.o program.o -lc
./bf_program
