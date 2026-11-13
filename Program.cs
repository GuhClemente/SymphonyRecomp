using RecompOne.Runtime.Memory;
using Recompiled;

AppDomain.CurrentDomain.UnhandledException += (_, e) =>
    Recompiled.CrashLogger.Write(e.ExceptionObject as Exception, "AppDomain.UnhandledException");

if (AutoUpdater.HandleRelaunch(args)) return 0;

var asm = System.Reflection.Assembly.GetExecutingAssembly();

if (Array.Find(asm.GetManifestResourceNames(), n => n.EndsWith(".languages.json", StringComparison.OrdinalIgnoreCase)) is { } languagesRes)
    RecompOne.Runtime.Runtime.AddLanguages(asm, languagesRes);

RecompOne.Runtime.Runtime.SetStartupNotice("startup.beta", "startup.title", "SymphonyRecompBetaAck");

RecompOne.Runtime.Features.FrameInterpolation = false;

Directory.CreateDirectory("translations");

DiscCheck.Register();
Recompiled.Translation.GameLanguage.Register();
WidescreenPatch.Register();
TextureSourcesPatch.Register();
RoomFills.Register();
GameMenu.Register();
CheatMenu.Register();
QualityOfLifeMenu.Register();
TrackerMenu.Register();
RandoMenu.Register();
AutoUpdater.Register();
HelpMenu.Register();

var title = AutoUpdater.CurrentTag is { } tag ? $"SymphonyRecomp {tag}" : "SymphonyRecomp"; //get version too

if (Array.Find(asm.GetManifestResourceNames(), n => n.EndsWith(".SymphonyRecomp.ico", StringComparison.OrdinalIgnoreCase)) is { } iconRes)
{
    using var iconStream = asm.GetManifestResourceStream(iconRes)!;
    using var iconMem = new MemoryStream();
    iconStream.CopyTo(iconMem);
    RecompOne.Runtime.Runtime.SetIcon(iconMem.ToArray());
}

const uint RamSize = 0x00800000; //8

// Disc Auto-Discovery: If not configured or missing, search 'disc/' for *.chd or *.cue
var explicitDisc = args.FirstOrDefault(a => !a.StartsWith('-') && File.Exists(a));
if (!string.IsNullOrEmpty(explicitDisc))
{
    RecompOne.Runtime.Config.Settings.System.DiscPath = Path.GetFullPath(explicitDisc);
    RecompOne.Runtime.Config.Settings.Save();
}
else if (string.IsNullOrWhiteSpace(RecompOne.Runtime.Config.Settings.System.DiscPath) ||
         !File.Exists(RecompOne.Runtime.Config.Settings.System.DiscPath))
{
    var searchDirs = new[] { "disc", "." };
    string? candidate = null;
    foreach (var dir in searchDirs)
    {
        if (!Directory.Exists(dir)) continue;
        var files = Directory.GetFiles(dir, "*.*", SearchOption.TopDirectoryOnly)
            .Where(f => f.EndsWith(".chd", StringComparison.OrdinalIgnoreCase) ||
                        f.EndsWith(".cue", StringComparison.OrdinalIgnoreCase))
            .OrderBy(f => f.EndsWith(".chd", StringComparison.OrdinalIgnoreCase) ? 0 : 1);
        foreach (var file in files)
        {
            var fullPath = Path.GetFullPath(file);
            if (RecompOne.Runtime.Runtime.ValidateDisc(fullPath) == null)
            {
                candidate = fullPath;
                break;
            }
        }
        if (candidate != null) break;
    }

    if (candidate != null)
    {
        RecompOne.Runtime.Config.Settings.System.DiscPath = candidate;
        RecompOne.Runtime.Config.Settings.Save();
        Console.WriteLine($"[Disc] Disco detectado e configurado automaticamente: {candidate}");
    }
}

try
{
    var discArg = explicitDisc ?? (File.Exists(RecompOne.Runtime.Config.Settings.System.DiscPath) ? RecompOne.Runtime.Config.Settings.System.DiscPath : null);
    RecompOne.Runtime.Runtime.Run(() => Entry.Run(new PSMemory(RamSize), discArg, title));
}
catch (Exception ex)
{
    Recompiled.CrashLogger.Write(ex, "Program.Main");
    throw;
}
return 0;
