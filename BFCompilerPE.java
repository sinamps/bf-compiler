import java.io.FileWriter;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.*;

public class BFCompilerPE {

    public static void main(String[] args) {
        boolean optSl = false;
        boolean optScan = false;
        if (!(args.length == 1 || args.length == 2)) {
            System.out.println("Usage: java BFPartialEvaluator <brainfuck_file.bf> [[--o-simpleloops] or [--o-scanloops] or -O]");
            return;
        }
        if (args.length == 2) {
            if (args[1].equals("--o-simpleloops")) {
                optSl = true;
            } else if (args[1].equals("--o-scanloops")) {
                optScan = true;
            } else if (args[1].equals("-O")) {
                optSl = true;
                optScan = true;
            }
        }

        String bfFile = args[0];
        String bfCode = "";
        try {
            bfCode = new String(Files.readAllBytes(Paths.get(bfFile)));
        } catch (IOException e) {
            System.err.println("Error reading the Brainfuck file: " + e.getMessage());
            return;
        }

        String assemblyCode = bfToX86_64(bfCode, optSl, optScan);

        String asmFile = "program.s";
        try (FileWriter fileWriter = new FileWriter(asmFile)) {
            fileWriter.write(assemblyCode);
        } catch (IOException e) {
            System.err.println("Error writing the assembly file: " + e.getMessage());
            return;
        }

        System.out.println("Assembly code has been written to " + asmFile);
    }

    // We will introduce a new method to perform partial evaluation
    public static String bfToX86_64(String srcBfCode, boolean optSl, boolean optScan) {
        // int tapeSize = 50000;
        String bfCode = cleanCode(srcBfCode);

        // New: Perform partial evaluation
        PartialEvaluationResult peResult = partialEvaluate(bfCode);

        if (peResult.isFullyEvaluated) {
            // The entire program was evaluated at compile time
            return generateOutputAssembly(peResult.outputs);
        } else {
            // Perform optimizations and generate assembly code
            Map<Integer, Map<Integer, Integer>> pointerEffectsMap = new HashMap<>();

            if (optScan) {
                bfCode = optimizeScanLoops(bfCode);
            }

            if (optSl) {
                OptimizeSimpleLoopsResult result = optimizeSimpleLoops(bfCode);
                bfCode = result.optimizedCode;
                pointerEffectsMap = result.pointerEffectsMap;
            }

            // Generate assembly code, integrating partial evaluation results
            return generateAssemblyCode(bfCode, pointerEffectsMap, peResult);
        }
    }

    // New class to hold the result of partial evaluation
    public static class PartialEvaluationResult {
        public boolean isFullyEvaluated;
        public List<Integer> outputs;
        public String remainingCode;
        public int[] tape;
        public int pointerPosition;

        public PartialEvaluationResult(boolean isFullyEvaluated, List<Integer> outputs, String remainingCode, int[] tape, int pointerPosition) {
            this.isFullyEvaluated = isFullyEvaluated;
            this.outputs = outputs;
            this.remainingCode = remainingCode;
            this.tape = tape;
            this.pointerPosition = pointerPosition;
        }
    }

