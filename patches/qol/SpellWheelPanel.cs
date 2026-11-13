using System;
using System.Numerics;
using ImGuiNET;
using RecompOne.Runtime.Host.Window;
using Sotn;

namespace Recompiled;

public sealed class SpellWheelPanel : IPanel
{
    public string Name => "Spell Quick Cast";
    public string TitleKey => "panel.spell_wheel";
    public bool IsOpen { get; set; }

    public void Draw()
    {
        ImGui.SetNextWindowSize(new Vector2(360, 480), ImGuiCond.FirstUseEver);
        bool open = IsOpen;
        if (!ImGui.Begin(this.Title(), ref open))
        {
            IsOpen = open;
            ImGui.End();
            return;
        }

        var m = RecompOne.Runtime.Runtime.Mem;
        if (m == null || !Cheats.InPlay())
        {
            ImGui.TextWrapped(Localization.T("common.not_in_play"));
            IsOpen = open;
            ImGui.End();
            return;
        }

        // MP Status Bar
        int curMp = Player.Mp;
        int maxMp = Player.MpMax;
        float mpRatio = maxMp > 0 ? Math.Clamp((float)curMp / maxMp, 0f, 1f) : 0f;

        ImGui.Text($"MP: {curMp} / {maxMp}");
        ImGui.ProgressBar(mpRatio, new Vector2(-1, 20), $"{curMp}/{maxMp} MP");
        ImGui.Spacing();

        bool dirty = false;
        dirty |= ImGui.Checkbox(Localization.T("spell.quick_cast_keys"), ref SpellWheel.QuickCastHotkeys);
        if (ImGui.IsItemHovered())
            ImGui.SetTooltip(Localization.T("spell.quick_cast_keys_hint"));

        dirty |= ImGui.Checkbox("Infinite Mana", ref SpellWheel.InfiniteMana);
        if (ImGui.IsItemHovered())
            ImGui.SetTooltip("Casting never costs MP");

        if (dirty) SpellWheel.Save();

        ImGui.SeparatorText(Localization.T("spell.spell_list"));

        ImGui.BeginChild("spell_list_child", Vector2.Zero, ImGuiChildFlags.Borders);

        for (int i = 0; i < SpellWheel.Spells.Length; i++)
        {
            var info = SpellWheel.Spells[i];
            bool hasSpell = Inventory.HasSpell(info.Spell);
            int mpCost = SpellWheel.GetMpCost(m, info);
            bool hasMp = SpellWheel.InfiniteMana || curMp >= mpCost;

            string name = Localization.T(info.NameKey);
            if (string.IsNullOrEmpty(name) || name.StartsWith("spell."))
                name = info.DefaultName;

            string hotkeyLabel = i < 6 ? $" [{i + 1}]" : "";
            string buttonLabel = $"{name}{hotkeyLabel} ({mpCost} MP)##spell_{i}";

            if (!hasMp)
            {
                ImGui.BeginDisabled();
            }

            if (ImGui.Button(buttonLabel, new Vector2(-1, 36)))
            {
                SpellWheel.CastSpell(info, m);
            }

            if (!hasMp)
            {
                ImGui.EndDisabled();
                if (ImGui.IsItemHovered(ImGuiHoveredFlags.AllowWhenDisabled))
                    ImGui.SetTooltip(Localization.T("spell.not_enough_mp"));
            }
        }

        ImGui.EndChild();

        IsOpen = open;
        ImGui.End();
    }
}
