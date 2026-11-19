using System;
using System.IO;
using RecompOne.Runtime.Cdrom;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Host;
using RecompOne.Runtime.Memory;
using Sotn;

namespace Recompiled.Ultimate;

/// <summary>
/// Maria rides into the castle on her white horse, as in SOTN Ultimate.
///
/// The Ultimate's NO3.BIN (mods/sotn-ultimate/src/castle/e_outdoor_ents_mod_horse.h, a port of the
/// Saturn's STAGE_0X entity 0x4E) turns EntityPushAlucard into ModHorseRide for any character but
/// Alucard: one sprite shows the horse with its rider while the player is hidden and pushed as in
/// Alucard's forest run; at the gate the rider leaps off and the horse (the same entity, params 0x100)
/// stays there rearing. This recomp runs the retail NO3, so the ride is ported here, instruction for
/// instruction from the v1.4 disc's code (0x801C3398-0x801C3C6C), and its art, frames, animations and
/// palettes are read from that disc image in disc/ (nothing of the mod ships with the recomp).
///
/// The game starts as Alucard (the prologue is always Richter's): on the first frame of the run, if a
/// Maria is selected, she takes his place, so she is already the player on the horse.
/// </summary>
public static class MariaHorseRide
{
    // ---- the v1.4 disc's NO3.BIN (base 0x80180000) --------------------------------------------
    private const uint UltBase = 0x80180000u;
    private const uint UltAnims = 0x801B7414u, UltAnimsEnd = 0x801B7464u;     // u8 {ticks, frame} lists
    private const uint UltLz = 0x801B7930u, UltStrips = 0x801BA4A8u;          // g_ModHorseLz, g_ModHorseStrips
    private const uint UltFrames = 0x801BA538u, UltSprites = 0x801BB028u;     // frame data, g_ModHorseSprites
    private const uint UltPals = 0x801BB0A4u;                                 // g_ModHorsePals
    private const int NStrips = 12, NSprites = 31, NPals = 5, PalBase = 0x2D9;
    // mar gallop / leap / air and the rearing horse, as offsets into the animation block
    private const uint AnimMarGallop = 0x801B7434u, AnimMarLeap = 0x801B7440u,
        AnimMarAir = 0x801B7454u, AnimRear = 0x801B745Cu;

    // ---- where they go in this game's RAM -------------------------------------------------------
    // Past the retail NO3.BIN (it ends at 0x801D7E18, its BSS included) and inside the stage area
    // (NO4.BIN reaches 0x801DF85C): nothing of NO3 or DRA lives there while the stage is loaded.
    private const uint RamBlock = 0x801D8000u;
    private const uint RamAnims = RamBlock;
    private const uint RamFrames = RamAnims + (UltAnimsEnd - UltAnims);
    private const uint RamSprites = RamFrames + (UltSprites - UltFrames);
    private const uint RamEnd = RamSprites + NSprites * 4;

    // ---- the engine --------------------------------------------------------------------------
    private const uint Player = Game.EntitiesAddr;           // PLAYER
    private const uint ScrollXHi = 0x8007308Eu;              // g_Tilemap.scrollX.i.hi
    private const uint PadSim = 0x80072EF4u;                 // g_Player.padSim
    private const uint DemoTimer = 0x80072EFCu;              // g_Player.demo_timer
    private const uint Unk14 = 0x8009740Cu;                  // g_unkGraphicsStruct.unk14 (camera lead)
    private const uint PauseAllowed = 0x8003C8B8u;           // g_PauseAllowed
    private const uint SpriteBanksPtr = 0x8003C788u;         // g_api.o.spriteBanks
    private const uint ClutIds = 0x8003C6B6u;                // g_ClutIds[PalBase]
    private const uint ClutRam = 0x8006CBCCu;                // g_Clut
    private const uint EInitSpawner = 0x80180AD0u;
    private const uint FirstFreeEntity = Game.EntitiesAddr + 192 * 0xBCu, EntitiesEnd = Game.EntitiesAddr + 256 * 0xBCu;
    private const ushort EPushAlucard = 0x4E;
    private const int HorseBank = 10;
    private const ushort HorseAnimSet = 0x800A;
    private const uint PadRight = 0x2000;
    private const uint FlagPosCameraLocked = 0x08000000u;

