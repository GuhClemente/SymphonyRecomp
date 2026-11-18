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
            Entity target = new(0);
            var entity = Entities.At(entitySlot);
            if (entity.IsValid && entity.IsEnemy && entity.HitPoints > 0)
            {
                target = entity;
            }
            else
            {
                // Slot mismatch fallback: find the first active enemy in the room
                foreach (var e in Entities.Enemies())
                {
                    if (e.IsValid && e.HitPoints > 0)
                    {
                        target = e;
                        break;
                    }
                }
            }

            if (target.IsValid)
            {
                target.HitPoints = remainingHp;
                target.NFramesInvincibility = 12; // Visual damage flash frame

                if (isDead || remainingHp <= 0)
                {
                    target.HitPoints = 0;
                    target.Kill();
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
