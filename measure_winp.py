import os
import subprocess
import time
import statistics

# List of .b files that do not require input
files_no_input = [
    "bench.b", "bottles.b", "deadcodetest.b", "hanoi.b", "hello.b", 
    "long.b", "loopremove.b", "mandel.b", "serptri.b", "twinkle.b"
]

# List of .b files that require input, along with their input file names
files_with_input = [
    ("Sudoku.bf", "inpSu.txt"),
    ("simple_w_input_1.bf", "inp1.txt"),
    ("simple_w_input_2.bf", "inp2.txt")
]

# Path to your benchmarks folder
benchmark_folder = "benches"
winp_bench_folder = "benches_winp"

# Number of runs to average
num_runs = 10

def compile_and_link(compiler, source_file):
    # Compile the source file using the specified compiler
    compiler_command = ["java", "-Xmx64G", compiler, source_file]
    
    # Run the compiler command
    subprocess.run(compiler_command, check=True)

    # Compile the wrapper and assemble the output
    subprocess.run(["gcc", "-c", "bf_wrapper.c", "-o", "bf_wrapper.o"], check=True)
    subprocess.run(["as", "-o", "program.o", "program.s"], check=True)
    subprocess.run(["gcc", "-o", "bf_program", "bf_wrapper.o", "program.o", "-lc"], check=True)

def get_average_user_time(input_file=None):
    user_times = []
    
    for _ in range(num_runs):
        # Open the input file before starting the timer
        infile = open(input_file, 'r') if input_file else subprocess.DEVNULL

        try:
            start_time = time.perf_counter_ns()
            # Run the bf_program and wait for it to complete
            subprocess.run(["./bf_program"], stdin=infile, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
            end_time = time.perf_counter_ns()
        finally:
            if infile is not subprocess.DEVNULL:
                infile.close()

        # Calculate elapsed time in nanoseconds
        elapsed_time = end_time - start_time
        user_times.append(elapsed_time)
    
    return statistics.mean(user_times)

def process_files():
    # Process files that do not require input
    for file in files_no_input:
        source_path = os.path.join(benchmark_folder, file)
        print("=======================================")
        print(f"Processing file: {file}")
        print("=======================================")

        # Compile and run using BFCompiler
        print("\n--- Running with BFCompiler ---")
        compile_and_link("BFCompiler", source_path)
        avg_time_bf = get_average_user_time()
        print(f"Average execution time (BFCompiler): {avg_time_bf:.6f} ns")

        # Compile and run using BFCompilerPE
        print("\n--- Running with BFCompilerPE ---")
        compile_and_link("BFCompilerPE", source_path)
        avg_time_bf_pe = get_average_user_time()
        print(f"Average execution time (BFCompilerPE): {avg_time_bf_pe:.6f} ns")

        print("\nFinished processing", file)
        print("=======================================")

    # Process files that require input
    for file, input_file in files_with_input:
        source_path = os.path.join(winp_bench_folder, file)
        input_path = os.path.join(winp_bench_folder, input_file)
        print("=======================================")
        print(f"Processing file: {file} with input {input_file}")
        print("=======================================")

        # Compile and run using BFCompiler
        print("\n--- Running with BFCompiler ---")
        compile_and_link("BFCompiler", source_path)
        avg_time_bf = get_average_user_time(input_file=input_path)
        print(f"Average execution time (BFCompiler): {avg_time_bf:.6f} ns")

        # Compile and run using BFCompilerPE
        print("\n--- Running with BFCompilerPE ---")
        compile_and_link("BFCompilerPE", source_path)
        avg_time_bf_pe = get_average_user_time(input_file=input_path)
        print(f"Average execution time (BFCompilerPE): {avg_time_bf_pe:.6f} ns")

        print("\nFinished processing", file)
        print("=======================================")

if __name__ == "__main__":
    process_files()
    print("All files processed.")
