using System;
using System.Collections.Generic;
using System.Numerics;
using ImGuiNET;
using RecompOne.Runtime.Host.Window;
using Sotn;

namespace Recompiled;

public sealed class FastTravelPanel : IPanel
{
    public string Name => "Fast Travel";
    public string TitleKey => "panel.fast_travel";
    public bool IsOpen { get; set; }

    private static bool _onlyVisited = true;
    private static string _search = "";

    /// <summary>Where a destination drops the player: map cell and pixel inside the room. <c>Verified</c>
    /// means the spot was measured in game (player standing on the floor, vy = 0), see CharacterTest.</summary>
    public record Destination(string Name, Stage Stage, int RoomX, int RoomY, bool Inverted,
        int OffsetX = 128, int OffsetY = 128, bool Verified = false, string Code = "");

    public static readonly Destination[] Destinations =
    [
        // Castelo Normal. Every spot was measured in game (CharacterTest, SOTN_TEST_PROBE): the cell is a
        // 1x1 room that is not a CD loading corridor, and the pixel is on the floor, left of the save point.
        new("Entrada do Castelo", Stage.CastleEntrance, 17, 37, false, 72, 164, true, "01_entrada"),
        new("Laboratório de Alquimia", Stage.AlchemyLaboratory, 7, 30, false, 72, 164, true, "02_alquimia"),
        new("Marble Gallery", Stage.MarbleGallery, 29, 27, false, 72, 164, true, "03_galeria"),
        new("Muralha Exterior", Stage.OuterWall, 55, 24, false, 72, 164, true, "04_muralha"),
        new("Biblioteca Longa", Stage.LongLibrary, 51, 19, false, 72, 164, true, "05_biblioteca"),
        new("Capela Real", Stage.RoyalChapel, 29, 13, false, 72, 164, true, "06_capela"),
        new("Aposentos de Olrox", Stage.OlroxsQuarters, 31, 17, false, 72, 164, true, "07_olrox"),
        new("Coliseu", Stage.Colosseum, 14, 23, false, 72, 164, true, "08_coliseu"),
        new("Torre do Relógio", Stage.ClockTower, 49, 14, false, 128, 132, true, "09_torre"),
        new("Keep do Castelo", Stage.CastleKeep, 39, 9, false, 72, 164, true, "10_keep"),
        new("Cavernas Subterrâneas", Stage.UndergroundCaverns, 40, 37, false, 72, 164, true, "11_cavernas"),
        new("Catacumbas", Stage.Catacombs, 30, 50, false, 72, 164, true, "12_catacumbas"),
        new("Mina Abandonada", Stage.AbandonedMine, 28, 41, false, 72, 164, true, "13_mina"),
        new("Sala de Teleporte (Warp)", Stage.Warp, 15, 38, false, 72, 180, true, "14_warp"),

        // Castelo Invertido: the same rooms upside down, the floor of a save room is at 168 there
        new("Entrada Invertida", Stage.ReverseEntrance, 46, 26, true, 72, 168, true, "r01_entrada"),
        new("Laboratório de Necromancia", Stage.NecromancyLaboratory, 56, 33, true, 72, 168, true, "r02_necromancia"),
        new("Galeria de Mármore Negro", Stage.BlackMarbleGallery, 34, 36, true, 72, 168, true, "r03_galeria"),
        new("Muralha Exterior Invertida", Stage.ReverseOuterWall, 8, 39, true, 72, 168, true, "r04_muralha"),
        new("Biblioteca Proibida", Stage.ForbiddenLibrary, 12, 44, true, 72, 168, true, "r05_biblioteca"),
        new("Anti-Capela", Stage.AntiChapel, 34, 50, true, 72, 168, true, "r06_anticapela"),
        new("Death Wing's Lair", Stage.DeathWingsLair, 32, 46, true, 72, 168, true, "r07_deathwing"),
        new("Coliseu Invertido", Stage.ReverseColosseum, 49, 40, true, 72, 168, true, "r08_coliseu"),
        new("Torre do Relógio Invertida", Stage.ReverseClockTower, 14, 49, true, 128, 180, true, "r09_torre"),
        new("Keep Invertido", Stage.ReverseCastleKeep, 24, 54, true, 72, 168, true, "r10_keep"),
        new("Cavernas Invertidas", Stage.ReverseCaverns, 23, 26, true, 72, 168, true, "r11_cavernas"),
        new("Catacumbas Flutuantes", Stage.FloatingCatacombs, 33, 13, true, 72, 168, true, "r12_catacumbas"),
        new("Caverna", Stage.Cave, 35, 22, true, 72, 168, true, "r13_caverna"),
        new("Teleporte Invertido (Warp)", Stage.ReverseWarp, 48, 25, true, 72, 180, true, "r14_warp"),
    ];

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