    // New method to perform partial evaluation
    public static PartialEvaluationResult partialEvaluate(String bfCode) {
        int[] tape = new int[50000];
        int pointer = 0;
        Stack<Integer> loopStack = new Stack<>();
        Map<Integer, Integer> loopStartToEnd = new HashMap<>();
        Map<Integer, Integer> loopEndToStart = new HashMap<>();

        // Precompute loop starts and ends
        Stack<Integer> loopTempStack = new Stack<>();
        for (int i = 0; i < bfCode.length(); i++) {
            char c = bfCode.charAt(i);
            if (c == '[') {
                loopTempStack.push(i);
            } else if (c == ']') {
                if (loopTempStack.isEmpty()) {
                    throw new RuntimeException("Unmatched ']' at position " + i);
                }
                int start = loopTempStack.pop();
                loopStartToEnd.put(start, i);
                loopEndToStart.put(i, start);
            }
        }
        if (!loopTempStack.isEmpty()) {
            throw new RuntimeException("Unmatched '[' at position " + loopTempStack.peek());
        }

        List<Integer> outputs = new ArrayList<>();
        int i = 0;
        while (i < bfCode.length()) {
            char c = bfCode.charAt(i);
            switch (c) {
                case '>':
                    pointer++;
                    i++;
                    break;
                case '<':
                    pointer--;
                    i++;
                    break;
                case '+':
                    tape[pointer] = (tape[pointer] + 1) % 256;
                    i++;
                    break;
                case '-':
                    tape[pointer] = (tape[pointer] - 1 + 256) % 256;
                    i++;
                    break;
                case '.':
                    outputs.add(tape[pointer]);
                    i++;
                    break;
                case ',':
                    // Input required, cannot proceed further
                    String remainingCode = bfCode.substring(i);
                    return new PartialEvaluationResult(false, outputs, remainingCode, tape, pointer);
                case '[':
                    if (tape[pointer] == 0) {
                        // Skip to the matching ']'
                        i = loopStartToEnd.get(i) + 1;
                    } else {
                        int loopEnd = loopStartToEnd.get(i);
                        if (bfCode.substring(i, loopEnd).contains(",")) {
                            // Stop partial eval
                            String remainingCodeFromLoopStart = bfCode.substring(i);
                            return new PartialEvaluationResult(false, outputs, remainingCodeFromLoopStart, tape, pointer);
                        } else {
                            loopStack.push(i);
                            i++;
                        }
                    }
                    break;
                case ']':
                    if (tape[pointer] != 0) {
                        // Jump back to the matching '['
                        i = loopEndToStart.get(i);
                    } else {
                        loopStack.pop();
                        i++;
                    }
                    break;
                default:
                    i++;
                    break;
            }
        }
        // Program fully evaluated
        return new PartialEvaluationResult(true, outputs, "", tape, pointer);
    }

    // New method to generate assembly code for fully evaluated programs
    public static String generateOutputAssembly(List<Integer> outputs) {
        StringBuilder assemblyCode = new StringBuilder();
        assemblyCode.append("""
        .global bf_main
        .section .text
    bf_main:
        """);
        for (int value : outputs) {
            assemblyCode.append("    movb $" + value + ", %dil\n");
            assemblyCode.append("    call putchar\n");
        }
        assemblyCode.append("    ret\n");
        return assemblyCode.toString();
    }

