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

    private static volatile int _pendingQuickCast = -1;
    public static void TriggerQuickCast(int index) => _pendingQuickCast = index;

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

                    if (num >= 0)
                    {
                        _pendingQuickCast = num;
                    }
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

        int castIndex = _pendingQuickCast;
        _pendingQuickCast = -1;

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

    public static void TriggerMariaCast(int index)
    {
        switch (index)
        {
            case 0: CastMariaSpecial("Soco Guardião (Guardian Knuckle)", 40, 0x8A1); break;
            case 1: CastMariaSpecial("Suzaku (Pássaro Carmesim)", 60, 0x8A2); break;
            case 2: CastMariaSpecial("Genbu (Tartaruga Sagrada)", 80, 0x887); break;
            case 3: CastMariaSpecial("Byakko (Tigre Branco)", 100, 0x888); break;
            case 4: CastMariaSpecial("Seiryuu (Dragão Azul)", 120, 0x889); break;
            case 5: CastMariaSpecial("As Quatro Bestas Sagradas", 150, 0x87E); break;
        }
    }

    public static bool CastMariaSpecial(string name, int mpCost, int sfxId)
    {
        if (!Game.InGame) return false;
        if (!InfiniteMana && Player.Mp < mpCost)
        {
            ToastNotifications.ShowText(name, $"MP insuficiente! ({Player.Mp}/{mpCost})", null, 2f);
            return false;
        }
        if (!InfiniteMana)
        {
            Player.Mp = Math.Max(0, Player.Mp - mpCost);
        }
        GameApi.PlaySfx(sfxId);
        ToastNotifications.ShowText(name, InfiniteMana ? "Invocação Ativada! (Infinito)" : $"MP -{mpCost} | Invocação Ativada!", null, 2.5f);
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
