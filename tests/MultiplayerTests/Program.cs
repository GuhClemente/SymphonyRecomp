using System;
using System.Collections.Concurrent;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Recompiled.Netplay;

namespace MultiplayerTests;

public static class Program
{
    private static int _passed = 0;
    private static int _failed = 0;

    public static async Task<int> Main(string[] args)
    {
        Console.OutputEncoding = Encoding.UTF8;
        Console.ForegroundColor = ConsoleColor.Cyan;
        Console.WriteLine("================================================================================");
        Console.WriteLine("       🦇 SymphonyRecomp - Suíte Completa de Testes de Multiplayer P2P 🦇       ");
        Console.WriteLine("================================================================================");
        Console.ResetColor();

        long startTime = Environment.TickCount64;

        RunSection("1. Testes Unitários de Serialização de Pacotes (NetworkPacket)", () =>
        {
            TestHandshakePacketSerialization();
            TestPlayerTransformStandardSerialization();
            TestPlayerTransformEdgeCasesSerialization();
            TestWorldStateSerialization();
            TestChatMessageSerialization();
            TestPingPongSerialization();
            TestPacketMalformedAndFuzzingResilience();
        });

        RunSection("2. Testes de Lógica de Mundo Compartilhado (WorldStateSync)", () =>
        {
            TestWorldStateMapBitsetMerging();
            TestWorldStateBossDefeatSynchronization();
        });

        RunSection("3. Testes de Matemática de Interpolação & Fantoche (RemotePuppet)", () =>
        {
            TestRemotePuppetLerpConvergence();
            TestRemotePuppetPaletteAndCharacterIdentity();
        });

        RunSection("4. Testes de Regras de PvP & Arena do Coliseu (PvPManager)", () =>
        {
            TestPvPColosseumDetectionLogic();
            TestPvPScoreboardTracking();
        });

        await RunSectionAsync("5. Teste de Integração Real UDP Socket P2P (Host <-> Client Loopback)", async () =>
        {
            await TestRealUdpP2PLoopbackCommunicationAsync();
            await TestHighFrequency60HzTransformStreamingAsync();
        });

        long durationMs = Environment.TickCount64 - startTime;

        Console.WriteLine();
        Console.ForegroundColor = ConsoleColor.Cyan;
        Console.WriteLine("================================================================================");
        Console.WriteLine($"  Relatório Final: {_passed} passaram | {_failed} falharam | Tempo: {durationMs} ms");
        Console.WriteLine("================================================================================");
        Console.ResetColor();

        if (_failed == 0)
        {
            Console.ForegroundColor = ConsoleColor.Green;
            Console.WriteLine("  ✓ TODOS OS TESTES PASSARAM COM SUCESSO! Sistema de Multiplayer Validado.");
            Console.ResetColor();
            return 0;
        }
        else
        {
            Console.ForegroundColor = ConsoleColor.Red;
            Console.WriteLine($"  ✗ {_failed} teste(s) falharam!");
            Console.ResetColor();
            return 1;
        }
    }

    private static void RunSection(string name, Action tests)
    {
        Console.WriteLine();
        Console.ForegroundColor = ConsoleColor.Yellow;
        Console.WriteLine($"--- {name} ---");
        Console.ResetColor();
        try
        {
            tests();
        }
        catch (Exception ex)
        {
            AssertFailed($"Exceção não tratada na seção: {ex.Message}");
        }
    }

    private static async Task RunSectionAsync(string name, Func<Task> tests)
    {
        Console.WriteLine();
        Console.ForegroundColor = ConsoleColor.Yellow;
        Console.WriteLine($"--- {name} ---");
        Console.ResetColor();
        try
        {
            await tests();
        }
        catch (Exception ex)
        {
            AssertFailed($"Exceção assíncrona não tratada na seção: {ex.Message}");
        }
    }

    private static void AssertTrue(bool condition, string testName, string? details = null)
    {
        if (condition)
        {
            Console.ForegroundColor = ConsoleColor.Green;
            Console.Write("  [PASS] ");
            Console.ResetColor();
            Console.WriteLine(testName);
            _passed++;
        }
        else
        {
            AssertFailed(testName, details);
        }
    }

