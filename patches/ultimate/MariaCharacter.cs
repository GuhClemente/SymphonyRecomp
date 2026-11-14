using System;
using System.IO;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Dispatch;
using RecompOne.Runtime.Events;
using RecompOne.Runtime.Host;
using RecompOne.Runtime.Host.Window;
using RecompOne.Runtime.Memory;
using Sotn;

namespace Recompiled.Ultimate;

public static class MariaCharacter
{
    // g_Status.D_80097C40 in the retail PlayerStatus layout (0x80097964 + 0x2DC).
    private const uint CostumeMagicAddr = Game.StatusAddr + 0x2DCu;
    public const uint RicSaturnCoatMagic = 0x32434952u; // "RIC2"

    private static readonly bool TestMode = Environment.GetEnvironmentVariable("SOTN_TEST_MARIA") == "1";

    // where the automated run (SOTN_TEST_MARIA=1) drops its screenshots
    private static readonly string TestDir =
        Environment.GetEnvironmentVariable("SOTN_TEST_DIR") ?? Path.GetTempPath();

    private static int _aerialJumpsRemaining = 2; // Allows triple jump
    private static bool _autoTransformFailed;
    private static bool _wasMaria;
    private static int _autoPadFrame = 0;
    private static bool _simulateSquare;
    private static bool _simulateCross;
    private static bool _reachedCastle;
    private static int _castleFrame = 0;

    static MariaCharacter()
    {
        Event.AddListener<KeyboardEvent>(e =>
        {
            if (e.Pressed && !e.Repeat && e.Key == (int)Silk.NET.Input.Key.C)
            {
                if (Game.Available && Game.InGame && Player.HasControl)
                    CycleNextCharacter();
            }
        });

        if (TestMode)
        {
            UltimateOptions.CharacterMode = CharacterSelection.SaturnMaria;
            RecompOne.Runtime.Events.Event.AddListener<RecompOne.Runtime.Events.PadReadEvent>(e =>
            {
                if (e.Port == 0)
                {
                    if (!Game.InGame)
                    {
                        _autoPadFrame++;
                        // In Title/Select screen, pulse Start / Confirm every 20 frames
                        if ((_autoPadFrame % 20) < 6)
                        {
                            // Active-low in swapped format:
                            // Start = 0x0800, Cross = 0x0040, Circle = 0x0020
                            e.Buttons = (ushort)(e.Buttons & ~(0x0800 | 0x0040 | 0x0020));
                        }
                    }
                    else
                    {
                        if (_simulateSquare) e.Buttons = (ushort)(e.Buttons & ~0x0080);
                        if (_simulateCross) e.Buttons = (ushort)(e.Buttons & ~0x0040);
                    }
                }
            });
        }
    }

    /// <summary>Lets the automatic transformation try again (e.g. after the option changes).</summary>
    public static void ResetState()
    {
        _autoTransformFailed = false;
    }

    private static bool WantsSaturnMaria =>
        UltimateOptions.CharacterMode == CharacterSelection.SaturnMaria;

    /// <summary>Playing as the Saturn Maria (MARIA.BIN), not the PSP one.</summary>
    public static bool IsSaturnMaria =>
        UltimateOptions.UltimateEnabled && Player.IsMaria &&
        UltimateOptions.CharacterMode != CharacterSelection.PSPMaria;

    public static bool IsPSPMaria =>
        UltimateOptions.UltimateEnabled && Player.IsMaria &&
        UltimateOptions.CharacterMode == CharacterSelection.PSPMaria;

    public static bool IsSaturnRichter =>
        UltimateOptions.UltimateEnabled &&
        UltimateOptions.CharacterMode == CharacterSelection.RichterSaturnCoat;

    public static void CycleNextCharacter()
    {
        if (!Game.Available || !Game.InGame || Game.IsLoading) return;
        var current = UltimateOptions.CharacterMode;
        CharacterSelection next = current switch
        {
            CharacterSelection.SaturnMaria => CharacterSelection.PSPMaria,
            CharacterSelection.PSPMaria => CharacterSelection.RichterClassic,
            CharacterSelection.RichterClassic => CharacterSelection.RichterSaturnCoat,
            CharacterSelection.RichterSaturnCoat => CharacterSelection.Alucard,
            _ => CharacterSelection.SaturnMaria
        };
        RequestCharacterSwitch(next);
    }

    public static void RequestCharacterSwitch(CharacterSelection selection)
    {
        GameThread.Post((c, m) => SwitchToCharacter(c, m, selection));
    }

