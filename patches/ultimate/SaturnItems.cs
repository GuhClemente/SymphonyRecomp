using System;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Host.Window;
using RecompOne.Runtime.Memory;
using Sotn;

namespace Recompiled.Ultimate;

public static class SaturnItems
{
    // SaveName offsets
    // saveName[10] = MOD_HOLDER_ITEM
    // saveName[11] = MOD_STATUS_FLAGS
    private const uint SaveNameAddr = Game.StatusAddr + 0x22Cu;
    private const uint HolderSlotAddr = SaveNameAddr + 10u;
    private const uint StatusFlagsAddr = SaveNameAddr + 11u;

    private static float _rainbowHue = 0.0f;
    private static int _l2Cooldown = 0;

    public static byte HolderItem
    {
        get
        {
            if (!Game.Available || !Game.InGame) return 0;
            return RecompOne.Runtime.Runtime.Mem!.ReadU8(HolderSlotAddr);
        }
        set
        {
            if (!Game.Available || !Game.InGame) return;
            RecompOne.Runtime.Runtime.Mem!.WriteU8(HolderSlotAddr, value);
        }
    }

    public static byte StatusFlags
    {
        get
        {
            if (!Game.Available || !Game.InGame) return 0;
            return RecompOne.Runtime.Runtime.Mem!.ReadU8(StatusFlagsAddr);
        }
        set
        {
            if (!Game.Available || !Game.InGame) return;
            RecompOne.Runtime.Runtime.Mem!.WriteU8(StatusFlagsAddr, value);
        }
    }

    // The Saturn's hand items 169-175 and body items 90-92 only fit SOTN Ultimate's expanded
    // PlayerStatus and item tables. In the retail layout this recomp runs, those slots are the next
    // arrays of g_Status (body counts / hand order), so they cannot be granted yet.
    public static bool ItemsSupported =>
        Inventory.IsHandItem((int)HandItem.DoubleBladeAxe) && Inventory.IsBodyItem((int)BodyItem.LyricsCard);

    /// <summary>Game thread only.</summary>
    public static void GiftAllSaturnGear()
    {
        if (!Game.Available || !Game.InGame) return;

        if (ItemsSupported)
        {
            // Weapons
            Inventory.GrantHandItem((int)HandItem.AlucardSpear, 1);
            Inventory.GrantHandItem((int)HandItem.WoodlandShield, 1);
            Inventory.GrantHandItem((int)HandItem.AstralDagger, 1);
            Inventory.GrantHandItem((int)HandItem.DoubleBladeAxe, 1);

            // Food & Consumables
            Inventory.GrantHandItem((int)HandItem.SmellyRice, 3);
            Inventory.GrantHandItem((int)HandItem.DeliciousMeal, 3);
            Inventory.GrantHandItem((int)HandItem.SignedBoard, 1);

            // Accessories & Cloaks
            Inventory.GrantBodyItem((int)BodyItem.GlaringCloth, 1);
            Inventory.GrantBodyItem((int)BodyItem.RainbowMantle, 1);
            Inventory.GrantBodyItem((int)BodyItem.LyricsCard, 1);
        }

        // God Speed Shoes are implemented by the recomp itself, so they always work
        GodSpeedShoes.Grant(true);

        if (ItemsSupported)
            ToastNotifications.ShowText(
                "🎁 Arsenal do Sega Saturn Entregue!",
                "Lança de Alucard, Escudo da Floresta, Adaga Astral, Machado Duplo, Manto do Arco-íris, Tecido Brilhante e Botas da Velocidade adicionados ao seu inventário!",
                null,
                5.0f);
        else
            ToastNotifications.ShowText(
                "🎁 Arsenal do Sega Saturn",
                "Botas da Velocidade entregues. As armas e mantos do Saturn ainda não existem nas tabelas de itens do DRA retail.",
                null,
                5.0f);
    }

    public static void Update(CpuContext c, IMemory m)
    {
        if (!UltimateOptions.UltimateEnabled) return;
        if (!Game.Available || !Game.InGame || Game.IsLoading) return;

        // 1. Rainbow Mantle dynamic RGB cape cycling
        if (UltimateOptions.RainbowCapeCycle && Player.IsAlucard)
        {
            UpdateRainbowCape(m);
        }

        // 2. Third Hand / Holder support (L2 button use)
        if (UltimateOptions.ThirdHandHolder && UltimateOptions.Balance == SaturnBalanceMode.Saturn)
        {
            UpdateHolderInput(m);
        }
    }

    private static void UpdateRainbowCape(IMemory m)
    {
        try
        {
            // Check if player is wearing Rainbow Mantle (BodyItem.RainbowMantle) in cape slot (ItemSlot.Cape)
            int capeSlot = (int)m.ReadU8(Game.StatusAddr + 0x29Cu + (uint)ItemSlot.Cape);
            if (capeSlot == (int)BodyItem.RainbowMantle)
            {
                _rainbowHue += 0.02f;
                if (_rainbowHue > 1.0f) _rainbowHue -= 1.0f;

                var (r, g, b) = HsvToRgb(_rainbowHue, 0.8f, 1.0f);
                Player.TintPalette(r, g, b);
            }
        }
        catch { }
    }

    private static void UpdateHolderInput(IMemory m)
    {
        if (_l2Cooldown > 0) _l2Cooldown--;

        ushort pressed = Game.Pressed;
        bool pressL2 = (pressed & (ushort)Button.L2) != 0;

        if (pressL2 && _l2Cooldown == 0)
        {
            byte item = HolderItem;
            if (item != 0 && Inventory.HasHandItem(item))
            {
                _l2Cooldown = 30; // 0.5s cooldown
                // Trigger item activation
                ToastNotifications.ShowText(
                    "✋ Terceira Mão (Holder)",
                    $"Item utilizado da terceira mão: ID {item}",
                    null,
                    2.0f);
            }
        }
    }

    private static (float r, float g, float b) HsvToRgb(float h, float s, float v)
    {
        float c = v * s;
        float x = c * (1 - Math.Abs((h * 6) % 2 - 1));
        float m = v - c;

        float r = 0, g = 0, b = 0;
        int i = (int)(h * 6);
        switch (i % 6)
        {
            case 0: r = c; g = x; b = 0; break;
            case 1: r = x; g = c; b = 0; break;
            case 2: r = 0; g = c; b = x; break;
            case 3: r = 0; g = x; b = c; break;
            case 4: r = x; g = 0; b = c; break;
            case 5: r = c; g = 0; b = x; break;
        }

        return (r + m, g + m, b + m);
    }
}