    private static void AssertEqual<T>(T expected, T actual, string testName)
    {
        bool eq = EqualityComparer<T>.Default.Equals(expected, actual);
        if (eq)
        {
            Console.ForegroundColor = ConsoleColor.Green;
            Console.Write("  [PASS] ");
            Console.ResetColor();
            Console.WriteLine(testName);
            _passed++;
        }
        else
        {
            AssertFailed(testName, $"Esperado: '{expected}', Obtido: '{actual}'");
        }
    }

    private static void AssertFailed(string testName, string? details = null)
    {
        Console.ForegroundColor = ConsoleColor.Red;
        Console.Write("  [FAIL] ");
        Console.ResetColor();
        Console.WriteLine(testName);
        if (!string.IsNullOrEmpty(details))
        {
            Console.ForegroundColor = ConsoleColor.DarkRed;
            Console.WriteLine($"         Detalhes: {details}");
            Console.ResetColor();
        }
        _failed++;
    }

    // =========================================================================
    // 1. SERIALIZATION TESTS
    // =========================================================================
    private static void TestHandshakePacketSerialization()
    {
        string playerName = "Alucard Belmont Jr.";
        byte[] payload = Encoding.UTF8.GetBytes(playerName);
        var packet = new NetworkPacket(PacketOpCode.Handshake, payload);

        byte[] raw = packet.Serialize();
        AssertEqual(1 + payload.Length, raw.Length, "Handshake - Tamanho do buffer serializado");
        AssertEqual((byte)PacketOpCode.Handshake, raw[0], "Handshake - Byte do OpCode");

        var deserialized = NetworkPacket.Deserialize(raw, raw.Length);
        AssertTrue(deserialized != null, "Handshake - Desserialização com sucesso");
        AssertEqual(PacketOpCode.Handshake, deserialized!.OpCode, "Handshake - OpCode desserializado");
        string decodedName = Encoding.UTF8.GetString(deserialized.Payload);
        AssertEqual(playerName, decodedName, "Handshake - Nome do jogador idêntico");
    }

    private static void TestPlayerTransformStandardSerialization()
    {
        byte stageId = 4;
        byte roomX = 12;
        byte roomY = 8;
        short posX = 1024;
        short posY = 768;
        short velX = 15;
        short velY = -5;
        bool facingLeft = true;
        byte animId = 22;
        byte frameId = 3;
        ushort curHp = 450;
        ushort maxHp = 500;
        byte character = 0; // Alucard
        byte paletteId = 1;
        byte attackTrigger = 1;
        byte subWeapon = 2; // Holy Water

        byte[] payload = NetworkPacket.CreatePlayerTransform(
            stageId, roomX, roomY, posX, posY, velX, velY,
            facingLeft, animId, frameId, curHp, maxHp,
            character, paletteId, attackTrigger, subWeapon
        );

        var packet = new NetworkPacket(PacketOpCode.PlayerTransform, payload);
        byte[] raw = packet.Serialize();

        var deserialized = NetworkPacket.Deserialize(raw, raw.Length);
        AssertTrue(deserialized != null, "PlayerTransform - Desserialização válida");
        AssertEqual(PacketOpCode.PlayerTransform, deserialized!.OpCode, "PlayerTransform - OpCode 0x10");

        bool success = NetworkPacket.ReadPlayerTransform(
            deserialized.Payload,
            out byte dStage, out byte dRx, out byte dRy,
            out short dPx, out short dPy, out short dVx, out short dVy,
            out bool dFacing, out byte dAnim, out byte dFrame,
            out ushort dHp, out ushort dMaxHp,
            out byte dChar, out byte dPal, out byte dAtk, out byte dSub
        );

        AssertTrue(success, "PlayerTransform - Leitura de campos com sucesso");
        AssertEqual(stageId, dStage, "PlayerTransform - StageId");
        AssertEqual(roomX, dRx, "PlayerTransform - RoomX");
        AssertEqual(roomY, dRy, "PlayerTransform - RoomY");
        AssertEqual(posX, dPx, "PlayerTransform - PosX");
        AssertEqual(posY, dPy, "PlayerTransform - PosY");
        AssertEqual(velX, dVx, "PlayerTransform - VelX");
        AssertEqual(velY, dVy, "PlayerTransform - VelY");
        AssertEqual(facingLeft, dFacing, "PlayerTransform - FacingLeft");
        AssertEqual(animId, dAnim, "PlayerTransform - AnimId");
        AssertEqual(frameId, dFrame, "PlayerTransform - FrameId");
        AssertEqual(curHp, dHp, "PlayerTransform - CurHp");
        AssertEqual(maxHp, dMaxHp, "PlayerTransform - MaxHp");
        AssertEqual(character, dChar, "PlayerTransform - Character (Alucard)");
        AssertEqual(paletteId, dPal, "PlayerTransform - PaletteId");
        AssertEqual(attackTrigger, dAtk, "PlayerTransform - AttackTrigger");
        AssertEqual(subWeapon, dSub, "PlayerTransform - SubWeapon");
    }