    // entity fields
    private const uint EPosX = 0x00, EPosY = 0x04, EVelY = 0x0C, EFacing = 0x14, EPalette = 0x16, EZPrio = 0x24,
        EStep = 0x2C, EParams = 0x30, EFlags = 0x34, EPose = 0x50, EPoseTimer = 0x52, EAnimSet = 0x54,
        EAnimFrame = 0x56, EUnk5A = 0x5A, EExt = 0x7C;
    // the ride's state in ext (ModHorseState)
    private const uint RSpeed = 0x00, RVelY = 0x04, RPosY = 0x08, RGround = 0x0C,
        RGallop = 0x10, RLeap = 0x14, RAir = 0x18, RStrip = 0x1C;

    private static readonly bool TestShots = Environment.GetEnvironmentVariable("SOTN_TEST_RIDE") == "1";
    private static readonly string ShotDir = Environment.GetEnvironmentVariable("SOTN_TEST_DIR") ?? Path.GetTempPath();

    private static byte[]? _no3;
    private static bool _searched;
    private static int _frame, _shot;

    private static bool WantsMaria =>
        UltimateOptions.UltimateEnabled &&
        UltimateOptions.CharacterMode is CharacterSelection.SaturnMaria or CharacterSelection.PSPMaria;

    /// <summary>Pre-hook of NO3's EntityPushAlucard (config/sotn.json). False: the ride ran instead.</summary>
    public static bool PushAlucardHook(CpuContext c, IMemory m)
    {
        uint self = c.A0;
        try
        {
            if ((m.ReadU16(self + EParams) & 0x100) != 0)
            {
                HorseRear(c, m, self);
                return false;
            }

            var who = (PlayableCharacter)(int)m.ReadU32(Game.PlayableCharacterAddr);
            if (m.ReadU16(self + EStep) == 0)
            {
                // the test run starts as the Saturn Maria; SOTN_TEST_RIDE_PSP=1 rides as the PSP one
                if (TestShots && Environment.GetEnvironmentVariable("SOTN_TEST_RIDE_PSP") == "1")
                    UltimateOptions.CharacterMode = CharacterSelection.PSPMaria;
                if (!WantsMaria || who == PlayableCharacter.Richter || LoadDisc() == null) return true;
                // MariaCharacter.Update usually makes her the player as NO3 loads; if not (or as the
                // other Maria) she takes the place before the run starts
                string overlay = UltimateOptions.CharacterMode == CharacterSelection.PSPMaria ? "maria2" : "maria";
                if (who != PlayableCharacter.Maria || Array.IndexOf(RecompOne.Runtime.Dispatch.Dispatcher.ActiveNames, overlay) < 0)
                {
                    MariaCharacter.SwitchToCharacter(c, m, UltimateOptions.CharacterMode);
                    if (!global::Sotn.Player.IsMaria) return true;
                    Console.WriteLine("[HorseRide] Maria entra no castelo a cavalo.");
                }
                _frame = 0;
            }
            else if (who != PlayableCharacter.Maria || m.ReadU16(self + EAnimSet) != HorseAnimSet)
            {
                return true; // Alucard's (or Richter's) own run
            }

            HorseRide(c, m, self);
            if (TestShots) TestCapture(m, self);
            return false;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[HorseRide] Erro: {ex.Message}");
            return true;
        }
    }

