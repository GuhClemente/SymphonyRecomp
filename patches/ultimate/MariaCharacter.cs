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

    private static int _aerialJumpsRemaining = 2; // Allows triple jump
    private static bool _autoTransformFailed;
    private static bool _wasMaria;
    private static int _autoPadFrame = 0;
    private static bool _reachedCastle;

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

        Stages.StageArrived += OnStageArrived;

        if (TestMode)
        {
            UltimateOptions.CharacterMode = Environment.GetEnvironmentVariable("SOTN_TEST_START") == "alucard"
                ? CharacterSelection.Alucard
                : CharacterSelection.SaturnMaria;
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
                        // active low; the script's buttons, see CharacterTest
                        e.Buttons = (ushort)(e.Buttons & ~CharacterTest.Held);
                    }
                }
            });
        }
    }

    private static CharacterSelection _lastDesiredChar = CharacterSelection.Alucard;
    private static CharacterSelection _currentRichterVariant = CharacterSelection.RichterClassic;

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
        UltimateOptions.CharacterMode is CharacterSelection.RichterSaturnCoat or CharacterSelection.RichterBrownCoat;

    public static bool IsBrownRichter =>
        UltimateOptions.UltimateEnabled &&
        UltimateOptions.CharacterMode == CharacterSelection.RichterBrownCoat;

    public static void CycleNextCharacter()
    {
        if (!Game.Available || !Game.InGame || Game.IsLoading) return;
        var current = UltimateOptions.CharacterMode;
        CharacterSelection next = current switch
        {
            CharacterSelection.SaturnMaria => CharacterSelection.PSPMaria,
            CharacterSelection.PSPMaria => CharacterSelection.RichterClassic,
            CharacterSelection.RichterClassic => CharacterSelection.RichterSaturnCoat,
            CharacterSelection.RichterSaturnCoat => CharacterSelection.RichterBrownCoat,
            CharacterSelection.RichterBrownCoat => CharacterSelection.Alucard,
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
    // Alucard has no overlay of his own, but the same slot holds his sprite sheet (BIN/ARC_F.BIN, which
    // the game loads there when it starts as Alucard). Without it back he is drawn from the bytes of the
    // last character's binary: an olive figure with a purple cloak, or nothing at all.
    private const string AlucardSpritesFile = "BIN/ARC_F.BIN";

    // Preserved Alucard equipment so he never loses his weapons when switching characters
    private static uint _savedAlucardRight = (uint)HandItem.AlucardSword;
    private static uint _savedAlucardLeft = (uint)HandItem.AlucardShield;
    private static uint _savedAlucardWornRight = (uint)HandItem.AlucardSword;
    private static uint _savedAlucardWornLeft = (uint)HandItem.AlucardShield;

    public static void SwitchToCharacter(CpuContext c, IMemory m, CharacterSelection selection)
    {
        if (!Game.Available || !Game.InGame || Game.IsLoading || Game.InPrologue()) return;

        try
        {
            // Save Alucard's equipment before switching away from him
            if (Player.IsAlucard)
            {
                uint r = Inventory.RightHand, l = Inventory.LeftHand;
                uint wr = Inventory.GetWornEquipment(ItemSlot.RightHand), wl = Inventory.GetWornEquipment(ItemSlot.LeftHand);
                if (r != 0 || wr != 0)
                {
                    _savedAlucardRight = r != 0 ? r : wr;
                    _savedAlucardWornRight = wr != 0 ? wr : r;
                }
                if (l != 0 || wl != 0)
                {
                    _savedAlucardLeft = l != 0 ? l : wl;
                    _savedAlucardWornLeft = wl != 0 ? wl : l;
                }
            }

            UltimateOptions.CharacterMode = selection;
            _autoTransformFailed = false;
            // what the outgoing character left behind must not outlive its code
            m.WriteU32(TimeStopAddr, 0);
            DestroyPlayerEntities(c, m);

            // every Init (the overlays' and AluInit) puts the player somewhere of its own, which can be
            // inside the floor of the room she is in: the character keeps its place and facing
            uint player = Game.EntitiesAddr;
            uint posX = m.ReadU32(player), posY = m.ReadU32(player + 4);
            ushort facing = m.ReadU16(player + 0x14);

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
                    if (ok) ToastNotifications.ShowText("Maria Renard (PSP)", "Maria do PSP Dracula X Chronicles ativada!", null, 3f);
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
                    if (ok) ApplyRichterCostume(m, brownCoat: false);
                    if (ok) ToastNotifications.ShowText("Richter (Saturn)", "Richter com sobretudo longo azul do Saturn ativado!", null, 3f);
                    break;

                case CharacterSelection.RichterBrownCoat:
                    m.WriteU32(CostumeMagicAddr, RicSaturnCoatMagic);
                    LoadGameGraphics(m, "BIN/F_GAME2.BIN");
                    ok = LoadPlayerOverlay(c, m, "ric2", "RIC2.BIN", PlayableCharacter.Richter);
                    if (ok) ApplyRichterCostume(m, brownCoat: true);
                    if (ok) ToastNotifications.ShowText("Richter (Couro Marrom)", "Richter com sobretudo de couro marrom ativado!", null, 3f);
                    break;

                case CharacterSelection.Alucard:
                    m.WriteU32(Game.PlayableCharacterAddr, (uint)PlayableCharacter.Alucard);
                    Dispatcher.Load("dra");
                    Dispatcher.Load("w0_043");
                    Dispatcher.Load("w1_052");
                    RestoreAlucardWeapons(m);
                    LoadGameGraphics(m, "BIN/F_GAME.BIN");
                    Player.SubWeapon = 0;
                    // his sprite data lives where the other characters' binaries were loaded
                    LoadCharacterBinary(m, AlucardSpritesFile);
                    InitAlucard(c, m);

                    // Restore & equip Alucard's weapons
                    uint rh = _savedAlucardRight != 0 ? _savedAlucardRight : (uint)HandItem.AlucardSword;
                    uint lh = _savedAlucardLeft != 0 ? _savedAlucardLeft : (uint)HandItem.AlucardShield;
                    uint wr = _savedAlucardWornRight != 0 ? _savedAlucardWornRight : rh;
                    uint wl = _savedAlucardWornLeft != 0 ? _savedAlucardWornLeft : lh;

                    if (!Inventory.OwnsHandItem((int)rh)) Inventory.GrantHandItem((int)rh, 1);
                    if (!Inventory.OwnsHandItem((int)lh)) Inventory.GrantHandItem((int)lh, 1);
                    if (!Inventory.OwnsHandItem((int)wr)) Inventory.GrantHandItem((int)wr, 1);
                    if (!Inventory.OwnsHandItem((int)wl)) Inventory.GrantHandItem((int)wl, 1);

                    Inventory.SetWornEquipment(ItemSlot.RightHand, wr);
                    Inventory.SetWornEquipment(ItemSlot.LeftHand, wl);
                    Inventory.RightHand = rh;
                    Inventory.LeftHand = lh;

                    // Ensure weapon programs and VRAM sprite tiles are cleanly initialized
                    try
                    {
                        c.A0 = 0;
                        SoTN_dra.LoadWeaponPrg(c, m);
                        c.A0 = 0;
                        SoTN_dra.InitWeapon(c, m);

                        c.A0 = 1;
                        SoTN_dra.LoadWeaponPrg(c, m);
                        c.A0 = 1;
                        SoTN_dra.InitWeapon(c, m);
                    }
                    catch { }

                    ToastNotifications.ShowText("Alucard", "Alucard reativado!", null, 3f);
                    break;
            }

            if (!ok)
            {
                Console.WriteLine($"[CharacterSwitcher] Troca para {selection} falhou.");
                return;
            }

            if (selection is CharacterSelection.RichterClassic or CharacterSelection.RichterSaturnCoat or CharacterSelection.RichterBrownCoat)
                _currentRichterVariant = selection;

            if (hadMariaVoice && selection is not (CharacterSelection.SaturnMaria or CharacterSelection.PSPMaria))
                MariaLoader.RestoreRetailSounds(c, m, alucard: selection == CharacterSelection.Alucard);

            UnloadOtherPlayerOverlays(selection);

            m.WriteU32(player, posX);
            m.WriteU32(player + 4, posY);
            m.WriteU16(player + 0x14, facing);
            Player.VelocityX = 0;
            Player.VelocityY = 0;

            Player.Step = PlayerStep.Standing;
            Player.StepSub = 0;
            _wasMaria = Player.IsMaria;
            ReloadRoomGraphics(c, m);
            RedrawHud(c, m);
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[CharacterSwitcher] Erro na troca de personagem: {ex}");
        }
    }

    // A save room (layout flag 0x20) and a CD corridor (0x40) draw their tiles from graphics the engine
    // loads over part of the F_GAME area on the way in; F_GAME(2) loaded by the switch replaces them
    // (the room turns into rows of one brick). The engine's own loads put them back.
    private const uint RoomLayoutFlagsAddr = 0x8003C708u;
    private static void ReloadRoomGraphics(CpuContext c, IMemory m)
    {
        try
        {
            ushort flags = m.ReadU16(RoomLayoutFlagsAddr);
            if ((flags & 0x20) != 0) MariaLoader.Invoke(c, m, SoTN_dra.LoadGfxAsync, 3);
            if ((flags & 0x40) != 0) MariaLoader.Invoke(c, m, SoTN_dra.LoadGfxAsync, 4);
        }
        catch { }
    }

    // DRA's own init for Alucard, the counterpart of the overlays' Init: it rebuilds the player entity
    // (sprite bank, palette 0x8100, g_Player) and the palettes his sprite parts use, which in
    // F_GAME.BIN are only placeholders.
    private static void InitAlucard(CpuContext c, IMemory m) => MariaLoader.Invoke(c, m, SoTN_dra.AluInit);

    // g_unkGraphicsStruct.unk28: 4 while the Saturn Maria casts Byakko / Seiryuu (MarTimeStop)
    private const uint TimeStopAddr = 0x80097420u;
    private const uint TimeStopMaria = 4;

    // Entities 1-63 belong to the player (subweapons, summons, effects): their update functions
    // live in the player overlay that is about to be replaced.
    private const int FirstPlayerEntity = 1, FirstStageEntity = 64;

    private static void DestroyPlayerEntities(CpuContext c, IMemory m)
    {
        for (int i = FirstPlayerEntity; i < FirstStageEntity; i++)
            MariaLoader.Invoke(c, m, SoTN_dra.DestroyEntity_dra, Game.EntitiesAddr + (uint)(i * Entity.Stride));
    }

    private static readonly (CharacterSelection Who, string Overlay)[] PlayerOverlays =
    [
        (CharacterSelection.SaturnMaria, MariaLoader.OverlayName),
        (CharacterSelection.PSPMaria, "maria2"),
        (CharacterSelection.RichterClassic, "ric"),
        (CharacterSelection.RichterSaturnCoat, "ric2"),
        (CharacterSelection.RichterBrownCoat, "ric2"),
    ];

    // The overlays share one address range and none of them covers all of another, so the dispatcher
    // keeps the old one's functions wherever the new one has none: a stale pointer would then run
    // code that is no longer in memory. Only the active character's overlay stays mapped.
    private static void UnloadOtherPlayerOverlays(CharacterSelection active)
    {
        string? activeOverlay = active switch
        {
            CharacterSelection.SaturnMaria => MariaLoader.OverlayName,
            CharacterSelection.PSPMaria => "maria2",
            CharacterSelection.RichterClassic => "ric",
            CharacterSelection.RichterSaturnCoat or CharacterSelection.RichterBrownCoat => "ric2",
            _ => null
        };
        foreach (var (who, overlay) in PlayerOverlays)
            if (overlay != activeOverlay) Dispatcher.Unload(overlay);
    }

    /// <summary>
    /// While she casts Byakko or Seiryuu the game is halted (MarTimeStop) and only her own code runs.
    /// SOTN Ultimate's DRA does that in its halted engine step; the retail one only runs the player
    /// there during the prologue, so she would never get to end the cast and everything stayed frozen.
    /// The same two calls the retail prologue branch makes: the overlay's Main and UpdatePlayerEntities.
    /// </summary>
    private static void RunMariaDuringTimeStop(CpuContext c, IMemory m)
    {
        uint main = m.ReadU32(PlayerOverlayBase), entities = m.ReadU32(PlayerOverlayBase + 8);
        if (!Dispatcher.CanCall(main) || !Dispatcher.CanCall(entities)) return;
        MariaLoader.Invoke(c, m, (cc, mm) => Dispatcher.Call(cc, mm, main));
        MariaLoader.Invoke(c, m, (cc, mm) => Dispatcher.Call(cc, mm, entities));
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
    // A stage loaded by Stages.Load (fast travel) wipes the player entity and frees every primitive, and
    // the engine does not run the character's init again. Alucard's update then follows the freed
    // primitives of his own effects (func_801093C4) and the overlay characters lose their sprite banks
    // (they keep moving, invisible). Alucard's update is skipped while the stage loads, and on arrival
    // the active character's init runs again, in place.
    public static bool PlayerReady(CpuContext c, IMemory m) => !Stages.StageLoading;

    private static void OnStageArrived(CpuContext c, IMemory m)
    {
        uint player = Game.EntitiesAddr;
        uint posX = m.ReadU32(player), posY = m.ReadU32(player + 4);
        ushort facing = m.ReadU16(player + 0x14);

        if (m.ReadU32(Game.PlayableCharacterAddr) == (uint)PlayableCharacter.Alucard)
        {
            InitAlucard(c, m);
        }
        else
        {
            uint init = m.ReadU32(PlayerOverlayBase + 4);
            if (init < PlayerOverlayBase) return;
            MariaLoader.Invoke(c, m, (cc, mm) => Dispatcher.Call(cc, mm, init));
            if (IsSaturnRichter) ApplyRichterCostume(m, IsBrownRichter);
        }

        m.WriteU32(player, posX);
        m.WriteU32(player + 4, posY);
        m.WriteU16(player + 0x14, facing);
        m.WriteU32(player + 8, 0);
        m.WriteU32(player + 0xC, 0);
        Player.Step = PlayerStep.Standing;
        Player.StepSub = 0;
    }

    // A stage loaded by Stages.Load frees every primitive and the HUD is not built again: its primitive
    // indices are left pointing at freed, reused primitives, and the HUD functions then follow garbage
    // pointers. Pre-hook of DrawHud, DrawHudSubweapon and the two Richter ones: nothing is drawn while
    // such a load is in flight, and the HUD is built again on the first frame after it.
    private const uint EngineStepAddr = 0x8003C9A4u;
    private static bool _hudStale, _hudRebuilding;
    private static int _hudLoadSeen;

    public static bool HudReady(CpuContext c, IMemory m)
    {
        if (_hudRebuilding) return true;
        if (Stages.StageLoadCount != _hudLoadSeen) _hudStale = true;
        if (Stages.StageLoading) return false;
        if (!_hudStale) return true;
        if (Game.IsLoading || !Game.InGame || m.ReadU32(EngineStepAddr) == 3) return false;

        // the old indices belong to someone else by now: forget them instead of freeing them
        m.WriteU32(PlayerHudAddr + 4, 0);
        m.WriteU32(PlayerHudAddr + 8, 0);
        RedrawHud(c, m);
        return false;
    }

    public static void RedrawHud(CpuContext c, IMemory m)
    {
        _hudStale = false;
        _hudLoadSeen = Stages.StageLoadCount;
        _hudRebuilding = true;
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
            MariaSaturnHud.Reset();

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
        finally
        {
            _hudRebuilding = false;
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

        // the Saturn Maria has her own HUD when its art is available (SOTN Ultimate disc in disc/)
        if (!_pspMariaLoaded && UltimateOptions.CharacterMode != CharacterSelection.PSPMaria &&
            MariaSaturnHud.Draw(m, margin)) return;

        // otherwise the Richter HUD stays; its plate is his name, which neither Maria shows
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
        if (!Game.Available || !Game.InGame || Game.IsLoading || Stages.StageLoading) return;

        bool isMaria = Player.IsMaria;

        if (_wasMaria && !isMaria) MariaLoader.OnMariaLeft(m);
        _wasMaria = isMaria;

        // Auto-initialize character from UltimateOptions on boot or character change
        CharacterSelection desired = UltimateOptions.CharacterMode;
        if (desired != _lastDesiredChar)
        {
            _lastDesiredChar = desired;
            _autoTransformFailed = false;
        }

        if (!_autoTransformFailed && !Game.InPrologue() && Player.HasControl)
        {
            bool needsSwitch = false;
            switch (desired)
            {
                case CharacterSelection.SaturnMaria:
                    needsSwitch = !isMaria || Array.IndexOf(Dispatcher.ActiveNames, "maria") < 0;
                    break;
                case CharacterSelection.PSPMaria:
                    needsSwitch = !isMaria || Array.IndexOf(Dispatcher.ActiveNames, "maria2") < 0;
                    break;
                case CharacterSelection.RichterClassic:
                    needsSwitch = !Player.IsRichter || Array.IndexOf(Dispatcher.ActiveNames, "ric") < 0;
                    break;
                case CharacterSelection.RichterSaturnCoat:
                    needsSwitch = !Player.IsRichter || Array.IndexOf(Dispatcher.ActiveNames, "ric2") < 0 || _currentRichterVariant != CharacterSelection.RichterSaturnCoat;
                    break;
                case CharacterSelection.RichterBrownCoat:
                    needsSwitch = !Player.IsRichter || Array.IndexOf(Dispatcher.ActiveNames, "ric2") < 0 || _currentRichterVariant != CharacterSelection.RichterBrownCoat;
                    break;
                case CharacterSelection.Alucard:
                    needsSwitch = !Player.IsAlucard;
                    break;
            }

            if (needsSwitch)
            {
                _autoTransformFailed = true; // prevent repeated triggers every frame
                SwitchToCharacter(c, m, desired);
            }
        }

        // Richter Saturn Coat Look
        if (Player.IsRichter && IsSaturnRichter)
            m.WriteU32(CostumeMagicAddr, RicSaturnCoatMagic);

        if (IsSaturnMaria && m.ReadU32(TimeStopAddr) == TimeStopMaria) RunMariaDuringTimeStop(c, m);

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
            // the script starts once she has walked into the castle and the player has control
            if (!_reachedCastle && Player.PosX >= 80 && Player.PosY >= 130 && Player.HasControl)
            {
                _reachedCastle = true;
                Console.WriteLine($"[MariaTest] No castelo (PosX={Player.PosX}, PosY={Player.PosY}): iniciando o roteiro de teste.");
            }
            if (_reachedCastle) CharacterTest.Tick(c, m);
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

    private static void ApplyRichterCostume(IMemory m, bool brownCoat = false)
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
                    if (brownCoat)
                    {
                        // Substitui tons de azul do sobretudo por couro marrom envelhecido (estilo Simon / Trevor)
                        if (i == 7) pal[i] = 0x0887;       // Couro marrom escuro (sombra)
                        else if (i == 14) pal[i] = 0x0D0F; // Couro marrom médio (corpo do casaco)
                        else if (i == 6) pal[i] = 0x1595;  // Couro marrom iluminado (dobras)
                    }
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
        if (cd == null || !cd.Fs.Exists(filename)) return;
        try
        {
            var data = cd.Fs.ReadFile(filename);
            if (data == null || data.Length < 0x42000) return;

            var gpu = RecompOne.Runtime.Runtime.Gpu;
            var backend = RecompOne.Runtime.Hle.GpuHle.Backend;

            // 16 chunks of 32x128 halfwords in VRAM (y=256 and y=384):
            // - Chunks 0..3 (0x200..0x23F): HUD, life gauge, numerals, Maria HUD art
            // - Chunks 4..7 (0x240..0x27F): Richter whip, brandish arms, subweapons & weapon art
            // - Chunks 8..11 (0x280..0x2BF): Subweapon projectiles
            // - Chunks 12..15 (0x2C0..0x2FF): Common effects & items
            // Chunks 16..31 (0x300..0x3FF) are left to stage/save room loaders to prevent room tile corruption.
            int[] chunkX = [
                0x200, 0x220, 0x200, 0x220,
                0x240, 0x260, 0x240, 0x260,
                0x280, 0x2A0, 0x280, 0x2A0,
                0x2C0, 0x2E0, 0x2C0, 0x2E0
            ];

            Span<ushort> chunk = stackalloc ushort[32 * 128];
            for (int i = 0; i < chunkX.Length; i++)
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
