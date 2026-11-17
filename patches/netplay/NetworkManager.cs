using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Host.Window;
using RecompOne.Runtime.Memory;
using Sotn;

namespace Recompiled.Netplay;

public enum NetworkRole
{
    None,
    Host,
    Client
}

public enum NetConnectionState
{
    Disconnected,
    Listening,
    Connecting,
    Connected
}

public static class NetworkManager
{
    public static NetworkRole Role { get; private set; } = NetworkRole.None;
    public static NetConnectionState State { get; private set; } = NetConnectionState.Disconnected;
    public static string TargetIp = "127.0.0.1";
    public static int Port = 7777;
    public static string LocalPlayerName = "Alucard";
    public static string RemotePlayerName = "Partner";
    public static int PingMs { get; private set; } = 0;

    // Chat Message Log
    public static readonly List<(string Sender, string Message, string Time)> ChatHistory = [];

    private static UdpClient? _udp;
    private static IPEndPoint? _remoteEndPoint;
    private static CancellationTokenSource? _cts;
    private static long _lastPingSentTicks;
    private static float _pingTimer = 0f;
    private static float _worldSyncTimer = 0f;
    private static float _lastPacketReceivedTimer = 0f;

    // Incoming thread-safe packet queue
    private static readonly ConcurrentQueue<NetworkPacket> _incomingQueue = new();

    public static bool IsConnected => State == NetConnectionState.Connected;

    public static void Load()
    {
        var v = RecompOne.Runtime.Runtime.View;
        TargetIp = v.GetString("NetplayTargetIp", "127.0.0.1");
        Port = v.GetInt("NetplayPort", 7777);
        LocalPlayerName = v.GetString("NetplayPlayerName", "Alucard");
        NatHelper.RefreshNetworkInterfaces();
        NatHelper.FetchPublicIpAsync();
    }

    public static void Save()
    {
        var v = RecompOne.Runtime.Runtime.View;
        v.SetString("NetplayTargetIp", TargetIp);
        v.SetInt("NetplayPort", Port);
        v.SetString("NetplayPlayerName", LocalPlayerName);
        RecompOne.Runtime.Runtime.SaveView();
    }

    public static void AddChatMessage(string sender, string message, string time)
    {
        lock (ChatHistory)
        {
            ChatHistory.Add((sender, message, time));
            if (ChatHistory.Count > 120)
                ChatHistory.RemoveAt(0);
        }
    }

    public static void StartHost(int port)
    {
        Stop();
        try
        {
            Port = port;
            Role = NetworkRole.Host;
            _udp = new UdpClient(port);
            _cts = new CancellationTokenSource();
            State = NetConnectionState.Listening;
            _lastPacketReceivedTimer = 0f;

            NatHelper.TryOpenPort(port);
            NatHelper.FetchPublicIpAsync();

            Task.Run(() => ReceiveLoop(_cts.Token));
            ToastNotifications.ShowText("Multiplayer Host", $"Servidor aberto na porta {port}. Aguardando parceiro...", null, 4.0f);
        }
        catch (Exception ex)
        {
            State = NetConnectionState.Disconnected;
            ToastNotifications.ShowText("Erro ao Hospedar", ex.Message, null, 5.0f);
        }
    }

    public static void ConnectTo(string ip, int port)
    {
        Stop();
        try
        {
            TargetIp = ip;
            Port = port;
            Role = NetworkRole.Client;

            IPAddress ipAddr;
            if (!IPAddress.TryParse(ip, out ipAddr!))
            {
                var addresses = Dns.GetHostAddresses(ip);
                ipAddr = addresses.Length > 0 ? addresses[0] : IPAddress.Loopback;
            }

            _remoteEndPoint = new IPEndPoint(ipAddr, port);
            _udp = new UdpClient();
            _udp.Connect(_remoteEndPoint);
            _cts = new CancellationTokenSource();
            State = NetConnectionState.Connecting;
            _lastPacketReceivedTimer = 0f;

            Task.Run(() => ReceiveLoop(_cts.Token));

            // Send Handshake
            byte[] handshakePayload = System.Text.Encoding.UTF8.GetBytes(LocalPlayerName);
            SendPacket(new NetworkPacket(PacketOpCode.Handshake, handshakePayload));

            ToastNotifications.ShowText("Conectando...", $"Tentando conexão com {ip}:{port}...", null, 3.0f);
        }
        catch (Exception ex)
        {
            State = NetConnectionState.Disconnected;
            ToastNotifications.ShowText("Erro de Conexão", ex.Message, null, 5.0f);
        }
    }

