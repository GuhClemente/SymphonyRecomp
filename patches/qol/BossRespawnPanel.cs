using System;
using System.Numerics;
using ImGuiNET;
using RecompOne.Runtime.Host.Window;
using Sotn;

namespace Recompiled;

public sealed class BossRespawnPanel : IPanel
{
    public string Name => "Boss Respawn & Arena Teleport";
    public string TitleKey => "panel.boss_respawn";
    public bool IsOpen { get; set; }

    public record BossData(
        string Name,
        TimeAttackEvent Event,
        Stage Stage,
        int RoomX,
        int RoomY,
        int CastleFlag = -1
    );

    public static readonly BossData[] Bosses =
    [
        // Normal Castle Bosses
        new("Slogra & Gaibon", TimeAttackEvent.SlograGaibonDefeat, Stage.AlchemyLaboratory, 44, 21),
        new("Doppleganger Lv10", TimeAttackEvent.Doppleganger10Defeat, Stage.OuterWall, 62, 19),
        new("Minotaur & Werewolf", TimeAttackEvent.MinotaurWerewolfDefeat, Stage.Colosseum, 21, 23),
        new("Scylla", TimeAttackEvent.ScyllaDefeat, Stage.UndergroundCaverns, 56, 38),
        new("Hippogryph", TimeAttackEvent.HippogryphDefeat, Stage.RoyalChapel, 11, 20),
        new("Olrox", TimeAttackEvent.OlroxDefeat, Stage.OlroxsQuarters, 34, 18),
        new("Granfaloon (Legion)", TimeAttackEvent.GranfaloonDefeat, Stage.Catacombs, 4, 38),
        new("Cerberus", TimeAttackEvent.CerberusDefeat, Stage.AbandonedMine, 38, 48),
        new("Succubus (Nightmare)", TimeAttackEvent.SuccubusDefeat, Stage.Nightmare, 2, 2),
        new("Karasuman", TimeAttackEvent.KarasumanDefeat, Stage.ClockTower, 50, 8),
        new("Lesser Demon", TimeAttackEvent.LesserDemonDefeat, Stage.LongLibrary, 52, 27),

        // Inverted Castle Bosses
        new("Darkwing Bat", TimeAttackEvent.DarkwingBatDefeat, Stage.ReverseClockTower, 13, 55),
        new("The Creature", TimeAttackEvent.CreatureDefeat, Stage.ReverseOuterWall, 1, 44),
        new("Medusa", TimeAttackEvent.MedusaDefeat, Stage.AntiChapel, 52, 43),
        new("Akmodan II", TimeAttackEvent.AkmodanDefeat, Stage.DeathWingsLair, 29, 45),
        new("Death", TimeAttackEvent.DeathDefeat, Stage.Cave, 25, 15),
        new("Doppleganger Lv40", TimeAttackEvent.Doppleganger40Defeat, Stage.ReverseCaverns, 7, 25),
        new("Fake Trevor, Grant, Sypha", TimeAttackEvent.TrioDefeat, Stage.ReverseColosseum, 42, 40),
        new("Galamoth", TimeAttackEvent.GalamothDefeat, Stage.FloatingCatacombs, 59, 25),
        new("Beelzebub", TimeAttackEvent.BeelzebubDefeat, Stage.NecromancyLaboratory, 19, 42),
    ];

    public void Draw()
    {
        ImGui.SetNextWindowSize(new Vector2(480, 560), ImGuiCond.FirstUseEver);
        bool open = IsOpen;
        if (!ImGui.Begin(this.Title(), ref open))
        {
            IsOpen = open;
            ImGui.End();
            return;
        }

        if (RecompOne.Runtime.Runtime.Mem == null || !Cheats.InPlay())
        {
            ImGui.TextWrapped(Localization.T("common.not_in_play"));
            IsOpen = open;
            ImGui.End();
            return;
        }

        ImGui.TextColored(new Vector4(1f, 0.8f, 0.2f, 1f), "Gerenciador de Batalhas de Chefes:");
        ImGui.TextDisabled("Reviva qualquer chefe derrotado para lutar novamente no mesmo save.");
        ImGui.Spacing();

        if (ImGui.Button("Reviver Todos os Chefes do Jogo", new Vector2(-1, 30)))
        {
            foreach (var b in Bosses)
            {
                Progress.SetTimeAttack(b.Event, 0);
            }
            ToastNotifications.ShowText("👑 Chefes Revividos!", "Todos os chefes do jogo foram reativados!", null, 3.0f);
        }

        ImGui.Separator();

        ImGui.BeginChild("boss_list_child", Vector2.Zero, ImGuiChildFlags.Borders);

        for (int i = 0; i < Bosses.Length; i++)
        {
            var b = Bosses[i];
            bool isDefeated = Progress.GetTimeAttack(b.Event) > 0;

            ImGui.PushID(i);

            if (isDefeated)
            {
                ImGui.TextColored(new Vector4(0.9f, 0.4f, 0.4f, 1f), $"☠ {b.Name}");
                ImGui.SameLine();
                ImGui.TextDisabled("(Derrotado)");
            }
            else
            {
                ImGui.TextColored(new Vector4(0.3f, 1f, 0.4f, 1f), $"⚔ {b.Name}");
                ImGui.SameLine();
                ImGui.TextColored(new Vector4(0.4f, 0.9f, 0.4f, 1f), "(Vivo / Pronto para Batalha)");
            }

            ImGui.TextDisabled($"Área: {b.Stage}  [Sala: {b.RoomX}, {b.RoomY}]");

            if (isDefeated)
            {
                if (ImGui.Button("Reviver Chefe##revive"))
                {
                    Progress.SetTimeAttack(b.Event, 0);
                    ToastNotifications.ShowText(b.Name, "Chefe revivido com sucesso!", null, 2.0f);
                }
                ImGui.SameLine();
            }

            if (ImGui.Button("Teleportar para a Arena##teleport"))
            {
                Player.TeleportTo(b.Stage, b.RoomX, b.RoomY);
                ToastNotifications.ShowText(b.Name, "Teleportado para a arena do chefe!", null, 2.0f);
            }

            ImGui.Separator();
            ImGui.PopID();
        }

        ImGui.EndChild();

        IsOpen = open;
        ImGui.End();
    }
}
