using System;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Memory;
using Sotn;

namespace Recompiled.Ultimate;

public static class SaturnBalance
{
    private const uint EquipmentTableAddr = 0x800A4B04u;
    private const uint EquipmentStride = 0x34u;

    private const uint EnemyTableAddr = 0x800A8900u;
    private const uint EnemyStride = 0x28u;

    private static readonly int[] IFramesItems = [
        104, 105, 106, 108, 110, 111, 112, 113, 115, 116, 117, 118,
        122, 123, 124, 125, 126, 139, 140, 141, 168
    ];

    private static bool _lastAppliedSaturn = false;
    private static bool _initialized = false;

    public static void Update(IMemory m)
    {
        if (!Game.Available || !Game.InGame) return;

        bool isSaturn = UltimateOptions.UltimateEnabled && UltimateOptions.Balance == SaturnBalanceMode.Saturn;

        if (_initialized && _lastAppliedSaturn == isSaturn) return;

        ApplyBalance(m, isSaturn);
        _lastAppliedSaturn = isSaturn;
        _initialized = true;
    }

    public static void ForceReapply(IMemory m)
    {
        _initialized = false;
        Update(m);
    }

    private static void ApplyBalance(IMemory m, bool isSaturn)
    {
        try
        {
            // 1. Enemy Invincibility Frames on heavy/slashing weapons (Saturn = 21 vs PS1 = 14)
            byte iframes = (byte)(isSaturn ? 21 : 14);
            foreach (int itemId in IFramesItems)
            {
                uint addr = EquipmentTableAddr + (uint)itemId * EquipmentStride + 0x1Au;
                m.WriteU8(addr, iframes);
            }

            // 2. Terminus Est (116): unk14 (Saturn = 5 vs PS1 = 2)
            m.WriteU8(EquipmentTableAddr + 116u * EquipmentStride + 0x14u, (byte)(isSaturn ? 5 : 2));

            // 3. Mourneblade (122): unk14 (Saturn = 6 vs PS1 = 2)
            m.WriteU8(EquipmentTableAddr + 122u * EquipmentStride + 0x14u, (byte)(isSaturn ? 6 : 2));

            // 4. Great Sword (127): lockDuration (Saturn = 16 vs PS1 = 18)
            m.WriteU8(EquipmentTableAddr + 127u * EquipmentStride + 0x15u, (byte)(isSaturn ? 16 : 18));

            // 5. Mablung Sword (124): comboMain (Saturn = 0x7FFE vs PS1 = 0x3FFE)
            m.WriteU32(EquipmentTableAddr + 124u * EquipmentStride + 0x20u, isSaturn ? 0x7FFEu : 0x3FFEu);

            // 6. Shield Rod (4): comboMain (Saturn = 0x7FFE vs PS1 = 0x3FFE) -> enables Woodland Shield spell combo!
            m.WriteU32(EquipmentTableAddr + 4u * EquipmentStride + 0x20u, isSaturn ? 0x7FFEu : 0x3FFEu);

            // 7. Boss Richter (0x133 = 307): attack (Saturn = 45 vs PS1 = 25)
            m.WriteU16(EnemyTableAddr + 0x133u * EnemyStride + 0x06u, (ushort)(isSaturn ? 45 : 25));

            // 8. Fire Warg (0x094 = 148): rareItemDropRate (Saturn = 8 vs PS1 = 4)
            m.WriteU16(EnemyTableAddr + 0x094u * EnemyStride + 0x1Eu, (ushort)(isSaturn ? 8 : 4));

            // 9. Spear Guard (0x05D = 93): flags (Saturn = 0x9A00D410 vs PS1 = 0xAA00D410)
            m.WriteU32(EnemyTableAddr + 0x05Du * EnemyStride + 0x24u, isSaturn ? 0x9A00D410u : 0xAA00D410u);
        }
        catch
        {
            // Ignore if tables are not loaded yet
        }
    }
}