    // ModHorseRide (0x801C3520)
    private static void HorseRide(CpuContext c, IMemory m, uint self)
    {
        uint ride = self + EExt;
        bool gallop = false, init = false;

        m.WriteU16(Player + EAnimFrame, 0);
        m.WriteU32(PadSim, PadRight);
        switch (m.ReadU16(self + EStep))
        {
            case 0:
                MariaLoader.Invoke(c, m, SoTN_no3.InitializeEntity_no3, EInitSpawner);
                HorseInit(m, self);
                m.WriteU32(DemoTimer, 0xFF);
                m.WriteU32(PadSim, 0);
                m.WriteU16(Player + EPosX + 2, 0);
                m.WriteU32(Unk14, 0);
                m.WriteU32(ride + RVelY, 0xFFFBA000u); // FIX(-4.375)
                m.WriteU32(PauseAllowed, 0);
                m.WriteU16(ride + RStrip, 0);
                m.WriteU32(ride + RGallop, Ram(AnimMarGallop));
                m.WriteU32(ride + RLeap, Ram(AnimMarLeap));
                m.WriteU32(ride + RAir, Ram(AnimMarAir));
                m.WriteU16(self + EParams, 2); // the rearing horse's palette offset: Maria's white horse
                init = true;
                break;

            // off screen: the player is pushed right until the scene starts; the art goes to VRAM meanwhile
            case 1:
                UploadStrip(m, ride);
                Add(m, Player + EPosX, 0x60000);
                if ((short)m.ReadU16(ScrollXHi) > 0x800 && (short)m.ReadU16(ride + RStrip) >= NStrips) StepNext(m, self);
                break;

            // the horse gallops in, the camera catches up
            case 2:
                Animate(c, m, m.ReadU32(ride + RGallop), self);
                Add(m, Player + EPosX, 0x88000);
                Add(m, Unk14, 4);
                if (m.ReadU32(Unk14) == 0xC0)
                {
                    StepNext(m, self);
                    m.WriteU32(ride + RSpeed, 0x48000);
                }
                gallop = true;
                break;

            case 3:
            {
                Animate(c, m, m.ReadU32(ride + RGallop), self);
                int lead = (int)m.ReadU32(Unk14);
                uint speed;
                if (lead > 128)
                {
                    m.WriteU32(Unk14, (uint)(lead - 1));
                    speed = 0x38000;
                }
                else speed = 0x48000;
                m.WriteU32(ride + RSpeed, speed);
                Add(m, Player + EPosX, speed);
                if (speed == 0x48000) StepNext(m, self);
                gallop = true;
                break;
            }

            // at the gate the rider leaps off and the horse stays behind, rearing
            case 4:
                gallop = true;
                Animate(c, m, m.ReadU32(ride + RGallop), self);
                Add(m, Player + EPosX, 0x48000);
                if ((short)m.ReadU16(ScrollXHi) > 0xF80)
                {
                    gallop = false;
                    uint horse = MariaLoader.Invoke(c, m, SoTN_no3.AllocEntity_no3, FirstFreeEntity, EntitiesEnd);
                    if (horse != 0)
                    {
                        MariaLoader.Invoke(c, m, SoTN_no3.CreateEntityFromEntity_no3, EPushAlucard, Player, horse);
                        m.WriteU16(horse + EParams, (ushort)(m.ReadU16(self + EParams) | 0x100));
                    }
                    m.WriteU16(self + EPose, 0);
                    m.WriteU16(self + EPoseTimer, 0);
                    Animate(c, m, m.ReadU32(ride + RLeap), self);
                    m.WriteU32(ride + RGround, m.ReadU32(Player + EPosY));
                    m.WriteU16(Player + EPosY + 2, (ushort)(m.ReadU16(Player + EPosY + 2) - 32));
                    m.WriteU32(ride + RPosY, m.ReadU32(Player + EPosY));
                    // the Saturn's neigh (MOD_SFX_HORSE_NEIGH) lives in the Ultimate's NO3RIDE.BIN bank,
                    // which this recomp does not load: slot 0x79E of the retail bank is not that sound
                    StepNext(m, self);
                }
                break;

            // the leap (an arc about 87 px high), then the fall to the next room
            case 5:
            case 6:
            {
                int velY = (int)m.ReadU32(ride + RVelY) + 0x1C00; // FIX(0.109375)
                m.WriteU32(ride + RVelY, (uint)velY);
                int posY = (int)m.ReadU32(ride + RPosY) + velY;
                int ground = (int)m.ReadU32(ride + RGround);
                if (ground < posY) posY = ground;
                m.WriteU32(ride + RPosY, (uint)posY);
                m.WriteU32(Player + EVelY, 0);
                m.WriteU32(Player + EPosY, (uint)posY);
                if (m.ReadU16(self + EStep) == 5)
                {
                    if (Animate(c, m, m.ReadU32(ride + RLeap), self) == 0)
                    {
                        m.WriteU16(self + EPose, 0);
                        m.WriteU16(self + EPoseTimer, 0);
                        StepNext(m, self);
                    }
                }
                else Animate(c, m, m.ReadU32(ride + RAir), self);
                Add(m, Player + EPosX, 0x48000);
                break;
            }
        }

        if (!init) m.WriteU32(DemoTimer, 1);
        m.WriteU32(self + EPosX, m.ReadU32(Player + EPosX));
        m.WriteU32(self + EPosY, m.ReadU32(Player + EPosY));
        m.WriteU16(self + EZPrio, m.ReadU16(Player + EZPrio));
        // the gallop (MOD_SFX_HORSE_GALLOP, every 32 frames) is in the Ultimate's own NO3 bank; the
        // retail bank has SFX_SCIFI_BLAST in that slot, so the ride stays silent
        _ = gallop;
    }