    // Modified method to generate assembly code with partial evaluation results
    public static String generateAssemblyCode(String bfCode, Map<Integer, Map<Integer, Integer>> pointerEffectsMap, PartialEvaluationResult peResult) {
        StringBuilder assemblyCode = new StringBuilder();
        assemblyCode.append("""
        .global bf_main
        .section .text
    bf_main:
        """);

        assemblyCode.append("    push %r13\n");
        assemblyCode.append("    push %r14\n");
        assemblyCode.append("    push %r15\n");

        // Initialize the tape pointer and set precomputed cell values
        assemblyCode.append("    # Initialize tape pointer\n");
        assemblyCode.append("    mov %rdi, %r15\n"); // Tape pointer
        assemblyCode.append("    lea 50000(%r15), %r14\n"); // End of tape
        assemblyCode.append("    mov %r15, %r13\n"); // Start of tape

        // Set the tape to the precomputed values from partial evaluation
        for (int i = 0; i < peResult.tape.length; i++) {
            if (peResult.tape[i] != 0) {
                // assemblyCode.append("    movb $" + peResult.tape[i] + ", " + (i - peResult.pointerPosition) + "(%r15)\n");
                assemblyCode.append("    movb $" + peResult.tape[i] + ", " + (i) + "(%r15)\n");
            }
        }

        // Adjust the pointer to the position after partial evaluation
        if (peResult.pointerPosition != 0) {
            assemblyCode.append("    add $" + peResult.pointerPosition + ", %r15\n");
        }

        // Output any precomputed outputs
        for (int value : peResult.outputs) {
            assemblyCode.append("    movb $" + value + ", %dil\n");
            assemblyCode.append("    call putchar\n");
        }

        
        // Continue generating code for the remaining Brainfuck code
        // Build loop mappings for the remaining code
        String remainingCode = peResult.remainingCode;
        LoopMappings loopMappings = buildLoopMappings(remainingCode);
        Map<Integer, Integer> loopStartToEnd = loopMappings.loopStartToEnd;
        Map<Integer, Integer> loopEndToStart = loopMappings.loopEndToStart;
    
        
        Map<Integer, String[]> loopLabelsMap = new HashMap<>();
        
        int labelId = 0;
        // Stack<String[]> loopStack = new Stack<>();
        int loopId = 0;

        // String remainingCode = peResult.remainingCode;
        for (int i = 0; i < remainingCode.length(); i++) {
            char c = remainingCode.charAt(i);
            switch (c) {
                case '>':
                    assemblyCode.append("    inc %r15\n");
                    break;
                case '<':
                    assemblyCode.append("    dec %r15\n");
                    break;
                case '+':
                    assemblyCode.append("    incb (%r15)\n");
                    break;
                case '-':
                    assemblyCode.append("    decb (%r15)\n");
                    break;
                case '.':
                    assemblyCode.append("    movzbl (%r15), %edi\n");
                    assemblyCode.append("    call putchar\n");
                    break;
                case ',':
                    assemblyCode.append("    call getchar\n");
                    assemblyCode.append("    movb %al, (%r15)\n");
                    break;
                case '[':
                    String loopStart = "loop_start_" + labelId;
                    String loopEnd = "loop_end_" + labelId;
                    labelId++;
                    loopLabelsMap.put(i, new String[]{loopStart, loopEnd});

                    assemblyCode.append(loopStart + ":\n");
                    assemblyCode.append("    movzbl (%r15), %eax\n");
                    assemblyCode.append("    test %eax, %eax\n");
                    assemblyCode.append("    je " + loopEnd + "\n");
                    break;
                case ']':
                    int matchingStart = loopEndToStart.get(i);
                    String[] labels = loopLabelsMap.get(matchingStart);
                    String loopStartLabel = labels[0];
                    String loopEndLabel = labels[1];
                    assemblyCode.append("    jmp " + loopStartLabel + "\n");
                    assemblyCode.append(loopEndLabel + ":\n");
                    break;
                case 'G':
                    if (pointerEffectsMap.containsKey(i)) {
                        Map<Integer, Integer> pointerEffects = pointerEffectsMap.get(i);

                        assemblyCode.append("    movzbl (%r15), %eax\n");

                        for (Map.Entry<Integer, Integer> entry : pointerEffects.entrySet()) {
                            int pos = entry.getKey();
                            int effect = entry.getValue();
                            if (pos == 0) continue;
                            if (effect == 0) continue;

                            assemblyCode.append("    movzbl " + pos + "(%r15), %ebx\n");

                            if (effect == 1) {
                                assemblyCode.append("    add %eax, %ebx\n");
                            } else if (effect == -1) {
                                assemblyCode.append("    sub %eax, %ebx\n");
                            } else {
                                assemblyCode.append("    mov $" + Math.abs(effect) + ", %ecx\n");
                                assemblyCode.append("    imul %eax, %ecx\n");
                                if (effect > 0) {
                                    assemblyCode.append("    add %ecx, %ebx\n");
                                } else {
                                    assemblyCode.append("    sub %ecx, %ebx\n");
                                }
                            }
                            assemblyCode.append("    movb %bl, " + pos + "(%r15)\n");
                        }

                        assemblyCode.append("    movb $0, (%r15)\n");
                    }
                    break;
                case 'R':
                assemblyCode.append(String.format("""
                    test $15, %%r15
                    jne scan_right_fallback_%d

                    scan_right_loop_%d:
                        cmp %%r14, %%r15            # Boundary check: tape_end
                        jge end_program            # If pointer is at the end, exit
                        movdqa (%%r15), %%xmm0        # Load 16 bytes into xmm0
                        pxor   %%xmm1, %%xmm1
                        pcmpeqb %%xmm1, %%xmm0        # Compare for zeros
                        pmovmskb %%xmm0, %%eax        # Move mask of comparison results to %%eax
                        test %%eax, %%eax             # Test if any zero was found
                        jne scan_right_found_%d
                        add $16, %%r15               # Move 16 bytes to the right
                        jmp scan_right_loop_%d

                    scan_right_found_%d:
                        tzcnt %%eax, %%ecx            # Find the index of the first zero
                        add %%rcx, %%r15              # Move pointer to the position of the first zero
                        jmp scan_right_exit_%d

                    scan_right_fallback_%d:
                        cmp %%r14, %%r15
                        jge end_program 
                        movzbl (%%r15), %%eax
                        test %%eax, %%eax
                        je scan_right_exit_%d
                        inc %%r15
                        jmp scan_right_fallback_%d

                    scan_right_exit_%d:
                    """, loopId, loopId, loopId, loopId, loopId, loopId, loopId, loopId, loopId, loopId));
                    loopId++;
                    break;
                case 'L':
                assemblyCode.append(String.format("""
                    test $15, %%r15
                    jne scan_left_fallback_%d

                    scan_left_loop_%d:
                        cmp %%r15, %%r13            # Boundary check: tape_end
                        jge end_program            # If pointer is at the end, exit
                        movdqa -16(%%r15), %%xmm0     # Load 16 bytes into xmm0 (move left)
                        pxor   %%xmm1, %%xmm1
                        pcmpeqb %%xmm1, %%xmm0        # Compare for zeros
                        pmovmskb %%xmm0, %%eax        # Move mask of comparison results to %%eax
                        test %%eax, %%eax             # Test if any zero was found
                        jne scan_left_found_%d
                        sub $16, %%r15               # Move 16 bytes to the left
                        jmp scan_left_loop_%d

                    scan_left_found_%d:
                        tzcnt %%eax, %%ecx            # Find the index of the first zero
                        sub %%rcx, %%r15              # Move pointer to the position of the first zero
                        jmp scan_left_exit_%d

                    scan_left_fallback_%d:
                        cmp %%r15, %%r13
                        jge end_program 
                        movzbl (%%r15), %%eax
                        test %%eax, %%eax
                        je scan_left_exit_%d
                        dec %%r15
                        jmp scan_left_fallback_%d

                    scan_left_exit_%d:
                    """, loopId, loopId, loopId, loopId, loopId, loopId, loopId, loopId, loopId, loopId));
                    loopId++;
                    break;
            }
        }

        assemblyCode.append("""
    end_program:
        pop %r15
        pop %r14
        pop %r13
        ret                      # Return from the Brainfuck main function
    """);

        return assemblyCode.toString();
    }

