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
        ImGui.SetNextWindowSize(new Vector2(380, 520), ImGuiCond.FirstUseEver);
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

        bool isRichter = Player.IsRichter || Game.InPrologue();
        bool isMaria = Player.IsMaria;

        // Resource Status Bar
        if (isRichter)
        {
            int curHearts = Player.Hearts;
            int maxHearts = Player.HeartsMax;
            float heartRatio = maxHearts > 0 ? Math.Clamp((float)curHearts / maxHearts, 0f, 1f) : 0f;
            ImGui.Text($"Corações: {curHearts} / {maxHearts}");
            ImGui.ProgressBar(heartRatio, new Vector2(-1, 20), $"{curHearts}/{maxHearts} Corações");
        }
        else
        {
            int curMp = Player.Mp;
            int maxMp = Player.MpMax;
            float mpRatio = maxMp > 0 ? Math.Clamp((float)curMp / maxMp, 0f, 1f) : 0f;
            ImGui.Text($"MP: {curMp} / {maxMp}");
            ImGui.ProgressBar(mpRatio, new Vector2(-1, 20), $"{curMp}/{maxMp} MP");
        }

        ImGui.Spacing();

        bool dirty = false;
        dirty |= ImGui.Checkbox(Localization.T("spell.quick_cast_keys"), ref SpellWheel.QuickCastHotkeys);
        if (ImGui.IsItemHovered())
            ImGui.SetTooltip(Localization.T("spell.quick_cast_keys_hint"));

        dirty |= ImGui.Checkbox(isRichter ? "Corações Infinitos" : "Infinite Mana", ref SpellWheel.InfiniteMana);
        if (ImGui.IsItemHovered())
            ImGui.SetTooltip(isRichter ? "Item Crashes nunca gastam corações" : "Casting never costs MP");

        if (dirty) SpellWheel.Save();

        if (isRichter)
        {
            ImGui.SeparatorText("🗡 Habilidades & Item Crashes de Richter");
            ImGui.BeginChild("spell_list_child", Vector2.Zero, ImGuiChildFlags.Borders);

            (string name, int cost, Subweapon sub)[] richterMoves =
            [
                ("Hydro Storm [1] (Água Benta)", 15, Subweapon.HolyWater),
                ("Grand Cross [2] (Crucifixo)", 20, Subweapon.Cross),
                ("Thousand Blades [3] (Adaga)", 10, Subweapon.Dagger),
                ("Axe Rain [4] (Machado)", 10, Subweapon.Axe),
                ("Burning Bible [5] (Bíblia)", 15, Subweapon.Bible)
            ];

            for (int i = 0; i < richterMoves.Length; i++)
            {
                var (name, cost, sub) = richterMoves[i];
                bool canCast = SpellWheel.InfiniteMana || Player.Hearts >= cost;

                if (!canCast) ImGui.BeginDisabled();
                if (ImGui.Button($"{name} ({cost} Corações)##richter_{i}", new Vector2(-1, 36)))
                {
                    SpellWheel.TriggerQuickCast(i); // runs on the game thread
                }
                if (!canCast)
                {
                    ImGui.EndDisabled();
                    if (ImGui.IsItemHovered(ImGuiHoveredFlags.AllowWhenDisabled))
                        ImGui.SetTooltip("Corações insuficientes!");
                }
            }

            ImGui.Spacing();
            ImGui.SeparatorText("🥋 Artes Marciais de Richter");
            if (ImGui.Button("Blade Dash [6] (Avanço Cortante)##dash", new Vector2(-1, 36)))
            {
                SpellWheel.TriggerQuickCast(5);
            }
            if (ImGui.Button("Super Jump (Super Salto)##sjump", new Vector2(-1, 36)))
            {
                SpellWheel.TriggerQuickCast(6);
            }

            ImGui.EndChild();
        }
        else if (isMaria)
        {
            ImGui.SeparatorText("🌟 Invocações & Especiais da Maria (Saturn)");
            ImGui.BeginChild("spell_list_child", Vector2.Zero, ImGuiChildFlags.Borders);

            for (int i = 0; i < SpellWheel.MariaSpecials.Length; i++)
            {
                var special = SpellWheel.MariaSpecials[i];
                bool canCast = SpellWheel.InfiniteMana || Player.Mp >= special.MpCost;

                if (!canCast) ImGui.BeginDisabled();
                if (ImGui.Button($"{special.Name} [{i + 1}] ({special.MpCost} MP)##maria_{i}", new Vector2(-1, 36)))
                {
                    SpellWheel.TriggerQuickCast(i); // runs on the game thread
                }
                if (!canCast)
                {
                    ImGui.EndDisabled();
                    if (ImGui.IsItemHovered(ImGuiHoveredFlags.AllowWhenDisabled))
                        ImGui.SetTooltip(Localization.T("spell.not_enough_mp"));
                }
            }

            ImGui.EndChild();
        }
        else
        {
            ImGui.SeparatorText(Localization.T("spell.spell_list"));
            ImGui.BeginChild("spell_list_child", Vector2.Zero, ImGuiChildFlags.Borders);

            int curMp = Player.Mp;
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
                    SpellWheel.TriggerQuickCast(i);
                }

                if (!hasMp)
                {
                    ImGui.EndDisabled();
                    if (ImGui.IsItemHovered(ImGuiHoveredFlags.AllowWhenDisabled))
                        ImGui.SetTooltip(Localization.T("spell.not_enough_mp"));
                }
            }

            ImGui.EndChild();
        }

        IsOpen = open;
        ImGui.End();
    }
}