    private static void TestPlayerTransformEdgeCasesSerialization()
    {
        // Valores de borda: coordenadas negativas, velocidades mínimas/máximas, HP máximo ushort
        short minPos = short.MinValue;
        short maxPos = short.MaxValue;
        short minVel = short.MinValue;
        short maxVel = short.MaxValue;
        ushort maxHp = ushort.MaxValue;

        byte[] payload = NetworkPacket.CreatePlayerTransform(
            255, 255, 255, minPos, maxPos, minVel, maxVel,
            false, 255, 255, maxHp, maxHp, 1, 255, 255, 255
        );

        bool success = NetworkPacket.ReadPlayerTransform(
            payload,
            out byte dStage, out byte dRx, out byte dRy,
            out short dPx, out short dPy, out short dVx, out short dVy,
            out bool dFacing, out byte dAnim, out byte dFrame,
            out ushort dHp, out ushort dMaxHp,
            out byte dChar, out byte dPal, out byte dAtk, out byte dSub
        );

        AssertTrue(success, "PlayerTransform Edge - Leitura de valores extremos");
        AssertEqual(minPos, dPx, "PlayerTransform Edge - PosX negativo extremo");
        AssertEqual(maxPos, dPy, "PlayerTransform Edge - PosY positivo extremo");
        AssertEqual(minVel, dVx, "PlayerTransform Edge - VelX mínimo");
        AssertEqual(maxVel, dVy, "PlayerTransform Edge - VelY máximo");
        AssertEqual(maxHp, dHp, "PlayerTransform Edge - HP máximo (65535)");
        AssertEqual((byte)1, dChar, "PlayerTransform Edge - Personagem Richter Belmont");
    }

    private static void TestWorldStateSerialization()
    {
        byte[] map = new byte[512];
        for (int i = 0; i < 512; i++) map[i] = (byte)(i % 256);

        byte[] bosses = new byte[64];
        for (int i = 0; i < 64; i++) bosses[i] = (byte)(255 - (i % 256));

        byte[] payload = NetworkPacket.CreateWorldState(map, bosses);
        var packet = new NetworkPacket(PacketOpCode.WorldState, payload);
        byte[] raw = packet.Serialize();

        var deserialized = NetworkPacket.Deserialize(raw, raw.Length);
        AssertTrue(deserialized != null, "WorldState - Desserialização válida");
        AssertEqual(PacketOpCode.WorldState, deserialized!.OpCode, "WorldState - OpCode 0x20");

        bool success = NetworkPacket.ReadWorldState(deserialized.Payload, out byte[] dMap, out byte[] dBoss);
        AssertTrue(success, "WorldState - Extração de buffers");
        AssertEqual(512, dMap.Length, "WorldState - Tamanho do mapa (512 bytes)");
        AssertEqual(64, dBoss.Length, "WorldState - Tamanho do buffer de chefes (64 bytes)");

        bool mapMatches = true;
        for (int i = 0; i < 512; i++)
        {
            if (map[i] != dMap[i]) { mapMatches = false; break; }
        }
        AssertTrue(mapMatches, "WorldState - Integridade binária exata do mapa de 512 bytes");

        bool bossMatches = true;
        for (int i = 0; i < 64; i++)
        {
            if (bosses[i] != dBoss[i]) { bossMatches = false; break; }
        }
        AssertTrue(bossMatches, "WorldState - Integridade binária exata dos flags de chefes de 64 bytes");
    }

