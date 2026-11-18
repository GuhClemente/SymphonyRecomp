using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;
using Recompiled.Ultimate;

namespace Recompiled.Netplay;

public sealed class DiscoveredServer
{
    public IPAddress Address { get; set; } = IPAddress.Loopback;
    public int Port { get; set; } = 7777;
    public string HostName { get; set; } = "Host";
    public byte Character { get; set; }
    public string StageName { get; set; } = "Entrada";
    public byte Players { get; set; } = 1;
    public bool IsInGame { get; set; }
    public DateTime LastSeen { get; set; } = DateTime.UtcNow;

    public bool IsLocalhost => IPAddress.IsLoopback(Address) || Address.ToString() == "127.0.0.1";

    public string CharacterName => Character switch
    {
        1 => "Richter (Clássico)",
        2 => "Richter (Saturn)",
        5 => "Richter (Couro Marrom)",
        3 => "Maria (Saturn)",
        4 => "Maria (PSP)",
        _ => "Alucard"
    };

    public string Key => $"{Address}:{Port}";
}

public static class LanDiscoveryService
{
    private static readonly ConcurrentDictionary<string, DiscoveredServer> _servers = new();
    private static CancellationTokenSource? _cts;
    private static Task? _scanTask;
    private static UdpClient? _discoverySocket;

    public static IReadOnlyCollection<DiscoveredServer> Servers => _servers.Values.ToArray();

    public static bool HasDiscoveredLocalServer(out DiscoveredServer? server)
    {
        PruneExpired();
        foreach (var s in _servers.Values)
        {
            if (s.IsLocalhost)
            {
                server = s;
                return true;
            }
        }
        server = null;
        return false;
    }

    public static void Start()
    {
        if (_scanTask != null) return;
        _cts = new CancellationTokenSource();
        _scanTask = Task.Run(() => ScanLoop(_cts.Token));
    }

    public static void Stop()
    {
        _cts?.Cancel();
        _cts?.Dispose();
        _cts = null;
        _discoverySocket?.Dispose();
        _discoverySocket = null;
        _scanTask = null;
    }

    private static async Task ScanLoop(CancellationToken token)
    {
        try
        {
            _discoverySocket = new UdpClient(0)
            {
                EnableBroadcast = true
            };
            _discoverySocket.Client.ReceiveTimeout = 500;
        }
        catch
        {
            return;
        }

        byte[] pingPacket = new NetworkPacket(PacketOpCode.DiscoveryPing, []).Serialize();

        // Background listener task for pongs
        _ = Task.Run(async () =>
        {
            while (!token.IsCancellationRequested && _discoverySocket != null)
            {
                try
                {
                    var res = await _discoverySocket.ReceiveAsync(token);
                    if (res.Buffer.Length > 0 && res.Buffer[0] == (byte)PacketOpCode.DiscoveryPong)
                    {
                        var packet = NetworkPacket.Deserialize(res.Buffer, res.Buffer.Length);
                        if (packet != null && NetworkPacket.ReadDiscoveryPong(
                            packet.Payload,
                            out ushort port,
                            out string hostName,
                            out byte character,
                            out string stageName,
                            out byte players,
                            out bool inGame))
                        {
                            var s = new DiscoveredServer
                            {
                                Address = res.RemoteEndPoint.Address,
                                Port = port > 0 ? port : res.RemoteEndPoint.Port,
                                HostName = hostName,
                                Character = character,
                                StageName = stageName,
                                Players = players,
                                IsInGame = inGame,
                                LastSeen = DateTime.UtcNow
                            };
                            _servers[s.Key] = s;
                        }
                    }
                }
                catch (OperationCanceledException) { break; }
                catch { await Task.Delay(100, token); }
            }
        }, token);

        while (!token.IsCancellationRequested)
        {
            try
            {
                PruneExpired();

                // Only send discovery probes if not currently connected in a session
                if (!NetworkManager.IsConnected && NetworkManager.Role != NetworkRole.Host && _discoverySocket != null)
                {
                    // 1. Probe localhost port 7777 and 7778
                    await _discoverySocket.SendAsync(pingPacket, pingPacket.Length, new IPEndPoint(IPAddress.Loopback, 7777));
                    await _discoverySocket.SendAsync(pingPacket, pingPacket.Length, new IPEndPoint(IPAddress.Loopback, 7778));

                    // 2. Broadcast on LAN
                    await _discoverySocket.SendAsync(pingPacket, pingPacket.Length, new IPEndPoint(IPAddress.Broadcast, 7777));
                }
            }
            catch { }

            try
            {
                await Task.Delay(1500, token);
            }
            catch (OperationCanceledException) { break; }
        }
    }

    private static void PruneExpired()
    {
        var now = DateTime.UtcNow;
        foreach (var kvp in _servers)
        {
            if ((now - kvp.Value.LastSeen).TotalSeconds > 4.5)
            {
                _servers.TryRemove(kvp.Key, out _);
            }
        }
    }

    /// <summary>
    /// Ação inteligente de 1-Clique:
    /// Se houver servidor no localhost ou LAN, conecta automaticamente.
    /// Caso contrário, inicia como Host na porta 7777.
    /// </summary>
    public static async Task SmartConnectOrHostAsync()
    {
        PruneExpired();

        // 1. Check if a server was already discovered
        foreach (var s in _servers.Values)
        {
            Console.WriteLine($"[SmartConnect] Servidor encontrado: {s.Address}:{s.Port} ({s.HostName} - {s.CharacterName}). Conectando...");
            NetworkManager.TargetIp = s.Address.ToString();
            NetworkManager.Port = s.Port;
            NetworkManager.ConnectTo(s.Address.ToString(), s.Port);
            return;
        }

        // 2. Try quick active probe on 127.0.0.1:7777
        try
        {
            using var testSocket = new UdpClient();
            testSocket.Client.ReceiveTimeout = 300;
            byte[] ping = new NetworkPacket(PacketOpCode.DiscoveryPing, []).Serialize();
            await testSocket.SendAsync(ping, ping.Length, new IPEndPoint(IPAddress.Loopback, 7777));

            var receiveTask = testSocket.ReceiveAsync();
            var completed = await Task.WhenAny(receiveTask, Task.Delay(350));
            if (completed == receiveTask)
            {
                var res = receiveTask.Result;
                if (res.Buffer.Length > 0 && res.Buffer[0] == (byte)PacketOpCode.DiscoveryPong)
                {
                    Console.WriteLine("[SmartConnect] Host local ativo detectado na porta 7777! Conectando como Cliente...");
                    NetworkManager.TargetIp = "127.0.0.1";
                    NetworkManager.Port = 7777;
                    NetworkManager.ConnectTo("127.0.0.1", 7777);
                    return;
                }
            }
        }
        catch { }

        // 3. No server found -> Start as Host
        Console.WriteLine("[SmartConnect] Nenhum host ativo detectado. Iniciando como Host (P1) na porta 7777...");
        NetworkManager.Port = 7777;
        NetworkManager.StartHost(7777);
    }
}
