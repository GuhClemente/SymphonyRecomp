using System;
using System.IO;
using RecompOne.Runtime.Cdrom;
using RecompOne.Runtime.Memory;
using Sotn;

namespace Recompiled.Ultimate;

/// <summary>
/// The Saturn's own HUD for the playable Maria: the vine gauge holder, HP and MP bars in its two
/// tubes, the hearts digits in its green box and the MARIA plate, whose letters glow with her charge.
///
/// In SOTN Ultimate this is drawn by DRA (mods/sotn-ultimate/src/maria_saturn/5F60C_mod_maria_hud.h)
/// from art that mods/sotn-ultimate/tools/gen_mar_hud.py puts in an empty part of F_GAME2.BIN. The
/// retail DRA this recomp runs has neither, so the art is read from the SOTN Ultimate disc image in
/// disc/ and the sprites are written over the Richter-style HUD's primitives every frame. Without
/// that disc she keeps the Richter HUD (minus his name plate).
/// </summary>
public static class MariaSaturnHud
{
    // tpage 0x18 u128-255 v136-231 = VRAM x 544-575, y 392-487: chunk 3 of F_GAME2 (x 0x220, y 384)
    private const int ArtVramX = 544, ArtVramY = 392, ArtW = 32, ArtH = 96;
    private const int ArtFileOffset = 3 * 32 * 128 * 2 + (ArtVramY - 384) * 32 * 2;
    private const string ArtFile = "BIN/F_GAME2.BIN";
    private const string UltimateMarker = "BIN/MARIA.BIN";

    private const ushort Tpage = 0x18;
    private const ushort RichterTpage = 0x1B;
    private const byte V = 136;
    private const int ClutBase = 0x106; // 0x106 plate, 0x107 MP bar, 0x108 holder
    private const ushort DrawAbsPos = 0x2000;
    private const ushort DrawHide = 8;

    private const uint PlayerHudAddr = 0x8013796Cu;
    private const uint PrimBufAddr = 0x80086FECu;
    private const uint PrimStride = 0x34u;
    private const uint ClutRamAddr = 0x8006CBCCu;
    private const uint ChargeTimerAddr = 0x80072F44u; // g_Player.unk24, where MARIA.BIN copies her charge

    // palettes 0x106 (the Saturn's palette 0), 0x107 (MP bar), 0x108 (the Saturn's palette 1)
    private static readonly ushort[] BasePalettes =
    [
        0x0000, 0xFE9F, 0xFCDA, 0xCCF8, 0x8013, 0x801F, 0xA99F, 0xDEDF,
        0x8000, 0xBC00, 0x88CA, 0x952D, 0xAA16, 0xCF7F, 0xEFFF, 0xFFFF,
        0x0000, 0xFD8C, 0xFD8C, 0xFD8C, 0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x8C97, 0x8415, 0xFFFF, 0x98C6, 0x8CC2, 0x8000, 0xEFFF,
        0xCE73, 0xB38C, 0x8D43, 0x95C5, 0x9A28, 0x96A6, 0x9122, 0xB18C,
    ];
    // plate colours 9 (background) and 15 (letters) while she charges
    private static readonly ushort[,] Glow =
        { { 0xBC00, 0x801F }, { 0xBC00, 0x83E0 }, { 0x83E0, 0xFC00 }, { 0xFFFF, 0x801F } };
    // the full MP bar's colour ramp (red to white); 15 is the normal blue
    private static readonly ushort[] MpRamp =
    [
        0xB0A5, 0x843F, 0x8C7F, 0x94BF, 0x9CFF, 0xA53F, 0xAD7F, 0xB5BF,
        0xBDFF, 0xC63F, 0xCE7F, 0xD6BF, 0xDEFF, 0xE73F, 0xF7BF, 0xFD8C,
    ];
    // the plate folds away with the HUD's pause animation (g_PlayerHud.unk24): {y, h, v}
    private static readonly byte[] PauseStep = [1, 6, 12, 17, 51, 57, 61, 66];
    private static readonly byte[] PauseFrame = [1, 2, 3, 4, 3, 2, 1, 0];
    private static readonly byte[,] PauseSprite =
        { { 21, 24, V }, { 25, 16, 0 }, { 29, 8, 16 }, { 25, 16, 24 }, { 21, 24, 64 } };

    private static ushort[]? _art;
    private static bool _artSearched;
    private static readonly ushort[] _palettes = new ushort[BasePalettes.Length];
    private static readonly ushort[] _lastPalettes = new ushort[BasePalettes.Length];
    private static int _displayHp = -1;
    private static short _iconY = short.MinValue;
    private static int _frame;