    // Existing methods like cleanCode, optimizeScanLoops, optimizeSimpleLoops, etc., remain the same
    // ...

    // We can leave the rest of the methods as they are unless further modification is needed

    // For brevity, those methods are not repeated here
    public static String cleanCode(String code) {
        StringBuilder sb = new StringBuilder();
        for (char c : code.toCharArray()) {
            if (c == '.' || c == ',' || c == '[' || c == ']' || c == '<' || c == '>' || c == '+' || c == '-') {
                sb.append(c);
            }
        }
        return sb.toString();
    }

    public static class ScanLoopResult {
        public boolean isOptimizable;
        public int netDirection;

        public ScanLoopResult(boolean isOptimizable, int netDirection) {
            this.isOptimizable = isOptimizable;
            this.netDirection = netDirection;
        }
    }

    public static ScanLoopResult isOptimizableScanLoop(String bfCode, int loopStart, int loopEnd) {
        String loopBody = bfCode.substring(loopStart + 1, loopEnd);
        int pointer = 0;

        for (char c : loopBody.toCharArray()) {
            if (c == '>') {
                pointer++;
            } else if (c == '<') {
                pointer--;
            } else {
                return new ScanLoopResult(false, 0);
            }
        }

        if (pointer == 0) {
            return new ScanLoopResult(false, 0);
        }

        int absPointer = Math.abs(pointer);
        if ((absPointer & (absPointer - 1)) != 0) {
            return new ScanLoopResult(false, 0);
        }

        int netDirection = pointer > 0 ? 1 : -1;
        return new ScanLoopResult(true, netDirection);
    }

