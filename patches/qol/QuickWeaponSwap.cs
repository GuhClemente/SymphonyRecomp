using System;
using System.Text.RegularExpressions;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Host.Window;
using RecompOne.Runtime.Memory;
using Sotn;

namespace Recompiled;

public static class QuickWeaponSwap
{
    public static bool Enabled = true;
    public static int CurrentSlot = 0;

    // 3 Loadouts for hands: (RightHand, LeftHand)
    private static readonly uint[] SlotRight = new uint[3];
    private static readonly uint[] SlotLeft = new uint[3];
    private static ushort _prevPad = 0;

    public static void Load()
    {
        var v = RecompOne.Runtime.Runtime.View;
        Enabled = v.GetBool("QolQuickWeaponSwap", true);
        CurrentSlot = Math.Clamp(v.GetInt("QolCurrentWeaponSlot", 0), 0, 2);
        for (int i = 0; i < 3; i++)
        {
            SlotRight[i] = (uint)v.GetInt($"QolLoadoutRight_{i}", 0);
            SlotLeft[i] = (uint)v.GetInt($"QolLoadoutLeft_{i}", 0);
        }
    }

    public static void Save()
    {
        var v = RecompOne.Runtime.Runtime.View;
        v.SetBool("QolQuickWeaponSwap", Enabled);
        v.SetInt("QolCurrentWeaponSlot", CurrentSlot);
        for (int i = 0; i < 3; i++)
        {
            v.SetInt($"QolLoadoutRight_{i}", (int)SlotRight[i]);
            v.SetInt($"QolLoadoutLeft_{i}", (int)SlotLeft[i]);
        }
        RecompOne.Runtime.Runtime.SaveView();
    }

    private static volatile bool _qTapped;

    static QuickWeaponSwap()
    {
        RecompOne.Runtime.Events.Event.AddListener<RecompOne.Runtime.Events.KeyboardEvent>(e =>
        {
            if (e.Pressed && !e.Repeat && e.Key == (int)Silk.NET.Input.Key.Q)
                _qTapped = true;
        });
    }

    public static void Update(CpuContext c, IMemory m)
    {
        if (!Enabled || !Game.Available || !Game.InGame || Game.IsLoading) return;
        if (!Player.IsAlucard) return;

        ushort pad = m.ReadU16(Game.PadsAddr);
        ushort tapped = (ushort)(pad & ~_prevPad);
        _prevPad = pad;

        bool qTapped = _qTapped;
        _qTapped = false;

        // Trigger on R3 (Right analog stick button) or keyboard 'Q'
        bool r3Tapped = (tapped & (ushort)Button.R3) != 0;

        if (r3Tapped || qTapped)
        {
            SwapToNextSlot(c, m);
        }
    }

    public static void SwapToNextSlot(CpuContext c, IMemory m)
    {
        // 1. Record the current equipment in the current slot
        uint currRight = Inventory.GetWornEquipment(ItemSlot.RightHand);
        uint currLeft = Inventory.GetWornEquipment(ItemSlot.LeftHand);
        SlotRight[CurrentSlot] = currRight;
        SlotLeft[CurrentSlot] = currLeft;

        // 2. Advance to the next slot (0 -> 1 -> 2 -> 0)
        CurrentSlot = (CurrentSlot + 1) % 3;

        // If next slot is completely unassigned (both 0), initialize it with current equipment
        if (SlotRight[CurrentSlot] == 0 && SlotLeft[CurrentSlot] == 0)
        {
            SlotRight[CurrentSlot] = currRight;
            SlotLeft[CurrentSlot] = currLeft;
            ToastNotifications.ShowText(
                $"Loadout {CurrentSlot + 1}",
                $"{FormatItemName(currRight)} | {FormatItemName(currLeft)}",
                null,
                2.0f);
            Save();
            return;
        }

        // 3. Equip the items in the new slot
        uint targetRight = SlotRight[CurrentSlot];
        uint targetLeft = SlotLeft[CurrentSlot];

        Inventory.SetWornEquipment(ItemSlot.RightHand, targetRight);
        Inventory.SetWornEquipment(ItemSlot.LeftHand, targetLeft);
        Inventory.RightHand = targetRight;
        Inventory.LeftHand = targetLeft;

        // Re-load weapon code and re-initialize weapon states in the engine
        try
        {
            c.A0 = 0;
            SoTN_dra.LoadWeaponPrg(c, m);
            c.A0 = 0;
            SoTN_dra.InitWeapon(c, m);

            c.A0 = 1;
            SoTN_dra.LoadWeaponPrg(c, m);
            c.A0 = 1;
            SoTN_dra.InitWeapon(c, m);
        }
        catch { }

        Save();

        // 4. Show toast notification
        string rightName = FormatItemName(targetRight);
        string leftName = FormatItemName(targetLeft);
        ToastNotifications.ShowText(
            $"Loadout {CurrentSlot + 1}",
            $"{rightName} | {leftName}",
            null,
            2.0f);

        try
        {
            GameApi.PlaySfx(0x6A9);
        }
        catch { }
    }

    public static string FormatItemName(uint id)
    {
        if (id == 0) return "Empty Hand";
        string raw = ((HandItem)id).ToString();
        return Regex.Replace(raw, "([a-z])([A-Z])", "$1 $2");
    }
}
