using System;
using System.Collections.Generic;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Events;
using RecompOne.Runtime.Host.Window;
using RecompOne.Runtime.Memory;
using Sotn;

namespace Recompiled;

public static class SpellWheel
{
    public static bool Enabled = true;
    public static bool QuickCastHotkeys = true;
    public static bool InfiniteMana = false;

    // The game's own per-spell MP cost table (one byte per spell, indexed by MpIndex below) and
    // the two shared helpers every spell's own check function calls once its input is confirmed:
    // FUN_800fdc94(mpIndex) checks AND atomically deducts the cost, FUN_800fdce0(mpIndex) marks
    // the spell active for whatever tracks currently-playing effects. Confirmed against all 5
    // spells' real functions in Ghidra.
    const uint MpCostTableBase = 0x800A841C;
    const uint MpCostTableStride = 0x1C;
    const uint MpCheckAndDeductAddr = 0x800FDC94;
    const uint MarkActiveAddr = 0x800FDCE0;

    public record SpellInfo(
        Spell Spell,
        string NameKey,
        string DefaultName,
        int MpIndex,
        uint ExecuteAddr,
        int RequiredForm = 0 // 0 = Human, 1 = Bat, 2 = Wolf, 3 = Sword Familiar
    );

    public static readonly SpellInfo[] Spells =
    [
        new(Spell.SummonSpirit, "spell.summon_spirit", "Summon Spirit", 1, 0x8010FC50u),
        new(Spell.TetraSpirit, "spell.tetra_spirit", "Tetra Spirit", 3, 0x8010FCB8u),
        new(Spell.DarkMetamorphosis, "spell.dark_meta", "Dark Metamorphosis", 0, 0x8010FB68u),
        new(Spell.Hellfire, "spell.hellfire", "Hellfire", 2, 0x8010FB24u),
        new(Spell.SoulSteal, "spell.soul_steal", "Soul Steal", 5, 0x8010FBF4u),
    ];

    private static int _pendingQuickCast = -1; // written by the UI / keyboard threads, taken by the game thread
    public static void TriggerQuickCast(int index) => System.Threading.Volatile.Write(ref _pendingQuickCast, index);

    private static ushort _simulatedPadMask = 0xFFFF;
    private static int _simulatedPadFrames = 0;

    public static void PressButton(Button btn, int frames = 4)
    {
        _simulatedPadMask = (ushort)~(ushort)btn;
        _simulatedPadFrames = frames;
    }

    static SpellWheel()
    {
        Event.AddListener<PadReadEvent>(e =>
        {
            if (e.Port == 0 && _simulatedPadFrames > 0)
            {
                e.Buttons = (ushort)(e.Buttons & _simulatedPadMask);
                _simulatedPadFrames--;
            }
        });

        Event.AddListener<KeyboardEvent>(e =>
        {
            if (e.Pressed && !e.Repeat)
            {
                // Number keys 1-6 for instant quick casting if enabled
                if (QuickCastHotkeys)
                {
                    int num = -1;
                    if (e.Key == (int)Silk.NET.Input.Key.Number1) num = 0;
                    else if (e.Key == (int)Silk.NET.Input.Key.Number2) num = 1;
                    else if (e.Key == (int)Silk.NET.Input.Key.Number3) num = 2;
                    else if (e.Key == (int)Silk.NET.Input.Key.Number4) num = 3;
                    else if (e.Key == (int)Silk.NET.Input.Key.Number5) num = 4;
                    else if (e.Key == (int)Silk.NET.Input.Key.Number6) num = 5;

                    if (num >= 0) TriggerQuickCast(num);
                }
            }
        });
    }

    public static void Load()
    {
        var v = RecompOne.Runtime.Runtime.View;
        Enabled = v.GetBool("QolSpellWheel", true);
        QuickCastHotkeys = v.GetBool("QolSpellQuickCastHotkeys", true);
        InfiniteMana = v.GetBool("QolSpellInfiniteMana", false);
    }

    public static void Save()
    {
        var v = RecompOne.Runtime.Runtime.View;
        v.SetBool("QolSpellWheel", Enabled);
        v.SetBool("QolSpellQuickCastHotkeys", QuickCastHotkeys);
        v.SetBool("QolSpellInfiniteMana", InfiniteMana);
        RecompOne.Runtime.Runtime.SaveView();
    }