    // The player overlay slot. In SOTN Ultimate the game loads BIN/F_GAME2.BIN (the non-Alucard HUD,
    // palettes and sprite sheet) and only then the player overlay, whose init puts its own art on top.
    // The same order is kept here: the sheet first, then the binary, then its init.
    private const uint PlayerOverlayBase = 0x8013C000u;

    public static void SwitchToCharacter(CpuContext c, IMemory m, CharacterSelection selection)
    {
        if (!Game.Available || !Game.InGame || Game.IsLoading || Game.InPrologue()) return;

        try
        {
            UltimateOptions.CharacterMode = selection;
            _autoTransformFailed = false;
            // a Maria swaps the two voice banks: the next Richter / Alucard gets the disc's own back
            bool hadMariaVoice = MariaLoader.SoundsLoaded;
            MariaLoader.OnMariaLeft(m);
            ReleasePspHud(m);

            bool ok = true;
            switch (selection)
            {
                case CharacterSelection.SaturnMaria:
                    LoadGameGraphics(m, "BIN/F_GAME2.BIN");
                    ok = MariaLoader.TransformToMaria(c, m);
                    break;

                case CharacterSelection.PSPMaria:
                    LoadGameGraphics(m, "BIN/F_GAME2.BIN");
                    ok = LoadPlayerOverlay(c, m, "maria2", "MARIA2.BIN", PlayableCharacter.Maria);
                    // her PSP voice sits in Richter's voice slots, with the retail priorities
                    if (ok) MariaLoader.LoadMariaSounds(c, m, "MARIA2_SD.BIN", saturnPriorities: false);
                    if (ok) ToastNotifications.ShowText("🌟 Maria Renard (PSP)", "Maria do PSP Dracula X Chronicles ativada!", null, 3f);
                    break;

                case CharacterSelection.RichterClassic:
                    m.WriteU32(CostumeMagicAddr, 0);
                    LoadGameGraphics(m, "BIN/F_GAME2.BIN");
                    ok = LoadPlayerOverlay(c, m, "ric", RicDiscFile, PlayableCharacter.Richter);
                    if (ok) ToastNotifications.ShowText("Richter Belmont", "Richter Clássico ativado!", null, 3f);
                    break;

                case CharacterSelection.RichterSaturnCoat:
                    m.WriteU32(CostumeMagicAddr, RicSaturnCoatMagic);
                    LoadGameGraphics(m, "BIN/F_GAME2.BIN");
                    ok = LoadPlayerOverlay(c, m, "ric2", "RIC2.BIN", PlayableCharacter.Richter);
                    if (ok) ApplyRichterCostume(m);
                    if (ok) ToastNotifications.ShowText("Richter (Saturn)", "Richter com casaco longo do Saturn ativado!", null, 3f);
                    break;

                case CharacterSelection.Alucard:
                    m.WriteU32(Game.PlayableCharacterAddr, (uint)PlayableCharacter.Alucard);
                    Dispatcher.Load("dra");
                    Dispatcher.Load("w0_043");
                    Dispatcher.Load("w1_052");
                    RestoreAlucardWeapons(m);
                    LoadGameGraphics(m, "BIN/F_GAME.BIN");
                    Player.SubWeapon = 0;
                    InitAlucard(c, m);
                    ToastNotifications.ShowText("Alucard", "Alucard reativado!", null, 3f);
                    break;
            }

            if (!ok)
            {
                Console.WriteLine($"[CharacterSwitcher] Troca para {selection} falhou.");
                return;
            }

            if (hadMariaVoice && selection is not (CharacterSelection.SaturnMaria or CharacterSelection.PSPMaria))
                MariaLoader.RestoreRetailSounds(c, m, alucard: selection == CharacterSelection.Alucard);

            Player.Step = PlayerStep.Standing;
            Player.StepSub = 0;
            _wasMaria = Player.IsMaria;
            RedrawHud(c, m);
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[CharacterSwitcher] Erro na troca de personagem: {ex}");
        }
    }

    // DRA's own init for Alucard, the counterpart of the overlays' Init: it rebuilds the player entity
    // (sprite bank, palette 0x8100, g_Player) and the palettes his sprite parts use, which in
    // F_GAME.BIN are only placeholders. It also puts him at x = 32, so his place is kept.
    private static void InitAlucard(CpuContext c, IMemory m)
    {
        uint player = Game.EntitiesAddr;
        uint posX = m.ReadU32(player), posY = m.ReadU32(player + 4);
        ushort facing = m.ReadU16(player + 0x14);
        MariaLoader.Invoke(c, m, SoTN_dra.AluInit);
        m.WriteU32(player, posX);
        m.WriteU32(player + 4, posY);
        m.WriteU16(player + 0x14, facing);
    }

