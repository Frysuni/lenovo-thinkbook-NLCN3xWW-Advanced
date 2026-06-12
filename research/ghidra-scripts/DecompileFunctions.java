// Ghidra headless script. Decompile functions at explicit addresses.

import java.io.FileWriter;
import java.io.PrintWriter;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

public class DecompileFunctions extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            println("Usage: DecompileFunctions.java output.txt addr [addr...]");
            return;
        }
        try (PrintWriter out = new PrintWriter(new FileWriter(args[0]))) {
            DecompInterface ifc = new DecompInterface();
            ifc.openProgram(currentProgram);
            out.println("Program: " + currentProgram.getName());
            out.println("Image base: " + currentProgram.getImageBase());
            for (int i = 1; i < args.length; i++) {
                Address addr = toAddr(args[i]);
                Function fn = getFunctionContaining(addr);
                if (fn == null) {
                    fn = getFunctionAt(addr);
                }
                out.println();
                out.println("## requested " + args[i]);
                if (fn == null) {
                    out.println("no function");
                    continue;
                }
                out.println("function " + fn.getName() + " @ " + fn.getEntryPoint());
                DecompileResults res = ifc.decompileFunction(fn, 60, monitor);
                if (res != null && res.decompileCompleted()) {
                    out.println(res.getDecompiledFunction().getC());
                } else {
                    out.println("decompile failed");
                }
            }
            ifc.dispose();
        }
    }
}
