using System.Numerics;
using ImGuiNET;
using RecompOne.Runtime.Config;
using RecompOne.Runtime.Host.Window;

namespace Recompiled;

public sealed class QualityOfLifePanel : IPanel
{
    public string Name => "Quality Of Life Options";
    public string TitleKey => "panel.qol";
    public bool IsOpen { get; set; }

    public void Draw()
    {
        ImGui.SetNextWindowSize(new Vector2(320, 420), ImGuiCond.FirstUseEver);
        bool open = IsOpen;
        if (!ImGui.Begin(this.Title(), ref open))
        {
            IsOpen = open;
            ImGui.End();
            return;
        }

        ImGui.SeparatorText(Localization.T("qol.toggles"));

        /* Toggles */
        Toggle(QualityOfLifeOptions.ColorBlind, "qol.color_blind");
        Toggle(QualityOfLifeOptions.RemoveFlashing, "qol.remove_flashing");
        Toggle(QualityOfLifeOptions.BugFixes, "qol.bug_fixes");
        Toggle(QualityOfLifeOptions.ClearFile, "qol.clear_file");
        Toggle(QualityOfLifeOptions.AntiFreeze, "qol.anti_freeze");
        Toggle(QualityOfLifeOptions.InfiniteWingSmash, "qol.infinite_wing_smash");
        Toggle(QualityOfLifeOptions.UseEasySpellInput, "qol.easy_spell_input");
        Toggle(QualityOfLifeOptions.IncreaseInvincibilityFrames, "qol.invincibility_frames");
        Toggle(QualityOfLifeOptions.SkipPrologue, "qol.skip_prologue");

        /* Enhancements */
        ImGui.SeparatorText(Localization.T("qol.enhancements"));
        Toggle(QualityOfLifeOptions.RestoreFairySong, "qol.fairy_song");

        bool dirty = false;
        dirty |= ToggleManual("qol.quick_weapon_swap", ref QuickWeaponSwap.Enabled);
        dirty |= ToggleManual("qol.alucard_dash", ref AlucardDash.Enabled);
        dirty |= ToggleManual("qol.skip_cd_rooms", ref SkipCdRooms.Enabled);
        dirty |= ToggleManual("qol.richter_qol", ref RichterQoL.Enabled);

        if (dirty) QualityOfLife.Save();

        IsOpen = open;
        ImGui.End();
    }

    static void Toggle(Option<bool> option, string key)
    {
        SettingsWidgets.Toggle(option, key, key + ".hint");
    }

    static bool ToggleManual(string key, ref bool value)
    {
        bool changed = ImGui.Checkbox(Localization.T(key), ref value);
        ImGui.SetItemTooltip(Localization.T(key + ".hint"));
        return changed;
    }
}
