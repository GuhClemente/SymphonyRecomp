using System;
using System.Collections.Generic;
using System.IO;
using RecompOne.Runtime.Context;
using RecompOne.Runtime;
using RecompOne.Runtime.Host;
using RecompOne.Runtime.Host.Window;
using RecompOne.Runtime.Memory;
using Sotn;

namespace Recompiled.Ultimate;

/// <summary>
/// The automated run behind SOTN_TEST_MARIA=1: every playable character, one after the other, doing
/// its basic actions and every quick-cast special, with a screenshot and a line of state per step.
///
///   SOTN_TEST_MARIA=1 SOTN_TEST_DIR=/some/dir ./mac_run.sh
///
/// Screenshots go to SOTN_TEST_DIR as NN_character_step.bmp; the log gets one "[TEST]" line per
/// screenshot (player step, HP / MP / hearts, and whether the SPU played voice-bank samples since the
/// previous one). The game exits when the script ends.
/// </summary>
public static class CharacterTest
{
    /// <summary>Buttons the pad listener holds down this frame.</summary>
    public static ushort Held;

    private const uint CostumeAddr = Game.StatusAddr + 0x2DCu; // g_Status.D_80097C40
    private const uint ChargeTimerAddr = 0x80072F44u;          // g_Player.unk24
    // sound RAM windows of the two voice banks (MariaLoader)
    private const int Vab1Start = 0x42CC0, Vab2Start = 0x50E50, Vab2End = 0x60A40;

    private static readonly string Dir =
        Environment.GetEnvironmentVariable("SOTN_TEST_DIR") ?? Path.GetTempPath();

    // a step runs at its frame; a gate holds the script's clock until the player is ready (or it times out)
    private static List<(int Frame, Action<CpuContext, IMemory>? Run, Func<IMemory, bool>? Gate, int Timeout)>? _script;
    private static int _gateWait;
    private static int _cursor;
    private static int _frame;
    private static int _shot;
    private static string _who = "";
    private static bool _topUpHp = true;
    private static bool _voice1, _voice2, _other;
    private static readonly Spu.VoiceDebug[] _voices = new Spu.VoiceDebug[24];

    // ---- script building ---------------------------------------------------------------------
    private static int _t;

    private static void Do(Action<CpuContext, IMemory> run) => _script!.Add((_t, run, null, 0));

    private const uint TimeStopAddr = 0x80097420u;         // g_unkGraphicsStruct.unk28
    private const uint PlayerAttackBusyAddr = 0x80072F66u; // g_Player.unk46

    // standing with control, nothing of the previous move still running
    private static bool Idle(IMemory m) =>
        Player.Step == PlayerStep.Standing && Player.HasControl &&
        m.ReadU32(TimeStopAddr) == 0 && m.ReadU16(PlayerAttackBusyAddr) == 0;

    private static void WaitIdle(int timeout = 1500)
    {
        _script!.Add((_t, null, Idle, timeout));
        Wait(6);
    }
    private static void Wait(int frames) => _t += frames;
    private static void Press(Button b) => Do((_, _) => Held |= (ushort)b);
    private static void Release(Button b) => Do((_, _) => Held &= (ushort)~(ushort)b);

    private static void Tap(Button b, int frames = 4)
    {
        Press(b);
        Wait(frames);
        Release(b);
    }

    private static void Shot(string step)
    {
        Do((_, m) => Capture(m, step));
        Wait(5);
    }

    private static void Switch(CharacterSelection who, string name)
    {
        WaitIdle();
        Do((c, m) =>
        {
            _who = name;
            Held = 0;
            Console.WriteLine($"[TEST] ===== {name} =====");
            MariaCharacter.SwitchToCharacter(c, m, who);
        });
        Wait(50);
        Shot("parado");
    }