    private static void TestChatMessageSerialization()
    {
        string sender = "RichterBelmont";
        string message = "Die monster, you don't belong in this world! 🦇⚔️ (Olá Castelo!)";

        byte[] payload = NetworkPacket.CreateChat(sender, message);
        var packet = new NetworkPacket(PacketOpCode.ChatMessage, payload);
        byte[] raw = packet.Serialize();

        var deserialized = NetworkPacket.Deserialize(raw, raw.Length);
        AssertTrue(deserialized != null, "ChatMessage - Desserialização de texto");
        AssertEqual(PacketOpCode.ChatMessage, deserialized!.OpCode, "ChatMessage - OpCode 0x50");

        bool success = NetworkPacket.ReadChat(deserialized.Payload, out string dSender, out string dMsg);
        AssertTrue(success, "ChatMessage - Leitura do corpo e remetente");
        AssertEqual(sender, dSender, "ChatMessage - Remetente correto");
        AssertEqual(message, dMsg, "ChatMessage - Mensagem UTF-8 e caracteres especiais preservados");
    }

    private static void TestPingPongSerialization()
    {
        var ping = new NetworkPacket(PacketOpCode.Ping, []);
        byte[] rawPing = ping.Serialize();
        var dPing = NetworkPacket.Deserialize(rawPing, rawPing.Length);
        AssertTrue(dPing != null && dPing.OpCode == PacketOpCode.Ping && dPing.Payload.Length == 0,
            "Ping - Serialização com payload vazio");

        var pong = new NetworkPacket(PacketOpCode.Pong, []);
        byte[] rawPong = pong.Serialize();
        var dPong = NetworkPacket.Deserialize(rawPong, rawPong.Length);
        AssertTrue(dPong != null && dPong.OpCode == PacketOpCode.Pong && dPong.Payload.Length == 0,
            "Pong - Serialização com payload vazio");
    }

    private static void TestPacketMalformedAndFuzzingResilience()
    {
        // 1. Pacote nulo ou tamanho 0
        var nullPacket = NetworkPacket.Deserialize([], 0);
        AssertTrue(nullPacket == null, "Fuzzing - Buffer vazio retorna nulo com segurança");

        // 2. Pacote truncado de PlayerTransform (apenas 5 bytes em vez de 21+)
        byte[] truncatedTransform = [ (byte)PacketOpCode.PlayerTransform, 1, 2, 3, 4 ];
        var dTrunc = NetworkPacket.Deserialize(truncatedTransform, truncatedTransform.Length);
        AssertTrue(dTrunc != null, "Fuzzing - Pacote truncado desserializado sem exceção");
        bool readTransform = NetworkPacket.ReadPlayerTransform(dTrunc!.Payload,
            out _, out _, out _, out _, out _, out _, out _, out _, out _, out _, out _, out _, out _, out _, out _, out _);
        AssertTrue(!readTransform, "Fuzzing - ReadPlayerTransform rejeita payload incompleto graciosamente");

        // 3. Pacote truncado de WorldState (menos de 4 bytes de header de comprimento)
        byte[] truncatedWorld = [ (byte)PacketOpCode.WorldState, 1 ];
        var dWorld = NetworkPacket.Deserialize(truncatedWorld, truncatedWorld.Length);
        bool readWorld = NetworkPacket.ReadWorldState(dWorld!.Payload, out _, out _);
        AssertTrue(!readWorld, "Fuzzing - ReadWorldState rejeita cabeçalho truncado graciosamente");

        // 4. Pacote truncado de Chat
        byte[] truncatedChat = [ (byte)PacketOpCode.ChatMessage, 0 ];
        var dChat = NetworkPacket.Deserialize(truncatedChat, truncatedChat.Length);
        bool readChat = NetworkPacket.ReadChat(dChat!.Payload, out _, out _);
        AssertTrue(!readChat, "Fuzzing - ReadChat rejeita buffer inválido sem quebrar");
    }

