using System;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Host.Window;
using RecompOne.Runtime.Memory;
using Sotn;

namespace Recompiled.Ultimate;

public static class SaturnStages
{
    // Mod Flags from sotn-ultimate:
    // MOD_PRS_SWITCH_FLAG = 0xF9
    // MOD_GDN_BOSS_FLAG = 0x167 (bit 7)
    public const int PrisonSwitchFlag = 0xF9;
    public const int GardenBossFlag = 0x167;

    public static bool IsInSaturnStage =>
        Stages.Current == Stage.UndergroundGarden ||
        Stages.Current == Stage.CursedPrison ||
        Stages.Current == Stage.HellGarden ||
        Stages.Current == Stage.SoulPrison;

    public static void WarpToUndergroundGarden()
    {
        if (!Player.CanTeleport) return;
        ToastNotifications.ShowText(
            "🌿 Underground Garden (Jardim Subterrâneo)",
            "Viajando para o Jardim Subterrâneo exclusivo do Sega Saturn...",
            null,
            3.0f);
        Player.TeleportTo(Stage.UndergroundGarden, 0, 0, 128, 128);
    }

    public static void WarpToCursedPrison()
    {
        if (!Player.CanTeleport) return;
        ToastNotifications.ShowText(
            "⛓ Cursed Prison (Prisão Amaldiçoada)",
            "Viajando para a Prisão Amaldiçoada exclusiva do Sega Saturn...",
            null,
            3.0f);
        Player.TeleportTo(Stage.CursedPrison, 0, 0, 128, 128);
    }

    public static void WarpToHellGarden()
    {
        if (!Player.CanTeleport) return;
        ToastNotifications.ShowText(
            "🔥 Hell Garden (Jardim do Inferno)",
            "Viajando para o Jardim do Inferno (Castelo Invertido Saturn)...",
            null,
            3.0f);
        Player.TeleportTo(Stage.HellGarden, 0, 0, 128, 128);
    }

    public static void WarpToSoulPrison()
    {
        if (!Player.CanTeleport) return;
        ToastNotifications.ShowText(
            "💀 Soul Prison (Prisão das Almas)",
            "Viajando para a Prisão das Almas (Castelo Invertido Saturn)...",
            null,
            3.0f);
        Player.TeleportTo(Stage.SoulPrison, 0, 0, 128, 128);
    }

    public static void Update(CpuContext c, IMemory m)
    {
        if (!UltimateOptions.UltimateEnabled || !UltimateOptions.SaturnStages) return;
        if (!Game.Available || !Game.InGame || Game.IsLoading) return;

        // Entrance shaft detection:
        // When Alucard drops down to the bottom of the Entrance shaft in normal castle:
        if (Stages.Current == Stage.CastleEntrance || Stages.Current == Stage.CastleEntranceFirstVisit)
        {
            CheckNormalEntranceShaft();
        }
        else if (Stages.Current == Stage.ReverseEntrance)
        {
            CheckReverseEntranceShaft();
        }
    }

    private static void CheckNormalEntranceShaft()
    {
        // Shaft room coordinates in Entrance
        int roomX = Stages.RoomX;
        int roomY = Stages.RoomY;
        int posY = Player.PosY;

        // If player falls below the bottom floor in the shaft room (roomY >= 42 and posY > 440)
        if (roomX == 3 && roomY >= 42 && posY > 440)
        {
            // Drop down into Underground Garden
            WarpToUndergroundGarden();
        }
    }

    private static void CheckReverseEntranceShaft()
    {
        int roomX = Stages.RoomX;
        int roomY = Stages.RoomY;
        int posY = Player.PosY;

        // Reverse Entrance shaft room
        if (roomX == 60 && roomY <= 21 && posY < 50)
        {
            WarpToHellGarden();
        }
    }
}