    private static void Basics(Subweapon subweapon, bool pause)
    {
        Press(Button.Square); Wait(10); Shot("ataque"); Release(Button.Square); Wait(45);
        Press(Button.Cross); Wait(14); Shot("pulo"); Release(Button.Cross); Wait(60);
        Press(Button.Down); Wait(14); Shot("agachado"); Release(Button.Down); Wait(20);
        Press(Button.Right); Wait(24); Shot("andando"); Release(Button.Right); Wait(6);
        Press(Button.Left); Wait(24); Release(Button.Left); Wait(20);

        Do((_, _) => { Player.Hearts = 99; Player.SubWeapon = (int)subweapon; });
        Wait(20);
        Press(Button.Up | Button.Square); Wait(14); Shot("subweapon"); Release(Button.Up | Button.Square); Wait(70);

        // the HP bar following a drop, then the refill
        Do((_, _) => { _topUpHp = false; Player.Hp = Math.Max(1, Player.HpMax / 3); });
        Wait(30);
        Shot("hp_baixo");
        Do((_, _) => { Player.Hp = Player.HpMax; _topUpHp = true; });
        Wait(30);

        if (pause)
        {
            Tap(Button.Start); Wait(12); Shot("pausa_animacao"); Wait(25); Shot("pausa");
            Tap(Button.Start); Wait(45);
        }
    }

    private static void Cast(int index, string step, int settle, int second = 70)
    {
        WaitIdle();
        Do((_, _) =>
        {
            Player.MpMax = Math.Max(Player.MpMax, 200);
            Player.Mp = Player.MpMax;
            Player.Hearts = 99;
            SpellWheel.TriggerQuickCast(index);
        });
        Wait(settle);
        Shot(step);
        if (second > 0)
        {
            Wait(second);
            Shot(step + "_b");
        }
        Wait(20);
    }