    /// <summary>The art was found on a SOTN Ultimate disc image.</summary>
    public static bool Available => LoadArt() != null;

    private static ushort[]? LoadArt()
    {
        if (_artSearched) return _art;
        _artSearched = true;

        string? current = null;
        try { current = Path.GetFullPath(RecompOne.Runtime.Config.Settings.System.DiscPath ?? ""); }
        catch { }

        foreach (string dir in new[] { "disc", Path.Combine(AppContext.BaseDirectory, "disc") })
        {
            if (!Directory.Exists(dir)) continue;
            foreach (string path in Directory.GetFiles(dir))
            {
                if (!path.EndsWith(".cue", StringComparison.OrdinalIgnoreCase) &&
                    !path.EndsWith(".chd", StringComparison.OrdinalIgnoreCase)) continue;
                if (string.Equals(Path.GetFullPath(path), current, StringComparison.OrdinalIgnoreCase)) continue;

                try
                {
                    using var fs = DiscFs.Open(path);
                    if (!fs.Locate(UltimateMarker, out _, out _)) continue;

                    byte[] sheet = fs.ReadFile(ArtFile);
                    if (sheet.Length < ArtFileOffset + ArtW * ArtH * 2) continue;

                    var art = new ushort[ArtW * ArtH];
                    bool any = false;
                    for (int i = 0; i < art.Length; i++)
                    {
                        art[i] = (ushort)(sheet[ArtFileOffset + i * 2] | (sheet[ArtFileOffset + i * 2 + 1] << 8));
                        any |= art[i] != 0;
                    }
                    if (!any) continue;

                    _art = art;
                    Console.WriteLine($"[MariaHud] Arte do HUD da Maria carregada de {Path.GetFileName(path)}.");
                    return _art;
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"[MariaHud] {Path.GetFileName(path)}: {ex.Message}");
                }
            }
        }

