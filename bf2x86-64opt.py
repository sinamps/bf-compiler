import sys


def clean_code(code):
  return ''.join(filter(lambda x: x in ['.', ',', '[', ']', '<', '>', '+', '-'], code))


def is_innermost(code, loop_start, loop_end):
    loop_body = code[loop_start + 1:loop_end]
    return '[' not in loop_body


def is_simple(code, loop_start, loop_end):
    loop_body = code[loop_start + 1:loop_end]
    in_body_pointer = 0
    start_cell_change = 0

    for command in loop_body:
        if command == '[' or command == ']':
            raise RuntimeError("You have called is_simple on a loop that is not innermost!")
        elif command == '>':
            in_body_pointer += 1
        elif command == '<':
            in_body_pointer -= 1
        elif command == '+':
            if in_body_pointer == 0:
                start_cell_change += 1
        elif command == '-':
            if in_body_pointer == 0:
                start_cell_change -= 1
        elif command == '.' or command == ',':
            return False  # If there's I/O, this is not a simple loop
        else: # cannot support outer simple loop if inner becomes G; reason=cannot know number of iters G would execute to know its effect
            return False

    return in_body_pointer == 0 and (start_cell_change == 1 or start_cell_change == -1)


def is_optimizable_scan_loop(bf_code, loop_start, loop_end):
    """
    Check if the loop contains only '>' or '<', meaning it is a scanning loop.
    Additionally, check if the loop moves the pointer in a consistent direction.
    """
    loop_body = bf_code[loop_start + 1:loop_end]
    pointer = 0

    for char in loop_body:
        if char == '>':
            pointer += 1
        elif char == '<':
            pointer -= 1
        else:
            return False, 0  # Contains instructions other than '>' or '<'

    if pointer == 0:
        return False, 0  # Net pointer movement is zero (not moving)

    # Check if the net pointer movement is a power of two
    abs_pointer = abs(pointer)
    if (abs_pointer & (abs_pointer - 1)) != 0:
        return False, 0  # Not a power of two

    # Return True for scan loop and the net direction of movement
    net_direction = 1 if pointer > 0 else -1
    return True, net_direction


def find_loops(code):
    """Finds all loop boundaries and returns a list of (start, end) tuples."""
    loop_stack = []
    loops = []

    for i, char in enumerate(code):
        if char == '[':
            loop_stack.append(i)
        elif char == ']':
            if loop_stack:
                start = loop_stack.pop()
                loops.append((start, i))

    return loops


def optimize_scan_loops(bf_code):
    """
    Optimizes scan loops in Brainfuck code by replacing them with a vector operations.
    """
    optimized_code = bf_code
    loops = find_loops(optimized_code)
    while True:
        optimized_any = False
        for loop_start, loop_end in loops:
            is_optimizable, dir = is_optimizable_scan_loop(optimized_code, loop_start, loop_end)
            if is_optimizable:
                optimized_any = True
                if dir == 1:
                    optimized_code = optimized_code[:loop_start] + 'R' + optimized_code[loop_end + 1:]
                else: # dir == -1 (has to)
                    optimized_code = optimized_code[:loop_start] + 'L' + optimized_code[loop_end + 1:]
                break
        
        if not optimized_any:
            break

        loops = find_loops(optimized_code)

    return optimized_code


def optimize_simple_loops(bf_code):
    """
    Optimizes simple loops in Brainfuck code by replacing them with a simplified
    representation and tracking pointer effects.
    """
    optimized_code = bf_code
    loops = find_loops(optimized_code)
    pointer_effects_map = {}

    while True:
        optimized_any = False
        for loop_start, loop_end in loops:
            # First check if the loop is innermost
            if is_innermost(optimized_code, loop_start, loop_end):
                # Then check if the loop is simple
                if is_simple(optimized_code, loop_start, loop_end):
                    optimized_any = True
                    # Optimize the simple loop and retrieve pointer effects
                    optimized_code, pointer_effects = optimize_loop(optimized_code, loop_start, loop_end)
                    # Track the pointer effects for this loop
                    pointer_effects_map[loop_start] = pointer_effects
                    # print(f"DEBUG: adding {pointer_effects} to {loop_start}")
                    break  # Start a new pass after optimization

        if not optimized_any:
            break

        # After optimization, find loops again in the new code
        loops = find_loops(optimized_code)

    return optimized_code, pointer_effects_map


