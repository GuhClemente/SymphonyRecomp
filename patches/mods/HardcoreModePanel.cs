using System;
using System.Numerics;
using ImGuiNET;
using RecompOne.Runtime.Host.Window;
using Sotn;

namespace Recompiled;

public sealed class HardcoreModePanel : IPanel
{
    public string Name => "Difficulty & Hardcore Mode";
    public string TitleKey => "panel.difficulty";
    public bool IsOpen { get; set; }

    public void Draw()
    {
        ImGui.SetNextWindowSize(new Vector2(460, 480), ImGuiCond.FirstUseEver);
        bool open = IsOpen;
        if (!ImGui.Begin(this.Title(), ref open))
        {
            IsOpen = open;
            ImGui.End();
            return;
        }

        ImGui.TextColored(new Vector4(1f, 0.85f, 0.2f, 1f), "Seletor de Dificuldade & Desafio:");
        ImGui.Separator();

        bool dirty = false;
        int diff = (int)HardcoreMode.CurrentDifficulty;

        string[] diffs = [
            "Normal (Padrão Original PS1)",
            "Modo Hard (Dano 2.0x + Inimigos Agressivos)",
            "Modo HARDCORE (Dano 2.0x + Morte Permanente / Permadeath!)"
        ];

        ImGui.Text("Dificuldade:");
        for (int i = 0; i < diffs.Length; i++)
        {
            if (i == 2) ImGui.PushStyleColor(ImGuiCol.Text, new Vector4(1f, 0.3f, 0.3f, 1f));
            else if (i == 1) ImGui.PushStyleColor(ImGuiCol.Text, new Vector4(1f, 0.7f, 0.2f, 1f));

            if (ImGui.RadioButton(diffs[i], diff == i))
            {
                diff = i;
                HardcoreMode.CurrentDifficulty = (HardcoreMode.Difficulty)diff;
                dirty = true;
            }

            if (i > 0) ImGui.PopStyleColor();
        }

        ImGui.Spacing();

        if (HardcoreMode.IsHardcore)
        {
            ImGui.BeginChild("hardcore_warning_child", new Vector2(-1, 80), ImGuiChildFlags.Borders);
            ImGui.TextColored(new Vector4(1f, 0.2f, 0.2f, 1f), "⚠ AVISO DO MODO HARDCORE:");
            ImGui.TextWrapped("Se o Alucard morrer, o progresso da campanha será permanentemente deletado! Jogue com máxima cautela.");
            ImGui.EndChild();
            ImGui.Spacing();
        }

        if (HardcoreMode.IsHard)
        {
            ImGui.SeparatorText("Ajustes Avançados de Dificuldade");
            float mult = HardcoreMode.DamageMultiplier;
            if (ImGui.SliderFloat("Multiplicador de Dano Recebido", ref mult, 1.0f, 5.0f, "%.1fx Dano"))
            {
                HardcoreMode.DamageMultiplier = mult;
                dirty = true;
            }

            dirty |= ImGui.Checkbox("Anti-Spam de Poções em Batalhas", ref HardcoreMode.AntiPotionSpam);
        }

        if (dirty) HardcoreMode.Save();

        IsOpen = open;
        ImGui.End();
    }
}
