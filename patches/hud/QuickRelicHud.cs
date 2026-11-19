using System;
using System.Numerics;
using ImGuiNET;
using RecompOne.Runtime.Events;
using RecompOne.Runtime.Host.Window;
using Sotn;

namespace Recompiled;

public static class QuickRelicHud
{
    public static bool Enabled = true;

    private static bool _initialized = false;
    private static volatile bool _toggleRequested = false;

    private static readonly (Relic Relic, string Code, string Name, uint ActiveColor)[] CoreRelics =
    [
        (Relic.SoulOfBat,    "BAT",  "Soul of Bat",    0xFF40D0FF),
        (Relic.SoulOfWolf,   "WOLF", "Soul of Wolf",   0xFF30A0FF),
        (Relic.FormOfMist,   "MIST", "Form of Mist",   0xFFE0B0FF),
        (Relic.LeapStone,    "LEAP", "Leap Stone",     0xFF40FF60),
        (Relic.GravityBoots, "BOOT", "Gravity Boots",  0xFFFFD020),
        (Relic.JewelOfOpen,  "KEY",  "Jewel of Open",  0xFF3080FF),
        (Relic.HolySymbol,   "HOLY", "Holy Symbol",    0xFFFFE040),
    ];

    public static void Initialize()
    {
        if (_initialized) return;

        Load();

        OutputPanel.OnRenderGameOverlay += Render;

        Event.AddListener<KeyboardEvent>(e =>
        {
            if (e.Pressed && !e.Repeat && e.Key == (int)Silk.NET.Input.Key.H)
            {
                _toggleRequested = true;
            }
        });

        _initialized = true;
    }

    public static void Load()
    {
        var v = RecompOne.Runtime.Runtime.View;
        Enabled = v.GetBool("QolQuickRelicHud", true);
    }

    public static void Save()
    {
        var v = RecompOne.Runtime.Runtime.View;
        v.SetBool("QolQuickRelicHud", Enabled);
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
                "Tracker de Relíquias",
                Enabled ? "HUD de Relíquias Ativado" : "HUD de Relíquias Desativado",
                null,
                2.0f);
        }

        if (!Enabled) return;
        if (Recompiled.Ultimate.PlayerSelectScreen.IsOpen) return;
        if (!Game.Available || !Game.InGame || Game.IsLoading || Game.MapOpen) return;

        float scaleX = imageSize.X / 256.0f;
        float scaleY = imageSize.Y / 224.0f;
        var dl = ImGui.GetWindowDrawList();

        // Calculate total collected relics
        int totalRelics = 0;
        for (int i = 0; i < Inventory.RelicCount; i++)
        {
            if (Inventory.HasRelic(i)) totalRelics++;
        }

        // Layout: Sleek bottom-center dock
        float itemW = 28f * scaleX;
        float itemH = 14f * scaleY;
        float spacing = 4f * scaleX;
        float barW = CoreRelics.Length * (itemW + spacing) + 12f * scaleX;
        float barH = 22f * scaleY;

        Vector2 barPos = imageMin + new Vector2((imageSize.X - barW) * 0.5f, imageSize.Y - barH - 8f * scaleY);
        Vector2 barMax = barPos + new Vector2(barW, barH);

        // Dark gothic pill container
        dl.AddRectFilled(barPos, barMax, 0xDD0E1018, 4f);
        dl.AddRect(barPos, barMax, 0xFF35B0D4, 4f, ImDrawFlags.None, 1f);

        float curX = barPos.X + 6f * scaleX;
        float curY = barPos.Y + (barH - itemH) * 0.5f;

        foreach (var r in CoreRelics)
        {
            bool has = Inventory.HasRelic(r.Relic);
            bool active = has && Inventory.IsRelicActive(r.Relic);

            Vector2 bMin = new(curX, curY);
            Vector2 bMax = new(curX + itemW, curY + itemH);

            uint fillCol = active ? ((r.ActiveColor & 0x00FFFFFFu) | 0x35000000u) : (has ? 0x20303540u : 0x15101015u);
            uint borderCol = active ? r.ActiveColor : (has ? 0x80708090u : 0x30404550u);
            uint textCol = active ? 0xFFFFFFFFu : (has ? 0xFFA0B0C0u : 0x50707580u);

            dl.AddRectFilled(bMin, bMax, fillCol, 2f);
            dl.AddRect(bMin, bMax, borderCol, 2f, ImDrawFlags.None, active ? 1.2f : 0.8f);

            Vector2 txtSz = ImGui.CalcTextSize(r.Code);
            Vector2 tPos = bMin + new Vector2((itemW - txtSz.X) * 0.5f, (itemH - txtSz.Y) * 0.5f);
            dl.AddText(tPos, textCol, r.Code);

            curX += itemW + spacing;
        }
    }
}