def optimize_loop(bf_code, loop_start, loop_end):
    """
    Optimizes a simple loop by marking it for later assembly generation.
    The optimized loop is replaced with a placeholder for easier handling in assembly.
    """
    loop_body = bf_code[loop_start + 1:loop_end]

    # Track pointer position and effects on memory cells
    pointer_position = 0
    pointer_effects = {}

    # if 'G' in loop_body:
    #     pos_g = bf_code.index('G')
    #     pointer_effects = all_pointer_effects_map.pop(pos_g)

    for command in loop_body:
        if command == '>':
            pointer_position += 1
        elif command == '<':
            pointer_position -= 1
        elif command == '+':
            pointer_effects[pointer_position] = pointer_effects.get(pointer_position, 0) + 1
        elif command == '-':
            pointer_effects[pointer_position] = pointer_effects.get(pointer_position, 0) - 1

    # Replace the loop in the code with a placeholder for general simple loop optimization
    return bf_code[:loop_start] + 'G' + bf_code[loop_end + 1:], pointer_effects


def bf_to_x86_64(src_bf_code, opt_sl, opt_scan):
    tape_size = 50000  # Size of the Brainfuck tape
    bf_code = clean_code(src_bf_code)
    # Step 1: Optimize the Brainfuck code before generating assembly
    if opt_scan:
        bf_code = optimize_scan_loops(bf_code)
        # print(f"Optimized Brainfuck code after scan loop optimization: \n{bf_code}\n")
    if opt_sl:
        bf_code, pointer_effects_map = optimize_simple_loops(bf_code)

    # Step 2: Generate the x86-64 assembly
    assembly_code = """
    .global bf_main
    .section .text
bf_main:
    # The tape pointer is passed in %rdi
    mov %rdi, %r15            # Save the tape pointer in a register (using %r15)
    lea 50000(%r15), %r14     # Store the end of the tape in %r14 (tape_end)
    mov %r15, %r13            # Store the start of the tape in %r13 (tape_start)
main_loop:
"""
    label_id = 0
    loop_stack = []
    loop_id = 0

    for i, char in enumerate(bf_code):
        if char == '>':
            assembly_code += "    inc %r15\n"  # Increment data pointer
        elif char == '<':
            assembly_code += "    dec %r15\n"  # Decrement data pointer
        elif char == '+':
            assembly_code += "    incb (%r15)\n"  # Increment the byte at the data pointer
        elif char == '-':
            assembly_code += "    decb (%r15)\n"  # Decrement the byte at the data pointer
        elif char == '.':
            assembly_code += "    movzbl (%r15), %edi\n"  # Move the byte at the data pointer to %edi
            assembly_code += "    call putchar\n"         # Call putchar
        elif char == ',':
            assembly_code += "    call getchar\n"         # Call getchar
            assembly_code += "    movb %al, (%r15)\n"     # Store getchar result
        elif char == '[':
            loop_start = f"loop_start_{label_id}"
            loop_end = f"loop_end_{label_id}"
            label_id += 1
            loop_stack.append((loop_start, loop_end))

            assembly_code += f"{loop_start}:\n"
            assembly_code += "    movzbl (%r15), %eax\n"
            assembly_code += f"    test %eax, %eax\n"
            assembly_code += f"    je {loop_end}\n"
        elif char == ']':
            loop_start, loop_end = loop_stack.pop()
            assembly_code += f"    jmp {loop_start}\n"
            assembly_code += f"{loop_end}:\n"
        elif char == 'G':
            # Generate assembly for simple loops based on pointer_effects_map
            if i in pointer_effects_map:
                pointer_effects = pointer_effects_map[i]
                # print(f"DEBUG: Applying simple loop optimization at index {i} with effects: {pointer_effects}")

                # Load the value of p[0] into %eax (zero extend)
                assembly_code += "    movzbl (%r15), %eax\n"  # Load p[0] into %eax
                
                # Apply each effect relative to the current tape pointer (%r15)
                for pos, effect in pointer_effects.items():
                    if pos == 0:
                        continue  # Skip p[0], as it will be zeroed out at the end
                    if effect == 0:
                        continue  # No change
                    
                    # Load the byte at p[pos] into %ebx
                    assembly_code += f"    movzbl {pos}(%r15), %ebx\n"  # Load p[pos] into %ebx
                    
                    # Apply the change
                    if effect == 1:
                        assembly_code += f"    add %eax, %ebx\n"  # Add p[0] to p[pos]
                    elif effect == -1:
                        assembly_code += f"    sub %eax, %ebx\n"  # Subtract p[0] from p[pos]
                    else:
                        # Apply more significant changes
                        assembly_code += f"    mov ${abs(effect)}, %ecx\n"  # Move the multiplier into %ecx
                        assembly_code += f"    imul %eax, %ecx\n"  # Multiply p[0] by the multiplier
                        if effect > 0:
                            assembly_code += f"    add %ecx, %ebx\n"  # Add the result to p[pos]
                        else:
                            assembly_code += f"    sub %ecx, %ebx\n"  # Subtract the result from p[pos]
                    
                    # Store the result back into p[pos]
                    assembly_code += f"    movb %bl, {pos}(%r15)\n"  # Store %bl back to p[pos]

                # Zero out p[0] (after applying the effects)
                assembly_code += "    movb $0, (%r15)\n"  # Zero out p[0]
        
        elif char == 'R':
            # Optimized rightward memory scan loop using SSE/AVX with alignment check and fallback
            assembly_code += f"""
            test $15, %r15
            jne scan_right_fallback_{loop_id}

            scan_right_loop_{loop_id}:
                cmp %r14, %r15            # Boundary check: tape_end
                jge end_program            # If pointer is at the end, exit
                movdqa (%r15), %xmm0        # Load 16 bytes into xmm0
                pxor   %xmm1, %xmm1
                pcmpeqb %xmm1, %xmm0        # Compare for zeros
                pmovmskb %xmm0, %eax        # Move mask of comparison results to %eax
                test %eax, %eax             # Test if any zero was found
                jne scan_right_found_{loop_id}
                add $16, %r15               # Move 16 bytes to the right
                jmp scan_right_loop_{loop_id}

            scan_right_found_{loop_id}:
                tzcnt %eax, %ecx            # Find the index of the first zero
                add %rcx, %r15              # Move pointer to the position of the first zero
                jmp scan_right_exit_{loop_id}

            scan_right_fallback_{loop_id}:
                cmp %r14, %r15
                jge end_program 
                movzbl (%r15), %eax
                test %eax, %eax
                je scan_right_exit_{loop_id}
                inc %r15
                jmp scan_right_fallback_{loop_id}

            scan_right_exit_{loop_id}:
            """
            loop_id += 1
        elif char == 'L':
            # Optimized leftward memory scan loop using SSE/AVX with alignment check and fallback
            assembly_code += f"""
            test $15, %r15
            jne scan_left_fallback_{loop_id}

            scan_left_loop_{loop_id}:
                cmp %r15, %r13            # Boundary check: tape_end
                jge end_program            # If pointer is at the end, exit
                movdqa -16(%r15), %xmm0     # Load 16 bytes into xmm0 (move left)
                pxor   %xmm1, %xmm1
                pcmpeqb %xmm1, %xmm0        # Compare for zeros
                pmovmskb %xmm0, %eax        # Move mask of comparison results to %eax
                test %eax, %eax             # Test if any zero was found
                jne scan_left_found_{loop_id}
                sub $16, %r15               # Move 16 bytes to the left
                jmp scan_left_loop_{loop_id}

            scan_left_found_{loop_id}:
                tzcnt %eax, %ecx            # Find the index of the first zero
                sub %rcx, %r15              # Fix: Use %rcx (64-bit) instead of %ecx
                jmp scan_left_exit_{loop_id}

            scan_left_fallback_{loop_id}:
                cmp %r15, %r13
                jge end_program 
                movzbl (%r15), %eax
                test %eax, %eax
                je scan_left_exit_{loop_id}
                dec %r15
                jmp scan_left_fallback_{loop_id}

            scan_left_exit_{loop_id}:
            """
            loop_id += 1

    assembly_code += """
end_program:
    ret                      # Return from the Brainfuck main function
"""
    
    return assembly_code


def main():
    opt_sl = False
    opt_scan = False
    if not (len(sys.argv) == 2 or len(sys.argv) == 3):
        print("Usage: python bf_compiler.py <brainfuck_file.bf> [[--o-simpleloops] or [--o-scanloops] or -O]")
        return
    if len(sys.argv) == 3:
        if sys.argv[2] == "--o-simpleloops":
            opt_sl = True
        if sys.argv[2] == "--o-scanloops":
            opt_scan = True
        if sys.argv[2] == "-O":
            opt_sl = True
            opt_scan = True

    bf_file = sys.argv[1]
    with open(bf_file, 'r') as file:
        bf_code = file.read()

    assembly_code = bf_to_x86_64(bf_code, opt_sl, opt_scan)
    # assembly_code = my_test()

    # asm_file = bf_file.replace('.b', '.s')
    asm_file = "program.s"
    with open(asm_file, 'w') as file:
        file.write(assembly_code)

    print(f"Assembly code has been written to {asm_file}")

if __name__ == "__main__":
    main()