    private static bool LoadPlayerOverlay(CpuContext c, IMemory m, string overlay, string file,
        PlayableCharacter character)
    {
        if (!Dispatcher.Overlays.ContainsKey(overlay))
        {
            Console.WriteLine($"[CharacterSwitcher] Overlay '{overlay}' não foi gerado (config/sotn.json).");
            ToastNotifications.ShowText("Troca de personagem", $"Overlay '{overlay}' ausente: refaça o build inicial.", null, 5f);
            return false;
        }
        if (!LoadCharacterBinary(m, file))
        {
            ToastNotifications.ShowText("Troca de personagem", $"{file} não encontrado.", null, 5f);
            return false;
        }

        Dispatcher.Load(overlay);
        m.WriteU32(Game.PlayableCharacterAddr, (uint)character);
        // the file starts with its PlayerOvl table: {Main, Init, ...}
        uint init = m.ReadU32(PlayerOverlayBase + 4);
        MariaLoader.Invoke(c, m, (cc, mm) => Dispatcher.Call(cc, mm, init));
        return true;
    }

    private const uint PlayerHudAddr = 0x8013796Cu;
    private const uint BossBarStateAddr = 0x8003C744u;
    private const uint PrimBufAddr = 0x80086FECu;
    private const uint PrimStride = 0x34u;

    /// <summary>
    /// Rebuilds the HUD for the character that is now active, the way the game does on a stage
    /// load: free the old primitives and let DrawHud allocate the right set. Nothing is hidden by
    /// hand: for Richter / Maria the boss bar (prims 1, 3, 4 and the colour bars) sits past the right
    /// edge of the screen until the Dracula fight slides it in, and prim 2 is the player's own life bar.
    /// </summary>
    public static void RedrawHud(CpuContext c, IMemory m)
    {
        try
        {
            uint p1 = m.ReadU32(PlayerHudAddr + 4);
            if (p1 != 0)
            {
                MariaLoader.Invoke(c, m, SoTN_dra.FreePrimitives, p1);
                m.WriteU32(PlayerHudAddr + 4, 0);
            }
            uint p2 = m.ReadU32(PlayerHudAddr + 8);
            if (p2 != 0)
            {
                MariaLoader.Invoke(c, m, SoTN_dra.FreePrimitives, p2);
                m.WriteU32(PlayerHudAddr + 8, 0);
            }

            m.WriteU32(PlayerHudAddr + 0x24u, 0);
            if (!Player.IsAlucard) m.WriteU32(BossBarStateAddr, 0u);

            if (Player.HpMax <= 0) Player.HpMax = 50;
            if (Player.Hp <= 0) Player.Hp = Player.HpMax;

            MariaLoader.Invoke(c, m, SoTN_dra.DrawHud);
            MariaLoader.Invoke(c, m, SoTN_dra.DrawHudSubweapon);

            Console.WriteLine($"[CharacterSwitcher] HUD redesenhado para {UltimateOptions.CharacterMode} (PlayableChar={m.ReadU32(Game.PlayableCharacterAddr)})");
            if (TestMode) DumpHudPrims(m, $"AfterRedraw-{UltimateOptions.CharacterMode}");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[CharacterSwitcher] Erro ao redesenhar HUD: {ex.Message}");
        }
    }

    // ---------------------------------------------------------------------------------------------
    // PSP Maria's HUD: her two subweapon icons. SOTN Ultimate's DRA calls MARIA2.BIN's Mar2HudIcons
    // from DrawRichterHudSubweapon through g_ModMar2HudIcons, with one extra primitive after the
    // subweapon icon. The retail DRA has neither, so the pointer is parked in g_api (config/sotn.json
    // relocations) and the call is made here, after the retail function has run.
    // ---------------------------------------------------------------------------------------------
    private const uint ModMar2HudIconsAddr = 0x8003C8ACu;
    // The Richter-style HUD's primitives (DrawRichterHud allocates 9): 7 is the subweapon icon,
    // 8 the "RICHTER / REST" plate, 1, 3 and 4 the Dracula boss gauge parked off screen.
    private const int HudSubweaponPrim = 7;
    private const int HudPlatePrim = 8;
    private const int HudSparePrim = 4;
    private const ushort DrawHide = 8;