    private static void Build()
    {
        _script = [];
        _t = 10;
        Directory.CreateDirectory(Dir);

        if (Environment.GetEnvironmentVariable("SOTN_TEST_TELEPORT") == "1")
        {
            BuildTeleportScript();
            return;
        }

        // SOTN_TEST_START=alucard: the game starts as the untouched Alucard; a capture and his palettes
        // (RAM copy and VRAM row) before any switch, then away and back, to compare
        if (Environment.GetEnvironmentVariable("SOTN_TEST_START") == "alucard")
        {
            Wait(60);
            Do((_, m) => { _who = "alucard_original"; DumpPalettes(m, "original"); DumpPlayer(m, "original"); });
            Shot("parado");
            Do((c, m) => { _who = "maria_saturn"; MariaCharacter.SwitchToCharacter(c, m, CharacterSelection.SaturnMaria); });
            Wait(40); Shot("parado");
            Do((c, m) => { _who = "alucard_volta"; MariaCharacter.SwitchToCharacter(c, m, CharacterSelection.Alucard); });
            Wait(40);
            Do((_, m) => { DumpPalettes(m, "volta"); DumpPlayer(m, "volta"); });
            Shot("parado");
            Press(Button.Circle);
            Wait(14);
            Shot("ataque_espada");
            Release(Button.Circle);
            Wait(30);
            Do((_, _) => { _who = "ultimate"; PlayerSelectScreen.Open(1); });
            Wait(25);
            Shot("player_select_classico");
            Do((_, _) => { PlayerSelectScreen.Open(2); });
            Wait(25);
            Shot("player_select_maria_config");
            Do((_, _) => PlayerSelectScreen.Close());
            Wait(15);
            Do((_, _) => Console.WriteLine("[MariaTest] SEQUÊNCIA COMPLETA"));
            Wait(30);
            Do((_, _) => Environment.Exit(0));
            return;
        }

        if (Environment.GetEnvironmentVariable("SOTN_TEST_QUICK") == "1")
        {
            // only the switches: where each character ends up and whether it stands
            Do((_, _) => _who = "maria_saturn");
            Shot("parado");
            foreach (var (who, name) in new[]
                     {
                         (CharacterSelection.PSPMaria, "maria_psp"),
                         (CharacterSelection.RichterClassic, "richter"),
                         (CharacterSelection.RichterSaturnCoat, "richter_casaco"),
                         (CharacterSelection.RichterBrownCoat, "richter_marrom"),
                         (CharacterSelection.Alucard, "alucard"),
                         (CharacterSelection.SaturnMaria, "maria_saturn_volta"),
                     })
            {
                Do((c, m) => { _who = name; MariaCharacter.SwitchToCharacter(c, m, who); });
                Wait(2); Shot("troca_2q"); Wait(40); Shot("parado");
            }
            Do((_, _) => Console.WriteLine("[MariaTest] SEQUÊNCIA COMPLETA"));
            Wait(30);
            Do((_, _) => Environment.Exit(0));
            return;
        }

        // ---------------- Saturn Maria (already active: the auto-transformation) ----------------
        Do((_, _) => { _who = "maria_saturn"; Console.WriteLine("[TEST] ===== maria_saturn ====="); });
        Shot("parado");
        Basics(Subweapon.Cross, pause: true);
        // triple jump
        Tap(Button.Cross, 10); Wait(6); Tap(Button.Cross, 10); Wait(6); Press(Button.Cross); Wait(8);
        Shot("pulo_triplo"); Release(Button.Cross); Wait(80);
        string[] beasts = ["soco_guardiao", "suzaku", "genbu", "byakko", "seiryuu", "quatro_bestas"];
        for (int i = 0; i < beasts.Length; i++)
            Cast(i, $"especial_{i + 1}_{beasts[i]}", 35);
        WaitIdle();
        // charge: the plate's glow
        Press(Button.Square); Wait(130); Shot("carga_1"); Wait(110); Shot("carga_2"); Wait(110); Shot("carga_3");
        Release(Button.Square); Wait(30); Shot("carga_solta"); Wait(80);

        // ---------------- PSP Maria ----------------
        Switch(CharacterSelection.PSPMaria, "maria_psp");
        Basics(Subweapon.Dagger, pause: true);
        // her two subweapon slots: the cat in front, the cardinal behind, then Circle swaps them
        Do((_, m) => { Player.SubWeapon = (int)Subweapon.Axe; m.WriteU32(CostumeAddr, (uint)Subweapon.Dagger); });
        Wait(20); Shot("icones");
        Tap(Button.Circle); Wait(2); Shot("icones_trocando"); Wait(20); Shot("icones_trocados");
        Press(Button.Up | Button.Square); Wait(14); Shot("subweapon_2"); Release(Button.Up | Button.Square); Wait(70);
        Tap(Button.Triangle, 6); Wait(25); Shot("triangulo"); Wait(150);
        Do((_, _) => Player.SubWeapon = (int)Subweapon.HolyWater);
        Wait(20);
        Press(Button.Up | Button.Square); Wait(14); Shot("subweapon_3"); Release(Button.Up | Button.Square); Wait(90);
        Do((_, _) => Player.SubWeapon = (int)Subweapon.VibhutiHand);
        Wait(20);
        Press(Button.Up | Button.Square); Wait(14); Shot("subweapon_4"); Release(Button.Up | Button.Square); Wait(120);

        // ---------------- Richter, all 3 costumes ----------------
        foreach (var (who, name) in new[]
                 {
                     (CharacterSelection.RichterClassic, "richter"),
                     (CharacterSelection.RichterSaturnCoat, "richter_casaco"),
                     (CharacterSelection.RichterBrownCoat, "richter_marrom"),
                 })
        {
            Switch(who, name);
            Basics(Subweapon.Cross, pause: true);
            string[] crashes = ["hydro_storm", "grand_cross", "thousand_blades", "axe_rain", "burning_bible"];
            for (int i = 0; i < crashes.Length; i++)
                Cast(i, $"crash_{i + 1}_{crashes[i]}", 45);
            Cast(5, "blade_dash", 6, 0);
            Cast(6, "super_jump", 10, 0);
        }

        // ---------------- Alucard ----------------
        Switch(CharacterSelection.Alucard, "alucard");
        Basics(Subweapon.Dagger, pause: false);
        string[] spells = ["summon_spirit", "tetra_spirit", "dark_metamorphosis", "hellfire", "soul_steal"];
        for (int i = 0; i < spells.Length; i++)
            Cast(i, $"magia_{i + 1}_{spells[i]}", 40);

        // ---------------- and back ----------------
        Switch(CharacterSelection.SaturnMaria, "maria_saturn_volta");
        Press(Button.Square); Wait(10); Shot("ataque"); Release(Button.Square); Wait(40);

        Do((_, _) =>
        {
            Console.WriteLine($"[TEST] FIM: {_shot} capturas em {Dir}");
            Console.WriteLine("[MariaTest] SEQUÊNCIA COMPLETA");
        });
        Wait(40);
        Do((_, _) => Environment.Exit(0));
    }

