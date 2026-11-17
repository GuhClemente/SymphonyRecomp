using System;
using RecompOne.Runtime.Memory;
using Sotn;

namespace Recompiled.Netplay;

public static class EnemySyncManager
{
    private static bool _isApplyingRemoteDamage = false;
    public static bool IsApplyingRemoteDamage => _isApplyingRemoteDamage;

    public static void BroadcastEnemyDamage(byte stage, byte roomX, byte roomY, byte slot, short damage, short remainingHp, bool isDead)
    {
        if (_isApplyingRemoteDamage) return;
        if (!NetworkManager.IsConnected || !RemotePuppet.IsActive) return;

        byte[] payload = NetworkPacket.CreateEnemyDamage(stage, roomX, roomY, slot, damage, remainingHp, isDead);
        NetworkManager.SendPacket(new NetworkPacket(PacketOpCode.EnemyDamage, payload));
    }

    public static void HandleRemoteEnemyDamage(byte[] payload, IMemory m)
    {
        if (!Game.Available || !Game.InGame || Game.IsLoading) return;

        if (!NetworkPacket.ReadEnemyDamage(
            payload,
            out byte stage,
            out byte roomX,
            out byte roomY,
            out byte entitySlot,
            out short damage,
            out short remainingHp,
            out bool isDead))
        {
            return;
        }

        // Only process damage if both players are currently in the exact same stage and room
        if (stage != (byte)Stages.Current || roomX != (byte)Stages.RoomX || roomY != (byte)Stages.RoomY)
            return;

        _isApplyingRemoteDamage = true;
        try
        {
            var entity = Entities.At(entitySlot);
            if (entity.IsValid && entity.HitPoints > 0)
            {
                entity.HitPoints = remainingHp;
                entity.NFramesInvincibility = 12; // Visual flash frame

                if (isDead)
                {
                    entity.Kill();
                }
            }
        }
        catch { }
        finally
        {
            _isApplyingRemoteDamage = false;
        }
    }
}