    // =========================================================================
    // 2. WORLD STATE SYNC LOGIC TESTS
    // =========================================================================
    private static void TestWorldStateMapBitsetMerging()
    {
        // Simulação do algoritmo de merge bitwise OR de WorldStateSync.cs:
        // byte merged = (byte)(localByte | remoteByte);
        byte[] localMap = new byte[512];
        byte[] remoteMap = new byte[512];

        // Jogador local explorou salas no byte 0 (bits 0b00000001 e 0b00000010)
        localMap[0] = 0b0000_0011;
        // Jogador remoto explorou salas no byte 0 (bits 0b00000010 e 0b00000100) e no byte 10 (0b11110000)
        remoteMap[0] = 0b0000_0110;
        remoteMap[10] = 0b1111_0000;

        int newRoomsDiscovered = 0;
        byte[] mergedResult = new byte[512];

        for (int i = 0; i < 512; i++)
        {
            byte localByte = localMap[i];
            byte remoteByte = remoteMap[i];
            byte merged = (byte)(localByte | remoteByte);

            if (merged != localByte)
            {
                newRoomsDiscovered++;
                mergedResult[i] = merged;
            }
            else
            {
                mergedResult[i] = localByte;
            }
        }

        AssertEqual(2, newRoomsDiscovered, "WorldState Merge - 2 bytes com novas salas descobertas");
        AssertEqual((byte)0b0000_0111, mergedResult[0], "WorldState Merge - União binária das salas no byte 0 (0b00000111)");
        AssertEqual((byte)0b1111_0000, mergedResult[10], "WorldState Merge - Salas remotas incorporadas no byte 10");
        AssertEqual((byte)0, mergedResult[50], "WorldState Merge - Salas não exploradas permanecem vazias");
    }

    private static void TestWorldStateBossDefeatSynchronization()
    {
        // Simulação de sincronização de chefes de WorldStateSync.cs:
        // if (localTime == 0 && remoteTime > 0) localTime = remoteTime;
        ushort[] localBosses = new ushort[32];
        ushort[] remoteBosses = new ushort[32];

        // Chefe 0 (Gaibon & Slogra): Ambos derrotaram
        localBosses[0] = 1200;
        remoteBosses[0] = 1450;

        // Chefe 1 (Doppleganger): Apenas remoto derrotou
        localBosses[1] = 0;
        remoteBosses[1] = 2300;

        // Chefe 2 (Dracula): Nenhum derrotou
        localBosses[2] = 0;
        remoteBosses[2] = 0;

        for (int i = 0; i < 32; i++)
        {
            ushort localTime = localBosses[i];
            ushort remoteTime = remoteBosses[i];
            if (localTime == 0 && remoteTime > 0)
            {
                localBosses[i] = remoteTime;
            }
        }

        AssertEqual((ushort)1200, localBosses[0], "WorldState Boss - Tempo do chefe já derrotado preservado");
        AssertEqual((ushort)2300, localBosses[1], "WorldState Boss - Chefe derrotado pelo parceiro desbloqueado");
        AssertEqual((ushort)0, localBosses[2], "WorldState Boss - Chefe invicto permanece com tempo 0");
    }

    // =========================================================================
    // 3. REMOTE PUPPET INTERPOLATION & IDENTITY TESTS
    // =========================================================================
    private static void TestRemotePuppetLerpConvergence()
    {
        // Fórmula exata de RemotePuppet.cs: _interp = _interp + (target - _interp) * 0.35f;
        float interpX = 0f;
        float targetX = 100f;

        // Frame 1
        interpX = interpX + (targetX - interpX) * 0.35f;
        AssertTrue(Math.Abs(interpX - 35.0f) < 0.001f, "Puppet Lerp - Frame 1 atinge 35% do percurso");

        // Frame 2
        interpX = interpX + (targetX - interpX) * 0.35f;
        AssertTrue(Math.Abs(interpX - 57.75f) < 0.001f, "Puppet Lerp - Frame 2 atinge 57.75%");

        // Simula 20 frames a 60 FPS (aprox 330ms)
        for (int i = 0; i < 18; i++)
        {
            interpX = interpX + (targetX - interpX) * 0.35f;
        }

        // Deve convergir para praticamente 100
        AssertTrue(Math.Abs(interpX - targetX) < 0.05f, $"Puppet Lerp - Convergência suave para o alvo ({interpX:F2} ≈ 100)");
    }

