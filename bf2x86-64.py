import sys


def bf_to_x86_64(bf_code):
    assembly_code = """
    .global bf_main
    .section .text
bf_main:
    # The tape pointer is passed in %rdi
    mov %rdi, %r15            # Save the tape pointer in a register (using %r15)

main_loop:
"""
    label_id = 0
    loop_stack = []

    for char in bf_code:
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

    assembly_code += """
end_program:
    ret                      # Return from the Brainfuck main function
"""
    return assembly_code




def main():
    if len(sys.argv) < 2:
        print("Usage: python bf_compiler.py <brainfuck_file.bf>")
        return

    bf_file = sys.argv[1]
    with open(bf_file, 'r') as file:
        bf_code = file.read()

    assembly_code = bf_to_x86_64(bf_code)
    # assembly_code = my_test()

    # asm_file = bf_file.replace('.b', '.s')
    asm_file = "program.s"
    with open(asm_file, 'w') as file:
        file.write(assembly_code)

    print(f"Assembly code has been written to {asm_file}")

if __name__ == "__main__":
    main()
