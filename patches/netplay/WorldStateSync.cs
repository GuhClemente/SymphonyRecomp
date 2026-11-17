using System;
using RecompOne.Runtime.Host.Window;
using RecompOne.Runtime.Memory;
using Sotn;

namespace Recompiled.Netplay;

public static class WorldStateSync
{
    // Full castle map size (64 columns x 32 rows = 2048 bytes / 0x800)
    public const int MapBufferLength = Map.Length;
    public static readonly byte[] RemoteMapBitset = new byte[MapBufferLength];
    public static readonly byte[] LocalMapBitset = new byte[MapBufferLength];

    public static void SendSyncPacket(IMemory m)
    {
        if (!Game.Available || !NetworkManager.IsConnected) return;

        // Read Map Visited Buffer (2048 bytes for complete castle map)
        byte[] mapData = new byte[MapBufferLength];
        for (uint i = 0; i < MapBufferLength; i++)
        {
            byte b = m.ReadU8(Map.CastleMapAddr + i);
            mapData[i] = b;
            LocalMapBitset[i] = b;
        }

        // Read Time Attack / Boss Defeat events (64 bytes)
        byte[] bossData = new byte[64];
        for (uint i = 0; i < 64; i++)
        {
            bossData[i] = m.ReadU8(Progress.TimeAttackAddr + i);
        }

        byte[] payload = NetworkPacket.CreateWorldState(mapData, bossData);
        NetworkManager.SendPacket(new NetworkPacket(PacketOpCode.WorldState, payload));
    }

    public static void ReceiveSyncPacket(byte[] payload, IMemory m)
    {
        if (!Game.Available || !NetworkPacket.ReadWorldState(payload, out byte[] mapBitset, out byte[] bossBitset))
            return;

        // Store partner's exploration map
        Buffer.BlockCopy(mapBitset, 0, RemoteMapBitset, 0, Math.Min(mapBitset.Length, RemoteMapBitset.Length));

        int newRoomsDiscovered = 0;

        // Bitwise OR merge discovered map rooms into game memory
        for (uint i = 0; i < mapBitset.Length && i < MapBufferLength; i++)
        {
            byte localByte = m.ReadU8(Map.CastleMapAddr + i);
            byte remoteByte = mapBitset[i];
            byte merged = (byte)(localByte | remoteByte);

            if (merged != localByte)
            {
                newRoomsDiscovered++;
                m.WriteU8(Map.CastleMapAddr + i, merged);
            }
        }

        // Sync Boss Defeats
        for (uint i = 0; i < bossBitset.Length && i < 64; i += 2)
        {
            ushort localTime = m.ReadU16(Progress.TimeAttackAddr + i);
            ushort remoteTime = BitConverter.ToUInt16(bossBitset, (int)i);

            if (localTime == 0 && remoteTime > 0)
            {
                m.WriteU16(Progress.TimeAttackAddr + i, remoteTime);
            }
        }

        if (newRoomsDiscovered > 0)
        {
            ToastNotifications.ShowText("🗺️ Mapa Compartilhado", $"{NetworkManager.RemotePlayerName} explorou novas áreas do castelo!", null, 3.0f);
        }
    }
}
