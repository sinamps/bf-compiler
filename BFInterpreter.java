import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Stack;

public class BFInterpreter {
    public static String langCommands = "><+-.,[]";
    public static void main(String[] args) {
        if (args.length == 1){
            System.out.println("---> Interpreter: Running " + args[0] + "\n");
            run(args[0]);
        }
        else {
            System.out.println("Usage: java BFInterpreter <bf src file path>");
        }
    }

    public static void run(String bfSrcFilename) {
        StringBuilder srcCode = new StringBuilder();
        try {
            BufferedReader fileReader = new BufferedReader(new FileReader(bfSrcFilename));
            String line;
            while ((line = fileReader.readLine()) != null) {
                srcCode.append(line);
            }
            fileReader.close();
        } catch (IOException e) {
            System.out.println("Cannot read " + bfSrcFilename);
            e.printStackTrace();
        }
        if (srcCode.length() == 0) {
            throw new RuntimeException("BF source code is empty.");
        }
        StringBuilder cleanedCode = clean_code(srcCode);
        HashMap<Integer, Integer> braceMap = build_brace_map(cleanedCode);
        // System.out.println("len of code: " + cleanedCode.length());
        interpreter_loop(cleanedCode, braceMap);
    }

    public static StringBuilder clean_code(StringBuilder code) {
        StringBuilder cleanedCode = new StringBuilder();
        for (int i = 0; i < code.length(); i++) {
            char c = code.charAt(i);
            if (langCommands.indexOf(c) != -1) {
                cleanedCode.append(c);
            }
        }
        return cleanedCode;
    }

    public static HashMap<Integer, Integer> build_brace_map(StringBuilder code) {
        HashMap<Integer, Integer> map = new HashMap<>();
        Stack<Integer> stack = new Stack<>();
        for (int i = 0; i < code.length(); i++) {
            char curCmd = code.charAt(i);
            if (curCmd == '[') {
                stack.push(i);
            } else if (curCmd == ']') {
                int matchingBegin = stack.pop();
                map.put(matchingBegin, i);
                map.put(i, matchingBegin);
            }
        }
        return map;
    }

    public static void interpreter_loop(StringBuilder code, HashMap<Integer, Integer> braceMap) {
        List<Integer> tape = new ArrayList<>();
        tape.add(0);
        int progCounter = 0;
        int head = 0;
        while (true) {
            char curCmd = code.charAt(progCounter);
            switch (curCmd) {
                case '>':
                    head = head + 1;
                    if (head == tape.size()){
                        tape.add(0);
                    }
                    break;
                case '<':
                    if (head > 0) {
                        head = head - 1;
                    } else {
                        head = 0;
                        System.out.println("Your BF program tried to move left on the tape from index 0! We set the pointer to 0 and move on.");
                    }
                    break;
                case '+':
                    if (tape.get(head) < 255) {
                        tape.set(head, tape.get(head) + 1);
                    } else {
                        tape.set(head, 0);
                    }
                    break;
                case '-':
                    if (tape.get(head) > 0){
                        tape.set(head, tape.get(head) - 1);
                    } else {
                        tape.set(head, 255);
                    }
                    break;
                case '.':
                    System.out.print((char) (int) tape.get(head));
                    break;
                case ',':
                    try {
                        tape.set(head, System.in.read());
                    } catch (IOException e) {
                        throw new RuntimeException("Input value caused: " + e.getMessage());
                    }
                    break;
                case '[':
                    if (tape.get(head) == 0) {
                        progCounter = braceMap.get(progCounter);
                    }
                    break;
                case ']':
                    if (tape.get(head) != 0) {
                        progCounter = braceMap.get(progCounter);
                    }
                    break;
                default:
                    break;
            }
            progCounter = progCounter + 1;
            if (progCounter >= code.length()){
                break;
            }
        }
        System.out.println("\n---> Interpreter: Normal Execution\n");
    }
}