    public static void OnClientDisconnected(string reason = "")
    {
        RemotePuppet.Reset();

        if (Role == NetworkRole.Host)
        {
            string prevPartner = RemotePlayerName;
            _remoteEndPoint = null;
            State = NetConnectionState.Listening;
            RemotePlayerName = "Aguardando...";
            PingMs = 0;
            _lastPacketReceivedTimer = 0f;

            string msg = string.IsNullOrEmpty(reason)
                ? $"{prevPartner} desconectou. O servidor continua aberto na porta {Port}!"
                : $"{prevPartner} desconectou ({reason}). O servidor continua aberto na porta {Port}!";

            ToastNotifications.ShowText("Multiplayer Host", msg, null, 5.0f);
        }
        else
        {
            Stop();
            ToastNotifications.ShowText("Multiplayer", "Sessão desconectada.", null, 4.0f);
        }
    }

    public static void Stop()
    {
        if (_udp != null)
        {
            try
            {
                if (IsConnected)
                {
                    SendPacket(new NetworkPacket(PacketOpCode.Disconnect, []));
                }
                _cts?.Cancel();
                _udp.Close();
                _udp.Dispose();
            }
            catch { }
        }

        _udp = null;
        _remoteEndPoint = null;
        _cts = null;
        Role = NetworkRole.None;
        State = NetConnectionState.Disconnected;
        PingMs = 0;
        _lastPacketReceivedTimer = 0f;
        RemotePuppet.Reset();
    }

    public static void SendPacket(NetworkPacket packet)
    {
        if (_udp == null) return;
        try
        {
            byte[] data = packet.Serialize();
            if (Role == NetworkRole.Client)
            {
                _udp.Send(data, data.Length);
            }
            else if (Role == NetworkRole.Host && _remoteEndPoint != null)
            {
                _udp.Send(data, data.Length, _remoteEndPoint);
            }
        }
        catch { }
    }

    private static async Task ReceiveLoop(CancellationToken ct)
    {
        while (!ct.IsCancellationRequested && _udp != null)
        {
            try
            {
                var result = await _udp.ReceiveAsync(ct);
                byte[] raw = result.Buffer;
                if (raw.Length > 0)
                {
                    if (Role == NetworkRole.Host && _remoteEndPoint == null)
                    {
                        _remoteEndPoint = result.RemoteEndPoint;
                    }

                    var packet = NetworkPacket.Deserialize(raw, raw.Length);
                    if (packet != null)
                    {
                        _incomingQueue.Enqueue(packet);
                    }
                }
            }
            catch (OperationCanceledException) { break; }
            catch { }
        }
    }

    public static void Update(CpuContext c, IMemory m)
    {
        // Process incoming packet queue
        while (_incomingQueue.TryDequeue(out var packet))
        {
            _lastPacketReceivedTimer = 0f;
            HandlePacket(packet, m);
        }

        if (!IsConnected) return;

        // Inactivity timeout detection (6 seconds without any packet)
        _lastPacketReceivedTimer += 1f / 60f;
        if (_lastPacketReceivedTimer >= 6.0f)
        {
            _lastPacketReceivedTimer = 0f;
            OnClientDisconnected("tempo limite de inatividade");
            return;
        }

        // Periodic Ping calculation (every 1.0s)
        _pingTimer += 1f / 60f;
        if (_pingTimer >= 1.0f)
        {
            _pingTimer = 0f;
            _lastPingSentTicks = DateTime.UtcNow.Ticks;
            SendPacket(new NetworkPacket(PacketOpCode.Ping, []));
        }

        // Periodic World State sync (every 3.0s)
        _worldSyncTimer += 1f / 60f;
        if (_worldSyncTimer >= 3.0f)
        {
            _worldSyncTimer = 0f;
            WorldStateSync.SendSyncPacket(m);
        }

        // 60Hz Local Player Transform Broadcast
        if (Game.Available && Game.InGame && !Game.IsLoading)
        {
            byte stageId = (byte)Stages.Current;
            byte roomX = (byte)Stages.RoomX;
            byte roomY = (byte)Stages.RoomY;
            short posX = (short)Player.PosX;
            short posY = (short)Player.PosY;
            short velX = (short)Player.VelocityX;
            short velY = (short)Player.VelocityY;
            bool facingLeft = Player.FacingLeft;
            byte animId = (byte)m.ReadU8(0x80073404); // Animation frame
            byte frameId = (byte)m.ReadU8(0x80073406);
            ushort curHp = (ushort)Player.Hp;
            ushort maxHp = (ushort)Player.HpMax;
            byte character = Player.IsMaria ? (byte)2 : (Player.IsRichter ? (byte)1 : (byte)0);
            byte paletteId = (byte)(Role == NetworkRole.Host ? 0 : 1); // Alternate skin for Client
            byte attackTrigger = (byte)(Game.Tapped != 0 ? 1 : 0);
            byte subWeapon = (byte)Inventory.SubWeapon;

            byte[] transformPayload = NetworkPacket.CreatePlayerTransform(
                stageId, roomX, roomY, posX, posY, velX, velY,
                facingLeft, animId, frameId, curHp, maxHp,
                character, paletteId, attackTrigger, subWeapon
            );

            SendPacket(new NetworkPacket(PacketOpCode.PlayerTransform, transformPayload));
        }

        // Update Remote Puppet in the current room
        RemotePuppet.Update(c, m);
    }