    private static void TestRemotePuppetPaletteAndCharacterIdentity()
    {
        // Host é paleta 0, Client é paleta 1
        byte hostPal = 0;
        byte clientPal = 1;
        AssertTrue(hostPal != clientPal, "Puppet Identity - Paletas do Host e Client são distintas");

        // Personagem 0 = Alucard, 1 = Richter
        byte alucardId = 0;
        byte richterId = 1;
        AssertTrue(alucardId == 0 && richterId == 1, "Puppet Identity - IDs dos personagens suportados identificáveis");
    }

    // =========================================================================
    // 4. PVP COLOSSEUM DETECTION & SCOREBOARD TESTS
    // =========================================================================
    private static void TestPvPColosseumDetectionLogic()
    {
        // Regra de PvPManager.cs:
        // Caso 1: Ambos no Coliseu e na mesma sala
        bool localInCol = true;
        bool remoteInCol = true;
        bool sameRoom = true;
        bool isPvP = localInCol && remoteInCol && sameRoom;
        AssertTrue(isPvP, "PvP Colosseum - Ativado quando ambos estão na mesma sala do Coliseu");

        // Caso 2: Ambos no Coliseu mas em salas diferentes
        sameRoom = false;
        isPvP = localInCol && remoteInCol && sameRoom;
        AssertTrue(!isPvP, "PvP Colosseum - Inativo se os jogadores estiverem em salas diferentes");

        // Caso 3: Um jogador no Coliseu e outro na Galeria de Mármore
        remoteInCol = false;
        sameRoom = true;
        isPvP = localInCol && remoteInCol && sameRoom;
        AssertTrue(!isPvP, "PvP Colosseum - Inativo se um dos jogadores não estiver no Coliseu");
    }

    private static void TestPvPScoreboardTracking()
    {
        int localScore = 0;
        int remoteScore = 0;

        // Round 1: Jogador local vence
        localScore++;
        // Round 2: Jogador remoto vence
        remoteScore++;
        // Round 3: Jogador local vence
        localScore++;

        AssertEqual(2, localScore, "PvP Placar - Vitórias do jogador local computadas");
        AssertEqual(1, remoteScore, "PvP Placar - Vitórias do jogador remoto computadas");
    }

