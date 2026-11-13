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

    private record Destination(string Name, Stage Stage, int RoomX, int RoomY, bool Inverted);

    private static readonly Destination[] Destinations =
    [
        // Castelo Normal
        new("Entrada do Castelo", Stage.CastleEntrance, 8, 38, false),
        new("Laboratório de Alquimia", Stage.AlchemyLaboratory, 10, 30, false),
        new("Marble Gallery", Stage.MarbleGallery, 24, 24, false),
        new("Muralha Exterior", Stage.OuterWall, 60, 26, false),
        new("Biblioteca Longa", Stage.LongLibrary, 52, 18, false),
        new("Capela Real", Stage.RoyalChapel, 20, 12, false),
        new("Aposentos de Olrox", Stage.OlroxsQuarters, 30, 18, false),
        new("Coliseu", Stage.Colosseum, 16, 22, false),
        new("Torre do Relógio", Stage.ClockTower, 44, 10, false),
        new("Keep do Castelo", Stage.CastleKeep, 32, 4, false),
        new("Cavernas Subterrâneas", Stage.UndergroundCaverns, 34, 42, false),
        new("Catacumbas", Stage.Catacombs, 18, 52, false),
        new("Mina Abandonada", Stage.AbandonedMine, 22, 46, false),
        new("Sala de Teleporte (Warp)", Stage.Warp, 32, 24, false),

        // Castelo Invertido
        new("Entrada Invertida", Stage.ReverseEntrance, 8, 38, true),
        new("Laboratório de Necromancia", Stage.NecromancyLaboratory, 10, 30, true),
        new("Galeria de Mármore Negro", Stage.BlackMarbleGallery, 24, 24, true),
        new("Muralha Exterior Invertida", Stage.ReverseOuterWall, 60, 26, true),
        new("Biblioteca Proibida", Stage.ForbiddenLibrary, 52, 18, true),
        new("Anti-Capela", Stage.AntiChapel, 20, 12, true),
        new("Death Wing's Lair", Stage.DeathWingsLair, 30, 18, true),
        new("Coliseu Invertido", Stage.ReverseColosseum, 16, 22, true),
        new("Torre do Relógio Invertida", Stage.ReverseClockTower, 44, 10, true),
        new("Keep Invertido", Stage.ReverseCastleKeep, 32, 4, true),
        new("Cavernas Invertidas", Stage.ReverseCaverns, 34, 42, true),
        new("Catacumbas Flutuantes", Stage.FloatingCatacombs, 18, 52, true),
        new("Caverna", Stage.Cave, 22, 46, true),
        new("Teleporte Invertido (Warp)", Stage.ReverseWarp, 32, 24, true),
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

        int targetX = dest.RoomX;
        int targetY = dest.RoomY;

        // Tentar obter uma sala válida do estágio para segurança total de coordenadas. rooms[0]
        // (a primeira da tabela do próprio estágio) costuma ser só a primeira em ordem de dados,
        // não necessariamente uma sala jogável -- vários estágios (Outer Wall, Clock Tower, Warp,
        // Alchemy Laboratory) têm como primeira entrada uma célula de 1x1 tile (provavelmente um
        // marcador interno do mapa), e cair bem no centro dela com o offset fixo (128,128) deixa
        // o jogador preso/afundando no chão, ou caindo pra fora dependendo da colisão daquele
        // tile específico. Prefere a primeira sala com pelo menos 4 tiles de área (não é uma
        // "célula de passagem"), caindo pra rooms[0] só se nenhuma sala do estágio for maior que isso.
        try
        {
            var rooms = Stages.Rooms(dest.Stage);
            if (rooms.Count > 0)
            {
                var best = rooms[0];
                foreach (var r in rooms)
                {
                    int area = (r.Right - r.Left + 1) * (r.Bottom - r.Top + 1);
                    if (area >= 4) { best = r; break; }
                }
                targetX = best.Left;
                targetY = best.Top;
            }
        }
        catch { }

        bool success = Player.TeleportTo(dest.Stage, targetX, targetY, 128, 128);
        if (success)
        {
            ToastNotifications.ShowText(
                Localization.T("fast_travel.title"),
                $"{Localization.T("fast_travel.teleported_to")} {dest.Name}!",
                null,
                3.0f);

            try
            {
                GameApi.PlaySfx(0x635);
            }
            catch { }
        }
    }
}