    // ---- teleports -------------------------------------------------------------------------
    private const uint EngineStepAddr = 0x8003C9A4u; // 3 while a room transition runs

    private static bool Arrived(IMemory m, Stage target) =>
        Game.Available && Game.InGame && !Game.IsLoading && !Stages.LoadPending &&
        Stages.Current == target && m.ReadU32(EngineStepAddr) != 3 && Player.HasControl;

    private static void WaitArrival(Stage stage, int timeout = 3000)
    {
        Wait(4);
        _script!.Add((_t, null, m => Arrived(m, stage), timeout));
        Wait(1);
    }

    private static void WaitStanding(int timeout)
    {
        _script!.Add((_t, null, _ => Player.Step == PlayerStep.Standing && Player.VelocityY == 0, timeout));
        Wait(1);
    }

    // the spots of the Fast Travel panel that were measured in game, or the ones of a text file
    // (SOTN_TEST_LOCFILE, one "code,Stage,roomX,roomY,offsetX,offsetY" per line) to try new ones without a build
    private static List<FastTravelPanel.Destination> TeleportSpots()
    {
        var list = new List<FastTravelPanel.Destination>();
        string? file = Environment.GetEnvironmentVariable("SOTN_TEST_LOCFILE");
        if (!string.IsNullOrEmpty(file))
        {
            foreach (var line in File.ReadAllLines(file))
            {
                var p = line.Split(',', StringSplitOptions.TrimEntries);
                if (p.Length < 6 || line.StartsWith('#')) continue;
                list.Add(new FastTravelPanel.Destination(p[0], Enum.Parse<Stage>(p[1]), int.Parse(p[2]), int.Parse(p[3]),
                    false, int.Parse(p[4]), int.Parse(p[5]), true, p[0]));
            }
            return list;
        }
        // the ten key places of the first castle, or every destination of the panel
        bool all = Environment.GetEnvironmentVariable("SOTN_TEST_ALL") == "1";
        foreach (var d in FastTravelPanel.Destinations)
            if (d.Verified && (all || list.Count < 10)) list.Add(d);
        return list;
    }

    // every player palette row: the game's copy in RAM and what is in VRAM, as hashes, plus row 0x100 in full
    private static void DumpPalettes(IMemory m, string tag)
    {
        var gpu = RecompOne.Runtime.Runtime.Gpu;
        var sb = new System.Text.StringBuilder($"[PALETA] {tag}: entPal={m.ReadU16(0x800733D8u + 0x16):X4} animSet={m.ReadU16(0x800733D8u + 0x54):X4}");
        for (int id = 0x100; id < 0x120; id++)
        {
            uint ram = Palette.ClutRamAddr + (uint)(id * 32), hr = 0, hv = 0;
            ushort clut = Palette.ClutOf(id);
            int x = (clut & 0x3F) << 4, y = clut >> 6;
            for (int k = 0; k < 16; k++)
            {
                hr = hr * 31 + m.ReadU16(ram + (uint)(k * 2));
                if (gpu != null) hv = hv * 31 + gpu.Shadow[x + k, y];
            }
            sb.Append($" {id:X3}:{hr:X8}/{hv:X8}");
        }
        Console.WriteLine(sb.ToString());
        if (Environment.GetEnvironmentVariable("SOTN_TEST_RAWPAL") == "1")
            for (int id = 0x100; id < 0x120; id++)
            {
                var row = new System.Text.StringBuilder($"[PALROW] {tag} {id:X3}");
                for (int k = 0; k < 16; k++) row.Append($" {m.ReadU16(Palette.ClutRamAddr + (uint)(id * 32 + k * 2)):X4}");
                Console.WriteLine(row.ToString());
            }
    }