    public static String optimizeScanLoops(String bfCode) {
        String optimizedCode = bfCode;
        List<int[]> loops = findLoops(optimizedCode);
        while (true) {
            boolean optimizedAny = false;
            for (int[] loop : loops) {
                int loopStart = loop[0];
                int loopEnd = loop[1];
                ScanLoopResult result = isOptimizableScanLoop(optimizedCode, loopStart, loopEnd);
                if (result.isOptimizable) {
                    optimizedAny = true;
                    if (result.netDirection == 1) {
                        optimizedCode = optimizedCode.substring(0, loopStart) + 'R' + optimizedCode.substring(loopEnd + 1);
                    } else {
                        optimizedCode = optimizedCode.substring(0, loopStart) + 'L' + optimizedCode.substring(loopEnd + 1);
                    }
                    break;
                }
            }
            if (!optimizedAny) {
                break;
            }
            loops = findLoops(optimizedCode);
        }
        return optimizedCode;
    }

    public static class OptimizeSimpleLoopsResult {
        public String optimizedCode;
        public Map<Integer, Map<Integer, Integer>> pointerEffectsMap;

        public OptimizeSimpleLoopsResult(String optimizedCode, Map<Integer, Map<Integer, Integer>> pointerEffectsMap) {
            this.optimizedCode = optimizedCode;
            this.pointerEffectsMap = pointerEffectsMap;
        }
    }

    public static OptimizeSimpleLoopsResult optimizeSimpleLoops(String bfCode) {
        String optimizedCode = bfCode;
        List<int[]> loops = findLoops(optimizedCode);
        Map<Integer, Map<Integer, Integer>> pointerEffectsMap = new HashMap<>();

        while (true) {
            boolean optimizedAny = false;
            for (int[] loop : loops) {
                int loopStart = loop[0];
                int loopEnd = loop[1];
                if (isInnermost(optimizedCode, loopStart, loopEnd)) {
                    if (isSimple(optimizedCode, loopStart, loopEnd)) {
                        optimizedAny = true;
                        OptimizeLoopResult optResult = optimizeLoop(optimizedCode, loopStart, loopEnd);
                        optimizedCode = optResult.optimizedCode;
                        Map<Integer, Integer> pointerEffects = optResult.pointerEffects;
                        pointerEffectsMap.put(loopStart, pointerEffects);
                        break;
                    }
                }
            }
            if (!optimizedAny) {
                break;
            }
            loops = findLoops(optimizedCode);
        }

        return new OptimizeSimpleLoopsResult(optimizedCode, pointerEffectsMap);
    }

    public static class OptimizeLoopResult {
        public String optimizedCode;
        public Map<Integer, Integer> pointerEffects;

        public OptimizeLoopResult(String optimizedCode, Map<Integer, Integer> pointerEffects) {
            this.optimizedCode = optimizedCode;
            this.pointerEffects = pointerEffects;
        }
    }