    const int StuckCastFrameLimit = 90; // 1.5s at 60fps -- generous for any real cast animation
    static int _stuckCastFrames;

    public static void Update(CpuContext c, IMemory m)
    {
        if (!Enabled || !Game.Available || !Game.InGame || Game.IsLoading) return;

        int castIndex = System.Threading.Interlocked.Exchange(ref _pendingQuickCast, -1);

        if (castIndex >= 0)
        {
            if (Player.IsRichter || Game.InPrologue())
            {
                TriggerRichterCast(castIndex);
            }
            else if (Player.IsMaria)
            {
                TriggerMariaCast(castIndex);
            }
            else if (Player.IsAlucard && castIndex < Spells.Length)
            {
                CastSpell(Spells[castIndex], m);
            }
        }

        if (Player.IsAlucard && !Game.InPrologue())
        {
            var step = Player.Step;
            bool inCastAnim = step is PlayerStep.DarkMetamorphosis or PlayerStep.SummonSpirit
                or PlayerStep.TetraSpirit;

            if (inCastAnim)
            {
                _stuckCastFrames++;
                if (_stuckCastFrames > StuckCastFrameLimit)
                {
                    Console.WriteLine($"[SpellWheel] Player.Step stuck in {step} for {_stuckCastFrames} frames, forcing back to Standing");
                    Player.Step = PlayerStep.Standing;
                    _stuckCastFrames = 0;
                }
            }
            else
            {
                _stuckCastFrames = 0;
            }
        }
    }

    public static void TriggerRichterCast(int index)
    {
        switch (index)
        {
            case 0: CastRichterCrash(Subweapon.HolyWater, "Hydro Storm", 15); break;
            case 1: CastRichterCrash(Subweapon.Cross, "Grand Cross", 20); break;
            case 2: CastRichterCrash(Subweapon.Dagger, "Thousand Blades", 10); break;
            case 3: CastRichterCrash(Subweapon.Axe, "Axe Rain", 10); break;
            case 4: CastRichterCrash(Subweapon.Bible, "Burning Bible", 15); break;
            case 5: CastRichterBladeDash(); break;
            case 6: CastRichterSuperJump(); break;
        }
    }

    public static bool CastRichterCrash(Subweapon subweapon, string name, int heartCost)
    {
        if (!Game.InGame) return false;
        if (!InfiniteMana && Player.Hearts < heartCost)
        {
            ToastNotifications.ShowText(name, $"Corações insuficientes! ({Player.Hearts}/{heartCost})", null, 2f);
            return false;
        }
        if (!InfiniteMana)
        {
            Player.Hearts = Math.Max(0, Player.Hearts - heartCost);
        }
        Player.SubWeapon = (int)subweapon;
        PressButton(Button.Triangle, 6);
        ToastNotifications.ShowText(name, InfiniteMana ? "Item Crash! (Infinito)" : $"Corações -{heartCost} | Item Crash!", null, 2f);
        return true;
    }

    public static bool CastRichterBladeDash()
    {
        if (!Game.InGame) return false;
        Player.VelocityX = Player.FacingLeft ? -0x00080000 : 0x00080000;
        GameApi.PlaySfx(0x6FC); // Richter attack yell
        ToastNotifications.ShowText("Blade Dash", "Avanço Cortante executado!", null, 2f);
        return true;
    }

    public static bool CastRichterSuperJump()
    {
        if (!Game.InGame) return false;
        Player.VelocityY = -0x000A0000;
        Player.Step = PlayerStep.AirJump;
        GameApi.PlaySfx(0x6FA); // High jump sound
        ToastNotifications.ShowText("Super Jump", "Super Salto executado!", null, 2f);
        return true;
    }

    // Maria's command specials, in hotkey order. Move is the overlay's own index (mar_sp_cost /
    // mar_sp_anims in mods/sotn-ultimate/src/maria_saturn/mar_specials.h).
    public readonly record struct MariaSpecial(string Name, int Move, int MpCost);