    // ModHorseRear (0x801C3494): the horse left at the gate
    private static void HorseRear(CpuContext c, IMemory m, uint self)
    {
        if (m.ReadU16(self + EStep) == 0)
        {
            MariaLoader.Invoke(c, m, SoTN_no3.InitializeEntity_no3, EInitSpawner);
            // stays at the gate as the camera follows the rider
            m.WriteU32(self + EFlags, m.ReadU32(self + EFlags) | FlagPosCameraLocked);
            m.WriteU16(self + EAnimSet, HorseAnimSet);
            m.WriteU16(self + EZPrio, 0x88);
            m.WriteU16(self + EUnk5A, 0);
            m.WriteU16(self + EFacing, 0);
            m.WriteU16(self + EPose, 0);
            m.WriteU16(self + EPoseTimer, 0);
            m.WriteU16(self + EPalette, (ushort)(m.ReadU16(self + EParams) & 0xFF));
        }
        Animate(c, m, Ram(AnimRear), self);
    }

    // ModHorseInit (0x801C3398): sprite bank, palettes and the entity's look
    private static void HorseInit(IMemory m, uint self)
    {
        byte[] d = _no3!;
        // animations, frames and the bank's pointer table, relocated into RamBlock
        for (uint a = UltAnims; a < UltAnimsEnd; a++) m.WriteU8(Ram(a), d[a - UltBase]);
        for (uint a = UltFrames; a < UltSprites; a++) m.WriteU8(RamFrames + (a - UltFrames), d[a - UltBase]);
        for (int i = 0; i < NSprites; i++)
        {
            uint p = U32(d, UltSprites + (uint)(i * 4));
            m.WriteU32(RamSprites + (uint)(i * 4), p == 0 ? 0 : RamFrames + (p - UltFrames));
        }
        m.WriteU32(m.ReadU32(SpriteBanksPtr) + HorseBank * 4, RamSprites);

        Span<ushort> pal = stackalloc ushort[Palette.Colors];
        for (int k = 0; k < NPals; k++)
        {
            for (int i = 0; i < Palette.Colors; i++)
            {
                pal[i] = (ushort)U16(d, UltPals + (uint)((k * Palette.Colors + i) * 2));
                m.WriteU16(ClutRam + (uint)(((PalBase + k) * Palette.Colors + i) * 2), pal[i]);
            }
            WriteVram((m.ReadU16(ClutIds + (uint)(k * 2)) & 0x3F) << 4, m.ReadU16(ClutIds + (uint)(k * 2)) >> 6,
                Palette.Colors, 1, pal);
        }

        m.WriteU16(self + EAnimSet, HorseAnimSet);
        m.WriteU16(self + EUnk5A, 0);
        m.WriteU16(self + EPalette, 0);
        m.WriteU16(self + EFacing, 0);
        m.WriteU16(self + EAnimFrame, 0);
    }

    // ModHorseUploadStrip (0x801C32E4): one strip of the art (up to 32 rows of a 32-wide quadrant) per call
    private static void UploadStrip(IMemory m, uint ride)
    {
        int n = (short)m.ReadU16(ride + RStrip);
        if (n >= NStrips) return;
        m.WriteU16(ride + RStrip, (ushort)(n + 1));

        byte[] d = _no3!;
        uint s = UltStrips + (uint)(n * 12);
        int off = (int)U32(d, s), x = U16(d, s + 4), y = U16(d, s + 6), rows = U16(d, s + 8);
        byte[] px = Unpack(d, (int)(UltLz - UltBase) + off, rows * 64);
        var pixels = new ushort[32 * rows];
        for (int i = 0; i < pixels.Length; i++) pixels[i] = (ushort)(px[i * 2] | (px[i * 2 + 1] << 8));
        WriteVram(x, y, 32, rows, pixels);
    }

    // the MARIA.BIN LZSS format: a flag byte, then per bit a literal or {offset 12 bits, length 4 bits}
    private static byte[] Unpack(byte[] src, int at, int size)
    {
        var dst = new byte[size];
        int o = 0;
        while (o < size)
        {
            int flags = src[at++];
            for (int bit = 0; bit < 8 && o < size; bit++, flags >>= 1)
            {
                if ((flags & 1) != 0) dst[o++] = src[at++];
                else
                {
                    int v = src[at] | (src[at + 1] << 8);
                    at += 2;
                    int p = o - (v & 0xFFF) - 1;
                    for (int len = (v >> 12) + 3; len > 0 && o < size; len--) dst[o++] = dst[p++];
                }
            }
        }
        return dst;
    }

