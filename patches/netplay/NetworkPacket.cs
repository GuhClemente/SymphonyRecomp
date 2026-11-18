using System;
using System.IO;
using System.Text;

namespace Recompiled.Netplay;

public enum PacketOpCode : byte
{
    Ping = 0x01,
    Pong = 0x02,
    Handshake = 0x03,
    HandshakeAck = 0x04,
    Disconnect = 0x05,
    DiscoveryPing = 0x06,
    DiscoveryPong = 0x07,
    PlayerTransform = 0x10,
    WorldState = 0x20,
    BuddyWarp = 0x30,
    PvPHit = 0x40,
    PvPKnockout = 0x41,
    ChatMessage = 0x50,
    LobbySync = 0x60,
    StartGameSync = 0x61,
    EnterLobbyRoom = 0x62,
    EnemyDamage = 0x70,
    PlayerDowned = 0x80,
    PlayerRevived = 0x81,
    CoopPing = 0x82
}

public sealed class NetworkPacket
{
    public PacketOpCode OpCode { get; set; }
    public byte[] Payload { get; set; } = [];

    public NetworkPacket(PacketOpCode opCode, byte[] payload)
    {
        OpCode = opCode;
        Payload = payload;
    }

    public byte[] Serialize()
    {
        byte[] data = new byte[1 + Payload.Length];
        data[0] = (byte)OpCode;
        Buffer.BlockCopy(Payload, 0, data, 1, Payload.Length);
        return data;
    }

    public static NetworkPacket? Deserialize(byte[] raw, int length)
    {
        if (raw == null || length < 1) return null;
        PacketOpCode op = (PacketOpCode)raw[0];
        byte[] payload = new byte[length - 1];
        if (length > 1)
        {
            Buffer.BlockCopy(raw, 1, payload, 0, length - 1);
        }
        return new NetworkPacket(op, payload);
    }

    // 60Hz Player Transform Delta Packet (~36 bytes)
    public static byte[] CreatePlayerTransform(
        byte stageId,
        byte roomX,
        byte roomY,
        short posX,
        short posY,
        short velX,
        short velY,
        bool facingLeft,
        byte animId,
        byte frameId,
        ushort curHp,
        ushort maxHp,
        byte character,
        byte paletteId,
        byte attackTrigger,
        byte subWeapon
    )
    {
        using var ms = new MemoryStream(36);
        using var w = new BinaryWriter(ms);
        w.Write(stageId);
        w.Write(roomX);
        w.Write(roomY);
        w.Write(posX);
        w.Write(posY);
        w.Write(velX);
        w.Write(velY);
        w.Write((byte)(facingLeft ? 1 : 0));
        w.Write(animId);
        w.Write(frameId);
        w.Write(curHp);
        w.Write(maxHp);
        w.Write(character);
        w.Write(paletteId);
        w.Write(attackTrigger);
        w.Write(subWeapon);
        return ms.ToArray();
    }

    public static bool ReadPlayerTransform(
        byte[] payload,
        out byte stageId,
        out byte roomX,
        out byte roomY,
        out short posX,
        out short posY,
        out short velX,
        out short velY,
        out bool facingLeft,
        out byte animId,
        out byte frameId,
        out ushort curHp,
        out ushort maxHp,
        out byte character,
        out byte paletteId,
        out byte attackTrigger,
        out byte subWeapon
    )
    {
        stageId = roomX = roomY = animId = frameId = character = paletteId = attackTrigger = subWeapon = 0;
        posX = posY = velX = velY = 0;
        facingLeft = false;
        curHp = maxHp = 0;

        if (payload == null || payload.Length < 21) return false;

        using var ms = new MemoryStream(payload);
        using var r = new BinaryReader(ms);
        stageId = r.ReadByte();
        roomX = r.ReadByte();
        roomY = r.ReadByte();
        posX = r.ReadInt16();
        posY = r.ReadInt16();
        velX = r.ReadInt16();
        velY = r.ReadInt16();
        facingLeft = r.ReadByte() != 0;
        animId = r.ReadByte();
        frameId = r.ReadByte();
        curHp = r.ReadUInt16();
        maxHp = r.ReadUInt16();
        character = r.ReadByte();
        paletteId = r.ReadByte();
        attackTrigger = r.ReadByte();
        subWeapon = r.ReadByte();
        return true;
    }

    // World State Sync (Visited map tiles, Boss Defeat bitmask, Levers)
    public static byte[] CreateWorldState(byte[] mapBitset, byte[] bossBitset)
    {
        using var ms = new MemoryStream();
        using var w = new BinaryWriter(ms);
        w.Write((ushort)mapBitset.Length);
        w.Write(mapBitset);
        w.Write((ushort)bossBitset.Length);
        w.Write(bossBitset);
        return ms.ToArray();
    }

