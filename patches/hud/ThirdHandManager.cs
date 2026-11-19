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

    public static void Initialize()
    {
        if (_initialized) return;

        Load();

        OutputPanel.OnRenderGameOverlay += RenderHud;

        Event.AddListener<KeyboardEvent>(e =>
        {
            if (e.Pressed && !e.Repeat && (e.Key == (int)Silk.NET.Input.Key.F || e.Key == (int)Silk.NET.Input.Key.C))
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
        if (!Enabled || !Game.Available || !Game.InGame || Game.IsLoading) return;

        float dt = 1f / 60f;
        if (_cooldownTimer > 0f) _cooldownTimer = Math.Max(0f, _cooldownTimer - dt);
        if (_healEffectTimer > 0f) _healEffectTimer = Math.Max(0f, _healEffectTimer - dt);

        // Check Controller Trigger: L2 / LT / L3 button
        ushort pad = m.ReadU16(Game.PadsAddr);
        ushort tapped = (ushort)(pad & ~_prevPad);
        _prevPad = pad;

        // Button.L2 is mapped as 0x0001 or 0x0100 depending on mapping; L3 is also checked
        bool padTriggered = (tapped & (ushort)Button.L2) != 0 || (tapped & (ushort)Button.L3) != 0;

        if (_useRequested || padTriggered)
        {
            _useRequested = false;
            if (_cooldownTimer <= 0f)
            {
                ExecuteThirdHand(c, m);
            }
        }
    }

    private static void ExecuteThirdHand(CpuContext c, IMemory m)
    {
        if (!Player.IsAlucard && !Player.IsRichter && !Player.IsMaria) return;

        int curHp = Player.Hp;
        int maxHp = Player.HpMax;
        int curMp = Player.Mp;
        int maxMp = Player.MpMax;

        // 1. Status Recovery Priority: Curse
        if (Player.HasStatus(PlayerStatus.Curse) && Inventory.GetHandCount(HandItem.Uncurse) > 0)
        {
            Inventory.RemoveHandItem(HandItem.Uncurse, 1);
            Player.Status &= ~PlayerStatus.Curse;
            TriggerFeedback("Uncurse", "Maldição Removida!", 0, Inventory.GetHandCount(HandItem.Uncurse));
            return;
        }

        // 2. Status Recovery Priority: Poison
        if (Player.HasStatus(PlayerStatus.Poison) && Inventory.GetHandCount(HandItem.Antivenom) > 0)
        {
            Inventory.RemoveHandItem(HandItem.Antivenom, 1);
            Player.Status &= ~PlayerStatus.Poison;
            TriggerFeedback("Antivenom", "Veneno Neutralizado!", 0, Inventory.GetHandCount(HandItem.Antivenom));
            return;
        }

        // 3. Health Recovery Priority
        int missingHp = maxHp - curHp;
        if (missingHp > 0)
        {
            // Elixir (Full HP & MP) if critical HP <= 35%
            if (curHp <= maxHp * 0.35f && Inventory.GetHandCount(HandItem.Elixir) > 0)
            {
                Inventory.RemoveHandItem(HandItem.Elixir, 1);
                Player.Hp = maxHp;
                Player.Mp = maxMp;
                TriggerFeedback("Elixir", $"+{missingHp} HP & MP Máximo!", missingHp, Inventory.GetHandCount(HandItem.Elixir));
                return;
            }

            // High Potion (+100 HP) if missing >= 60 HP
            if (missingHp >= 60 && Inventory.GetHandCount(HandItem.HighPotion) > 0)
            {
                Inventory.RemoveHandItem(HandItem.HighPotion, 1);
                int heal = Math.Min(100, missingHp);
                Player.Hp += heal;
                TriggerFeedback("High Potion", $"+{heal} HP", heal, Inventory.GetHandCount(HandItem.HighPotion));
                return;
            }

            // Regular Potion (+50 HP)
            if (Inventory.GetHandCount(HandItem.Potion) > 0)
            {
                Inventory.RemoveHandItem(HandItem.Potion, 1);
                int heal = Math.Min(50, missingHp);
                Player.Hp += heal;
                TriggerFeedback("Potion", $"+{heal} HP", heal, Inventory.GetHandCount(HandItem.Potion));
                return;
            }

            // Food Items Search
            HandItem[] foodItems = [
                HandItem.Apple, HandItem.Banana, HandItem.Grapes, HandItem.Strawberry,
                HandItem.Pineapple, HandItem.Cheesecake, HandItem.Shortcake
            ];

            foreach (var food in foodItems)
            {
                int count = Inventory.GetHandCount(food);
                if (count > 0)
                {
                    Inventory.RemoveHandItem(food, 1);
                    int heal = Math.Min(35, missingHp);
                    Player.Hp += heal;
                    TriggerFeedback(food.ToString(), $"+{heal} HP", heal, count - 1);
                    return;
                }
            }
        }

        // 4. Mana Recovery: Manna Prism
        if (curMp <= maxMp * 0.25f && Inventory.GetHandCount(HandItem.MannaPrism) > 0)
        {
            Inventory.RemoveHandItem(HandItem.MannaPrism, 1);
            Player.Mp = maxMp;
            TriggerFeedback("Manna Prism", "MP Restaurado ao Máximo!", 0, Inventory.GetHandCount(HandItem.MannaPrism));
            return;
        }

        // Nothing to heal or no items in bag
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

    public static (string Name, int Count) GetBestAvailableConsumable()
    {
        if (!Game.Available) return ("Nenhum", 0);

        if (Inventory.GetHandCount(HandItem.Elixir) > 0) return ("Elixir", Inventory.GetHandCount(HandItem.Elixir));
        if (Inventory.GetHandCount(HandItem.HighPotion) > 0) return ("High Potion", Inventory.GetHandCount(HandItem.HighPotion));
        if (Inventory.GetHandCount(HandItem.Potion) > 0) return ("Potion", Inventory.GetHandCount(HandItem.Potion));

        HandItem[] foodItems = [HandItem.Apple, HandItem.Banana, HandItem.Shortcake];
        foreach (var food in foodItems)
        {
            int c = Inventory.GetHandCount(food);
            if (c > 0) return (food.ToString(), c);
        }

        return ("Vazio", 0);
    }

    public static void RenderHud(Vector2 imageMin, Vector2 imageSize)
    {
        if (!Enabled || !ShowHudSlot) return;
        if (Recompiled.Ultimate.PlayerSelectScreen.IsOpen) return;
        if (!Game.Available || !Game.InGame || Game.IsLoading || Game.MapOpen) return;

        float scaleX = imageSize.X / 256.0f;
        float scaleY = imageSize.Y / 224.0f;
        var dl = ImGui.GetWindowDrawList();

        var (name, count) = GetBestAvailableConsumable();

        // Position: Top-right, right below the Minimap HUD
        float boxW = 86f * scaleX;
        float boxH = 20f * scaleY;
        Vector2 slotPos = imageMin + new Vector2(imageSize.X - boxW - 12f * scaleX, 134f * scaleY);
        Vector2 slotMax = slotPos + new Vector2(boxW, boxH);

        // Slot backdrop pill
        uint bgCol = 0xDD10121A;
        uint borderCol = count > 0 ? 0xFF40D0F5 : 0xFF505565;
        dl.AddRectFilled(slotPos, slotMax, bgCol, 4f);
        dl.AddRect(slotPos, slotMax, borderCol, 4f, ImDrawFlags.None, 1f);

        // Text: [3ª Mão] Item xCount
        string label = count > 0 ? $"{name} x{count}" : "Mochila Vazia";
        string subLabel = "[F / L2]";
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
