using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;
using RecompOne.Runtime.Cdrom;

namespace ExtractRom;

public static class Program
{
    public static void Main(string[] args)
    {
        var baseDir = AppContext.BaseDirectory;
        // Locate repo root
        var repoRoot = Path.GetFullPath(Path.Combine(baseDir, @"..\..\..\..\"));
        if (!Directory.Exists(Path.Combine(repoRoot, "disc")))
        {
            repoRoot = @"c:\github\SymphonyRecomp";
        }

        var cuePath = Path.Combine(repoRoot, "disc", "Castlevania - Symphony of the Night (USA).cue");
        var outDir = Path.Combine(repoRoot, "ghidra_files");
        if (args.Length > 0 && File.Exists(args[0])) cuePath = args[0];

        Console.WriteLine($"[Extractor] Opening disc: {cuePath}");
        using var fs = DiscFs.Open(cuePath);

        string[] testFiles = ["SLUS_000.67", "MARIA.BIN", "MARIA2.BIN", "RIC2.BIN", "TITLE.BIN", "SELECT.BIN", "F_GAME.BIN", "F_GAME2.BIN", "ST0.BIN", "DRA.BIN"];
        foreach (var tf in testFiles)
        {
            var match = fs.FindFile(tf);
            Console.WriteLine($"  FindFile({tf}) -> {(match != null ? match : "NOT FOUND")}");
        }
        return;

        Directory.CreateDirectory(outDir);

        var configText = File.ReadAllText(Path.Combine(repoRoot, "config", "sotn.json"));
        using var doc = JsonDocument.Parse(configText);
        var overlays = doc.RootElement.GetProperty("overlays");

        var manifest = new List<object>();

        // 1. Extract main executable
        try
        {
            Console.WriteLine("[Extractor] Extracting main executable SLUS_000.67...");
            var exeData = fs.ReadFile("SLUS_000.67");
            var exePath = Path.Combine(outDir, "SLUS_000.67.exe");
            File.WriteAllBytes(exePath, exeData);
            Console.WriteLine($"[Extractor] Wrote {exePath} ({exeData.Length:N0} bytes)");

            manifest.Add(new
            {
                name = "main",
                file = "SLUS_000.67.exe",
                description = "Main PSX Executable (PlayStation Binary)",
                baseAddress = "0x80010000",
                size = exeData.Length
            });
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[Extractor] Error extracting main executable: {ex.Message}");
        }

        // Also extract SYSTEM.CNF
        try
        {
            var cnfData = fs.ReadFile("SYSTEM.CNF");
            File.WriteAllBytes(Path.Combine(outDir, "SYSTEM.CNF"), cnfData);
        }
        catch { }

        // 2. Extract overlays
        int count = 0;
        foreach (var ovl in overlays.EnumerateArray())
        {
            var name = ovl.GetProperty("name").GetString()!;
            var file = ovl.GetProperty("file").GetString()!;
            var baseAddr = ovl.GetProperty("base").GetString()!;

            try
            {
                var data = fs.ReadFile(file);
                var destPath = Path.Combine(outDir, file.Replace('/', Path.DirectorySeparatorChar));
                Directory.CreateDirectory(Path.GetDirectoryName(destPath)!);
                File.WriteAllBytes(destPath, data);
                count++;

                manifest.Add(new
                {
                    name,
                    file,
                    baseAddress = baseAddr,
                    size = data.Length
                });
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[Extractor] Warning: could not extract {file}: {ex.Message}");
            }
        }

        Console.WriteLine($"[Extractor] Extracted {count} overlay files successfully!");

        var manifestJson = JsonSerializer.Serialize(manifest, new JsonSerializerOptions { WriteIndented = true });
        File.WriteAllText(Path.Combine(outDir, "ghidra_memory_map.json"), manifestJson);
        Console.WriteLine($"[Extractor] Memory map saved to: {Path.Combine(outDir, "ghidra_memory_map.json")}");
    }
}
