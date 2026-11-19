using System;
using System.Numerics;
using ImGuiNET;
using RecompOne.Runtime.Events;
using RecompOne.Runtime.Host.Window;
using RecompOne.Runtime.Memory;
using Sotn;

namespace Recompiled;

public static class BloodstainedMinimapHud
{
    public static bool Enabled = true;
    public static bool ShowPartnerDirection = true;
    public static float Opacity = 0.88f;

    private static bool _initialized = false;
    private static volatile bool _toggleRequested = false;

    // Castle memory offsets
    private const uint ZoneAddr = 0x800974A0;
    private const uint CastleXAddr = 0x800730B0;
    private const uint CastleYAddr = 0x800730B4;
    private const uint RoomXAddr = 0x800973F0;
    private const uint RoomYAddr = 0x800973F4;
    private const uint PlayStateAddr = 0x80073060;
    private const uint MapModeAddr = 0x8003C9A4;

    public static void Initialize()
    {
        if (_initialized) return;

        Load();

        OutputPanel.OnRenderGameOverlay += Render;

        Event.AddListener<KeyboardEvent>(e =>
        {
            if (e.Pressed && !e.Repeat && e.Key == (int)Silk.NET.Input.Key.M)
            {
                _toggleRequested = true;
            }
        });

        _initialized = true;
    }

    public static void Load()
    {
        var v = RecompOne.Runtime.Runtime.View;
        Enabled = v.GetBool("QolBloodstainedMinimap", true);
        ShowPartnerDirection = v.GetBool("QolMinimapPartnerDirection", true);
        Opacity = v.GetFloat("QolMinimapOpacity", 0.88f);
    }

    public static void Save()
    {
        var v = RecompOne.Runtime.Runtime.View;
        v.SetBool("QolBloodstainedMinimap", Enabled);
        v.SetBool("QolMinimapPartnerDirection", ShowPartnerDirection);
        v.SetFloat("QolMinimapOpacity", Opacity);
        RecompOne.Runtime.Runtime.SaveView();
    }

    public static void Render(Vector2 imageMin, Vector2 imageSize)
    {
        if (_toggleRequested)
        {
            _toggleRequested = false;
            Enabled = !Enabled;
            Save();
            ToastNotifications.ShowText(
                "Minimap HUD",
                Enabled ? "Radar In-Game Ativado (Estilo Bloodstained)" : "Radar In-Game Desativado",
                null,
                2.0f);
        }

        if (!Enabled) return;
        if (Recompiled.Ultimate.PlayerSelectScreen.IsOpen) return;
        if (!Game.Available || !Game.InGame || Game.IsLoading) return;
        if (Game.MapOpen) return; // Full pause map is open, don't overlap

        var m = RecompOne.Runtime.Runtime.Mem;
        if (m == null) return;

        // Ensure player is in normal active play mode
        if (m.ReadU8(PlayStateAddr) != 3 || m.ReadU8(MapModeAddr) != 1) return;

        float scaleX = imageSize.X / 256.0f;
        float scaleY = imageSize.Y / 224.0f;
        var dl = ImGui.GetWindowDrawList();
        float time = (float)ImGui.GetTime();

        // 1. Calculate Player Castle Coordinates
        int castleX = (int)m.ReadU8(CastleXAddr);
        int castleY = (int)m.ReadU8(CastleYAddr);
        int roomX = (int)m.ReadU16(RoomXAddr) / 256;
        int roomY = (int)m.ReadU16(RoomYAddr) / 256;
        int curCellX = castleX + roomX;
        int curCellY = castleY + roomY;

        bool isSecondCastle = (m.ReadU8(ZoneAddr) & 0x20) == 0x20;
        if (isSecondCastle) curCellY -= 7;

        if (curCellX < 0 || curCellX >= 64 || curCellY < 0 || curCellY >= 64) return;

        // 2. Radar Geometry: 7x7 View Window
        const int Radius = 3; // 7x7 grid
        float cellSize = 10.5f * scaleX;
        float radarW = (Radius * 2 + 1) * cellSize;
        float radarH = (Radius * 2 + 1) * cellSize;

        // Position: Top-Right of the game screen
        float padRight = 12f * scaleX;
        float padTop = 10f * scaleY;
        Vector2 boxMin = imageMin + new Vector2(imageSize.X - radarW - padRight - 8f * scaleX, padTop);
        Vector2 boxMax = boxMin + new Vector2(radarW + 16f * scaleX, radarH + 28f * scaleY);
        Vector2 gridOrigin = boxMin + new Vector2(8f * scaleX, 18f * scaleY);

        // 3. Dark Gothic Container
        uint bgCol = ((uint)(Opacity * 240) << 24) | 0x0E0F16;
        dl.AddRectFilled(boxMin, boxMax, bgCol, 5f);
        dl.AddRect(boxMin, boxMax, 0xFF35B0D4, 5f, ImDrawFlags.None, 1.2f);

        // Header: Map Percentage & Castle Indicator
        float percent = Math.Clamp((float)Map.VisitedCount / 948f * 100f, 0.1f, 200.6f);
        string castleTag = isSecondCastle ? "2º CASTELO" : "1º CASTELO";
        string headerText = $"{castleTag} • {percent:0.0}%";
        Vector2 hSz = ImGui.CalcTextSize(headerText);
        dl.AddText(new Vector2(boxMin.X + (boxMax.X - boxMin.X - hSz.X) * 0.5f, boxMin.Y + 3f), 0xFFD8EEF5, headerText);

        // 4. Render 7x7 Grid Cells
        for (int dy = -Radius; dy <= Radius; dy++)
        {
            for (int dx = -Radius; dx <= Radius; dx++)
            {
                int cx = curCellX + dx;
                int cy = curCellY + dy;

                Vector2 cMin = gridOrigin + new Vector2((dx + Radius) * cellSize, (dy + Radius) * cellSize);
                Vector2 cMax = cMin + new Vector2(cellSize - 1f, cellSize - 1f);

                if (cx >= 0 && cx < 64 && cy >= 0 && cy < 64)
                {
                    byte roomByte = Map.GetRoom(cx, cy);
                    bool visited = roomByte != 0;

                    if (visited)
                    {
                        uint roomCol = isSecondCastle ? 0xFF652075 : 0xFF8A4018; // Inverted / Normal
                        dl.AddRectFilled(cMin, cMax, roomCol, 1.5f);
                        dl.AddRect(cMin, cMax, 0x50FFFFFF, 1.5f, ImDrawFlags.None, 0.8f);
                    }
                    else
                    {
                        // Unvisited cell backdrop
                        dl.AddRectFilled(cMin, cMax, 0x30151822, 1f);
                    }
                }
                else
                {
                    dl.AddRectFilled(cMin, cMax, 0x15000000, 1f);
                }
            }
        }

        // 5. Current Player Cursor (Center Cell - Pulsing Bright Neon Dot)
        Vector2 centerCell = gridOrigin + new Vector2(Radius * cellSize, Radius * cellSize);
        float pulse = (MathF.Sin(time * 7f) + 1f) * 0.5f;
        uint curCol = 0xFF40FFFF;
        dl.AddRect(centerCell, centerCell + new Vector2(cellSize - 1f, cellSize - 1f), curCol, 1.5f, ImDrawFlags.None, 1.8f);
        dl.AddCircleFilled(centerCell + new Vector2(cellSize * 0.5f, cellSize * 0.5f), 2.5f * scaleX + pulse * 1.0f, 0xFFFFFFFF);

        // 6. Co-op Partner Integration on Radar
        if (Netplay.NetworkManager.IsConnected && Netplay.RemotePuppet.IsActive)
        {
            DrawPartnerOnRadar(dl, gridOrigin, boxMin, boxMax, curCellX, curCellY, Radius, cellSize, scaleX, scaleY, time, isSecondCastle);
        }
    }

