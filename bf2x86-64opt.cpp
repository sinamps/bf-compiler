#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <stack>
#include <map>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <stdexcept>

std::string clean_code(const std::string& code) {
    std::string cleaned_code;
    for (char c : code) {
        if (c == '.' || c == ',' || c == '[' || c == ']' ||
            c == '<' || c == '>' || c == '+' || c == '-') {
            cleaned_code += c;
        }
    }
    return cleaned_code;
}

bool is_innermost(const std::string& code, size_t loop_start, size_t loop_end) {
    std::string loop_body = code.substr(loop_start + 1, loop_end - loop_start - 1);
    return loop_body.find('[') == std::string::npos;
}

bool is_simple(const std::string& code, size_t loop_start, size_t loop_end) {
    std::string loop_body = code.substr(loop_start + 1, loop_end - loop_start - 1);
    int in_body_pointer = 0;
    int start_cell_change = 0;

    for (char command : loop_body) {
        if (command == '[' || command == ']') {
            throw std::runtime_error("You have called is_simple on a loop that is not innermost!");
        } else if (command == '>') {
            in_body_pointer += 1;
        } else if (command == '<') {
            in_body_pointer -= 1;
        } else if (command == '+') {
            if (in_body_pointer == 0) {
                start_cell_change += 1;
            }
        } else if (command == '-') {
            if (in_body_pointer == 0) {
                start_cell_change -= 1;
            }
        } else if (command == '.' || command == ',') {
            return false;  // If there's I/O, this is not a simple loop
        } else {
            return false;
        }
    }
    return in_body_pointer == 0 && (start_cell_change == 1 || start_cell_change == -1);
}

std::pair<bool, int> is_optimizable_scan_loop(const std::string& bf_code, size_t loop_start, size_t loop_end) {
    std::string loop_body = bf_code.substr(loop_start + 1, loop_end - loop_start - 1);
    int pointer = 0;

    for (char c : loop_body) {
        if (c == '>') {
            pointer += 1;
        } else if (c == '<') {
            pointer -= 1;
        } else {
            return std::make_pair(false, 0); // Contains instructions other than '>' or '<'
        }
    }

    if (pointer == 0) {
        return std::make_pair(false, 0); // Net pointer movement is zero
    }

    int abs_pointer = std::abs(pointer);
    if ((abs_pointer & (abs_pointer - 1)) != 0) {
        return std::make_pair(false, 0); // Not a power of two
    }

    int net_direction = (pointer > 0) ? 1 : -1;
    return std::make_pair(true, net_direction);
}

std::vector<std::pair<size_t, size_t>> find_loops(const std::string& code) {
    std::stack<size_t> loop_stack;
    std::vector<std::pair<size_t, size_t>> loops;

    for (size_t i = 0; i < code.size(); ++i) {
        char c = code[i];
        if (c == '[') {
            loop_stack.push(i);
        } else if (c == ']') {
            if (!loop_stack.empty()) {
                size_t start = loop_stack.top();
                loop_stack.pop();
                loops.push_back(std::make_pair(start, i));
            }
        }
    }

    return loops;
}

std::string optimize_scan_loops(std::string bf_code) {
    std::string optimized_code = bf_code;
    auto loops = find_loops(optimized_code);
    while (true) {
        bool optimized_any = false;
        for (const auto& loop : loops) {
            size_t loop_start = loop.first;
            size_t loop_end = loop.second;
            auto result = is_optimizable_scan_loop(optimized_code, loop_start, loop_end);
            bool is_optimizable = result.first;
            int dir = result.second;
            if (is_optimizable) {
                optimized_any = true;
                if (dir == 1) {
                    optimized_code = optimized_code.substr(0, loop_start) + 'R' + optimized_code.substr(loop_end + 1);
                } else {
                    optimized_code = optimized_code.substr(0, loop_start) + 'L' + optimized_code.substr(loop_end + 1);
                }
                break;
            }
        }

        if (!optimized_any) {
            break;
        }

        loops = find_loops(optimized_code);
    }

    return optimized_code;
}

std::string optimize_loop(const std::string& bf_code, size_t loop_start, size_t loop_end, std::map<int, int>& pointer_effects) {
    std::string loop_body = bf_code.substr(loop_start + 1, loop_end - loop_start - 1);

    int pointer_position = 0;

    for (char command : loop_body) {
        if (command == '>') {
            pointer_position += 1;
        } else if (command == '<') {
            pointer_position -= 1;
        } else if (command == '+') {
            pointer_effects[pointer_position] += 1;
        } else if (command == '-') {
            pointer_effects[pointer_position] -= 1;
        }
    }

    std::string new_code = bf_code.substr(0, loop_start) + 'G' + bf_code.substr(loop_end + 1);
    return new_code;
}

