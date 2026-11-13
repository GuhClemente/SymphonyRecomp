using System;
using System.Collections.Generic;
using RecompOne.Runtime.Context;
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

    // Each entry here was verified individually against the real per-spell check function in
    // Ghidra (CheckSummonSpiritInput, CheckTetraSpiritInput, CheckDarkMetamorphosisInput,
    // CheckHellfireInput, CheckSoulStealInput -- there is no shared dispatcher, they're five
    // separate functions). Two of the original addresses in this table (Summon Spirit, Dark
    // Metamorphosis) turned out to point at unrelated helper functions with no MP check or cast
    // logic at all -- not just a timing bug, the previous table was simply wrong for those two.
    public static readonly SpellInfo[] Spells =
    [
        new(Spell.SummonSpirit, "spell.summon_spirit", "Summon Spirit", 1, 0x8010FC50u),
        new(Spell.TetraSpirit, "spell.tetra_spirit", "Tetra Spirit", 3, 0x8010FCB8u),
        new(Spell.DarkMetamorphosis, "spell.dark_meta", "Dark Metamorphosis", 0, 0x8010FB68u),
        new(Spell.Hellfire, "spell.hellfire", "Hellfire", 2, 0x8010FB24u),
        new(Spell.SoulSteal, "spell.soul_steal", "Soul Steal", 5, 0x8010FBF4u),
    ];

    private static volatile int _pendingQuickCast = -1;

    static SpellWheel()
    {
        RecompOne.Runtime.Events.Event.AddListener<RecompOne.Runtime.Events.KeyboardEvent>(e =>
        {
            if (e.Pressed && !e.Repeat)
            {
                // Number keys 1-5 for instant quick casting if enabled
                if (QuickCastHotkeys)
                {
                    if (e.Key == (int)Silk.NET.Input.Key.Number1) _pendingQuickCast = 0; // Summon Spirit
                    else if (e.Key == (int)Silk.NET.Input.Key.Number2) _pendingQuickCast = 1; // Tetra Spirit
                    else if (e.Key == (int)Silk.NET.Input.Key.Number3) _pendingQuickCast = 2; // Dark Metamorphosis
                    else if (e.Key == (int)Silk.NET.Input.Key.Number4) _pendingQuickCast = 3; // Hellfire
                    else if (e.Key == (int)Silk.NET.Input.Key.Number5) _pendingQuickCast = 4; // Soul Steal
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

    // Safety net: reported live, casting via CastSpell can leave Player.Step stuck in the
    // spell's own cast-animation state (e.g. TetraSpirit) indefinitely -- control only came back
    // after a teleport (full stage reload) or casting a *different* spell (which forces a new
    // Step). Calling the spell's execute function directly (see CastSpell) clearly sets Player.Step
    // into the cast animation the same way the normal input path does, but something the normal
    // path also does to time that animation out again isn't happening here -- root cause still
    // under investigation. Until that's found, force Player.Step back to Standing if it's been
    // sitting in one of these four states for too long to be a real cast animation.
    const int StuckCastFrameLimit = 90; // 1.5s at 60fps -- generous for any real cast animation
    static int _stuckCastFrames;

    public static void Update(CpuContext c, IMemory m)
    {
        if (!Enabled || !Game.Available || !Game.InGame || Game.IsLoading) return;
        if (!Player.IsAlucard) return;

        int castIndex = _pendingQuickCast;
        _pendingQuickCast = -1;

        if (castIndex >= 0 && castIndex < Spells.Length)
        {
            CastSpell(Spells[castIndex], m);
        }

        var step = Player.Step;
        // Hellfire is a full multi-phase interactive teleport attack lasting ~240 frames (~4s)
        // with its own state machine and entity cleanup (func_8010E570).
        // It must NOT be clamped to the 90-frame simple cast animation timeout, otherwise
        // it gets aborted mid-teleport while invisible, leaving Alucard permanently invisible.
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

    public static int GetMpCost(IMemory m, SpellInfo info) =>
        m.ReadU8(MpCostTableBase + (uint)info.MpIndex * MpCostTableStride);

    // Casts the spell by calling the same three functions the game's own command-buffer state
    // machine calls once it recognizes the input: MP check-and-deduct, the spell's own execute
    // function, then mark-active. This was previously done by forging the "Square tapped" bit
    // each check function reads -- but that bit (0x80072eec) is itself derived fresh every frame
    // by EntityAlucard from the real pad state (0x80097490), not from anything writable ahead of
    // time, so the forged value only ever landed by luck, if at all. Calling these three
    // functions directly sidesteps that whole pipeline.
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

            // FUN_800fdc94 both checks and atomically deducts the cost -- skip it entirely under
            // Infinite Mana so nothing gets subtracted, rather than deducting and refunding after.
            if (GameApi.Call(MpCheckAndDeductAddr, (uint)info.MpIndex) == 0)
                return false; // MP changed between our check and the game's own deduct
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