        if (RecompOne.Runtime.Runtime.Mem == null || !Cheats.InPlay())
        {
            ImGui.TextWrapped(Localization.T("common.not_in_play"));
            IsOpen = open;
            ImGui.End();
            return;
        }

        ImGui.Checkbox(Localization.T("fast_travel.only_visited"), ref _onlyVisited);
        ImGui.SameLine();
        ImGui.TextDisabled("(?)");
        if (ImGui.IsItemHovered())
            ImGui.SetTooltip(Localization.T("fast_travel.only_visited_hint"));

        ImGui.Spacing();
        ImGui.SetNextItemWidth(-1f);
        ImGui.InputTextWithHint("##fasttravelsearch", Localization.T("common.search"), ref _search, 32);
        ImGui.Spacing();

        bool inSecondCastle = Game.SecondCastle;

        if (ImGui.BeginTabBar("fasttravel_tabs"))
        {
            if (ImGui.BeginTabItem(Localization.T("fast_travel.first_castle")))
            {
                DrawDestinations(false);
                ImGui.EndTabItem();
            }

            if (ImGui.BeginTabItem(Localization.T("fast_travel.second_castle")))
            {
                DrawDestinations(true);
                ImGui.EndTabItem();
            }

            ImGui.EndTabBar();
        }

        IsOpen = open;
        ImGui.End();
    }

    private void DrawDestinations(bool inverted)
    {
        ImGui.BeginChild("dest_list_" + (inverted ? "inv" : "norm"), new Vector2(0, 0), ImGuiChildFlags.Borders);

        foreach (var dest in Destinations)
        {
            if (dest.Inverted != inverted) continue;

            if (!string.IsNullOrWhiteSpace(_search) &&
                !dest.Name.Contains(_search, StringComparison.OrdinalIgnoreCase))
                continue;

            bool visited = IsStageVisited(dest.Stage);

            if (_onlyVisited && !visited)
            {
                ImGui.BeginDisabled();
            }

            string label = $"{dest.Name}##{dest.Stage}";
            if (ImGui.Button(label, new Vector2(-1, 32)))
            {
                TriggerTravel(dest);
            }

            if (_onlyVisited && !visited)
            {
                ImGui.EndDisabled();
                if (ImGui.IsItemHovered(ImGuiHoveredFlags.AllowWhenDisabled))
                    ImGui.SetTooltip(Localization.T("fast_travel.not_visited"));
            }
        }

        ImGui.EndChild();
    }

    private static bool IsStageVisited(Stage stage)
    {
        try
        {
            var rooms = Stages.Rooms(stage);
            if (rooms.Count == 0) return false;
            foreach (var r in rooms)
            {
                if (Map.GetRoom(r.Left, r.Top) != 0)
                    return true;
            }
        }
        catch { }
        return false;
    }

    private static void TriggerTravel(Destination dest)
    {
        if (!Player.CanTeleport)
        {
            ToastNotifications.ShowText(
                Localization.T("fast_travel.error_title"),
                Localization.T("fast_travel.error_busy"),
                null,
                2.5f);
            return;
        }

        // game code has to run on the game thread, this is the interface's
        GameThread.Post((_, _) =>
        {
            if (!Player.CanTeleport) return;
            Player.TeleportTo(dest.Stage, dest.RoomX, dest.RoomY, dest.OffsetX, dest.OffsetY);
            try { GameApi.PlaySfx(0x635); } catch { }
        });

        ToastNotifications.ShowText(
            Localization.T("fast_travel.title"),
            $"{Localization.T("fast_travel.teleported_to")} {dest.Name}!",
            null,
            3.0f);
    }
}
