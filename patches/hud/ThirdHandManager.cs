using System;
using System.Numerics;
using ImGuiNET;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Events;
using RecompOne.Runtime.Host.Window;
using RecompOne.Runtime.Memory;
using Sotn;

namespace Recompiled;

public static class ThirdHandManager
{
    public static bool Enabled = true;
    public static bool ShowHudSlot = true;

    private static bool _initialized = false;
    private static volatile bool _useRequested = false;
    private static float _cooldownTimer = 0f;
    private static float _healEffectTimer = 0f;
    private static int _lastHealAmount = 0;
    private static ushort _prevPad = 0;

    private static readonly HandItem[] FoodItems =
    [
        HandItem.Apple, HandItem.Banana, HandItem.Grapes, HandItem.Strawberry,
        HandItem.Pineapple, HandItem.Cheesecake, HandItem.Shortcake
    ];

    public static void Initialize()
    {
        if (_initialized) return;

        Load();

        OutputPanel.OnRenderGameOverlay += RenderHud;

        // T: F is bound to L3 in the default keys, and L3 / R3 (and C) cycle the character (MariaCharacter).
        Event.AddListener<KeyboardEvent>(e =>
        {
            if (e.Pressed && !e.Repeat && e.Key == (int)Silk.NET.Input.Key.T && !HotkeyGuard.IsTyping())
            {
                _useRequested = true;
            }
        });

        _initialized = true;
    }

    public static void Load()
    {
        var v = RecompOne.Runtime.Runtime.View;
        Enabled = v.GetBool("QolThirdHandEnabled", true);
        ShowHudSlot = v.GetBool("QolThirdHandShowHud", true);
    }

    public static void Save()
    {
        var v = RecompOne.Runtime.Runtime.View;
        v.SetBool("QolThirdHandEnabled", Enabled);
        v.SetBool("QolThirdHandShowHud", ShowHudSlot);
        RecompOne.Runtime.Runtime.SaveView();
    }

    public static void Update(CpuContext c, IMemory m)
    {
        // HasControl stays true in the pause menu, so the menu and map flags are checked as well.
        if (!Enabled || !Game.Available || !Game.InGame || Game.IsLoading || !Player.HasControl ||
            Game.MenuOpen || Game.MapOpen)
        {
            // Drop presses made outside of play (menus, pause, loading) so they don't fire later.
            _useRequested = false;
            if (Game.Available) _prevPad = m.ReadU16(Game.PadsAddr);
            return;
        }

        float dt = 1f / 60f;
        if (_cooldownTimer > 0f) _cooldownTimer = Math.Max(0f, _cooldownTimer - dt);
        if (_healEffectTimer > 0f) _healEffectTimer = Math.Max(0f, _healEffectTimer - dt);

        ushort pad = m.ReadU16(Game.PadsAddr);
        ushort tapped = (ushort)(pad & ~_prevPad);
        _prevPad = pad;

        // L2 only: L3 is the character-cycle button
        bool padTriggered = (tapped & (ushort)Button.L2) != 0;

        if (_useRequested || padTriggered)
        {
            _useRequested = false;
            if (_cooldownTimer <= 0f)
            {
                ExecuteThirdHand();
            }
        }
    }

    private static int FoodCount()
    {
        int total = 0;
        foreach (var f in FoodItems) total += Inventory.GetHandCount(f);
        return total;
    }

    private static ThirdHandState ReadState() => new(
        Player.Hp, Player.HpMax, Player.Mp, Player.MpMax,
        Player.HasStatus(PlayerStatus.Curse), Player.HasStatus(PlayerStatus.Poison),
        Inventory.GetHandCount(HandItem.Uncurse), Inventory.GetHandCount(HandItem.Antivenom),
        Inventory.GetHandCount(HandItem.Elixir), Inventory.GetHandCount(HandItem.HighPotion),
        Inventory.GetHandCount(HandItem.Potion), FoodCount(), Inventory.GetHandCount(HandItem.MannaPrism));

    private static void ExecuteThirdHand()
    {
        if (!Player.IsAlucard && !Player.IsRichter && !Player.IsMaria) return;

        var s = ReadState();
        var choice = HudLogic.ChooseThirdHand(s);
        int missingHp = s.MaxHp - s.Hp;
        int heal = HudLogic.HealAmount(choice, missingHp);

        switch (choice)
        {
            case ThirdHandChoice.Uncurse:
                Inventory.RemoveHandItem(HandItem.Uncurse, 1);
                Player.Status &= ~PlayerStatus.Curse;
                TriggerFeedback("Uncurse", "Maldição Removida!", 0, s.Uncurse - 1);
                return;

            case ThirdHandChoice.Antivenom:
                Inventory.RemoveHandItem(HandItem.Antivenom, 1);
                Player.Status &= ~PlayerStatus.Poison;
                TriggerFeedback("Antivenom", "Veneno Neutralizado!", 0, s.Antivenom - 1);
                return;

            case ThirdHandChoice.Elixir:
                Inventory.RemoveHandItem(HandItem.Elixir, 1);
                Player.Hp = s.MaxHp;
                Player.Mp = s.MaxMp;
                TriggerFeedback("Elixir", $"+{heal} HP & MP Máximo!", heal, s.Elixir - 1);
                return;

            case ThirdHandChoice.HighPotion:
                Inventory.RemoveHandItem(HandItem.HighPotion, 1);
                Player.Hp += heal;
                TriggerFeedback("High Potion", $"+{heal} HP", heal, s.HighPotion - 1);
                return;

            case ThirdHandChoice.Potion:
                Inventory.RemoveHandItem(HandItem.Potion, 1);
                Player.Hp += heal;
                TriggerFeedback("Potion", $"+{heal} HP", heal, s.Potion - 1);
                return;

            case ThirdHandChoice.Food:
                foreach (var food in FoodItems)
                {
                    int count = Inventory.GetHandCount(food);
                    if (count <= 0) continue;
                    Inventory.RemoveHandItem(food, 1);
                    Player.Hp += heal;
                    TriggerFeedback(food.ToString(), $"+{heal} HP", heal, count - 1);
                    return;
                }
                return;

            case ThirdHandChoice.MannaPrism:
                Inventory.RemoveHandItem(HandItem.MannaPrism, 1);
                Player.Mp = s.MaxMp;
                TriggerFeedback("Manna Prism", "MP Restaurado ao Máximo!", 0, s.MannaPrism - 1);
                return;
        }

        ToastNotifications.ShowText("Terceira Mão", "Sem consumíveis disponíveis na mochila ou vida cheia!", null, 2.0f);
    }

