using System;
using RecompOne.Runtime.Memory;
using Sotn;

namespace Recompiled.Ultimate;

/// <summary>
/// SOTN_TEST_HUDSWAP=1 (with SOTN_TEST_MARIA=1): switches Saturn Maria -> Richter -> PSP Maria -> Richter
/// -> Alucard -> Richter and, after each, logs the Richter HUD subweapon icon's VRAM area against the
/// same area of F_GAME2.BIN, with a screenshot, to see what leaves PSP Maria's icon on Richter's HUD.
/// </summary>
public static partial class CharacterTest
{
    // tpage 0x1E (x 896, y 256): the Richter-style HUD's subweapon icons
    private const int IconAreaX = 896, IconAreaY = 256 + 192, IconAreaW = 64, IconAreaH = 64;

    private static ulong VramHash()
    {
        var gpu = RecompOne.Runtime.Runtime.Gpu!;
        ulong h = 1469598103934665603UL;
        for (int y = 0; y < IconAreaH; y++)
            for (int x = 0; x < IconAreaW; x++)
                h = (h ^ gpu.Shadow[IconAreaX + x, IconAreaY + y]) * 1099511628211UL;
        return h;
    }

    private static ulong FileHash()
    {
        // F_GAME2 holds 32 chunks of 32x128: x = 0x200 + 0x40*(i/4) + 0x20*(i%2), y = 256 + 128*((i%4)/2)
        var data = RecompOne.Runtime.Runtime.Cd!.Fs.ReadFile("BIN/F_GAME2.BIN");
        ulong h = 1469598103934665603UL;
        for (int y = 0; y < IconAreaH; y++)
            for (int x = 0; x < IconAreaW; x++)
            {
                int vx = IconAreaX + x, vy = IconAreaY + y;
                int col = (vx - 0x200) / 0x40, half = ((vx - 0x200) / 0x20) & 1, row = (vy - 256) / 128;
                int i = col * 4 + row * 2 + half;
                int p = (vy - 256 - row * 128) * 32 + (vx & 31);
                int off = i * 32 * 128 * 2 + p * 2;
                ushort px = off + 1 < data.Length ? (ushort)(data[off] | (data[off + 1] << 8)) : (ushort)0;
                h = (h ^ px) * 1099511628211UL;
            }
        return h;
    }

    private static void IconReport(string tag) => Do((_, m) =>
    {
        uint head = m.ReadU32(0x8013796Cu + 4);
        uint prim = head == 0 ? 0 : 0x80086FECu + head * 0x34u;
        for (int i = 0; i < 7 && prim != 0; i++) prim = m.ReadU32(prim);
        string icon = prim == 0 ? "-" :
            $"uv=({m.ReadU8(prim + 0xC)},{m.ReadU8(prim + 0xD)}) tpage=0x{m.ReadU16(prim + 0x1A):X} clut=0x{m.ReadU16(prim + 0x0E):X}";
        Console.WriteLine($"[HUDSWAP] {tag}: char={m.ReadU32(Game.PlayableCharacterAddr)} sub={Player.SubWeapon} icone {icon} vram=0x{VramHash():X16} F_GAME2=0x{FileHash():X16}");
    });

    private static void BuildHudSwapScript()
    {
        Do((_, _) => _who = "maria_saturn");
        Wait(30);
        IconReport("inicio maria_saturn");
        foreach (var (who, name) in new[]
                 {
                     (CharacterSelection.RichterClassic, "richter_1"),
                     (CharacterSelection.PSPMaria, "maria_psp"),
                     (CharacterSelection.RichterClassic, "richter_2"),
                     (CharacterSelection.Alucard, "alucard"),
                     (CharacterSelection.RichterClassic, "richter_3"),
                 })
        {
            Switch(who, name);
            Do((_, _) => Player.SubWeapon = 5); // an icon from the item icon page (tpage 0x1E)
            Wait(30);
            IconReport(name);
            Shot("hud");
        }
        Do((_, _) => Console.WriteLine("[MariaTest] SEQUÊNCIA COMPLETA"));
        Wait(30);
        Do((_, _) => Environment.Exit(0));
    }
}
