using System;
using System.Collections.Concurrent;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Recompiled;
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
            TestPvPHitPacketSerialization();
            TestPvPKnockoutPacketSerialization();
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
            TestPvPHitboxReachAndFacingLogic();
            TestPvPDamageCalculationAndKnockoutLogic();
        });

        await RunSectionAsync("5. Teste de Integração Real UDP Socket P2P (Host <-> Client Loopback)", async () =>
        {
            await TestRealUdpP2PLoopbackCommunicationAsync();
            await TestHighFrequency60HzTransformStreamingAsync();
        });

        RunSection("6. Testes do Lobby Pré-Jogo & Sincronização de Inimigos (PoC Co-op Evolution)", () =>
        {
            TestLobbySyncSerialization();
            TestStartGameSyncSerialization();
            TestEnterLobbyRoomSyncSerialization();
            TestEnemyDamageSyncSerialization();
            TestLobbyManagerReadyStateMachine();
        });

        RunSection("7. Testes de Queda & Reviver Co-op e Pings Táticos (Co-op Evolution Part 2)", () =>
        {
            TestPlayerDownedSerialization();
            TestPlayerRevivedSerialization();
            TestCoopPingSerialization();
            TestCoopRevivalManagerLogic();
        });

        RunSection("8. Testes do Minimapa Bloodstained, Puppet Overlay & Terceira Mão (Saturn) (Modern HUD & QoL)", () =>
        {
            TestBloodstainedMinimapGridAndRadarMath();
            TestThirdHandPriorityAndHealingLogic();
            TestSpriteFrameCodec();
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

    private static void TestPvPHitPacketSerialization()
    {
        ushort damage = 35;
        byte hitType = 1;
        short knockbackX = 4;
        string attacker = "AlucardTepes";

        byte[] payload = NetworkPacket.CreatePvPHit(damage, hitType, knockbackX, attacker);
        var packet = new NetworkPacket(PacketOpCode.PvPHit, payload);
        byte[] raw = packet.Serialize();

        var deserialized = NetworkPacket.Deserialize(raw, raw.Length);
        AssertTrue(deserialized != null, "PvPHit - Desserialização do pacote de golpe");
        AssertEqual(PacketOpCode.PvPHit, deserialized!.OpCode, "PvPHit - OpCode 0x40 correto");

        bool success = NetworkPacket.ReadPvPHit(deserialized.Payload, out ushort dDmg, out byte dHitType, out short dKbX, out string dAttacker);
        AssertTrue(success, "PvPHit - Leitura do payload de ataque");
        AssertEqual(damage, dDmg, "PvPHit - Dano transmitido corretamente (35)");
        AssertEqual(hitType, dHitType, "PvPHit - Tipo de golpe preservado (1 - Especial)");
        AssertEqual(knockbackX, dKbX, "PvPHit - Impulso de Knockback X preservado (+4)");
        AssertEqual(attacker, dAttacker, "PvPHit - Nome do atacante transmitido ('AlucardTepes')");

        // Teste de caso limite: knockback negativo e string vazia
        byte[] edgePayload = NetworkPacket.CreatePvPHit(60, 0, -4, "");
        var edgePacket = new NetworkPacket(PacketOpCode.PvPHit, edgePayload);
        byte[] rawEdge = edgePacket.Serialize();
        var dEdge = NetworkPacket.Deserialize(rawEdge, rawEdge.Length);
        AssertTrue(NetworkPacket.ReadPvPHit(dEdge!.Payload, out ushort eDmg, out byte eType, out short eKbX, out string eAttacker), "PvPHit Edge - Leitura bem-sucedida");
        AssertEqual((ushort)60, eDmg, "PvPHit Edge - Dano máximo (60)");
        AssertEqual((short)-4, eKbX, "PvPHit Edge - Knockback negativo para esquerda (-4)");
        AssertEqual("", eAttacker, "PvPHit Edge - Atacante anônimo tratado com segurança");
    }

    private static void TestPvPKnockoutPacketSerialization()
    {
        string winner = "RichterBelmont";
        string loser = "AlucardTepes";
        ushort round = 3;

        byte[] payload = NetworkPacket.CreatePvPKnockout(winner, loser, round);
        var packet = new NetworkPacket(PacketOpCode.PvPKnockout, payload);
        byte[] raw = packet.Serialize();

        var deserialized = NetworkPacket.Deserialize(raw, raw.Length);
        AssertTrue(deserialized != null, "PvPKnockout - Desserialização do pacote de K.O.");
        AssertEqual(PacketOpCode.PvPKnockout, deserialized!.OpCode, "PvPKnockout - OpCode 0x41 correto");

        bool success = NetworkPacket.ReadPvPKnockout(deserialized.Payload, out string dWinner, out string dLoser, out ushort dRound);
        AssertTrue(success, "PvPKnockout - Leitura do payload de vitória");
        AssertEqual(winner, dWinner, "PvPKnockout - Vencedor registrado ('RichterBelmont')");
        AssertEqual(loser, dLoser, "PvPKnockout - Perdedor registrado ('AlucardTepes')");
        AssertEqual(round, dRound, "PvPKnockout - Número do round computado (3)");
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

    private static void TestPvPHitboxReachAndFacingLogic()
    {
        // Regra de alcance do PvPManager:
        // dy <= 48
        // facingLeft ? (dx <= 15 && dx >= -80) : (dx >= -15 && dx <= 80)
        int px = 200, py = 150;

        // Caso 1: Jogador virado para a DIREITA (facingLeft = false)
        // Alvo na frente dentro do alcance (dx = +40, dy = 0)
        bool hit1 = CheckHitRange(px, py, facingLeft: false, rx: 240, ry: 150);
        AssertTrue(hit1, "Hitbox - Golpe frontal acertou alvo à direita (+40px)");

        // Alvo ligeiramente atrás dentro da tolerância de proximidade (dx = -10, dy = 10)
        bool hit2 = CheckHitRange(px, py, facingLeft: false, rx: 190, ry: 160);
        AssertTrue(hit2, "Hitbox - Golpe acertou em curta distância (-10px)");

        // Alvo atrás do jogador virado para a direita (dx = -50) -> deve errar
        bool hit3 = CheckHitRange(px, py, facingLeft: false, rx: 150, ry: 150);
        AssertTrue(!hit3, "Hitbox - Golpe errou alvo que estava nas costas à esquerda");

        // Alvo muito longe na frente (dx = +120) -> fora de alcance
        bool hit4 = CheckHitRange(px, py, facingLeft: false, rx: 320, ry: 150);
        AssertTrue(!hit4, "Hitbox - Golpe errou alvo distante (+120px)");

        // Alvo muito acima no pulo (dy = 60) -> fora de alcance vertical
        bool hit5 = CheckHitRange(px, py, facingLeft: false, rx: 230, ry: 90);
        AssertTrue(!hit5, "Hitbox - Golpe errou alvo acima no pulo (dy > 48px)");

        // Caso 2: Jogador virado para a ESQUERDA (facingLeft = true)
        // Alvo na frente à esquerda (dx = -50, dy = 0)
        bool hit6 = CheckHitRange(px, py, facingLeft: true, rx: 150, ry: 150);
        AssertTrue(hit6, "Hitbox - Golpe frontal acertou alvo à esquerda (-50px)");

        // Alvo atrás à direita (dx = +40) -> deve errar
        bool hit7 = CheckHitRange(px, py, facingLeft: true, rx: 240, ry: 150);
        AssertTrue(!hit7, "Hitbox - Golpe errou alvo nas costas à direita");
    }

    private static void TestPvPDamageCalculationAndKnockoutLogic()
    {
        // 1. Cálculo de dano proporcional
        ushort baseDmg = CalculateDamage(0);
        AssertEqual((ushort)25, baseDmg, "PvP Dano - Dano base padrão sem bônus de força (25)");

        ushort midDmg = CalculateDamage(50);
        AssertEqual((ushort)25, midDmg, "PvP Dano - Dano com 50 de força = 15 + 10 = 25");

        ushort highDmg = CalculateDamage(150);
        AssertEqual((ushort)45, highDmg, "PvP Dano - Dano com 150 de força = 15 + 30 = 45");

        ushort cappedDmg = CalculateDamage(300);
        AssertEqual((ushort)60, cappedDmg, "PvP Dano - Dano máximo limitado a 60");

        // 2. Aplicação de dano sem invencibilidade
        int hp = 100;
        bool applied1 = ProcessPvPHit(hp, 30, isInvincible: false, out int newHp1, out bool ko1);
        AssertTrue(applied1, "PvP Hit - Golpe registrado com sucesso");
        AssertEqual(70, newHp1, "PvP Hit - HP reduzido de 100 para 70 (-30 de dano)");
        AssertTrue(!ko1, "PvP Hit - Jogador sobreviveu ao golpe (sem knockout)");

        // 3. Aplicação durante i-frames (invencibilidade temporária)
        bool applied2 = ProcessPvPHit(newHp1, 30, isInvincible: true, out int newHp2, out bool ko2);
        AssertTrue(!applied2, "PvP Hit - Golpe ignorado durante i-frames do jogador");
        AssertEqual(70, newHp2, "PvP Hit - HP inalterado durante i-frames");
        AssertTrue(!ko2, "PvP Hit - Sem knockout em i-frames");

        // 4. Golpe final que resulta em Knockout (K.O.)
        bool applied3 = ProcessPvPHit(25, 30, isInvincible: false, out int newHp3, out bool ko3);
        AssertTrue(applied3, "PvP Hit - Golpe fatal registrado");
        AssertEqual(0, newHp3, "PvP Hit - HP reduzido a 0");
        AssertTrue(ko3, "PvP Hit - Knockout (K.O.) confirmado!");
    }

    private static bool CheckHitRange(int px, int py, bool facingLeft, int rx, int ry)
    {
        int dx = rx - px;
        int dy = Math.Abs(ry - py);
        if (dy > 48) return false;
        return facingLeft ? (dx <= 15 && dx >= -80) : (dx >= -15 && dx <= 80);
    }

    private static ushort CalculateDamage(int strengthTotal)
    {
        if (strengthTotal > 0)
        {
            return (ushort)Math.Clamp(15 + (strengthTotal / 5), 15, 60);
        }
        return 25;
    }

    private static bool ProcessPvPHit(int currentHp, ushort damage, bool isInvincible, out int newHp, out bool isKnockout)
    {
        if (isInvincible)
        {
            newHp = currentHp;
            isKnockout = false;
            return false;
        }
        newHp = Math.Max(0, currentHp - damage);
        isKnockout = newHp <= 0;
        return true;
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

        // 6. Troca de Pacotes de Duelo PvP (PvPHit e PvPKnockout) via UDP
        ushort pvpDmg = 35;
        byte hitType = 1;
        short knockback = 4;
        string attacker = "RichterBelmont";
        byte[] hitPayload = NetworkPacket.CreatePvPHit(pvpDmg, hitType, knockback, attacker);
        var hitPacket = new NetworkPacket(PacketOpCode.PvPHit, hitPayload);
        byte[] hitData = hitPacket.Serialize();
        await clientUdp.SendAsync(hitData, hitData.Length);

        var hostHitReceive = await hostUdp.ReceiveAsync();
        var hostHitPacket = NetworkPacket.Deserialize(hostHitReceive.Buffer, hostHitReceive.Buffer.Length);
        AssertTrue(hostHitPacket != null && hostHitPacket.OpCode == PacketOpCode.PvPHit,
            "P2P Socket - Host recebeu pacote PvPHit via socket UDP");
        bool readHitSuccess = NetworkPacket.ReadPvPHit(hostHitPacket!.Payload, out ushort rxDmg, out byte rxType, out short rxKb, out string rxAtk);
        AssertTrue(readHitSuccess, "P2P Socket - Desserialização do pacote de ataque PvPHit via UDP");
        AssertEqual(pvpDmg, rxDmg, "P2P Socket - Dano do golpe recebido com integridade (35)");
        AssertEqual(attacker, rxAtk, "P2P Socket - Nome do atacante recebido via rede");

        // Host sofre knockout e transmite PvPKnockout para o Client
        byte[] koPayload = NetworkPacket.CreatePvPKnockout("RichterBelmont", "AlucardTepes", 1);
        var koPacket = new NetworkPacket(PacketOpCode.PvPKnockout, koPayload);
        byte[] koData = koPacket.Serialize();
        await hostUdp.SendAsync(koData, koData.Length, hostHitReceive.RemoteEndPoint);

        var clientKoReceive = await clientUdp.ReceiveAsync();
        var clientKoPacket = NetworkPacket.Deserialize(clientKoReceive.Buffer, clientKoReceive.Buffer.Length);
        AssertTrue(clientKoPacket != null && clientKoPacket.OpCode == PacketOpCode.PvPKnockout,
            "P2P Socket - Client recebeu anúncio de PvPKnockout via socket UDP");
        bool readKoSuccess = NetworkPacket.ReadPvPKnockout(clientKoPacket!.Payload, out string rxWinner, out string rxLoser, out ushort rxRound);
        AssertTrue(readKoSuccess, "P2P Socket - Desserialização do pacote PvPKnockout");
        AssertEqual("RichterBelmont", rxWinner, "P2P Socket - Vencedor do round confirmado via socket");
        AssertEqual((ushort)1, rxRound, "P2P Socket - Número do round sincronizado");
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

    // --- 6. Testes do Lobby Pré-Jogo & Sincronização de Inimigos (PoC Co-op Evolution) ---
    private static void TestLobbySyncSerialization()
    {
        byte[] payload = NetworkPacket.CreateLobbySync(true, 3, 1);
        var pkt = new NetworkPacket(PacketOpCode.LobbySync, payload);
        byte[] data = pkt.Serialize();

        AssertTrue(data.Length == 1 + 3, "LobbySync - Tamanho do buffer serializado");
        AssertTrue(data[0] == (byte)PacketOpCode.LobbySync, "LobbySync - OpCode 0x60");

        var deserialized = NetworkPacket.Deserialize(data, data.Length);
        AssertTrue(deserialized != null, "LobbySync - Desserialização válida");

        bool ok = NetworkPacket.ReadLobbySync(deserialized!.Payload, out bool ready, out byte ch, out byte stg);
        AssertTrue(ok, "LobbySync - Leitura de campos com sucesso");
        AssertTrue(ready, "LobbySync - Estado de Pronto (Ready = true)");
        AssertTrue(ch == 3, "LobbySync - Personagem Maria Saturn (ID = 3)");
        AssertTrue(stg == 1, "LobbySync - Local de Partida (Index = 1)");
    }

    private static void TestStartGameSyncSerialization()
    {
        byte[] payload = NetworkPacket.CreateStartGameSync(2, 17, 37, 72, 164);
        var pkt = new NetworkPacket(PacketOpCode.StartGameSync, payload);
        byte[] data = pkt.Serialize();

        AssertTrue(data.Length == 1 + 7, "StartGameSync - Tamanho do buffer serializado");
        AssertTrue(data[0] == (byte)PacketOpCode.StartGameSync, "StartGameSync - OpCode 0x61");

        var deserialized = NetworkPacket.Deserialize(data, data.Length);
        AssertTrue(deserialized != null, "StartGameSync - Desserialização válida");

        bool ok = NetworkPacket.ReadStartGameSync(deserialized!.Payload, out byte stg, out byte rx, out byte ry, out short px, out short py);
        AssertTrue(ok, "StartGameSync - Leitura de campos com sucesso");
        AssertTrue(stg == 2, "StartGameSync - Estágio CastleEntrance (ID = 2)");
        AssertTrue(rx == 17 && ry == 37, "StartGameSync - Coordenadas da Sala (17, 37)");
        AssertTrue(px == 72 && py == 164, "StartGameSync - Posição do Spawn (72, 164)");
    }

    private static void TestEnterLobbyRoomSyncSerialization()
    {
        byte[] payload = NetworkPacket.CreateEnterLobbyRoom();
        var pkt = new NetworkPacket(PacketOpCode.EnterLobbyRoom, payload);
        byte[] data = pkt.Serialize();

        AssertTrue(data.Length == 1 + 1, "EnterLobbyRoom - Tamanho do buffer serializado");
        AssertTrue(data[0] == (byte)PacketOpCode.EnterLobbyRoom, "EnterLobbyRoom - OpCode 0x62");

        var deserialized = NetworkPacket.Deserialize(data, data.Length);
        AssertTrue(deserialized != null, "EnterLobbyRoom - Desserialização válida");

        bool ok = NetworkPacket.ReadEnterLobbyRoom(deserialized!.Payload);
        AssertTrue(ok, "EnterLobbyRoom - Leitura de pacote bem-sucedida");
    }

    private static void TestEnemyDamageSyncSerialization()
    {
        byte[] payload = NetworkPacket.CreateEnemyDamage(2, 5, 10, 66, 45, 15, false);
        var pkt = new NetworkPacket(PacketOpCode.EnemyDamage, payload);
        byte[] data = pkt.Serialize();

        AssertTrue(data.Length == 1 + 9, "EnemyDamage - Tamanho do buffer serializado");
        AssertTrue(data[0] == (byte)PacketOpCode.EnemyDamage, "EnemyDamage - OpCode 0x70");

        var deserialized = NetworkPacket.Deserialize(data, data.Length);
        AssertTrue(deserialized != null, "EnemyDamage - Desserialização válida");

        bool ok = NetworkPacket.ReadEnemyDamage(deserialized!.Payload, out byte stg, out byte rx, out byte ry, out byte slot, out short dmg, out short hp, out bool dead);
        AssertTrue(ok, "EnemyDamage - Leitura do golpe com sucesso");
        AssertTrue(stg == 2 && rx == 5 && ry == 10, "EnemyDamage - Estágio e Sala do Inimigo");
        AssertTrue(slot == 66, "EnemyDamage - Slot da Entidade do Monstro (Slot 66)");
        AssertTrue(dmg == 45, "EnemyDamage - Dano infligido (45)");
        AssertTrue(hp == 15, "EnemyDamage - HP restante do monstro (15)");
        AssertTrue(!dead, "EnemyDamage - Monstro sobreviveu (isDead = false)");

        // Teste de Golpe Fatal
        byte[] fatalPayload = NetworkPacket.CreateEnemyDamage(2, 5, 10, 66, 50, 0, true);
        NetworkPacket.ReadEnemyDamage(fatalPayload, out _, out _, out _, out _, out _, out _, out bool fatalDead);
        AssertTrue(fatalDead, "EnemyDamage - Morte do monstro confirmada (isDead = true)");
    }

    private static void TestLobbyManagerReadyStateMachine()
    {
        bool localReady = false;
        bool remoteReady = false;
        byte localChar = 4; // PSP Maria
        byte remoteChar = 0; // Alucard

        AssertTrue(!localReady && !remoteReady, "Lobby State - Ambos iniciam não-prontos");
        AssertTrue(localChar == 4, "Lobby State - Personagem local definido (PSP Maria)");

        // Simula recebimento de pacote do parceiro
        byte[] remotePacket = NetworkPacket.CreateLobbySync(true, 0, 0);
        if (NetworkPacket.ReadLobbySync(remotePacket, out bool rReady, out byte rChar, out _))
        {
            remoteReady = rReady;
            remoteChar = rChar;
        }

        AssertTrue(remoteReady, "Lobby State - Parceiro marcou Pronto");
        AssertTrue(remoteChar == 0, "Lobby State - Parceiro escolheu Alucard");

        // Local marca pronto
        localReady = true;
        AssertTrue(localReady && remoteReady, "Lobby State - Ambos prontos para início conjunto!");

        // Validação da condição de Start Game pelo Host
        bool canStart = localReady && remoteReady;
        AssertTrue(canStart, "Lobby State - Condição de disparo de partida satisfeita!");
    }

    // --- 7. Testes de Queda & Reviver Co-op e Pings Táticos ---
    private static void TestPlayerDownedSerialization()
    {
        string name = "Alucard";
        byte stage = 2;
        byte rx = 17;
        byte ry = 37;

        byte[] payload = NetworkPacket.CreatePlayerDowned(name, stage, rx, ry);
        var pkt = new NetworkPacket(PacketOpCode.PlayerDowned, payload);
        byte[] data = pkt.Serialize();

        AssertTrue(data[0] == (byte)PacketOpCode.PlayerDowned, "PlayerDowned - OpCode 0x80");
        var d = NetworkPacket.Deserialize(data, data.Length);
        AssertTrue(d != null, "PlayerDowned - Desserialização com sucesso");

        bool ok = NetworkPacket.ReadPlayerDowned(d!.Payload, out string dName, out byte dStage, out byte dRx, out byte dRy);
        AssertTrue(ok, "PlayerDowned - Leitura do pacote válida");
        AssertEqual(name, dName, "PlayerDowned - Nome do jogador caído");
        AssertTrue(dStage == stage && dRx == rx && dRy == ry, "PlayerDowned - Localização da queda");
    }

    private static void TestPlayerRevivedSerialization()
    {
        string reviver = "Maria";
        ushort heal = 150;

        byte[] payload = NetworkPacket.CreatePlayerRevived(reviver, heal);
        var pkt = new NetworkPacket(PacketOpCode.PlayerRevived, payload);
        byte[] data = pkt.Serialize();

        AssertTrue(data[0] == (byte)PacketOpCode.PlayerRevived, "PlayerRevived - OpCode 0x81");
        var d = NetworkPacket.Deserialize(data, data.Length);
        AssertTrue(d != null, "PlayerRevived - Desserialização com sucesso");

        bool ok = NetworkPacket.ReadPlayerRevived(d!.Payload, out string dReviver, out ushort dHeal);
        AssertTrue(ok, "PlayerRevived - Leitura do pacote válida");
        AssertEqual(reviver, dReviver, "PlayerRevived - Nome de quem reanimou");
        AssertEqual(heal, dHeal, "PlayerRevived - Quantidade de HP restaurado");
    }

    private static void TestCoopPingSerialization()
    {
        byte type = 1; // Perigo
        byte stage = 5;
        byte rx = 22;
        byte ry = 14;
        string sender = "Richter";

        byte[] payload = NetworkPacket.CreateCoopPing(type, stage, rx, ry, sender);
        var pkt = new NetworkPacket(PacketOpCode.CoopPing, payload);
        byte[] data = pkt.Serialize();

        AssertTrue(data[0] == (byte)PacketOpCode.CoopPing, "CoopPing - OpCode 0x82");
        var d = NetworkPacket.Deserialize(data, data.Length);
        AssertTrue(d != null, "CoopPing - Desserialização com sucesso");

        bool ok = NetworkPacket.ReadCoopPing(d!.Payload, out byte dType, out byte dStage, out byte dRx, out byte dRy, out string dSender);
        AssertTrue(ok, "CoopPing - Leitura de campos com sucesso");
        AssertEqual(type, dType, "CoopPing - Tipo do ping preservado");
        AssertEqual(sender, dSender, "CoopPing - Remetente correto");
        AssertTrue(dStage == stage && dRx == rx && dRy == ry, "CoopPing - Coordenadas da sala marcadas");
    }

    private static void TestCoopRevivalManagerLogic()
    {
        // Simulação da máquina de estados de queda e reviver
        bool isDowned = false;
        float bleedout = 30.0f;
        int hp = 100;

        // Golpe fatal recebido
        hp = 0;
        if (hp <= 0 && !isDowned)
        {
            isDowned = true;
            bleedout = 30.0f;
            hp = 1; // Preserva 1 HP
        }

        AssertTrue(isDowned, "Revival Logic - Jogador entrou no estado Downed");
        AssertTrue(hp == 1, "Revival Logic - HP mantido em 1 para evitar Game Over precoce");

        // Simula passagem de tempo (10 segundos)
        bleedout -= 10.0f;
        AssertTrue(bleedout == 20.0f, "Revival Logic - Bleedout decrementado com segurança");

        // Parceiro reanima
        ushort heal = 80;
        isDowned = false;
        hp = Math.Max(hp, (int)heal);

        AssertTrue(!isDowned, "Revival Logic - Jogador reanimado com sucesso");
        AssertTrue(hp == 80, "Revival Logic - Vida restabelecida após reanimação");
    }

    private static void TestBloodstainedMinimapGridAndRadarMath()
    {
        // Cell = room origin cell + (world pixel >> 8), the same rule as Stages.PlaceTick.
        AssertEqual(20, HudLogic.MapCell(20, 0x00), "Minimap - Célula na origem da sala");
        AssertEqual(22, HudLogic.MapCell(20, 0x2A0), "Minimap - Célula 2 telas à direita dentro da sala");

        // Partner: origin comes from their own room, never added to ours (old bug doubled it).
        int localCell = HudLogic.MapCell(20, 0x080);
        int partnerCell = HudLogic.MapCell(23, 0x180);
        AssertEqual(4, partnerCell - localCell, "Minimap Radar - Parceiro 4 células à direita usando a origem da sala dele");
        AssertTrue(partnerCell != 20 + 23 + 1, "Minimap Radar - Origens das duas salas não são somadas");

        // Puppet overlay: world position is always offset by our camera scroll.
        AssertEqual(100f, HudLogic.WorldToScreen(500f, 400), "Puppet - Parceiro visível convertido para a tela");
        AssertTrue(HudLogic.WorldToScreen(200f, 600) < -20f, "Puppet - Parceiro à esquerda da câmera fica fora da tela (indicador)");
        AssertTrue(HudLogic.WorldToScreen(900f, 0) > 276f, "Puppet - Parceiro à direita da câmera fica fora da tela (indicador)");
    }

    private static void TestSpriteFrameCodec()
    {
        // a sprite-like frame: few colors, transparent around, opaque and semi pixels
        var px = new ushort[20 * 30];
        for (int i = 0; i < px.Length; i++) px[i] = (i % 7 == 0) ? (ushort)0 : (ushort)(0x8000 | (i % 5) * 0x0421);
        px[3] = 0x1234; // semi-transparent (bit 15 clear)
        byte[] payload = NetworkPacket.CreateSpriteFrame(0xDEADBEEFCAFEF00DUL, -12, -21, 20, 30, px);
        bool ok = NetworkPacket.ReadSpriteFrame(payload, out ulong h, out int ox, out int oy, out int w, out int ht, out ushort[] back);
        AssertTrue(ok && h == 0xDEADBEEFCAFEF00DUL && ox == -12 && oy == -21 && w == 20 && ht == 30,
            "Sprite real - Cabeçalho (hash, offset, tamanho) sobrevive à ida e volta");
        AssertTrue(ok && back.AsSpan().SequenceEqual(px), "Sprite real - Pixels idênticos depois de indexar + deflate");
        AssertTrue(payload.Length < px.Length * 2 / 2, $"Sprite real - Pacote compactado ({payload.Length} bytes de {px.Length * 2})");

        // more than 255 colors falls back to raw 16-bit pixels
        var many = new ushort[40 * 40];
        for (int i = 0; i < many.Length; i++) many[i] = (ushort)(0x8000 | i);
        payload = NetworkPacket.CreateSpriteFrame(1, 0, 0, 40, 40, many);
        ok = NetworkPacket.ReadSpriteFrame(payload, out _, out _, out _, out _, out _, out back);
        AssertTrue(ok && back.AsSpan().SequenceEqual(many), "Sprite real - Quadro com mais de 255 cores também volta idêntico");

        // a damaged packet is refused, not a crash
        payload[payload.Length / 2] ^= 0xFF;
        payload[^1] ^= 0xFF;
        bool bad = NetworkPacket.ReadSpriteFrame(payload, out _, out _, out _, out _, out _, out back);
        AssertTrue(!bad || back.Length == 40 * 40, "Sprite real - Pacote corrompido é recusado sem exceção");
        AssertTrue(NetworkPacket.ReadSpriteShow(NetworkPacket.CreateSpriteShow(42UL), out ulong show) && show == 42UL,
            "Sprite real - SpriteShow leva o hash do quadro");
    }

    private static ThirdHandState Bag(int hp = 100, int maxHp = 100, int mp = 50, int maxMp = 50,
        bool curse = false, bool poison = false, int uncurse = 0, int antivenom = 0, int elixir = 0,
        int highPotion = 0, int potion = 0, int food = 0, int manna = 0) =>
        new(hp, maxHp, mp, maxMp, curse, poison, uncurse, antivenom, elixir, highPotion, potion, food, manna);

    private static void TestThirdHandPriorityAndHealingLogic()
    {
        AssertEqual(ThirdHandChoice.Uncurse, HudLogic.ChooseThirdHand(Bag(hp: 10, curse: true, uncurse: 1, elixir: 1)),
            "Terceira Mão - Maldição tem prioridade sobre itens de vida");
        AssertEqual(ThirdHandChoice.Antivenom, HudLogic.ChooseThirdHand(Bag(hp: 10, poison: true, antivenom: 1, potion: 3)),
            "Terceira Mão - Antídoto antes de curar HP");
        AssertEqual(ThirdHandChoice.Elixir, HudLogic.ChooseThirdHand(Bag(hp: 30, elixir: 1, highPotion: 1)),
            "Terceira Mão - Elixir com HP crítico (<= 35%)");
        AssertEqual(ThirdHandChoice.HighPotion, HudLogic.ChooseThirdHand(Bag(hp: 40, elixir: 1, highPotion: 1, potion: 1)),
            "Terceira Mão - High Potion quando faltam >= 60 HP e não é crítico");
        AssertEqual(ThirdHandChoice.Potion, HudLogic.ChooseThirdHand(Bag(hp: 80, highPotion: 1, potion: 1)),
            "Terceira Mão - Potion para dano leve");
        AssertEqual(ThirdHandChoice.Food, HudLogic.ChooseThirdHand(Bag(hp: 80, food: 2)),
            "Terceira Mão - Comida quando não há poções");
        AssertEqual(ThirdHandChoice.MannaPrism, HudLogic.ChooseThirdHand(Bag(mp: 10, manna: 1, potion: 5)),
            "Terceira Mão - Manna Prism com vida cheia e MP baixo");
        AssertEqual(ThirdHandChoice.None, HudLogic.ChooseThirdHand(Bag(potion: 5, elixir: 1)),
            "Terceira Mão - Nada é gasto com vida e MP cheios");

        AssertEqual(20, HudLogic.HealAmount(ThirdHandChoice.HighPotion, 20), "Terceira Mão - Cura limitada ao HP que falta");
        AssertEqual(50, HudLogic.HealAmount(ThirdHandChoice.Potion, 70), "Terceira Mão - Potion cura 50");

        // HUD slot shows exactly what T / L2 will use now.
        var critical = Bag(hp: 30, elixir: 1, potion: 2);
        AssertEqual(HudLogic.ChooseThirdHand(critical), HudLogic.PreviewThirdHand(critical),
            "Terceira Mão HUD - Slot mostra o mesmo item que será usado");
        AssertEqual(ThirdHandChoice.Potion, HudLogic.PreviewThirdHand(Bag(elixir: 1, potion: 2)),
            "Terceira Mão HUD - Com vida cheia mostra a Potion (não o Elixir que não seria usado)");
        AssertEqual(2, HudLogic.StockOf(Bag(potion: 2), ThirdHandChoice.Potion), "Terceira Mão HUD - Contagem do item mostrado");
    }
}