    private static bool _mariaHud;       // refreshed once per frame in Update
    private static bool _pspMariaLoaded;

    private static bool PspMariaActive =>
        UltimateOptions.CharacterMode == CharacterSelection.PSPMaria && Player.IsMaria &&
        Array.IndexOf(Dispatcher.ActiveNames, "maria2") >= 0;

    private static uint HudPrimAt(IMemory m, int index)
    {
        uint head = m.ReadU32(PlayerHudAddr + 4);
        if (head == 0) return 0;
        uint prim = PrimBufAddr + head * PrimStride;
        for (int i = 0; i < index && prim != 0; i++) prim = m.ReadU32(prim);
        return prim;
    }

    private static void ReleasePspHud(IMemory m)
    {
        _pspMariaLoaded = false;
        _mariaHud = false;
        m.WriteU32(ModMar2HudIconsAddr, 0);
    }

    /// <summary>Called by WidescreenPatch.PostDrawRichterHudSubweapon (game thread), every frame.</summary>
    public static void DrawMariaHud(CpuContext c, IMemory m, int margin)
    {
        if (!_mariaHud) return;

        // the plate is Richter's name: neither Maria shows it
        uint plate = HudPrimAt(m, HudPlatePrim);
        if (plate != 0) m.WriteU16(plate + 0x32u, DrawHide);

        if (!_pspMariaLoaded) return;
        uint fn = m.ReadU32(ModMar2HudIconsAddr);
        if (fn == 0 || !Dispatcher.CanCall(fn)) return;

        uint icon = HudPrimAt(m, HudSubweaponPrim);
        uint spare = HudPrimAt(m, HudSparePrim);
        if (icon == 0 || spare == 0) return;

        // Mar2HudIcons(prim) draws the slot she throws into prim and the other one into prim->next.
        // The retail HUD has no primitive after the icon for that, so a boss gauge one (never on
        // screen for her) is lent for the call; the list itself is left as the game built it.
        uint next = m.ReadU32(icon);
        uint spareNext = m.ReadU32(spare);
        m.WriteU32(icon, spare);
        try
        {
            MariaLoader.Invoke(c, m, (cc, mm) => Dispatcher.Call(cc, mm, fn), icon);
        }
        finally
        {
            m.WriteU32(icon, next);
            m.WriteU32(spare, spareNext);
        }

        if (margin != 0)
        {
            ShiftPrimX(m, icon, -margin);
            ShiftPrimX(m, spare, -margin);
        }
    }

    private static void ShiftPrimX(IMemory m, uint prim, int dx)
    {
        foreach (uint off in (ReadOnlySpan<uint>)[0x08u, 0x14u, 0x20u, 0x2Cu])
            m.WriteU16(prim + off, (ushort)((short)m.ReadU16(prim + off) + dx));
    }

    // Richter's own overlay is the retail game's: it is read from the disc image, like F_GAME.BIN and
    // the weapons, not shipped in assets/. The mod binaries (MARIA.BIN, MARIA2.BIN, RIC2.BIN) are files.
    private const string RicDiscFile = "BIN/RIC.BIN";

    private static bool LoadCharacterBinary(IMemory m, string filename)
    {
        byte[]? bytes = null;
        if (filename.StartsWith("BIN/", StringComparison.Ordinal))
        {
            bytes = RecompOne.Runtime.Runtime.Cd?.Fs.ReadFile(filename);
        }
        else
        {
            string path = Path.Combine("assets", "characters", filename);
            if (!File.Exists(path)) path = Path.Combine(AppContext.BaseDirectory, "assets", "characters", filename);
            if (File.Exists(path)) bytes = File.ReadAllBytes(path);
        }
        if (bytes == null || bytes.Length == 0 || bytes.Length > 0x80180000u - PlayerOverlayBase) return false;

        for (int i = 0; i < bytes.Length; i++)
            m.WriteU8(PlayerOverlayBase + (uint)i, bytes[i]);
        Console.WriteLine($"[CharacterSwitcher] Injetados {bytes.Length} bytes de {filename} em 0x{PlayerOverlayBase:X8}");
        return true;
    }