    private static void DumpPlayer(IMemory m, string tag)
    {
        var sb = new System.Text.StringBuilder($"[PLAYER] {tag}:");
        for (uint o = 0; o < 0xBC; o += 4) sb.Append($" {o:X2}={m.ReadU32(0x800733D8u + o):X8}");
        Console.WriteLine(sb.ToString());
    }

    private static void BuildTeleportScript()
    {
        var spots = TeleportSpots();
        if (Environment.GetEnvironmentVariable("SOTN_TEST_PROBE") == "1")
        {
            Wait(30);
            Do((_, m) => { _who = "inicio"; DumpPlayer(m, "inicio"); });
            Shot("antes");
        }
        bool probe = Environment.GetEnvironmentVariable("SOTN_TEST_PROBE") == "1";

        (CharacterSelection Who, string Name)[] characters = probe
            ? [(CharacterSelection.Alucard, "alucard")]
            :
            [
                (CharacterSelection.SaturnMaria, "saturn_maria"),
                (CharacterSelection.PSPMaria, "psp_maria"),
                (CharacterSelection.RichterClassic, "richter_classic"),
                (CharacterSelection.RichterSaturnCoat, "richter_coat"),
                (CharacterSelection.RichterBrownCoat, "richter_brown"),
                (CharacterSelection.Alucard, "alucard"),
            ];

        int only = int.TryParse(Environment.GetEnvironmentVariable("SOTN_TEST_LOCATION"), out int n) ? n : 0;
        Console.WriteLine($"[TELEPORT] {spots.Count} locais" + (only > 0 ? $", só o {only}" : "") + (probe ? " (sondagem)" : ""));

        for (int i = 0; i < spots.Count; i++)
        {
            if (only > 0 && i != only - 1) continue;

            int number = i + 1;
            var spot = spots[i];
            Do((_, _) =>
            {
                _who = spot.Code;
                Held = 0;
                Console.WriteLine($"[TELEPORT] >>> [{number}/{spots.Count}] {spot.Name} ({spot.Stage}) Room=({spot.RoomX},{spot.RoomY}) Offset=({spot.OffsetX},{spot.OffsetY})");
                Player.TeleportTo(spot.Stage, spot.RoomX, spot.RoomY, spot.OffsetX, spot.OffsetY);
            });
            WaitArrival(spot.Stage);

            // the first frame with control: is the player already on the floor?
            Do((_, _) => Console.WriteLine(
                $"[TELEPORT] chegada {spot.Code}: stage={Stages.Current} pos={Player.PosX},{Player.PosY}" +
                $" step=0x{(int)Player.Step:X2} vy={Player.VelocityY}"));
            WaitStanding(probe ? 600 : 120);
            Wait(30);
            Do((_, m) => Console.WriteLine(
                $"[TELEPORT] apoiado {spot.Code}: pos={Player.PosX},{Player.PosY} step=0x{(int)Player.Step:X2}" +
                $" vy={Player.VelocityY} hp={Player.Hp}/{Player.HpMax}" +
                $" sala={m.ReadU32(Stages.RoomXAddr)},{m.ReadU32(Stages.RoomYAddr)}"));
            if (probe) Do((_, m) => DumpPlayer(m, spot.Code));

            Do((c, m) =>
            {
                Player.HpMax = Math.Max(Player.HpMax, 200);
                Player.Hp = Player.HpMax;
                Player.MpMax = Math.Max(Player.MpMax, 200);
                Player.Mp = Player.MpMax;
                Player.Hearts = 99;
                MariaCharacter.RedrawHud(c, m);
            });
            Wait(10);

            foreach (var (who, charName) in characters)
            {
                if (!probe)
                    Do((c, m) => MariaCharacter.SwitchToCharacter(c, m, who));
                Do((_, _) => _who = $"{spot.Code}_{charName}");
                Wait(40);
                Shot("parado");
                if (probe) continue;

                Press(Button.Square);
                Wait(10);
                Shot("ataque");
                Release(Button.Square);
                Wait(40);
            }
        }

        Do((_, _) =>
        {
            Console.WriteLine($"[TELEPORT] FIM: {_shot} capturas em {Dir}");
            Console.WriteLine("[MariaTest] SEQUÊNCIA COMPLETA");
        });
        Wait(40);
        Do((_, _) => Environment.Exit(0));
    }