    private static void WriteVram(int x, int y, int w, int h, ReadOnlySpan<ushort> pixels)
    {
        var gpu = RecompOne.Runtime.Runtime.Gpu;
        if (gpu != null)
            for (int i = 0; i < w * h; i++) gpu.Shadow[x + i % w, y + i / w] = pixels[i];
        var backend = RecompOne.Runtime.Hle.GpuHle.Backend;
        if (RecompOne.Runtime.Hle.GpuHle.Active && backend is { Ready: true })
            backend.WriteVram(x, y, w, h, pixels);
    }

    private static uint Animate(CpuContext c, IMemory m, uint anim, uint self) =>
        MariaLoader.Invoke(c, m, SoTN_no3.AnimateEntity_no3, anim, self);

    private static void StepNext(IMemory m, uint self) => m.WriteU16(self + EStep, (ushort)(m.ReadU16(self + EStep) + 1));
    private static void Add(IMemory m, uint addr, uint v) => m.WriteU32(addr, m.ReadU32(addr) + v);
    private static uint Ram(uint ultAnim) => RamAnims + (ultAnim - UltAnims);
    private static uint U32(byte[] d, uint a) => BitConverter.ToUInt32(d, (int)(a - UltBase));
    private static int U16(byte[] d, uint a) => BitConverter.ToUInt16(d, (int)(a - UltBase));

    /// <summary>The Ultimate's NO3.BIN, from a SOTN Ultimate v1.4 disc image in disc/.</summary>
    private static byte[]? LoadDisc()
    {
        if (_searched) return _no3;
        _searched = true;
        foreach (string dir in new[] { "disc", Path.Combine(AppContext.BaseDirectory, "disc") })
        {
            if (!Directory.Exists(dir)) continue;
            foreach (string path in Directory.GetFiles(dir))
            {
                if (!path.EndsWith(".cue", StringComparison.OrdinalIgnoreCase) &&
                    !path.EndsWith(".chd", StringComparison.OrdinalIgnoreCase)) continue;
                try
                {
                    using var fs = DiscFs.Open(path);
                    if (!fs.Locate("BIN/MARIA.BIN", out _, out _) || !fs.Locate("ST/NO3/NO3.BIN", out _, out _)) continue;
                    byte[] d = fs.ReadFile("ST/NO3/NO3.BIN");
                    if (!LooksLikeV14(d)) continue;
                    _no3 = d;
                    Console.WriteLine($"[HorseRide] Cavalgada do NO3 carregada de {Path.GetFileName(path)}.");
                    return _no3;
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"[HorseRide] {Path.GetFileName(path)}: {ex.Message}");
                }
            }
        }
        Console.WriteLine("[HorseRide] NO3.BIN do SOTN Ultimate v1.4 não encontrado em disc/: a Maria corre a pé.");
        return null;
    }

    // the addresses above are the v1.4 build's: its EntityPushAlucard must branch to the ride, and the
    // tables must have the shape the builder gives them
    private static bool LooksLikeV14(byte[] d)
    {
        if (d.Length < UltPals + NPals * 32 - UltBase) return false;
        if (U32(d, 0x801C39F0u) != 0x0C070D48u) return false; // jal 0x801C3520 (ModHorseRide)
        if (U32(d, 0x801C39CCu) != 0x0C070D25u) return false; // jal 0x801C3494 (ModHorseRear)
        for (int i = 1; i < NSprites; i++)
        {
            uint p = U32(d, UltSprites + (uint)(i * 4));
            if (p < UltFrames || p >= UltSprites) return false;
        }
        for (int n = 0; n < NStrips; n++)
        {
            uint s = UltStrips + (uint)(n * 12);
            if (U32(d, s) >= UltStrips - UltLz || U16(d, s + 8) is 0 or > 32) return false;
        }
        return true;
    }

    private static void TestCapture(IMemory m, uint self)
    {
        _frame++;
        int step = m.ReadU16(self + EStep);
        if (_frame % (step >= 5 ? 4 : 30) != 0) return;
        if (_shot >= 60) return;
        _shot++;
        string file = Path.Combine(ShotDir, $"ride_{_shot:000}_step{step}.bmp");
        Console.WriteLine($"[HorseRide] {Path.GetFileName(file)} frame={_frame} scrollX={(short)m.ReadU16(ScrollXHi)} " +
                          $"x={(short)m.ReadU16(Player + 2)} y={(short)m.ReadU16(Player + 6)} anim={m.ReadU16(self + EAnimFrame)}");
        HostWindow.RequestScreenshot(file, 1);
    }
}