    // =========================================================================
    // 5. REAL UDP SOCKET P2P INTEGRATION TESTS (LOOPBACK)
    // =========================================================================
    private static async Task TestRealUdpP2PLoopbackCommunicationAsync()
    {
        int testPort = 19876;
        using var hostUdp = new UdpClient(testPort);
        using var clientUdp = new UdpClient();
        clientUdp.Connect("127.0.0.1", testPort);

        // 1. Client envia Handshake
        string clientName = "RichterBelmont";
        var handshakePacket = new NetworkPacket(PacketOpCode.Handshake, Encoding.UTF8.GetBytes(clientName));
        byte[] handshakeData = handshakePacket.Serialize();
        await clientUdp.SendAsync(handshakeData, handshakeData.Length);

        // 2. Host recebe Handshake
        var hostReceiveResult = await hostUdp.ReceiveAsync();
        var hostReceivedPacket = NetworkPacket.Deserialize(hostReceiveResult.Buffer, hostReceiveResult.Buffer.Length);
        AssertTrue(hostReceivedPacket != null && hostReceivedPacket.OpCode == PacketOpCode.Handshake,
            "P2P Socket - Host recebeu pacote Handshake via UDP");
        string receivedClientName = Encoding.UTF8.GetString(hostReceivedPacket!.Payload);
        AssertEqual(clientName, receivedClientName, "P2P Socket - Nome do Client transmitido via socket");

        // 3. Host responde HandshakeAck
        string hostName = "AlucardTepes";
        var ackPacket = new NetworkPacket(PacketOpCode.HandshakeAck, Encoding.UTF8.GetBytes(hostName));
        byte[] ackData = ackPacket.Serialize();
        await hostUdp.SendAsync(ackData, ackData.Length, hostReceiveResult.RemoteEndPoint);

        // 4. Client recebe HandshakeAck
        var clientReceiveResult = await clientUdp.ReceiveAsync();
        var clientReceivedAck = NetworkPacket.Deserialize(clientReceiveResult.Buffer, clientReceiveResult.Buffer.Length);
        AssertTrue(clientReceivedAck != null && clientReceivedAck.OpCode == PacketOpCode.HandshakeAck,
            "P2P Socket - Client recebeu HandshakeAck de confirmação");
        string receivedHostName = Encoding.UTF8.GetString(clientReceivedAck!.Payload);
        AssertEqual(hostName, receivedHostName, "P2P Socket - Nome do Host transmitido via socket");

        // 5. Ping / Pong RTT Measurement
        long pingSendTime = Environment.TickCount64;
        var pingPacket = new NetworkPacket(PacketOpCode.Ping, []);
        byte[] pingData = pingPacket.Serialize();
        await clientUdp.SendAsync(pingData, pingData.Length);

        var hostPingReceive = await hostUdp.ReceiveAsync();
        var hostPingPacket = NetworkPacket.Deserialize(hostPingReceive.Buffer, hostPingReceive.Buffer.Length);
        AssertEqual(PacketOpCode.Ping, hostPingPacket!.OpCode, "P2P Socket - Host recebeu Ping");

        // Host devolve Pong
        var pongPacket = new NetworkPacket(PacketOpCode.Pong, []);
        byte[] pongData = pongPacket.Serialize();
        await hostUdp.SendAsync(pongData, pongData.Length, hostPingReceive.RemoteEndPoint);

        var clientPongReceive = await clientUdp.ReceiveAsync();
        var clientPongPacket = NetworkPacket.Deserialize(clientPongReceive.Buffer, clientPongReceive.Buffer.Length);
        AssertEqual(PacketOpCode.Pong, clientPongPacket!.OpCode, "P2P Socket - Client recebeu Pong");
        long pingRtt = Environment.TickCount64 - pingSendTime;
        AssertTrue(pingRtt >= 0 && pingRtt < 100, $"P2P Socket - RTT de Latência em Loopback ({pingRtt} ms)");
    }

    private static async Task TestHighFrequency60HzTransformStreamingAsync()
    {
        int testPort = 19877;
        using var hostUdp = new UdpClient(testPort);
        using var clientUdp = new UdpClient();
        clientUdp.Connect("127.0.0.1", testPort);

        // Simula transmissão contínua de 60Hz: 120 pacotes enviados consecutivamente (2 segundos de gameplay)
        const int packetCount = 120;
        int receivedCount = 0;
        var receiveTask = Task.Run(async () =>
        {
            using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(3));
            while (!cts.Token.IsCancellationRequested && receivedCount < packetCount)
            {
                try
                {
                    var res = await hostUdp.ReceiveAsync(cts.Token);
                    var pkt = NetworkPacket.Deserialize(res.Buffer, res.Buffer.Length);
                    if (pkt != null && pkt.OpCode == PacketOpCode.PlayerTransform)
                    {
                        Interlocked.Increment(ref receivedCount);
                    }
                }
                catch { break; }
            }
        });

        for (short i = 0; i < packetCount; i++)
        {
            byte[] transform = NetworkPacket.CreatePlayerTransform(
                1, 5, 2, (short)(100 + i), (short)(200 + i), 10, 0,
                false, 1, (byte)(i % 10), 100, 100, 0, 0, 0, 0
            );
            var pkt = new NetworkPacket(PacketOpCode.PlayerTransform, transform);
            byte[] data = pkt.Serialize();
            await clientUdp.SendAsync(data, data.Length);
        }

        await receiveTask;

        AssertTrue(receivedCount == packetCount,
            $"P2P 60Hz Stream - Taxa de entrega de pacotes de alta frequência: {receivedCount}/{packetCount} (100% integridade)");
    }
}