    private static void DrawPartnerOnRadar(
        ImDrawListPtr dl,
        Vector2 gridOrigin,
        Vector2 boxMin,
        Vector2 boxMax,
        int curCellX,
        int curCellY,
        int radius,
        float cellSize,
        float scaleX,
        float scaleY,
        float time,
        bool isSecondCastle)
    {
        var m = RecompOne.Runtime.Runtime.Mem;
        if (m == null) return;

        bool remoteSecondCastle = (Netplay.RemotePuppet.StageId & 0x20) != 0;
        if (remoteSecondCastle != isSecondCastle)
        {
            // Partner is in opposite castle: draw badge at bottom of minimap
            string otherTxt = "[P2 no Outro Castelo]";
            Vector2 oSz = ImGui.CalcTextSize(otherTxt);
            dl.AddText(new Vector2(boxMin.X + (boxMax.X - boxMin.X - oSz.X) * 0.5f, boxMax.Y - oSz.Y - 2f), 0xFF80A0FF, otherTxt);
            return;
        }

        int partnerCastleX = (int)m.ReadU8(CastleXAddr);
        int partnerCastleY = (int)m.ReadU8(CastleYAddr);
        int partnerCellX = partnerCastleX + Netplay.RemotePuppet.RoomX;
        int partnerCellY = partnerCastleY + Netplay.RemotePuppet.RoomY;
        if (remoteSecondCastle) partnerCellY -= 7;

        int pDx = partnerCellX - curCellX;
        int pDy = partnerCellY - curCellY;

        // Is partner inside the 7x7 radar view?
        if (Math.Abs(pDx) <= radius && Math.Abs(pDy) <= radius)
        {
            Vector2 pCell = gridOrigin + new Vector2((pDx + radius) * cellSize, (pDy + radius) * cellSize);
            float pPulse = (MathF.Sin(time * 8f) + 1f) * 0.5f;

            // Partner pulsing emerald dot
            dl.AddCircleFilled(pCell + new Vector2(cellSize * 0.5f, cellSize * 0.5f), 3.0f * scaleX, 0xFF20FF50);
            dl.AddCircle(pCell + new Vector2(cellSize * 0.5f, cellSize * 0.5f), (4.5f + pPulse * 2.5f) * scaleX, 0xAA20FF50, 16, 1.2f);
        }
        else if (ShowPartnerDirection)
        {
            // Outside radar view: render directional chevron and distance
            Vector2 dir = Vector2.Normalize(new Vector2(pDx, pDy));
            Vector2 center = gridOrigin + new Vector2(radius * cellSize, radius * cellSize);

            float edgeDist = (radius + 0.6f) * cellSize;
            Vector2 edgePos = center + dir * edgeDist;

            uint arrowCol = Netplay.CoopRevivalManager.IsRemoteDowned ? 0xFF2020FF : 0xFF20E050;
            Vector2 a1 = edgePos + dir * 6f * scaleX;
            Vector2 a2 = edgePos + new Vector2(-dir.Y, dir.X) * 4f * scaleX - dir * 3f;
            Vector2 a3 = edgePos + new Vector2(dir.Y, -dir.X) * 4f * scaleX - dir * 3f;
            dl.AddTriangleFilled(a1, a2, a3, arrowCol);

            int distRooms = Math.Abs(pDx) + Math.Abs(pDy);
            string distText = $"P2: {distRooms} salas";
            Vector2 dSz = ImGui.CalcTextSize(distText);
            dl.AddText(new Vector2(boxMin.X + (boxMax.X - boxMin.X - dSz.X) * 0.5f, boxMax.Y - dSz.Y - 2f), arrowCol, distText);
        }
    }
}
