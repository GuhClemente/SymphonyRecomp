using System;
using System.Numerics;
using ImGuiNET;
using RecompOne.Runtime.Host.Window;
using Sotn;

namespace Recompiled;

public sealed class AchievementsPanel : IPanel
{
    public string Name => "Achievements & Trophies";
    public string TitleKey => "panel.achievements";
    public bool IsOpen { get; set; }

    private static string _categoryFilter = "All";

    public void Draw()
    {
        ImGui.SetNextWindowSize(new Vector2(500, 580), ImGuiCond.FirstUseEver);
        bool open = IsOpen;
        if (!ImGui.Begin(this.Title(), ref open))
        {
            IsOpen = open;
            ImGui.End();
            return;
        }

        int unlocked = Achievements.UnlockedCount;
        int total = Achievements.TotalCount;
        int pts = Achievements.UnlockedPoints;
        int maxPts = Achievements.TotalPoints;
        float progress = total > 0 ? (float)unlocked / total : 0f;

        // Header
        ImGui.TextColored(new Vector4(1f, 0.85f, 0.2f, 1f), $"Progresso Total: {unlocked} / {total} Conquistas ({pts} / {maxPts} Pontos)");
        ImGui.ProgressBar(progress, new Vector2(-1, 22), $"{progress * 100:0.0}% Concluído");
        ImGui.Spacing();

        // Filters
        string[] cats = ["All", "Story", "Combat", "Exploration", "Mastery", "Items", "Secret"];
        for (int i = 0; i < cats.Length; i++)
        {
            if (i > 0) ImGui.SameLine();
            bool selected = _categoryFilter == cats[i];
            if (selected) ImGui.PushStyleColor(ImGuiCol.Button, new Vector4(0.2f, 0.6f, 0.9f, 1f));
            if (ImGui.Button(cats[i])) _categoryFilter = cats[i];
            if (selected) ImGui.PopStyleColor();
        }

        ImGui.Separator();

        ImGui.BeginChild("achievements_list_child", Vector2.Zero, ImGuiChildFlags.Borders);

        foreach (var a in Achievements.All)
        {
            if (_categoryFilter != "All" && a.Category != _categoryFilter) continue;

            bool isUnlocked = Achievements.IsUnlocked(a.Id);
            string date = Achievements.GetUnlockDate(a.Id);

            ImGui.PushID(a.Id);

            if (isUnlocked)
            {
                ImGui.TextColored(new Vector4(1f, 0.85f, 0.2f, 1f), $"★ {a.Title} (+{a.Points} pts)");
                ImGui.SameLine();
                ImGui.TextDisabled($"[{a.Category}]");
                ImGui.TextWrapped(a.Description);
                if (!string.IsNullOrEmpty(date))
                    ImGui.TextColored(new Vector4(0.4f, 0.9f, 0.4f, 1f), $"Desbloqueado em: {date}");
            }
            else
            {
                ImGui.TextDisabled($"☆ {a.Title} ({a.Points} pts) [{a.Category}]");
                ImGui.TextDisabled(a.Description);
            }

            ImGui.Separator();
            ImGui.PopID();
        }

        ImGui.EndChild();

        IsOpen = open;
        ImGui.End();
    }
}