    private static void RestoreAlucardWeapons(IMemory m)
    {
        var cd = RecompOne.Runtime.Runtime.Cd;
        if (cd == null) return;
        try
        {
            var w0 = cd.Fs.ReadFile("BIN/WEAPON0.BIN");
            if (w0 != null)
            {
                // w0_043 (Alucard Sword): offset 1232896 + skip 16384 = 1249280, size 12288
                int srcOff = 1232896 + 16384;
                uint dest = 0x8017A000u;
                for (int i = 0; i < 12288 && (srcOff + i) < w0.Length; i++)
                    m.WriteU8(dest + (uint)i, w0[srcOff + i]);
                Console.WriteLine("[CharacterSwitcher] w0_043 (Alucard Sword) restaurado em 0x8017A000");
            }

            var w1 = cd.Fs.ReadFile("BIN/WEAPON1.BIN");
            if (w1 != null)
            {
                // w1_052 (Alucard Shield): offset 1490944 + skip 16384 = 1507328, size 12288
                int srcOff = 1490944 + 16384;
                uint dest = 0x8017D000u;
                for (int i = 0; i < 12288 && (srcOff + i) < w1.Length; i++)
                    m.WriteU8(dest + (uint)i, w1[srcOff + i]);
                Console.WriteLine("[CharacterSwitcher] w1_052 (Alucard Shield) restaurado em 0x8017D000");
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[CharacterSwitcher] Erro ao restaurar armas de Alucard: {ex.Message}");
        }
    }

    public static void Update(CpuContext c, IMemory m)
    {
        if (!UltimateOptions.UltimateEnabled) return;
        if (!Game.Available || !Game.InGame || Game.IsLoading) return;

        bool isMaria = Player.IsMaria;

        if (_wasMaria && !isMaria) MariaLoader.OnMariaLeft(m);
        _wasMaria = isMaria;

        if (WantsSaturnMaria && !isMaria && !_autoTransformFailed && !Game.InPrologue())
        {
            LoadGameGraphics(m, "BIN/F_GAME2.BIN");
            if (MariaLoader.TransformToMaria(c, m))
            {
                _wasMaria = true;
                isMaria = true;
                RedrawHud(c, m);
            }
            else
            {
                _autoTransformFailed = true;
            }
        }

        // Richter Saturn Coat Look
        if (Player.IsRichter && IsSaturnRichter)
            m.WriteU32(CostumeMagicAddr, RicSaturnCoatMagic);

        // Maria Triple Jump & Aerial Mechanics
        if (IsSaturnMaria) UpdateMariaJumps(m);

        // one overlay lookup per frame for the HUD hook
        _mariaHud = isMaria;
        _pspMariaLoaded = isMaria && PspMariaActive;

        if (Player.HasControl && (Game.IsTapped(Button.R3) || Game.IsTapped(Button.L3)))
        {
            CycleNextCharacter();
        }

        if (TestMode)
        {
            string testDir = TestDir;
            if (!_reachedCastle)
            {
                if (_autoPadFrame == 2000)
                {
                    Console.WriteLine("[MariaTest] Capturando 0: Floresta limpa sem HUD durante cutscene...");
                    HostWindow.RequestScreenshot(Path.Combine(testDir, "maria_0_forest_clean.bmp"), 1);
                }

                if (Player.PosX >= 80 && Player.PosY >= 130 && Player.HasControl)
                {
                    _reachedCastle = true;
                    Console.WriteLine($"[MariaTest] CHEGOU AO CASTELO! Iniciando testes (PosX={Player.PosX}, PosY={Player.PosY})...");
                }
            }
            else
            {
                _castleFrame++;
                UpdateTestSequence(c, m);
            }
        }
    }

    private static void UpdateTestSequence(CpuContext c, IMemory m)
    {
        string testDir = TestDir;

        switch (_castleFrame)
        {
            case 10:
                Console.WriteLine("[MariaTest] Capturando 1: Stand na entrada do castelo (Maria Saturn)...");
                DumpHudPrims(m, "Capture-1-SaturnMaria");
                HostWindow.RequestScreenshot(Path.Combine(testDir, "maria_1_stand.bmp"), 1);
                break;

            case 30:
                _simulateSquare = true;
                break;
            case 48:
                Console.WriteLine("[MariaTest] Capturando 2: Ataque com pombas no castelo...");
                HostWindow.RequestScreenshot(Path.Combine(testDir, "maria_2_attack.bmp"), 1);
                break;
            case 55:
                _simulateSquare = false;
                break;

            case 80:
                _simulateCross = true;
                break;
            case 95:
                Console.WriteLine("[MariaTest] Capturando 3: Pulo no ar no castelo...");
                HostWindow.RequestScreenshot(Path.Combine(testDir, "maria_3_jump.bmp"), 1);
                break;
            case 105:
                _simulateCross = false;
                break;

            case 150:
                Console.WriteLine("[MariaTest] Disparando Magia Guardian Knuckle no castelo...");
                Player.Mp = 99;
                Player.MpMax = 99;
                SpellWheel.TriggerMariaCast(0);
                break;

            case 175:
                Console.WriteLine("[MariaTest] Capturando 4: Magia / Invocação ativada...");
                HostWindow.RequestScreenshot(Path.Combine(testDir, "maria_4_magic.bmp"), 1);
                break;

            case 220:
                Console.WriteLine("[MariaTest] Testando Troca em Tempo Real -> PSP Maria (MARIA2.BIN)...");
                SwitchToCharacter(c, m, CharacterSelection.PSPMaria);
                break;

            case 250:
                Console.WriteLine("[MariaTest] Capturando 5: PSP Maria ativa...");
                DumpHudPrims(m, "Capture-5-PSPMaria");
                HostWindow.RequestScreenshot(Path.Combine(testDir, "maria_5_psp_maria.bmp"), 1);
                break;

            case 290:
                Console.WriteLine("[MariaTest] Testando Troca em Tempo Real -> Richter Saturn Coat (RIC2.BIN)...");
                SwitchToCharacter(c, m, CharacterSelection.RichterSaturnCoat);
                break;

            case 320:
                Console.WriteLine("[MariaTest] Capturando 6: Richter Saturn Coat ativo...");
                HostWindow.RequestScreenshot(Path.Combine(testDir, "maria_6_richter_coat.bmp"), 1);
                break;

            case 360:
                Console.WriteLine("[MariaTest] Testando Troca em Tempo Real -> Richter Clássico (RIC.BIN)...");
                SwitchToCharacter(c, m, CharacterSelection.RichterClassic);
                break;

            case 390:
                Console.WriteLine("[MariaTest] Capturando 7: Richter Clássico ativo...");
                HostWindow.RequestScreenshot(Path.Combine(testDir, "maria_7_richter.bmp"), 1);
                break;

            case 430:
                Console.WriteLine("[MariaTest] Testando Troca em Tempo Real -> Alucard (DRA.BIN)...");
                SwitchToCharacter(c, m, CharacterSelection.Alucard);
                break;

            case 460:
                Console.WriteLine("[MariaTest] Capturando 8: Alucard ativo...");
                DumpHudPrims(m, "Capture-8-Alucard");
                DumpPlayerPalette(m, "Capture-8-Alucard");
                HostWindow.RequestScreenshot(Path.Combine(testDir, "maria_8_alucard.bmp"), 1);
                break;

            case 500:
                Console.WriteLine("[MariaTest] Testando Troca em Tempo Real -> Retorno para Maria Saturn...");
                SwitchToCharacter(c, m, CharacterSelection.SaturnMaria);
                break;

            case 530:
                Console.WriteLine("[MariaTest] Capturando 9: Retorno para Maria Saturn ativo...");
                DumpHudPrims(m, "Capture-9-SaturnMariaReturn");
                HostWindow.RequestScreenshot(Path.Combine(testDir, "maria_9_maria_return.bmp"), 1);
                break;

            case 560:
                Console.WriteLine("[MariaTest] SEQUÊNCIA COMPLETA DE TODOS OS 5 PERSONAGENS FINALIZADA COM SUCESSO!");
                break;
        }
    }

    private static void UpdateMariaJumps(IMemory m)
    {
        if (Player.Step == PlayerStep.Standing || Player.Step == PlayerStep.Walk)
        {
            _aerialJumpsRemaining = 2;
        }

        ushort tapped = Game.Tapped;
        bool jumpTapped = (tapped & (ushort)Button.Cross) != 0;

        if (jumpTapped && _aerialJumpsRemaining > 0 &&
            (Player.Step == PlayerStep.AirJump || Player.Step == PlayerStep.Falling || Player.Step == PlayerStep.Jump))
        {
            _aerialJumpsRemaining--;
            Player.VelocityY = -0x00060000;
            Player.Step = PlayerStep.AirJump;
            m.WriteU16(Player.EffectActivatorAddr, 1);
        }
    }

    private const uint ClutRamAddr = 0x8006CBCCu; // g_Clut

    // RIC2.BIN ends its sprite sheet with {magic "RIC2", pointer}; the pointer leads to
    // {count, {palette id, 16 colours}...} and {count, {VRAM rect, pixels}...}: the coat's colours and
    // the whip-brandish arms that live in F_GAME2's part of VRAM. SOTN Ultimate's DRA applies them
    // after every load of the overlay (ModRicCostumeApply); the retail DRA does not, so it is done here.
    private const uint Ric2HeaderAddr = PlayerOverlayBase + 0x170ACu - 8u;
    private const uint PlayerOverlayEnd = 0x80180000u;

    private static void ApplyRichterCostume(IMemory m)
    {
        try
        {
            if (m.ReadU32(Ric2HeaderAddr) != RicSaturnCoatMagic) return;
            uint p = m.ReadU32(Ric2HeaderAddr + 4);
            if (p < PlayerOverlayBase || p >= PlayerOverlayEnd) return;

            Span<ushort> pal = stackalloc ushort[Palette.Colors];
            uint count = m.ReadU32(p);
            p += 4;
            if (count > 0x100) return;
            for (uint n = 0; n < count; n++)
            {
                int id = (int)m.ReadU32(p);
                p += 4;
                if (id < 0x100 || id >= 0x200) return;
                uint ram = ClutRamAddr + (uint)(id * Palette.Colors * 2);
                for (int i = 0; i < Palette.Colors; i++)
                {
                    pal[i] = m.ReadU16(p + (uint)(i * 2));
                    m.WriteU16(ram + (uint)(i * 2), pal[i]);
                }
                Palette.Write(id, pal);
                p += Palette.Colors * 2;
            }

            var gpu = RecompOne.Runtime.Runtime.Gpu;
            var backend = RecompOne.Runtime.Hle.GpuHle.Backend;
            uint rects = m.ReadU32(p);
            p += 4;
            if (rects > 0x100) return;
            for (uint n = 0; n < rects; n++)
            {
                int x = m.ReadU16(p), y = m.ReadU16(p + 2), w = m.ReadU16(p + 4), h = m.ReadU16(p + 6);
                p += 8;
                if (w <= 0 || h <= 0 || x + w > 1024 || y + h > 512 || p + (uint)(w * h * 2) > PlayerOverlayEnd) return;

                var pixels = new ushort[w * h];
                for (int i = 0; i < pixels.Length; i++)
                {
                    pixels[i] = m.ReadU16(p + (uint)(i * 2));
                    if (gpu != null) gpu.Shadow[x + i % w, y + i / w] = pixels[i];
                }
                if (RecompOne.Runtime.Hle.GpuHle.Active && backend is { Ready: true })
                    backend.WriteVram(x, y, w, h, pixels);
                p += (uint)((w * h + 1) / 2 * 4);
            }
            Console.WriteLine($"[CharacterSwitcher] Figurino do Richter (Saturn) aplicado: {count} paletas, {rects} blocos de VRAM.");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[CharacterSwitcher] Erro ao aplicar o figurino do Richter: {ex.Message}");
        }
    }

    /// <summary>
    /// Does what the game's CD loader does with BIN/F_GAME.BIN (Alucard) or BIN/F_GAME2.BIN (everyone
    /// else): the 0x200-0x3FF texture pages of the character / HUD sheet, then palettes 0x100-0x1FF,
    /// into g_Clut and into the VRAM row the game's own clut table gives each of them.
    /// </summary>
    public static void LoadGameGraphics(IMemory m, string filename)
    {
        var cd = RecompOne.Runtime.Runtime.Cd;
        if (cd == null) return;
        try
        {
            var data = cd.Fs.ReadFile(filename);
            if (data == null || data.Length < 0x42000) return;

            var gpu = RecompOne.Runtime.Runtime.Gpu;
            var backend = RecompOne.Runtime.Hle.GpuHle.Backend;

            // 32 chunks of 32x128 halfwords (CHUNK_X in VRAM)
            int[] chunkX = [
                0x200, 0x220, 0x200, 0x220, 0x240, 0x260, 0x240, 0x260,
                0x280, 0x2A0, 0x280, 0x2A0, 0x2C0, 0x2E0, 0x2C0, 0x2E0,
                0x300, 0x320, 0x300, 0x320, 0x340, 0x360, 0x340, 0x360,
                0x380, 0x3A0, 0x380, 0x3A0, 0x3C0, 0x3E0, 0x3C0, 0x3E0
            ];

            Span<ushort> chunk = stackalloc ushort[32 * 128];
            for (int i = 0; i < 32; i++)
            {
                int vx = chunkX[i];
                int vy = (i % 4 < 2) ? 256 : 384;
                int srcOff = i * 32 * 128 * 2;
                for (int p = 0; p < 32 * 128; p++)
                {
                    ushort pixel = (ushort)(data[srcOff + p * 2] | (data[srcOff + p * 2 + 1] << 8));
                    chunk[p] = pixel;
                    if (gpu != null) gpu.Shadow[vx + p % 32, vy + p / 32] = pixel;
                }
                if (RecompOne.Runtime.Hle.GpuHle.Active && backend is { Ready: true })
                    backend.WriteVram(vx, vy, 32, 128, chunk);
            }

            // 256 palettes at offset 0x40000 (ids 0x100..0x1FF)
            Span<ushort> pal = stackalloc ushort[Palette.Colors];
            for (int k = 0x100; k < 0x200; k++)
            {
                int palOff = 0x40000 + (k - 0x100) * 32;
                uint ram = ClutRamAddr + (uint)(k * Palette.Colors * 2);
                for (int j = 0; j < Palette.Colors; j++)
                {
                    pal[j] = (ushort)(data[palOff + j * 2] | (data[palOff + j * 2 + 1] << 8));
                    m.WriteU16(ram + (uint)(j * 2), pal[j]);
                }
                Palette.Write(k, pal);
            }
            Console.WriteLine($"[CharacterSwitcher] Gráficos de VRAM ({filename}) carregados.");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[CharacterSwitcher] Erro ao carregar gráficos {filename}: {ex.Message}");
        }
    }

    private static void DumpPlayerPalette(IMemory m, string label)
    {
        var e = Player.Entity;
        int id = e.PaletteId;
        string ram = "", vram = "";
        for (int i = 0; i < 16; i++)
            ram += $"{m.ReadU16(ClutRamAddr + (uint)((id * 16 + i) * 2)):X4} ";
        var gpu = RecompOne.Runtime.Runtime.Gpu;
        ushort clut = Palette.ClutOf(id);
        if (gpu != null)
            for (int i = 0; i < 16; i++) vram += $"{gpu.Shadow[((clut & 0x3F) << 4) + i, clut >> 6]:X4} ";
        Console.WriteLine($"[PAL_DUMP] {label}: palette=0x{e.Palette:X4} id=0x{id:X} unk40=0x{m.ReadU16(Player.DamagePaletteAddr):X4} clut=0x{clut:X} animSet=0x{m.ReadU16(e.Addr + 0x28):X} unk5A=0x{m.ReadU16(e.Addr + 0x5A):X}");
        Console.WriteLine($"[PAL_DUMP]   ram : {ram}");
        Console.WriteLine($"[PAL_DUMP]   vram: {vram}");
    }

    private static void DumpHudPrims(IMemory m, string label)
    {
        uint head = m.ReadU32(PlayerHudAddr + 4);
        uint colHead = m.ReadU32(PlayerHudAddr + 8);
        uint bossState = m.ReadU32(0x8003C744u);
        uint slide = m.ReadU32(0x80137980u);
        Console.WriteLine($"[HUD_DUMP] {label}: PlayableChar={m.ReadU32(Game.PlayableCharacterAddr)} head={head} colHead={colHead} BossState={bossState} Slide={slide}");
        if (head != 0)
        {
            uint primPtr = 0x80086FECu + head * 0x34u;
            for (int i = 0; i < 14 && primPtr != 0; i++, primPtr = m.ReadU32(primPtr))
            {
                short x0 = (short)m.ReadU16(primPtr + 8);
                short y0 = (short)m.ReadU16(primPtr + 10);
                short x1 = (short)m.ReadU16(primPtr + 0x14);
                short y1 = (short)m.ReadU16(primPtr + 0x16);
                byte u0 = m.ReadU8(primPtr + 0xC);
                byte v0 = m.ReadU8(primPtr + 0xD);
                ushort tpage = m.ReadU16(primPtr + 0x1A);
                ushort clut = m.ReadU16(primPtr + 0x0E);
                ushort prio = m.ReadU16(primPtr + 0x26);
                ushort flags = m.ReadU16(primPtr + 0x32);
                Console.WriteLine($"  Prim[{i}]: x0={x0},y0={y0} x1={x1},y1={y1} uv=({u0},{v0}) tpage=0x{tpage:X} clut=0x{clut:X} prio=0x{prio:X} drawMode=0x{flags:X}");
            }
        }
    }
}