    // ---- running -----------------------------------------------------------------------------
    /// <summary>Once per frame on the game thread, from the moment the player has control in the castle.</summary>
    public static void Tick(CpuContext c, IMemory m)
    {
        if (_script == null) Build();

        SampleSound();
        if (_topUpHp && Player.HpMax > 0 && Player.Hp < Player.HpMax / 2) Player.Hp = Player.HpMax;

        while (_cursor < _script!.Count && _script[_cursor].Frame <= _frame)
        {
            var step = _script[_cursor];
            if (step.Gate != null)
            {
                if (!step.Gate(m) && _gateWait < step.Timeout)
                {
                    _gateWait++;
                    return; // the clock stands still
                }
                if (_gateWait >= step.Timeout)
                    Console.WriteLine($"[TEST] AVISO: {_who} não voltou a ficar parado em {step.Timeout} quadros (step=0x{(int)Player.Step:X2})");
                else if (_gateWait > 120)
                    Console.WriteLine($"[TEST] ({_who}: esperou {_gateWait} quadros o movimento anterior terminar)");
                _gateWait = 0;
                _cursor++;
                continue;
            }
            try
            {
                step.Run!(c, m);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[TEST] ERRO no passo {_cursor}: {ex.Message}");
            }
            _cursor++;
        }
        _frame++;
    }

    // which sample bank the SPU's live voices are reading, accumulated between two screenshots
    private static void SampleSound()
    {
        var spu = RecompOne.Runtime.Runtime.Spu;
        if (spu == null) return;
        spu.CaptureDebug(_voices, out _);
        foreach (var v in _voices)
        {
            if (v.Phase == Spu.AdsrPhase.Off || v.AdsrVol == 0) continue;
            int addr = v.StartAddr * 8;
            if (addr >= Vab1Start && addr < Vab2Start) _voice1 = true;
            else if (addr >= Vab2Start && addr < Vab2End) _voice2 = true;
            else _other = true;
        }
    }

    private static void Capture(IMemory m, string step)
    {
        _shot++;
        string file = $"{_shot:000}_{_who}_{step}.bmp";
        Console.WriteLine(
            $"[TEST] {file} | char={m.ReadU32(Game.PlayableCharacterAddr)} step=0x{(int)Player.Step:X2}/{Player.StepSub}" +
            $" hp={Player.Hp}/{Player.HpMax} mp={Player.Mp}/{Player.MpMax} hearts={Player.Hearts} sub={Player.SubWeapon}" +
            $" carga={(int)m.ReadU32(ChargeTimerAddr)} pos={Player.PosX},{Player.PosY} vy={Player.VelocityY >> 16}" +
            $" | som: voz(vab1)={(_voice1 ? "sim" : "nao")} especiais(vab2)={(_voice2 ? "sim" : "nao")} outros={(_other ? "sim" : "nao")}");
        _voice1 = _voice2 = _other = false;
        HostWindow.RequestScreenshot(Path.Combine(Dir, file), 1);
    }
}
