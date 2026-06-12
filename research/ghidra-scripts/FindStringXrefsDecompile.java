// Find ASCII/UTF-16LE string occurrences, print references, and decompile referrers.
// Usage:
//   analyzeHeadless ... -scriptPath analysis -postScript FindStringXrefsDecompile.java OUT PATTERN...

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;
import ghidra.util.task.TaskMonitor;
import java.io.FileWriter;
import java.io.PrintWriter;
import java.util.LinkedHashSet;

public class FindStringXrefsDecompile extends GhidraScript {
    private byte[] utf16le(String s) {
        byte[] out = new byte[s.length() * 2];
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            out[i * 2] = (byte)(c & 0xff);
            out[i * 2 + 1] = (byte)((c >> 8) & 0xff);
        }
        return out;
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            println("Usage: OUT PATTERN...");
            return;
        }

        PrintWriter out = new PrintWriter(new FileWriter(args[0]));
        DecompInterface ifc = new DecompInterface();
        ifc.openProgram(currentProgram);
        LinkedHashSet<Function> funcs = new LinkedHashSet<Function>();

        out.printf("Program: %s%n", currentProgram.getName());
        out.printf("Image base: %s%n%n", currentProgram.getImageBase());

        for (int i = 1; i < args.length; i++) {
            String pat = args[i];
            out.printf("## pattern %s%n", pat);
            byte[][] encs = new byte[][] { pat.getBytes("US-ASCII"), utf16le(pat) };
            String[] names = new String[] { "ascii", "utf16le" };
            for (int e = 0; e < encs.length; e++) {
                int hits = 0;
                Memory mem = currentProgram.getMemory();
                for (MemoryBlock block : mem.getBlocks()) {
                    Address at = block.getStart();
                    while (at != null && at.compareTo(block.getEnd()) <= 0) {
                        Address hit = mem.findBytes(at, encs[e], null, true, TaskMonitor.DUMMY);
                        if (hit == null || !block.contains(hit)) {
                            break;
                        }
                        hits++;
                        out.printf("  %s hit %s block=%s%n", names[e], hit, block.getName());
                        Reference[] refs = getReferencesTo(hit);
                        out.printf("    refs-to-exact: %d%n", refs.length);
                        for (Reference ref : refs) {
                            Address from = ref.getFromAddress();
                            Function f = getFunctionContaining(from);
                            out.printf("      %s %s func=%s%n", from, ref.getReferenceType(), f == null ? "<none>" : f.getName());
                            if (f != null) {
                                funcs.add(f);
                            }
                        }
                        at = hit.add(1);
                    }
                }
                out.printf("%s total hits: %d%n", names[e], hits);
            }
            out.println();
        }

        out.printf("%n## decompiled referrer functions: %d%n", funcs.size());
        for (Function f : funcs) {
            out.printf("%n### %s @ %s%n", f.getName(), f.getEntryPoint());
            DecompileResults res = ifc.decompileFunction(f, 60, monitor);
            if (res.decompileCompleted()) {
                out.println(res.getDecompiledFunction().getC());
            } else {
                out.printf("<decompile failed: %s>%n", res.getErrorMessage());
            }
        }
        out.close();
        ifc.dispose();
    }
}