    private static void TriggerFeedback(string itemName, string detail, int healAmount, int remainingCount)
    {
        _cooldownTimer = 0.5f; // Prevent accidental double consumption
        _healEffectTimer = 0.8f;
        _lastHealAmount = healAmount;

        try
        {
            // Play sacred healing chime SFX
            GameApi.PlaySfx(0x635);
        }
        catch { }

        ToastNotifications.ShowText(
            "Terceira Mão (Saturn)",
            $"{itemName}: {detail} (Restam: {remainingCount})",
            null,
            2.5f);
    }

    /// <summary>The item the HUD slot shows: the one T / L2 would use now, else the first in stock.</summary>
    public static (string Name, int Count) GetBestAvailableConsumable()
    {
        if (!Game.Available) return ("Nenhum", 0);

        var s = ReadState();
        var choice = HudLogic.PreviewThirdHand(s);
        string name = choice switch
        {
            ThirdHandChoice.None => "Vazio",
            ThirdHandChoice.HighPotion => "High Potion",
            ThirdHandChoice.MannaPrism => "Manna Prism",
            ThirdHandChoice.Food => "Comida",
            _ => choice.ToString(),
        };
        return (name, HudLogic.StockOf(s, choice));
    }
    public static void RenderHud(Vector2 imageMin, Vector2 imageSize)
    {
        if (!Enabled || !ShowHudSlot) return;
        if (Recompiled.Ultimate.PlayerSelectScreen.IsOpen) return;
        if (!Game.Available || !Game.InGame || Game.IsLoading || Game.MapOpen) return;
        // hidden while the game drives the player (opening run / horse ride, scripted scenes)
        if (!Player.HasControl) return;

        float scaleX = imageSize.X / 256.0f;
        float scaleY = imageSize.Y / 224.0f;
        var dl = ImGui.GetWindowDrawList();

        var (name, count) = GetBestAvailableConsumable();

        // Position: Top-right, right below the Minimap HUD
        float boxW = 86f * scaleX;
        float boxH = 20f * scaleY;
        float slotY = BloodstainedMinimapHud.Enabled ? BloodstainedMinimapHud.BoxBottom(imageSize) + 4f * scaleY : 10f * scaleY;
        Vector2 slotPos = imageMin + new Vector2(imageSize.X - boxW - 12f * scaleX, slotY);
        Vector2 slotMax = slotPos + new Vector2(boxW, boxH);

        // Slot backdrop pill
        uint bgCol = 0xDD10121A;
        uint borderCol = count > 0 ? 0xFF40D0F5 : 0xFF505565;
        dl.AddRectFilled(slotPos, slotMax, bgCol, 4f);
        dl.AddRect(slotPos, slotMax, borderCol, 4f, ImDrawFlags.None, 1f);

        // Text: [3ª Mão] Item xCount
        string label = count > 0 ? $"{name} x{count}" : "Mochila Vazia";
        string subLabel = "[T / L2]";
        Vector2 tSz = ImGui.CalcTextSize(label);
        Vector2 sSz = ImGui.CalcTextSize(subLabel);

        dl.AddText(new Vector2(slotPos.X + 6f * scaleX, slotPos.Y + 2f), count > 0 ? 0xFFFFFFFF : 0xFF707585, label);
        dl.AddText(new Vector2(slotMax.X - sSz.X - 4f * scaleX, slotPos.Y + 3f), 0xFF35B0D4, subLabel);

        // Active Heal Flash Effect over player
        if (_healEffectTimer > 0f)
        {
            float alpha = Math.Clamp(_healEffectTimer / 0.8f, 0f, 1f);
            uint healCol = ((uint)(alpha * 180f) << 24) | 0x00FF40;
            // Draw floating green number
            string healText = _lastHealAmount > 0 ? $"+{_lastHealAmount}" : "CURA!";
            Vector2 hTxtSz = ImGui.CalcTextSize(healText);
            Vector2 center = imageMin + imageSize * 0.5f - new Vector2(hTxtSz.X * 0.5f, (40f + (1f - alpha) * 20f) * scaleY);
            dl.AddText(center + new Vector2(1, 1), 0xFF000000, healText);
            dl.AddText(center, healCol, healText);
        }
    }
}