std::string optimize_simple_loops(std::string bf_code, std::map<size_t, std::map<int, int>>& pointer_effects_map) {
    std::string optimized_code = bf_code;
    auto loops = find_loops(optimized_code);

    while (true) {
        bool optimized_any = false;
        for (const auto& loop : loops) {
            size_t loop_start = loop.first;
            size_t loop_end = loop.second;
            if (is_innermost(optimized_code, loop_start, loop_end)) {
                if (is_simple(optimized_code, loop_start, loop_end)) {
                    optimized_any = true;
                    std::map<int, int> pointer_effects;
                    optimized_code = optimize_loop(optimized_code, loop_start, loop_end, pointer_effects);
                    pointer_effects_map[loop_start] = pointer_effects;
                    break;  // Start a new pass after optimization
                }
            }
        }

        if (!optimized_any) {
            break;
        }

        loops = find_loops(optimized_code);
    }

    return optimized_code;
}

std::string bf_to_x86_64(const std::string& src_bf_code, bool opt_sl, bool opt_scan) {
    size_t tape_size = 50000; // Size of the Brainfuck tape
    std::string bf_code = clean_code(src_bf_code);

    if (opt_scan) {
        bf_code = optimize_scan_loops(bf_code);
    }

    std::map<size_t, std::map<int, int>> pointer_effects_map;
    if (opt_sl) {
        bf_code = optimize_simple_loops(bf_code, pointer_effects_map);
    }

    std::string assembly_code = R"(    .global bf_main
    .section .text
bf_main:
    # The tape pointer is passed in %rdi
    mov %rdi, %r15            # Save the tape pointer in a register (using %r15)
    lea 50000(%r15), %r14     # Store the end of the tape in %r14 (tape_end)
    mov %r15, %r13            # Store the start of the tape in %r13 (tape_start)
main_loop:
)";
    int label_id = 0;
    std::stack<std::pair<std::string, std::string>> loop_stack;
    int loop_id = 0;

    for (size_t i = 0; i < bf_code.size(); ++i) {
        char c = bf_code[i];
        switch (c) {
            case '>':
                assembly_code += "    inc %r15\n"; // Increment data pointer
                break;
            case '<':
                assembly_code += "    dec %r15\n"; // Decrement data pointer
                break;
            case '+':
                assembly_code += "    incb (%r15)\n"; // Increment the byte at the data pointer
                break;
            case '-':
                assembly_code += "    decb (%r15)\n"; // Decrement the byte at the data pointer
                break;
            case '.':
                assembly_code += "    movzbl (%r15), %edi\n"; // Move the byte at the data pointer to %edi
                assembly_code += "    call putchar\n";         // Call putchar
                break;
            case ',':
                assembly_code += "    call getchar\n";         // Call getchar
                assembly_code += "    movb %al, (%r15)\n";     // Store getchar result
                break;
            case '[': {
                std::string loop_start = "loop_start_" + std::to_string(label_id);
                std::string loop_end = "loop_end_" + std::to_string(label_id);
                label_id += 1;
                loop_stack.push(std::make_pair(loop_start, loop_end));

                assembly_code += loop_start + ":\n";
                assembly_code += "    movzbl (%r15), %eax\n";
                assembly_code += "    test %eax, %eax\n";
                assembly_code += "    je " + loop_end + "\n";
                break;
            }
            case ']': {
                auto loop_labels = loop_stack.top();
                loop_stack.pop();
                std::string loop_start = loop_labels.first;
                std::string loop_end = loop_labels.second;
                assembly_code += "    jmp " + loop_start + "\n";
                assembly_code += loop_end + ":\n";
                break;
            }
            case 'G': {
                auto it = pointer_effects_map.find(i);
                if (it != pointer_effects_map.end()) {
                    auto& pointer_effects = it->second;
                    assembly_code += "    movzbl (%r15), %eax\n";  // Load p[0] into %eax

                    for (const auto& pe : pointer_effects) {
                        int pos = pe.first;
                        int effect = pe.second;
                        if (pos == 0) {
                            continue;  // Skip p[0], as it will be zeroed out at the end
                        }
                        if (effect == 0) {
                            continue;  // No change
                        }

                        assembly_code += "    movzbl " + std::to_string(pos) + "(%r15), %ebx\n";  // Load p[pos] into %ebx

                        if (effect == 1) {
                            assembly_code += "    add %eax, %ebx\n";  // Add p[0] to p[pos]
                        } else if (effect == -1) {
                            assembly_code += "    sub %eax, %ebx\n";  // Subtract p[0] from p[pos]
                        } else {
                            assembly_code += "    mov $" + std::to_string(std::abs(effect)) + ", %ecx\n";  // Move multiplier into %ecx
                            assembly_code += "    imul %eax, %ecx\n";  // Multiply p[0] by multiplier
                            if (effect > 0) {
                                assembly_code += "    add %ecx, %ebx\n";  // Add result to p[pos]
                            } else {
                                assembly_code += "    sub %ecx, %ebx\n";  // Subtract result from p[pos]
                            }
                        }

                        assembly_code += "    movb %bl, " + std::to_string(pos) + "(%r15)\n";  // Store %bl back to p[pos]
                    }

                    assembly_code += "    movb $0, (%r15)\n";  // Zero out p[0]
                }
                break;
            }
            case 'R': {
                assembly_code +=
                    "    # Optimized rightward memory scan loop\n"
                    "    test $15, %r15\n"
                    "    jne scan_right_fallback_" + std::to_string(loop_id) + "\n\n"
                    "scan_right_loop_" + std::to_string(loop_id) + ":\n"
                    "    cmp %r14, %r15            # Boundary check: tape_end\n"
                    "    jge end_program           # If pointer is at the end, exit\n"
                    "    movdqa (%r15), %xmm0      # Load 16 bytes into xmm0\n"
                    "    pxor   %xmm1, %xmm1\n"
                    "    pcmpeqb %xmm1, %xmm0      # Compare for zeros\n"
                    "    pmovmskb %xmm0, %eax      # Move mask of comparison results to %eax\n"
                    "    test %eax, %eax           # Test if any zero was found\n"
                    "    jne scan_right_found_" + std::to_string(loop_id) + "\n"
                    "    add $16, %r15             # Move 16 bytes to the right\n"
                    "    jmp scan_right_loop_" + std::to_string(loop_id) + "\n\n"
                    "scan_right_found_" + std::to_string(loop_id) + ":\n"
                    "    tzcnt %eax, %ecx          # Find the index of the first zero\n"
                    "    add %rcx, %r15            # Move pointer to the position of the first zero\n"
                    "    jmp scan_right_exit_" + std::to_string(loop_id) + "\n\n"
                    "scan_right_fallback_" + std::to_string(loop_id) + ":\n"
                    "    cmp %r14, %r15\n"
                    "    jge end_program\n"
                    "    movzbl (%r15), %eax\n"
                    "    test %eax, %eax\n"
                    "    je scan_right_exit_" + std::to_string(loop_id) + "\n"
                    "    inc %r15\n"
                    "    jmp scan_right_fallback_" + std::to_string(loop_id) + "\n\n"
                    "scan_right_exit_" + std::to_string(loop_id) + ":\n";
                loop_id += 1;
                break;
            }
            case 'L': {
                assembly_code +=
                    "    # Optimized leftward memory scan loop\n"
                    "    test $15, %r15\n"
                    "    jne scan_left_fallback_" + std::to_string(loop_id) + "\n\n"
                    "scan_left_loop_" + std::to_string(loop_id) + ":\n"
                    "    cmp %r15, %r13            # Boundary check: tape_start\n"
                    "    jge end_program           # If pointer is at the start, exit\n"
                    "    movdqa -16(%r15), %xmm0   # Load 16 bytes into xmm0 (move left)\n"
                    "    pxor   %xmm1, %xmm1\n"
                    "    pcmpeqb %xmm1, %xmm0      # Compare for zeros\n"
                    "    pmovmskb %xmm0, %eax      # Move mask of comparison results to %eax\n"
                    "    test %eax, %eax           # Test if any zero was found\n"
                    "    jne scan_left_found_" + std::to_string(loop_id) + "\n"
                    "    sub $16, %r15             # Move 16 bytes to the left\n"
                    "    jmp scan_left_loop_" + std::to_string(loop_id) + "\n\n"
                    "scan_left_found_" + std::to_string(loop_id) + ":\n"
                    "    tzcnt %eax, %ecx          # Find the index of the first zero\n"
                    "    sub %rcx, %r15            # Move pointer to the position of the first zero\n"
                    "    jmp scan_left_exit_" + std::to_string(loop_id) + "\n\n"
                    "scan_left_fallback_" + std::to_string(loop_id) + ":\n"
                    "    cmp %r15, %r13\n"
                    "    jge end_program\n"
                    "    movzbl (%r15), %eax\n"
                    "    test %eax, %eax\n"
                    "    je scan_left_exit_" + std::to_string(loop_id) + "\n"
                    "    dec %r15\n"
                    "    jmp scan_left_fallback_" + std::to_string(loop_id) + "\n\n"
                    "scan_left_exit_" + std::to_string(loop_id) + ":\n";
                loop_id += 1;
                break;
            }
            default:
                break;
        }
    }

    assembly_code += R"(
