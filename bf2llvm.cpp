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

// Include LLVM headers
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/IR/LegacyPassManager.h"

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

// Help Functions
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
            // throw std::runtime_error("You have called is_simple on a loop that is not innermost!");
            std::cerr << "You have called is_simple on a loop that is not innermost!" << std::endl;
            std::exit(EXIT_FAILURE);
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
// End of Help Functions

void bf_to_llvm_ir(const std::string& src_bf_code, bool opt_sl, bool opt_scan, llvm::LLVMContext& context, std::unique_ptr<llvm::Module>& module) {
    size_t tape_size = 50000; // Size of the Brainfuck tape
    std::string bf_code = clean_code(src_bf_code);

    if (opt_scan) {
        bf_code = optimize_scan_loops(bf_code);
    }

    std::map<size_t, std::map<int, int>> pointer_effects_map;
    if (opt_sl) {
        bf_code = optimize_simple_loops(bf_code, pointer_effects_map);
    }

    // Create the function bf_main
    llvm::FunctionType* funcType = llvm::FunctionType::get(llvm::Type::getVoidTy(context), {llvm::Type::getInt8PtrTy(context)}, false);
    llvm::Function* bf_main = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "bf_main", module.get());

    // Set the function parameter name
    llvm::Function::arg_iterator args = bf_main->arg_begin();
    llvm::Value* tape_ptr_param = args++;
    tape_ptr_param->setName("tape");

    // Create a basic block and set the insertion point
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", bf_main);
    llvm::IRBuilder<> builder(entry);

    // Allocate local variables
    llvm::Value* tape_ptr_alloc = builder.CreateAlloca(llvm::Type::getInt8PtrTy(context), nullptr, "tape_ptr");
    llvm::Value* tape_start_alloc = builder.CreateAlloca(llvm::Type::getInt8PtrTy(context), nullptr, "tape_start");
    llvm::Value* tape_end_alloc = builder.CreateAlloca(llvm::Type::getInt8PtrTy(context), nullptr, "tape_end");

    // Initialize variables
    builder.CreateStore(tape_ptr_param, tape_ptr_alloc);
    builder.CreateStore(tape_ptr_param, tape_start_alloc);

    // Compute tape_end = tape_ptr + tape_size
    llvm::Value* tape_size_val = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), tape_size);
    llvm::Value* tape_end = builder.CreateGEP(llvm::Type::getInt8Ty(context), tape_ptr_param, tape_size_val, "tape_end");
    builder.CreateStore(tape_end, tape_end_alloc);

    // Stack for loops
    std::stack<std::pair<llvm::BasicBlock*, llvm::BasicBlock*>> loop_stack;
    int label_id = 0;

    for (size_t i = 0; i < bf_code.size(); ++i) {
        char c = bf_code[i];
        switch (c) {
            case '>':
                {
                    llvm::Value* tape_ptr = builder.CreateLoad(llvm::Type::getInt8PtrTy(context), tape_ptr_alloc, "tape_ptr");
                    llvm::Value* new_tape_ptr = builder.CreateGEP(llvm::Type::getInt8Ty(context), tape_ptr, llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 1), "new_tape_ptr");
                    builder.CreateStore(new_tape_ptr, tape_ptr_alloc);
                }
                break;
            case '<':
                {
                    llvm::Value* tape_ptr = builder.CreateLoad(llvm::Type::getInt8PtrTy(context), tape_ptr_alloc, "tape_ptr");
                    llvm::Value* new_tape_ptr = builder.CreateGEP(llvm::Type::getInt8Ty(context), tape_ptr, llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), -1, true), "new_tape_ptr");
                    builder.CreateStore(new_tape_ptr, tape_ptr_alloc);
                }
                break;
            case '+':
                {
                    llvm::Value* tape_ptr = builder.CreateLoad(llvm::Type::getInt8PtrTy(context), tape_ptr_alloc, "tape_ptr");
                    llvm::Value* cell_val = builder.CreateLoad(llvm::Type::getInt8Ty(context), tape_ptr, "cell_val");
                    llvm::Value* incremented = builder.CreateAdd(cell_val, llvm::ConstantInt::get(llvm::Type::getInt8Ty(context), 1), "incremented");
                    builder.CreateStore(incremented, tape_ptr);
                }
                break;
            case '-':
                {
                    llvm::Value* tape_ptr = builder.CreateLoad(llvm::Type::getInt8PtrTy(context), tape_ptr_alloc, "tape_ptr");
                    llvm::Value* cell_val = builder.CreateLoad(llvm::Type::getInt8Ty(context), tape_ptr, "cell_val");
                    llvm::Value* decremented = builder.CreateSub(cell_val, llvm::ConstantInt::get(llvm::Type::getInt8Ty(context), 1), "decremented");
                    builder.CreateStore(decremented, tape_ptr);
                }
                break;
            case '.':
                {
                    llvm::Value* tape_ptr = builder.CreateLoad(llvm::Type::getInt8PtrTy(context), tape_ptr_alloc, "tape_ptr");
                    llvm::Value* cell_val = builder.CreateLoad(llvm::Type::getInt8Ty(context), tape_ptr, "cell_val");
                    // Zero extend to i32
                    llvm::Value* cell_val_i32 = builder.CreateZExt(cell_val, llvm::Type::getInt32Ty(context), "cell_val_i32");
                    // Call putchar
                    llvm::Function* putchar_func = module->getFunction("putchar");
                    if (!putchar_func) {
                        llvm::FunctionType* putchar_type = llvm::FunctionType::get(llvm::Type::getInt32Ty(context), {llvm::Type::getInt32Ty(context)}, false);
                        putchar_func = llvm::Function::Create(putchar_type, llvm::Function::ExternalLinkage, "putchar", module.get());
                    }
                    builder.CreateCall(putchar_func, {cell_val_i32});
                }
                break;
            case ',':
                {
                    llvm::Function* getchar_func = module->getFunction("getchar");
                    if (!getchar_func) {
                        llvm::FunctionType* getchar_type = llvm::FunctionType::get(llvm::Type::getInt32Ty(context), {}, false);
                        getchar_func = llvm::Function::Create(getchar_type, llvm::Function::ExternalLinkage, "getchar", module.get());
                    }
                    llvm::Value* input_val = builder.CreateCall(getchar_func);
                    // Truncate to i8
                    llvm::Value* input_val_i8 = builder.CreateTrunc(input_val, llvm::Type::getInt8Ty(context), "input_val_i8");
                    // Store at *tape_ptr
                    llvm::Value* tape_ptr = builder.CreateLoad(llvm::Type::getInt8PtrTy(context), tape_ptr_alloc, "tape_ptr");
                    builder.CreateStore(input_val_i8, tape_ptr);
                }
                break;
            case '[':
                {
                    // Create loop blocks
                    llvm::BasicBlock* loop_cond = llvm::BasicBlock::Create(context, "loop_cond_" + std::to_string(label_id), bf_main);
                    llvm::BasicBlock* loop_body = llvm::BasicBlock::Create(context, "loop_body_" + std::to_string(label_id), bf_main);
                    llvm::BasicBlock* loop_end = llvm::BasicBlock::Create(context, "loop_end_" + std::to_string(label_id), bf_main);
                    label_id++;

                    // From the current block, branch to loop_cond
                    builder.CreateBr(loop_cond);

                    // Set insertion point to loop_cond
                    builder.SetInsertPoint(loop_cond);

                    // Load the cell value and check the condition
                    llvm::Value* tape_ptr = builder.CreateLoad(llvm::Type::getInt8PtrTy(context), tape_ptr_alloc, "tape_ptr");
                    llvm::Value* cell_val = builder.CreateLoad(llvm::Type::getInt8Ty(context), tape_ptr, "cell_val");
                    llvm::Value* cmp = builder.CreateICmpNE(cell_val, llvm::ConstantInt::get(llvm::Type::getInt8Ty(context), 0), "cmp");
                    builder.CreateCondBr(cmp, loop_body, loop_end);

                    // Push the loop blocks onto the stack
                    loop_stack.push(std::make_pair(loop_cond, loop_end));

                    // Set insertion point to loop_body
                    builder.SetInsertPoint(loop_body);

                    // The loop body will be generated here
                }
                break;
            case ']':
                {
                    // At the end of the loop body, branch back to loop_cond
                    builder.CreateBr(loop_stack.top().first);

                    // Set insertion point to loop_end
                    builder.SetInsertPoint(loop_stack.top().second);

                    // Pop the loop blocks from the stack
                    loop_stack.pop();
                }
                break;
            case 'G':
                {
                    // Handle optimized simple loops
                    auto it = pointer_effects_map.find(i);
                    if (it != pointer_effects_map.end()) {
                        auto& pointer_effects = it->second;

                        // Load p[0]
                        llvm::Value* tape_ptr = builder.CreateLoad(llvm::Type::getInt8PtrTy(context), tape_ptr_alloc, "tape_ptr");
                        llvm::Value* p0_val = builder.CreateLoad(llvm::Type::getInt8Ty(context), tape_ptr, "p0_val");
                        llvm::Value* p0_val_i16 = builder.CreateSExt(p0_val, llvm::Type::getInt16Ty(context), "p0_val_i16");

                        for (const auto& pe : pointer_effects) {
                            int pos = pe.first;
                            int effect = pe.second;
                            if (pos == 0) {
                                continue;  // Skip p[0], as it will be zeroed out at the end
                            }
                            if (effect == 0) {
                                continue;  // No change
                            }

                            // Compute target pointer
                            llvm::Value* pos_val = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), pos);
                            llvm::Value* target_ptr = builder.CreateGEP(llvm::Type::getInt8Ty(context), tape_ptr, pos_val, "target_ptr");

                            // Load p[pos]
                            llvm::Value* target_val = builder.CreateLoad(llvm::Type::getInt8Ty(context), target_ptr, "target_val");
                            llvm::Value* target_val_i16 = builder.CreateSExt(target_val, llvm::Type::getInt16Ty(context), "target_val_i16");

                            // Compute new value
                            llvm::Value* mult = llvm::ConstantInt::get(llvm::Type::getInt16Ty(context), effect);
                            llvm::Value* delta = builder.CreateMul(p0_val_i16, mult, "delta");
                            llvm::Value* new_target_val = builder.CreateAdd(target_val_i16, delta, "new_target_val");

                            // Truncate back to i8
                            llvm::Value* new_target_val_i8 = builder.CreateTrunc(new_target_val, llvm::Type::getInt8Ty(context), "new_target_val_i8");

                            // Store back to p[pos]
                            builder.CreateStore(new_target_val_i8, target_ptr);
                        }

                        // Zero out p[0]
                        llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt8Ty(context), 0);
                        builder.CreateStore(zero, tape_ptr);
                    }
                }
                break;
            case 'R':
                {
                    // Optimized rightward memory scan loop
                    llvm::BasicBlock* scan_loop = llvm::BasicBlock::Create(context, "scan_right_loop_" + std::to_string(label_id), bf_main);
                    llvm::BasicBlock* scan_body = llvm::BasicBlock::Create(context, "scan_right_body_" + std::to_string(label_id), bf_main);
                    llvm::BasicBlock* scan_end = llvm::BasicBlock::Create(context, "scan_right_end_" + std::to_string(label_id), bf_main);
                    label_id++;

                    // Initial branch to scan_loop
                    builder.CreateBr(scan_loop);

                    // Set insertion point to scan_loop
                    builder.SetInsertPoint(scan_loop);

                    // Load tape_ptr
                    llvm::Value* tape_ptr = builder.CreateLoad(
                        llvm::Type::getInt8PtrTy(context),
                        tape_ptr_alloc,
                        "tape_ptr"
                    );

                    // Load cell value and compare
                    llvm::Value* cell_val = builder.CreateLoad(
                        llvm::Type::getInt8Ty(context),
                        tape_ptr,
                        "cell_val"
                    );
                    llvm::Value* cmp = builder.CreateICmpNE(cell_val, builder.getInt8(0), "cmp");

                    // Conditional branch to either scan_body or scan_end
                    builder.CreateCondBr(cmp, scan_body, scan_end);

                    // Set insertion point to scan_body
                    builder.SetInsertPoint(scan_body);

                    // Move tape_ptr right
                    llvm::Value* new_tape_ptr = builder.CreateGEP(
                        llvm::Type::getInt8Ty(context),
                        tape_ptr,
                        builder.getInt64(1),
                        "new_tape_ptr"
                    );
                    builder.CreateStore(new_tape_ptr, tape_ptr_alloc);

                    // Branch back to scan_loop
                    builder.CreateBr(scan_loop);

                    // Set insertion point to scan_end
                    builder.SetInsertPoint(scan_end);
                }
                break;

            case 'L':
                {
                    // Optimized leftward memory scan loop
                    llvm::BasicBlock* scan_loop = llvm::BasicBlock::Create(context, "scan_left_loop_" + std::to_string(label_id), bf_main);
                    llvm::BasicBlock* scan_body = llvm::BasicBlock::Create(context, "scan_left_body_" + std::to_string(label_id), bf_main);
                    llvm::BasicBlock* scan_end = llvm::BasicBlock::Create(context, "scan_left_end_" + std::to_string(label_id), bf_main);
                    label_id++;

                    // Initial branch to scan_loop
                    builder.CreateBr(scan_loop);

                    // Set insertion point to scan_loop
                    builder.SetInsertPoint(scan_loop);

                    // Load tape_ptr
                    llvm::Value* tape_ptr = builder.CreateLoad(
                        llvm::Type::getInt8PtrTy(context),
                        tape_ptr_alloc,
                        "tape_ptr"
                    );

                    // Load cell value and compare
                    llvm::Value* cell_val = builder.CreateLoad(
                        llvm::Type::getInt8Ty(context),
                        tape_ptr,
                        "cell_val"
                    );
                    llvm::Value* cmp = builder.CreateICmpNE(cell_val, builder.getInt8(0), "cmp");

                    // Conditional branch to either scan_body or scan_end
                    builder.CreateCondBr(cmp, scan_body, scan_end);

                    // Set insertion point to scan_body
                    builder.SetInsertPoint(scan_body);

                    // Move tape_ptr left
                    llvm::Value* new_tape_ptr = builder.CreateGEP(
                        llvm::Type::getInt8Ty(context),
                        tape_ptr,
                        builder.getInt64(-1),
                        "new_tape_ptr"
                    );
                    builder.CreateStore(new_tape_ptr, tape_ptr_alloc);

                    // Branch back to scan_loop
                    builder.CreateBr(scan_loop);

                    // Set insertion point to scan_end
                    builder.SetInsertPoint(scan_end);
                }
                break;

            default:
                break;
        }
    }

    // After processing all commands, insert a ret void if necessary
    if (!builder.GetInsertBlock()->getTerminator()) {
        builder.CreateRetVoid();
    }
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

    // Initialize LLVM context and module
    llvm::LLVMContext context;
    std::unique_ptr<llvm::Module> module = std::make_unique<llvm::Module>("bf_module", context);

    // Generate LLVM IR code
    bf_to_llvm_ir(bf_code, opt_sl, opt_scan, context, module);

    // Verify the module
    if (llvm::verifyModule(*module, &llvm::errs())) {
        std::cerr << "Error generating LLVM IR code\n";
        return 1;
    }

    // Output the LLVM IR code to a file
    std::error_code EC;
    llvm::raw_fd_ostream output("program.ll", EC, llvm::sys::fs::OF_None);
    module->print(output, nullptr);

    std::cout << "LLVM IR code has been written to program.ll\n";

    return 0;
}