    public static readonly MariaSpecial[] MariaSpecials =
    [
        new("Soco Guardião (Guardian Knuckle)", 0, 40),
        new("Suzaku (Pássaro Carmesim)", 4, 60),
        new("Genbu (Tartaruga Sagrada)", 1, 80),
        new("Byakko (Tigre Branco)", 2, 100),
        new("Seiryuu (Dragão Azul)", 3, 120),
        new("As Quatro Bestas Sagradas", 5, 150),
    ];

    // MarSetSpecial(n) in MARIA.BIN: animation, step 0x30 + n, her voice and the beast's factory
    const uint MarSetSpecialAddr = 0x8016CB10u;
    const uint PlayerAttackBusyAddr = 0x80072F66u; // g_Player.unk46
    const uint PlayerCeilingAddr = 0x80072F92u;    // g_Player.unk72
    const ushort MariaStepCrouch = 2;              // PL_S_STAND / PL_S_WALK / PL_S_CROUCH

    public static void TriggerMariaCast(int index)
    {
        if (index >= 0 && index < MariaSpecials.Length) CastMariaSpecial(MariaSpecials[index]);
    }

    // Game thread only. Does what MarCheckCombo does once her pad command is recognised: same
    // conditions, same MP cost, then the overlay's own MarSetSpecial.
    public static bool CastMariaSpecial(MariaSpecial special)
    {
        var c = RecompOne.Runtime.Runtime.Cpu;
        var m = RecompOne.Runtime.Runtime.Mem;
        if (c == null || m == null || !Game.InGame || !Player.IsMaria) return false;

        if ((ushort)Player.Step > MariaStepCrouch || !Player.HasControl ||
            m.ReadU16(PlayerAttackBusyAddr) != 0 || m.ReadU16(PlayerCeilingAddr) != 0)
        {
            ToastNotifications.ShowText(special.Name, "Só é possível invocar parada, andando ou agachada.", null, 2f);
            return false;
        }

        if (!InfiniteMana)
        {
            if (Player.Mp < special.MpCost)
            {
                ToastNotifications.ShowText(special.Name, $"MP insuficiente! ({Player.Mp}/{special.MpCost})", null, 2f);
                return false;
            }
            Player.Mp -= special.MpCost;
        }

        // MarSetSpecial works on g_CurrentEntity, which is only the player during her own update
        uint savedEntity = m.ReadU32(Game.CurrentEntityAddr);
        m.WriteU32(Game.CurrentEntityAddr, Game.EntitiesAddr);
        try
        {
            GameApi.Call(c, m, MarSetSpecialAddr, (uint)special.Move);
        }
        finally
        {
            m.WriteU32(Game.CurrentEntityAddr, savedEntity);
        }

        ToastNotifications.ShowText(special.Name, InfiniteMana ? "Invocação! (Infinito)" : $"MP -{special.MpCost} | Invocação!", null, 2.5f);
        return true;
    }

    public static int GetMpCost(IMemory m, SpellInfo info) =>
        m.ReadU8(MpCostTableBase + (uint)info.MpIndex * MpCostTableStride);

    public static bool CastSpell(SpellInfo info, IMemory m)
    {
        if (!Game.InGame || !Player.IsAlucard) return false;

        int mpCost = GetMpCost(m, info);
        if (!InfiniteMana)
        {
            if (Player.Mp < mpCost)
            {
                ToastNotifications.ShowText(
                    Localization.T("spell.mp_low_title"),
                    $"{Localization.T("spell.mp_low_msg")} ({Player.Mp}/{mpCost} MP)",
                    null,
                    2.0f);
                return false;
            }

            if (GameApi.Call(MpCheckAndDeductAddr, (uint)info.MpIndex) == 0)
                return false;
        }

        GameApi.Call(info.ExecuteAddr);
        GameApi.Call(MarkActiveAddr, (uint)info.MpIndex);

        string spellName = Localization.T(info.NameKey);
        if (string.IsNullOrEmpty(spellName) || spellName.StartsWith("spell."))
            spellName = info.DefaultName;

        ToastNotifications.ShowText(
            spellName,
            InfiniteMana ? "Cast! (Infinite Mana)" : $"MP -{mpCost} | Cast!",
            null,
            2.0f);

        return true;
    }
}