    public static OptimizeLoopResult optimizeLoop(String bfCode, int loopStart, int loopEnd) {
        String loopBody = bfCode.substring(loopStart + 1, loopEnd);

        int pointerPosition = 0;
        Map<Integer, Integer> pointerEffects = new HashMap<>();

        for (char command : loopBody.toCharArray()) {
            if (command == '>') {
                pointerPosition++;
            } else if (command == '<') {
                pointerPosition--;
            } else if (command == '+') {
                pointerEffects.put(pointerPosition, pointerEffects.getOrDefault(pointerPosition, 0) + 1);
            } else if (command == '-') {
                pointerEffects.put(pointerPosition, pointerEffects.getOrDefault(pointerPosition, 0) - 1);
            }
        }

        String optimizedCode = bfCode.substring(0, loopStart) + 'G' + bfCode.substring(loopEnd + 1);

        return new OptimizeLoopResult(optimizedCode, pointerEffects);
    }

    public static boolean isSimple(String code, int loopStart, int loopEnd) {
        String loopBody = code.substring(loopStart + 1, loopEnd);
        int inBodyPointer = 0;
        int startCellChange = 0;

        for (char command : loopBody.toCharArray()) {
            if (command == '[' || command == ']') {
                throw new RuntimeException("You have called is_simple on a loop that is not innermost!");
            } else if (command == '>') {
                inBodyPointer++;
            } else if (command == '<') {
                inBodyPointer--;
            } else if (command == '+') {
                if (inBodyPointer == 0) {
                    startCellChange++;
                }
            } else if (command == '-') {
                if (inBodyPointer == 0) {
                    startCellChange--;
                }
            } else if (command == '.' || command == ',') {
                return false;
            } else {
                return false;
            }
        }

        return inBodyPointer == 0 && (startCellChange == 1 || startCellChange == -1);
    }

    public static boolean isInnermost(String code, int loopStart, int loopEnd) {
        String loopBody = code.substring(loopStart + 1, loopEnd);
        return !loopBody.contains("[");
    }

    public static List<int[]> findLoops(String code) {
        Stack<Integer> loopStack = new Stack<>();
        List<int[]> loops = new ArrayList<>();

        for (int i = 0; i < code.length(); i++) {
            char c = code.charAt(i);
            if (c == '[') {
                loopStack.push(i);
            } else if (c == ']') {
                if (!loopStack.isEmpty()) {
                    int start = loopStack.pop();
                    loops.add(new int[]{start, i});
                }
            }
        }
        return loops;
    }


    public static class LoopMappings {
        public Map<Integer, Integer> loopStartToEnd;
        public Map<Integer, Integer> loopEndToStart;
    
        public LoopMappings(Map<Integer, Integer> loopStartToEnd, Map<Integer, Integer> loopEndToStart) {
            this.loopStartToEnd = loopStartToEnd;
            this.loopEndToStart = loopEndToStart;
        }
    }

    public static LoopMappings buildLoopMappings(String code) {
        Map<Integer, Integer> loopStartToEnd = new HashMap<>();
        Map<Integer, Integer> loopEndToStart = new HashMap<>();
        Stack<Integer> loopStack = new Stack<>();
    
        for (int idx = 0; idx < code.length(); idx++) {
            char c = code.charAt(idx);
            if (c == '[') {
                loopStack.push(idx);
            } else if (c == ']') {
                if (loopStack.isEmpty()) {
                    throw new RuntimeException("Unmatched ']' at position " + idx);
                }
                int start = loopStack.pop();
                loopStartToEnd.put(start, idx);
                loopEndToStart.put(idx, start);
            }
        }
        if (!loopStack.isEmpty()) {
            throw new RuntimeException("Unmatched '[' at position " + loopStack.peek());
        }
    
        return new LoopMappings(loopStartToEnd, loopEndToStart);
    }
    

}