        Console.WriteLine("[MariaHud] Disco do SOTN Ultimate não encontrado em disc/: a Maria usa o HUD do Richter.");
        return null;
    }

    private static void UploadArt()
    {
        var art = _art;
        if (art == null) return;
        var gpu = RecompOne.Runtime.Runtime.Gpu;
        if (gpu != null)
            for (int i = 0; i < art.Length; i++)
                gpu.Shadow[ArtVramX + i % ArtW, ArtVramY + i / ArtW] = art[i];
        var backend = RecompOne.Runtime.Hle.GpuHle.Backend;
        if (RecompOne.Runtime.Hle.GpuHle.Active && backend is { Ready: true })
            backend.WriteVram(ArtVramX, ArtVramY, ArtW, ArtH, art);
    }

    /// <summary>Forget what was shown: the next frame uploads the art and palettes again.</summary>
    public static void Reset()
    {
        _displayHp = -1;
        _iconY = short.MinValue;
        _frame = 0;
        Array.Clear(_lastPalettes);
    }

    private static void UpdatePalettes(IMemory m)
    {
        BasePalettes.CopyTo(_palettes, 0);

        // glow: above 100 frames of charge every 8 frames, every 4 above 200, the other pair above 300
        int charge = (int)m.ReadU32(ChargeTimerAddr);
        if (charge > 100)
        {
            int i = (_frame >> (charge > 200 ? 2 : 3)) & 1;
            if (charge > 300) i += 2;
            _palettes[9] = Glow[i, 0];
            _palettes[15] = Glow[i, 1];
        }

        // the MP bar is one colour: blue, or at full MP a red <-> white triangle wave
        int t = 15;
        if (Player.MpMax > 0 && Player.Mp >= Player.MpMax)
        {
            t = _frame & 0x1F;
            if (t > 15) t = 31 - t;
            t = Math.Min(t + 1, 14);
        }
        _palettes[17] = _palettes[18] = _palettes[19] = MpRamp[t];

        for (int p = 0; p < 3; p++)
        {
            var colors = _palettes.AsSpan(p * Palette.Colors, Palette.Colors);
            if (colors.SequenceEqual(_lastPalettes.AsSpan(p * Palette.Colors, Palette.Colors))) continue;

            int id = ClutBase + p;
            uint ram = ClutRamAddr + (uint)(id * Palette.Colors * 2);
            for (int i = 0; i < Palette.Colors; i++) m.WriteU16(ram + (uint)(i * 2), colors[i]);
            Palette.Write(id, colors);
            colors.CopyTo(_lastPalettes.AsSpan(p * Palette.Colors, Palette.Colors));
        }
    }

    private static void Sprite(IMemory m, uint prim, int x, int y, int w, int h, int u, int v,
        int clut = 0x103, ushort priority = 0x1F0, ushort tpage = Tpage, bool visible = true)
    {
        if (prim == 0) return;
        Corner(m, prim + 0x08, x, y, u, v);
        Corner(m, prim + 0x14, x + w, y, u + w, v);
        Corner(m, prim + 0x20, x, y + h, u, v + h);
        Corner(m, prim + 0x2C, x + w, y + h, u + w, v + h);
        m.WriteU16(prim + 0x0E, (ushort)clut);
        m.WriteU16(prim + 0x1A, tpage);
        m.WriteU16(prim + 0x26, priority);
        m.WriteU16(prim + 0x32, visible ? DrawAbsPos : DrawHide);
    }

    private static void Corner(IMemory m, uint at, int x, int y, int u, int v)
    {
        m.WriteU16(at, (ushort)(short)x);
        m.WriteU16(at + 2, (ushort)(short)y);
        m.WriteU8(at + 4, (byte)u);
        m.WriteU8(at + 5, (byte)v);
    }

    /// <summary>
    /// Draws her HUD over the Richter-style HUD's nine primitives. Called every frame, after the retail
    /// DrawRichterHudSubweapon and the widescreen shifts (game thread). Returns false when the art is
    /// not available, so the caller keeps the Richter HUD.
    /// </summary>
    public static bool Draw(IMemory m, int margin)
    {
        if (LoadArt() == null) return false;

        uint head = m.ReadU32(PlayerHudAddr + 4);
        if (head == 0) return false;
        Span<uint> prims = stackalloc uint[9];
        uint prim = PrimBufAddr + head * PrimStride;
        for (int i = 0; i < prims.Length; i++)
        {
            if (prim == 0) return false; // not the Richter HUD's list
            prims[i] = prim;
            prim = m.ReadU32(prim);
        }

        // F_GAME2 is reloaded on every character switch, and the game may reload it too
        if (_frame % 120 == 0) UploadArt();
        UpdatePalettes(m);
        _frame++;

        int x = -margin;

        // the shown HP follows by 5 a frame
        int hp = Player.Hp, hpMax = Math.Max(Player.HpMax, 1);
        if (_displayHp < 0) _displayHp = hp;
        _displayHp += Math.Clamp(hp - _displayHp, -5, 5);

        // 0: the holder, under the rest
        Sprite(m, prims[0], x + 2, 24, 38, 96, 128, V, ClutBase + 2, 0x1EF);

        // 1, 2: the bars rise from y 116 in its tubes
        int n = Math.Clamp(_displayHp * 90 / hpMax, 0, 90);
        Sprite(m, prims[1], x + 6, 117 - n, 4, n, 240, V, visible: n > 0);
        n = Math.Clamp(Player.Mp * 57 / Math.Max(Player.MpMax, 1), 0, 57);
        // under the holder, whose glass highlight in this tube shows over it
        Sprite(m, prims[2], x + 21, 117 - n, 3, n, 245, V, ClutBase + 1, 0x1EE, visible: n > 0);

        // 3-5: hearts, ones first: the tens only from 10, the hundreds from 100
        int hearts = Math.Max(Player.Hearts, 0);
        for (int i = 0; i < 3; i++, hearts /= 10)
            Sprite(m, prims[3 + i], x + 27 - i * 7, 29, 7, 8, 168 + hearts % 10 * 7, V + 24,
                visible: i == 0 || hearts > 0);

        // 6 is not needed; 7 stays the game's own subweapon icon, 2 px lower, in her vine ring. The
        // game only rewrites the icon when it changes, so it is moved once per rewrite: a y that is
        // not the one left here last time is a fresh one.
        m.WriteU16(prims[6] + 0x32, DrawHide);
        short iconY = (short)m.ReadU16(prims[7] + 0x0A);
        if (iconY != _iconY)
        {
            foreach (uint off in (ReadOnlySpan<uint>)[0x0Au, 0x16u, 0x22u, 0x2Eu])
                m.WriteU16(prims[7] + off, (ushort)((short)m.ReadU16(prims[7] + off) + 2));
            _iconY = (short)(iconY + 2);
        }

        // 8: the plate, or the pause animation's sprite (Richter's copies of the same art)
        int step = (int)m.ReadU32(PlayerHudAddr + 0x24);
        int d = 0;
        for (int i = 0; i < PauseStep.Length; i++)
            if (step >= PauseStep[i]) d = PauseFrame[i];
        Sprite(m, prims[8], x + 36, PauseSprite[d, 0], 64, PauseSprite[d, 1], d != 0 ? 64 : 168, PauseSprite[d, 2],
            ClutBase, 0x1EF, d != 0 ? RichterTpage : Tpage);
        return true;
    }
}