end_program:
    ret                      # Return from the Brainfuck main function
)";
    return assembly_code;
}

int main(int argc, char* argv[]) {
    bool opt_sl = false;
    bool opt_scan = false;

    if (!(argc == 2 || argc == 3)) {
        std::cout << "Usage: " << argv[0] << " <brainfuck_file.bf> [[--o-simpleloops] or [--o-scanloops] or -O]\n";
        return 1;
    }

    if (argc == 3) {
        std::string option = argv[2];
        if (option == "--o-simpleloops") {
            opt_sl = true;
        }
        if (option == "--o-scanloops") {
            opt_scan = true;
        }
        if (option == "-O") {
            opt_sl = true;
            opt_scan = true;
        }
    }

    std::string bf_file = argv[1];
    std::ifstream file(bf_file);
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << bf_file << "\n";
        return 1;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string bf_code = buffer.str();

    std::string assembly_code = bf_to_x86_64(bf_code, opt_sl, opt_scan);

    std::string asm_file = "program.s";
    std::ofstream asm_output(asm_file);
    if (!asm_output.is_open()) {
        std::cerr << "Error writing to file: " << asm_file << "\n";
        return 1;
    }
    asm_output << assembly_code;

    std::cout << "Assembly code has been written to " << asm_file << "\n";

    return 0;
}
