using System;
using System.Numerics;
using ImGuiNET;
using RecompOne.Runtime.Host.Window;
using Sotn;

namespace Recompiled.Ultimate;

public sealed class UltimatePanel : IPanel
{
    public string Name => "SOTN Ultimate (Saturn & PSP)";
    public string TitleKey => "panel.ultimate";
    public bool IsOpen { get; set; }

    public void Draw()
    {
        ImGui.SetNextWindowSize(new Vector2(520, 640), ImGuiCond.FirstUseEver);
        bool open = IsOpen;
        if (!ImGui.Begin(this.Title(), ref open))
        {
            IsOpen = open;
            ImGui.End();
            return;
        }

        ImGui.TextColored(new Vector4(0.4f, 0.8f, 1f, 1f), "Castlevania: Symphony of the Night - Ultimate Edition");
        ImGui.TextDisabled("Porte integrado de todo o conteúdo do Sega Saturn e PSP Dracula X Chronicles");
        ImGui.Separator();

        bool dirty = false;

        // 1. Master Toggle
        bool ultEnabled = UltimateOptions.UltimateEnabled;
        if (ImGui.Checkbox("Ativar Recursos SOTN Ultimate", ref ultEnabled))
        {
            UltimateOptions.UltimateEnabled = ultEnabled;
            dirty = true;
        }

        if (!UltimateOptions.UltimateEnabled)
        {
            ImGui.TextColored(new Vector4(0.8f, 0.4f, 0.4f, 1f), "Os recursos Ultimate estão desativados. O jogo funcionará em modo Vanilla PS1 puro.");
            IsOpen = open;
            ImGui.End();
            return;
        }

        ImGui.Spacing();

        // 2. Balance Selection
        ImGui.SeparatorText("⚖ Balanceamento de Jogabilidade");
        int balance = (int)UltimateOptions.Balance;
        if (ImGui.RadioButton("Equilíbrio Saturn (I-frames 21f em armas pesadas, combos de escudo)", balance == (int)SaturnBalanceMode.Saturn))
        {
            UltimateOptions.Balance = SaturnBalanceMode.Saturn;
            dirty = true;
            if (Game.Available) SaturnBalance.ForceReapply(RecompOne.Runtime.Runtime.Mem!);
        }
        if (ImGui.RadioButton("Equilíbrio PS1 (Comportamento original do PlayStation)", balance == (int)SaturnBalanceMode.PS1))
        {
            UltimateOptions.Balance = SaturnBalanceMode.PS1;
            dirty = true;
            if (Game.Available) SaturnBalance.ForceReapply(RecompOne.Runtime.Runtime.Mem!);
        }

        ImGui.Spacing();

        // 3. Dub & Audio Mode
        ImGui.SeparatorText("🎙 Dublagem & Áudio");
        int dub = (int)UltimateOptions.Dub;
        if (ImGui.RadioButton("Dublagem PSP (Dracula X Chronicles - Script Revisado)", dub == (int)DubMode.PSPEnglish))
        {
            UltimateOptions.Dub = DubMode.PSPEnglish;
            dirty = true;
        }
        if (ImGui.RadioButton("Dublagem Clássica PS1 ('What is a man? A miserable little pile of secrets!')", dub == (int)DubMode.PS1Classic))
        {
            UltimateOptions.Dub = DubMode.PS1Classic;
            dirty = true;
        }
        if (ImGui.RadioButton("Dublagem Japonesa Original (Vozes em JP com legendas)", dub == (int)DubMode.Japanese))
        {
            UltimateOptions.Dub = DubMode.Japanese;
            dirty = true;
        }

        ImGui.Spacing();

        // 4. Playable Characters
        ImGui.SeparatorText("👤 Personagens & Estilos");
        int charChoice = (int)UltimateOptions.CharacterMode;
        string[] chars = [
            "Alucard (Padrão)",
            "Richter Belmont (Clássico PS1)",
            "Richter Belmont (Casaco Longo do Saturn)",
            "Maria Renard (Saturn - Pulo Triplo & Pombas)",
            "Maria Renard (PSP - Animais Celestiais)"
        ];
        if (ImGui.Combo("Personagem / Visual", ref charChoice, chars, chars.Length))
        {
            UltimateOptions.CharacterMode = (CharacterSelection)charChoice;
            dirty = true;
            if (charChoice == (int)CharacterSelection.SaturnMaria && Game.Available && Game.InGame)
            {
                MariaLoader.TransformToMaria(RecompOne.Runtime.Runtime.Mem!);
            }
        }

        if (ImGui.Button("🌟 Ativar Maria Renard no Gameplay (Saturn)"))
        {
            if (Game.Available && Game.InGame)
            {
                MariaLoader.TransformToMaria(RecompOne.Runtime.Runtime.Mem!);
            }
        }
        ImGui.SameLine();
        if (ImGui.Button("🚀 Pular para Entrada com Maria"))
        {
            if (Game.Available && Game.InGame)
            {
                UltimateOptions.CharacterMode = CharacterSelection.SaturnMaria;
                Player.TeleportTo(Stage.CastleEntrance, 8, 38, 128, 128);
                MariaLoader.TransformToMaria(RecompOne.Runtime.Runtime.Mem!);
            }
        }

        // Teste de Sons e Vozes Autênticas da Maria (Saturn)
        if (Player.IsMaria || UltimateOptions.CharacterMode == CharacterSelection.SaturnMaria)
        {
            ImGui.Spacing();
            ImGui.TextColored(new Vector4(1f, 0.84f, 0f, 1f), "🔊 Teste de Voz e Áudio da Maria (Saturn):");
            if (ImGui.Button("Recarregar Sons da Maria"))
            {
                if (Game.Available) MariaLoader.LoadMariaSounds(RecompOne.Runtime.Runtime.Mem!);
            }
            ImGui.SameLine();
            if (ImGui.Button("Grito Ataque 1")) GameApi.PlaySfx(0x6F9);
            ImGui.SameLine();
            if (ImGui.Button("Grito Ataque 2")) GameApi.PlaySfx(0x6FA);
            ImGui.SameLine();
            if (ImGui.Button("Chute (Yell)")) GameApi.PlaySfx(0x6FC);

            if (ImGui.Button("Voz: Guardian Knuckle")) GameApi.PlaySfx(0x8A1);
            ImGui.SameLine();
            if (ImGui.Button("Genbu (Tartaruga)")) GameApi.PlaySfx(0x887);
            ImGui.SameLine();
            if (ImGui.Button("Byakko (Tigre)")) GameApi.PlaySfx(0x888);
            ImGui.SameLine();
            if (ImGui.Button("Seiryuu (Dragão)")) GameApi.PlaySfx(0x889);
        }

        bool skipPrologue = QualityOfLife.SkipPrologue;
        if (ImGui.Checkbox("Pular Prelúdio (Novo Jogo começa direto na Entrada do Castelo)", ref skipPrologue))
        {
            QualityOfLife.SkipPrologue = skipPrologue;
            QualityOfLifeOptions.SkipPrologue.Value = skipPrologue;
        }

        ImGui.Spacing();

        // 5. Saturn Relics & Gear
        ImGui.SeparatorText("👟 Relíquias & Arsenal do Saturn");
        bool godSpeed = UltimateOptions.GodSpeedShoes;
        if (ImGui.Checkbox("Botas da Velocidade (God Speed Shoes - Relíquia 30)", ref godSpeed))
        {
            UltimateOptions.GodSpeedShoes = godSpeed;
            dirty = true;
        }
        ImGui.SameLine();
        string btnText = GodSpeedShoes.HasShoes ? (GodSpeedShoes.IsActive ? "Desativar" : "Ativar") : "Conceder";
        if (ImGui.Button(btnText))
        {
            if (Game.Available && Game.InGame) GodSpeedShoes.ToggleActive();
        }

        bool thirdHand = UltimateOptions.ThirdHandHolder;
        if (ImGui.Checkbox("Terceira Mão / Holder (Saturn: Item rápido com L2)", ref thirdHand))
        {
            UltimateOptions.ThirdHandHolder = thirdHand;
            dirty = true;
        }

        bool rainbow = UltimateOptions.RainbowCapeCycle;
        if (ImGui.Checkbox("Efeito Dinâmico de Cor no Manto do Arco-íris", ref rainbow))
        {
            UltimateOptions.RainbowCapeCycle = rainbow;
            dirty = true;
        }

        if (ImGui.Button("🎁 Conceder Todo o Arsenal do Sega Saturn"))
        {
            if (Game.Available && Game.InGame) SaturnItems.GiftAllSaturnGear();
        }
        ImGui.TextDisabled("Adiciona: Lança de Alucard, Escudo da Floresta, Adaga Astral, Machado Duplo,\nManto Arco-íris, Tecido Brilhante, Letras da Canção e Botas da Velocidade.");

        ImGui.Spacing();

        // 6. Stages & Teleports
        ImGui.SeparatorText("🏰 Fases Secretas Exclusivas do Sega Saturn");
        bool stagesEnabled = UltimateOptions.SaturnStages;
        if (ImGui.Checkbox("Conectar Fosso da Entrada às Fases Secretas", ref stagesEnabled))
        {
            UltimateOptions.SaturnStages = stagesEnabled;
            dirty = true;
        }

        ImGui.Text("Acesso Direto às Novas Fases:");
        if (ImGui.Button("🌿 Underground Garden (Jardim Subterrâneo)"))
        {
            if (Game.Available && Game.InGame) SaturnStages.WarpToUndergroundGarden();
        }
        ImGui.SameLine();
        if (ImGui.Button("⛓ Cursed Prison (Prisão Amaldiçoada)"))
        {
            if (Game.Available && Game.InGame) SaturnStages.WarpToCursedPrison();
        }

        if (ImGui.Button("🔥 Hell Garden (Jardim do Inferno)"))
        {
            if (Game.Available && Game.InGame) SaturnStages.WarpToHellGarden();
        }
        ImGui.SameLine();
        if (ImGui.Button("💀 Soul Prison (Prisão das Almas)"))
        {
            if (Game.Available && Game.InGame) SaturnStages.WarpToSoulPrison();
        }

        if (dirty) UltimateOptions.Save();

        IsOpen = open;
        ImGui.End();
    }
}