    private static void HandlePacket(NetworkPacket packet, IMemory m)
    {
        switch (packet.OpCode)
        {
            case PacketOpCode.Handshake:
                RemotePlayerName = System.Text.Encoding.UTF8.GetString(packet.Payload);
                State = NetConnectionState.Connected;
                SendPacket(new NetworkPacket(PacketOpCode.HandshakeAck, System.Text.Encoding.UTF8.GetBytes(LocalPlayerName)));
                ToastNotifications.ShowText("Multiplayer Conectado!", $"{RemotePlayerName} entrou na sessão!", null, 5.0f);
                WorldStateSync.SendSyncPacket(m);
                LobbyManager.SendLobbySync();
                break;

            case PacketOpCode.HandshakeAck:
                RemotePlayerName = System.Text.Encoding.UTF8.GetString(packet.Payload);
                State = NetConnectionState.Connected;
                ToastNotifications.ShowText("Multiplayer Conectado!", $"Conectado ao mundo de {RemotePlayerName}!", null, 5.0f);
                WorldStateSync.SendSyncPacket(m);
                LobbyManager.SendLobbySync();
                break;

            case PacketOpCode.Ping:
                SendPacket(new NetworkPacket(PacketOpCode.Pong, []));
                break;

            case PacketOpCode.Pong:
                long now = DateTime.UtcNow.Ticks;
                PingMs = (int)Math.Max(1, (now - _lastPingSentTicks) / TimeSpan.TicksPerMillisecond);
                break;

            case PacketOpCode.Disconnect:
                OnClientDisconnected();
                break;

            case PacketOpCode.PlayerTransform:
                RemotePuppet.ReceiveTransform(packet.Payload);
                break;

            case PacketOpCode.WorldState:
                WorldStateSync.ReceiveSyncPacket(packet.Payload, m);
                break;

            case PacketOpCode.BuddyWarp:
                HandleBuddyWarp(packet.Payload);
                break;

            case PacketOpCode.ChatMessage:
                if (NetworkPacket.ReadChat(packet.Payload, out string sender, out string msg))
                {
                    string time = DateTime.Now.ToString("HH:mm");
                    AddChatMessage(sender, msg, time);
                    ToastNotifications.ShowText($"[{sender}]", msg, null, 5.0f);
                }
                break;

            case PacketOpCode.PvPHit:
                if (NetworkPacket.ReadPvPHit(packet.Payload, out ushort dmg, out byte hitType, out short kbX, out string attacker))
                {
                    PvPManager.ApplyPvPHit(dmg, hitType, kbX, attacker);
                }
                break;

            case PacketOpCode.PvPKnockout:
                if (NetworkPacket.ReadPvPKnockout(packet.Payload, out string winner, out string loser, out ushort round))
                {
                    PvPManager.OnRemoteKnockout(winner, loser, round);
                }
                break;

            case PacketOpCode.LobbySync:
                LobbyManager.HandleLobbySync(packet.Payload);
                break;

            case PacketOpCode.StartGameSync:
                LobbyManager.HandleStartGameSync(packet.Payload);
                break;

            case PacketOpCode.EnemyDamage:
                EnemySyncManager.HandleRemoteEnemyDamage(packet.Payload, m);
                break;
        }
    }

    private static void HandleBuddyWarp(byte[] payload)
    {
        if (payload == null || payload.Length < 3) return;
        Stage targetStage = (Stage)payload[0];
        int targetX = payload[1];
        int targetY = payload[2];

        Stages.Load(targetStage, targetX, targetY);
        ToastNotifications.ShowText("Buddy Warp", $"Teleportado para a sala de {RemotePlayerName}!", null, 4.0f);
    }

    public static void RequestBuddyWarp()
    {
        if (!IsConnected || !RemotePuppet.IsActive) return;
        Stages.Load((Stage)RemotePuppet.StageId, RemotePuppet.RoomX, RemotePuppet.RoomY);
        ToastNotifications.ShowText("Buddy Warp", $"Teleportando para {RemotePlayerName}...", null, 3.0f);
    }
}