    public static bool ReadWorldState(byte[] payload, out byte[] mapBitset, out byte[] bossBitset)
    {
        mapBitset = [];
        bossBitset = [];
        if (payload == null || payload.Length < 4) return false;

        using var ms = new MemoryStream(payload);
        using var r = new BinaryReader(ms);
        ushort mapLen = r.ReadUInt16();
        mapBitset = r.ReadBytes(mapLen);
        ushort bossLen = r.ReadUInt16();
        bossBitset = r.ReadBytes(bossLen);
        return true;
    }

    // Chat / Message Packet
    public static byte[] CreateChat(string sender, string message)
    {
        using var ms = new MemoryStream();
        using var w = new BinaryWriter(ms, Encoding.UTF8);
        w.Write(sender);
        w.Write(message);
        return ms.ToArray();
    }

    public static bool ReadChat(byte[] payload, out string sender, out string message)
    {
        sender = "";
        message = "";
        if (payload == null || payload.Length < 2) return false;

        using var ms = new MemoryStream(payload);
        using var r = new BinaryReader(ms, Encoding.UTF8);
        sender = r.ReadString();
        message = r.ReadString();
        return true;
    }

    // PvP Hit Packet (OpCode 0x40)
    public static byte[] CreatePvPHit(ushort damage, byte hitType, short knockbackX, string attackerName)
    {
        using var ms = new MemoryStream();
        using var w = new BinaryWriter(ms, Encoding.UTF8);
        w.Write(damage);
        w.Write(hitType);
        w.Write(knockbackX);
        w.Write(attackerName ?? "");
        return ms.ToArray();
    }

    public static bool ReadPvPHit(byte[] payload, out ushort damage, out byte hitType, out short knockbackX, out string attackerName)
    {
        damage = 0;
        hitType = 0;
        knockbackX = 0;
        attackerName = "";
        if (payload == null || payload.Length < 5) return false;

        using var ms = new MemoryStream(payload);
        using var r = new BinaryReader(ms, Encoding.UTF8);
        damage = r.ReadUInt16();
        hitType = r.ReadByte();
        knockbackX = r.ReadInt16();
        if (ms.Position < ms.Length)
        {
            attackerName = r.ReadString();
        }
        return true;
    }

    // PvP Knockout Packet (OpCode 0x41)
    public static byte[] CreatePvPKnockout(string winnerName, string loserName, ushort round)
    {
        using var ms = new MemoryStream();
        using var w = new BinaryWriter(ms, Encoding.UTF8);
        w.Write(winnerName ?? "");
        w.Write(loserName ?? "");
        w.Write(round);
        return ms.ToArray();
    }

    public static bool ReadPvPKnockout(byte[] payload, out string winnerName, out string loserName, out ushort round)
    {
        winnerName = "";
        loserName = "";
        round = 0;
        if (payload == null || payload.Length < 4) return false;

        using var ms = new MemoryStream(payload);
        using var r = new BinaryReader(ms, Encoding.UTF8);
        winnerName = r.ReadString();
        loserName = r.ReadString();
        round = r.ReadUInt16();
        return true;
    }

    // Pre-Game Lobby Sync Packet (OpCode 0x60)
    public static byte[] CreateLobbySync(bool isReady, byte character, byte stageIndex)
    {
        return [(byte)(isReady ? 1 : 0), character, stageIndex];
    }

    public static bool ReadLobbySync(byte[] payload, out bool isReady, out byte character, out byte stageIndex)
    {
        isReady = false;
        character = 0;
        stageIndex = 0;
        if (payload == null || payload.Length < 3) return false;
        isReady = payload[0] != 0;
        character = payload[1];
        stageIndex = payload[2];
        return true;
    }

    // Synchronized Game Launch Packet (OpCode 0x61)
    public static byte[] CreateStartGameSync(byte stage, byte roomX, byte roomY, short posX, short posY)
    {
        using var ms = new MemoryStream();
        using var w = new BinaryWriter(ms);
        w.Write(stage);
        w.Write(roomX);
        w.Write(roomY);
        w.Write(posX);
        w.Write(posY);
        return ms.ToArray();
    }

    public static bool ReadStartGameSync(byte[] payload, out byte stage, out byte roomX, out byte roomY, out short posX, out short posY)
    {
        stage = roomX = roomY = 0;
        posX = posY = 0;
        if (payload == null || payload.Length < 7) return false;

        using var ms = new MemoryStream(payload);
        using var r = new BinaryReader(ms);
        stage = r.ReadByte();
        roomX = r.ReadByte();
        roomY = r.ReadByte();
        posX = r.ReadInt16();
        posY = r.ReadInt16();
        return true;
    }

    // Sanctuary Lobby Room Sync Packet (OpCode 0x62)
    public static byte[] CreateEnterLobbyRoom() => [1];
    public static bool ReadEnterLobbyRoom(byte[] payload) => payload != null && payload.Length > 0;

