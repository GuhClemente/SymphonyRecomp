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
        ImGui.SetNextWindowPos(new Vector2(720, 40), ImGuiCond.FirstUseEver);
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
        ImGui.SeparatorText("Balanceamento de Jogabilidade");
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
        ImGui.SeparatorText("Dublagem & Áudio");
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
        ImGui.SeparatorText("Personagens & Estilos");
        int charChoice = (int)UltimateOptions.CharacterMode;
        string[] chars = [
            "Alucard (Padrão)",
            "Richter Belmont (Clássico PS1 - Sem Mangas)",
            "Richter Belmont (Sobretudo Longo Azul - Saturn Oficial)",
            "Maria Renard (Saturn - Pulo Triplo & Pombas)",
            "Maria Renard (PSP - Animais Celestiais)",
            "Richter Belmont (Sobretudo Couro Marrom - Estilo Simon)"
        ];
        if (ImGui.Combo("Personagem / Visual", ref charChoice, chars, chars.Length))
        {
            UltimateOptions.CharacterMode = (CharacterSelection)charChoice;
            dirty = true;
            MariaCharacter.RequestCharacterSwitch((CharacterSelection)charChoice);
            Recompiled.Netplay.LobbyManager.SendLobbySync();
        }
        if (UltimateOptions.CharacterMode is CharacterSelection.PSPMaria)
            ImGui.TextColored(new Vector4(0.4f, 1f, 0.6f, 1f), "Maria PSP ativada! Pássaros, invocação de animais celestiais e voz PSP.");
        else if (UltimateOptions.CharacterMode is CharacterSelection.RichterSaturnCoat)
            ImGui.TextColored(new Vector4(0.4f, 0.8f, 1f, 1f), "Richter Saturn Coat ativado com sobretudo longo azul e física do Saturn.");
        else if (UltimateOptions.CharacterMode is CharacterSelection.RichterBrownCoat)
            ImGui.TextColored(new Vector4(0.85f, 0.6f, 0.35f, 1f), "Richter Couro Marrom ativado com sobretudo de couro rústico estilo Simon/Trevor.");

        // Everything below runs recompiled game code, so it is posted to the game thread
        bool inGame = Game.Available && Game.InGame;
        if (!inGame) ImGui.BeginDisabled();
        if (ImGui.Button("Ativar Maria Renard no Gameplay (Saturn)"))
        {
            UltimateOptions.CharacterMode = CharacterSelection.SaturnMaria;
            dirty = true;
            MariaCharacter.ResetState();
            MariaCharacter.RequestCharacterSwitch(CharacterSelection.SaturnMaria);
            Recompiled.Netplay.LobbyManager.SendLobbySync();
        }
        ImGui.SameLine();
        if (ImGui.Button("Pular para Entrada com Maria"))
        {
            UltimateOptions.CharacterMode = CharacterSelection.SaturnMaria;
            dirty = true;
            MariaCharacter.ResetState();
            Recompiled.Netplay.LobbyManager.SendLobbySync();
            GameThread.Post((c, m) =>
            {
                if (!Player.CanTeleport) return;
                Player.TeleportTo(Stage.CastleEntrance, 17, 37, 72, 164); // the save room, see FastTravelPanel
            });
        }
        if (!inGame) ImGui.EndDisabled();
        if (Game.Available && !MariaLoader.OverlayAvailable)
            ImGui.TextColored(new Vector4(0.9f, 0.5f, 0.3f, 1f), "Overlay 'maria' não gerado: rode o build inicial novamente.");

        // HUD do Saturn e Teste de Sons e Vozes Autênticas da Maria (Saturn)
        if (Player.IsMaria || UltimateOptions.CharacterMode == CharacterSelection.SaturnMaria)
        {
            bool saturnHud = MariaSaturnHud.Enabled;
            if (ImGui.Checkbox("HUD Original do Saturn (Experimental)", ref saturnHud))
            {
                MariaSaturnHud.Enabled = saturnHud;
            }
            if (saturnHud)
            {
                ImGui.TextColored(new Vector4(1f, 0.7f, 0.2f, 1f), "Aviso: O HUD do Saturn compartilha VRAM com cenários do PS1 e pode causar distorções visuais.");
            }

            ImGui.Spacing();
            ImGui.TextColored(new Vector4(1f, 0.84f, 0f, 1f), "Teste de Voz e Áudio da Maria (Saturn):");
            if (!inGame) ImGui.BeginDisabled();
            if (ImGui.Button("Recarregar Sons da Maria")) MariaLoader.RequestReloadSounds();
            ImGui.SameLine();
            if (ImGui.Button("Grito Ataque 1")) PlaySfx(0x6F9);
            ImGui.SameLine();
            if (ImGui.Button("Grito Ataque 2")) PlaySfx(0x6FA);
            ImGui.SameLine();
            if (ImGui.Button("Chute (Yell)")) PlaySfx(0x6FC);

            if (ImGui.Button("Voz: Guardian Knuckle")) PlaySfx(0x8A1);
            ImGui.SameLine();
            if (ImGui.Button("Genbu (Tartaruga)")) PlaySfx(0x887);
            ImGui.SameLine();
            if (ImGui.Button("Byakko (Tigre)")) PlaySfx(0x888);
            ImGui.SameLine();
            if (ImGui.Button("Seiryuu (Dragão)")) PlaySfx(0x889);
            if (!inGame) ImGui.EndDisabled();
        }

        bool skipPrologue = QualityOfLife.SkipPrologue;
        if (ImGui.Checkbox("Pular Prelúdio (Novo Jogo começa direto na Entrada do Castelo)", ref skipPrologue))
        {
            QualityOfLife.SkipPrologue = skipPrologue;
            QualityOfLifeOptions.SkipPrologue.Value = skipPrologue;
        }

        bool bypassDeath = UltimateOptions.BypassDeath;
        if (ImGui.Checkbox("Ignorar Encontro com a Morte (Bypass Death / Manter Equipamentos)", ref bypassDeath))
        {
            UltimateOptions.BypassDeath = bypassDeath;
            dirty = true;
        }
        if (ImGui.IsItemHovered())
            ImGui.SetTooltip("Pula o encontro com a Morte na Entrada do Castelo: Alucard mantém todos os seus equipamentos lendários iniciais e o corredor fica livre.");

        ImGui.Spacing();

        // 5. Saturn Relics & Gear
        ImGui.SeparatorText("Relíquias & Arsenal do Saturn");
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
            if (inGame) GameThread.Post(GodSpeedShoes.ToggleActive);
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

        if (ImGui.Button("Conceder Arsenal do Sega Saturn"))
        {
            if (inGame) GameThread.Post(SaturnItems.GiftAllSaturnGear);
        }
        ImGui.TextDisabled(SaturnItems.ItemsSupported
            ? "Adiciona: Lança de Alucard, Escudo da Floresta, Adaga Astral, Machado Duplo,\nManto Arco-íris, Tecido Brilhante, Letras da Canção e Botas da Velocidade."
            : "Por enquanto só as Botas da Velocidade: as armas e mantos do Saturn (ids 169-175 / 90-92)\nnão existem nas tabelas de itens do DRA retail.");

        ImGui.Spacing();

        // 6. Stages & Teleports
        ImGui.SeparatorText("Fases Secretas Exclusivas do Sega Saturn");
        bool stagesEnabled = UltimateOptions.SaturnStages;
        if (ImGui.Checkbox("Conectar Fosso da Entrada às Fases Secretas", ref stagesEnabled))
        {
            UltimateOptions.SaturnStages = stagesEnabled;
            dirty = true;
        }

        bool stagesReady = SaturnStages.Available;
        if (Game.Available && !stagesReady)
            ImGui.TextColored(new Vector4(0.9f, 0.5f, 0.3f, 1f),
                "Fases do Saturn ainda não portadas (overlays GDN/PRS/RGDN/RPRS ausentes).\nNo disco retail esses ids são as salas de teste TE1-TE4.");

        ImGui.Text("Acesso Direto às Novas Fases:");
        if (!stagesReady || !inGame) ImGui.BeginDisabled();
        if (ImGui.Button("Underground Garden (Jardim Subterrâneo)"))
            GameThread.Post(SaturnStages.WarpToUndergroundGarden);
        ImGui.SameLine();
        if (ImGui.Button("Cursed Prison (Prisão Amaldiçoada)"))
            GameThread.Post(SaturnStages.WarpToCursedPrison);

        if (ImGui.Button("Hell Garden (Jardim do Inferno)"))
            GameThread.Post(SaturnStages.WarpToHellGarden);
        ImGui.SameLine();
        if (ImGui.Button("Soul Prison (Prisão das Almas)"))
            GameThread.Post(SaturnStages.WarpToSoulPrison);
        if (!stagesReady || !inGame) ImGui.EndDisabled();

        if (dirty) UltimateOptions.Save();

        IsOpen = open;
        ImGui.End();
    }

    private static void PlaySfx(int id) => GameThread.Post(() => GameApi.PlaySfx(id));
}
