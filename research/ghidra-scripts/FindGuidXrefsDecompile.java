// Ghidra headless script. Searches key BIOS GUID byte patterns and decompiles
// functions that reference them.

import java.io.FileWriter;
import java.io.PrintWriter;
import java.util.HashSet;
import java.util.Set;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;
import ghidra.util.task.TaskMonitor;

public class FindGuidXrefsDecompile extends GhidraScript {
    private static class Pattern {
        String name;
        byte[] bytes;
        Pattern(String name, String hex) {
            this.name = name;
            this.bytes = fromHex(hex);
        }
    }

    private static byte[] fromHex(String hex) {
        hex = hex.replaceAll("[^0-9A-Fa-f]", "");
        byte[] out = new byte[hex.length() / 2];
        for (int i = 0; i < out.length; i++) {
            out[i] = (byte) Integer.parseInt(hex.substring(i * 2, i * 2 + 2), 16);
        }
        return out;
    }

    private void decompileOnce(PrintWriter out, DecompInterface ifc, Set<String> seen, Function fn) {
        if (fn == null) {
            out.println("    no containing function");
            return;
        }
        String key = fn.getEntryPoint().toString();
        out.println("    function " + fn.getName() + " @ " + fn.getEntryPoint());
        if (seen.contains(key)) {
            return;
        }
        seen.add(key);
        try {
            DecompileResults res = ifc.decompileFunction(fn, 45, monitor);
            if (res != null && res.decompileCompleted()) {
                out.println("----- decompile " + fn.getName() + " @ " + fn.getEntryPoint() + " -----");
                out.println(res.getDecompiledFunction().getC());
                out.println("----- end decompile -----");
            } else {
                out.println("    decompile failed");
            }
        } catch (Exception e) {
            out.println("    decompile exception: " + e.getMessage());
        }
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        String output = args.length > 0 ? args[0] : "ghidra_xrefs.txt";
        Pattern[] patterns = new Pattern[] {
            new Pattern("EFI_SIMPLE_TEXT_INPUT_PROTOCOL", "c1 77 74 38 c7 69 d2 11 8e 39 00 a0 c9 69 72 3b"),
            new Pattern("EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL", "34 75 9e dd 62 77 98 46 8c 14 f5 85 17 a6 25 aa"),
            new Pattern("EFI_FORM_BROWSER2_PROTOCOL", "60 c3 d4 b9 fb bc 9b 4f 92 98 53 c1 36 98 22 58"),
            new Pattern("ADVANCED_FORMSET_GUID", "9e 76 d4 c6 48 7f 2a 4d 98 e9 87 ad cc f3 5c cc"),
            new Pattern("AMD_CBS_VARSTORE_GUID", "02 75 99 3a 7a 64 82 4c 99 8e 52 ef 94 86 a2 47"),
            new Pattern("AMD_PBS_VARSTORE_GUID", "46 d7 39 a3 78 f6 b3 49 9f c7 54 ce 0f 9d f2 26"),
        };

        try (PrintWriter out = new PrintWriter(new FileWriter(output))) {
            out.println("Program: " + currentProgram.getName());
            out.println("Image base: " + currentProgram.getImageBase());
            out.println();

            Memory mem = currentProgram.getMemory();
            DecompInterface ifc = new DecompInterface();
            ifc.openProgram(currentProgram);
            Set<String> decompiled = new HashSet<>();

            for (Pattern pat : patterns) {
                out.println("## " + pat.name);
                int hits = 0;
                for (MemoryBlock block : mem.getBlocks()) {
                    Address at = block.getStart();
                    while (at != null && at.compareTo(block.getEnd()) <= 0) {
                        Address found = mem.findBytes(at, pat.bytes, null, true, TaskMonitor.DUMMY);
                        if (found == null || !block.contains(found)) {
                            break;
                        }
                        hits++;
                        out.println("  hit " + found + " block=" + block.getName());
                        Reference[] refs = getReferencesTo(found);
                        out.println("  refs=" + refs.length);
                        for (Reference ref : refs) {
                            Address from = ref.getFromAddress();
                            out.println("    ref from " + from + " type=" + ref.getReferenceType());
                            decompileOnce(out, ifc, decompiled, getFunctionContaining(from));
                        }
                        at = found.add(1);
                        if (hits > 32) {
                            out.println("  stopping after 32 hits for this pattern");
                            break;
                        }
                    }
                }
                if (hits == 0) {
                    out.println("  no hits");
                }
                out.println();
            }
            ifc.dispose();
        }
    }
}