    // Shared Room Enemy Damage Sync Packet (OpCode 0x70)
    public static byte[] CreateEnemyDamage(byte stage, byte roomX, byte roomY, byte entitySlot, short damage, short remainingHp, bool isDead)
    {
        using var ms = new MemoryStream();
        using var w = new BinaryWriter(ms);
        w.Write(stage);
        w.Write(roomX);
        w.Write(roomY);
        w.Write(entitySlot);
        w.Write(damage);
        w.Write(remainingHp);
        w.Write((byte)(isDead ? 1 : 0));
        return ms.ToArray();
    }

    public static bool ReadEnemyDamage(byte[] payload, out byte stage, out byte roomX, out byte roomY, out byte entitySlot, out short damage, out short remainingHp, out bool isDead)
    {
        stage = roomX = roomY = entitySlot = 0;
        damage = remainingHp = 0;
        isDead = false;
        if (payload == null || payload.Length < 9) return false;

        using var ms = new MemoryStream(payload);
        using var r = new BinaryReader(ms);
        stage = r.ReadByte();
        roomX = r.ReadByte();
        roomY = r.ReadByte();
        entitySlot = r.ReadByte();
        damage = r.ReadInt16();
        remainingHp = r.ReadInt16();
        isDead = r.ReadByte() != 0;
        return true;
    }

    // Co-op Downed State Packet (OpCode 0x80)
    public static byte[] CreatePlayerDowned(string playerName, byte stage, byte roomX, byte roomY)
    {
        using var ms = new MemoryStream();
        using var w = new BinaryWriter(ms);
        w.Write(playerName ?? "");
        w.Write(stage);
        w.Write(roomX);
        w.Write(roomY);
        return ms.ToArray();
    }

    public static bool ReadPlayerDowned(byte[] payload, out string playerName, out byte stage, out byte roomX, out byte roomY)
    {
        playerName = "";
        stage = roomX = roomY = 0;
        if (payload == null || payload.Length < 4) return false;

        using var ms = new MemoryStream(payload);
        using var r = new BinaryReader(ms);
        playerName = r.ReadString();
        stage = r.ReadByte();
        roomX = r.ReadByte();
        roomY = r.ReadByte();
        return true;
    }

    // Co-op Revival Packet (OpCode 0x81)
    public static byte[] CreatePlayerRevived(string reviverName, ushort restoredHp)
    {
        using var ms = new MemoryStream();
        using var w = new BinaryWriter(ms);
        w.Write(reviverName ?? "");
        w.Write(restoredHp);
        return ms.ToArray();
    }

    public static bool ReadPlayerRevived(byte[] payload, out string reviverName, out ushort restoredHp)
    {
        reviverName = "";
        restoredHp = 0;
        if (payload == null || payload.Length < 3) return false;

        using var ms = new MemoryStream(payload);
        using var r = new BinaryReader(ms);
        reviverName = r.ReadString();
        restoredHp = r.ReadUInt16();
        return true;
    }

    // Co-op Tactical Ping Packet (OpCode 0x82)
    public static byte[] CreateCoopPing(byte pingType, byte stage, byte roomX, byte roomY, string sender)
    {
        using var ms = new MemoryStream();
        using var w = new BinaryWriter(ms);
        w.Write(pingType);
        w.Write(stage);
        w.Write(roomX);
        w.Write(roomY);
        w.Write(sender ?? "");
        return ms.ToArray();
    }

    public static bool ReadCoopPing(byte[] payload, out byte pingType, out byte stage, out byte roomX, out byte roomY, out string sender)
    {
        pingType = stage = roomX = roomY = 0;
        sender = "";
        if (payload == null || payload.Length < 5) return false;

        using var ms = new MemoryStream(payload);
        using var r = new BinaryReader(ms);
        pingType = r.ReadByte();
        stage = r.ReadByte();
        roomX = r.ReadByte();
        roomY = r.ReadByte();
        sender = r.ReadString();
        return true;
    }

    // Discovery Pong Packet (OpCode 0x07)
    public static byte[] CreateDiscoveryPong(ushort port, string hostName, byte character, string stageName, byte players, bool isInGame)
    {
        using var ms = new MemoryStream();
        using var w = new BinaryWriter(ms);
        w.Write(port);
        w.Write(hostName ?? "Host");
        w.Write(character);
        w.Write(stageName ?? "Entrada");
        w.Write(players);
        w.Write(isInGame);
        return ms.ToArray();
    }

    public static bool ReadDiscoveryPong(byte[] payload, out ushort port, out string hostName, out byte character, out string stageName, out byte players, out bool isInGame)
    {
        port = 0;
        hostName = "";
        character = 0;
        stageName = "";
        players = 0;
        isInGame = false;
        if (payload == null || payload.Length < 6) return false;

        using var ms = new MemoryStream(payload);
        using var r = new BinaryReader(ms);
        port = r.ReadUInt16();
        hostName = r.ReadString();
        character = r.ReadByte();
        stageName = r.ReadString();
        players = r.ReadByte();
        isInGame = r.ReadBoolean();
        return true;
    }
}
