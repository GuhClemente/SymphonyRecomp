using System;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Host.Window;
using RecompOne.Runtime.Memory;
using Sotn;

namespace Recompiled.Netplay;

/// <summary>
/// Gerenciador de Reviver / Queda Cooperativa (Co-op Downed & Revival System).
/// Impede o Game Over instantâneo permitindo que um caçador caído seja reanimado
/// por seu companheiro de equipe dentro do tempo limite de 30 segundos.
/// </summary>
public static class CoopRevivalManager
{
    public const float MaxBleedoutSeconds = 30.0f;

    public static bool IsLocalDowned { get; private set; } = false;
    public static bool IsRemoteDowned { get; private set; } = false;
    public static float LocalBleedoutTimer { get; private set; } = MaxBleedoutSeconds;
    public static float RemoteBleedoutTimer { get; private set; } = MaxBleedoutSeconds;

    public static void Reset()
    {
        IsLocalDowned = false;
        IsRemoteDowned = false;
        LocalBleedoutTimer = MaxBleedoutSeconds;
        RemoteBleedoutTimer = MaxBleedoutSeconds;
    }

    public static void CheckLocalPlayerHealth(CpuContext c, IMemory m)
    {
        if (!Game.Available || !Game.InGame || Game.IsLoading || !NetworkManager.IsConnected) return;

        // Se o HP do jogador local chegou a zero e ele ainda não está no estado de queda
        if (Player.Hp <= 0 && !IsLocalDowned)
        {
            TriggerLocalDowned();
        }
    }

    public static void TriggerLocalDowned()
    {
        IsLocalDowned = true;
        LocalBleedoutTimer = MaxBleedoutSeconds;

        // Mantém 1 HP para evitar que a engine acione o Game Over tradicional imediatamente
        Player.Hp = 1;

        // Notifica o companheiro de rede
        byte[] payload = NetworkPacket.CreatePlayerDowned(
            NetworkManager.LocalPlayerName,
            (byte)Stages.Current,
            (byte)Stages.RoomX,
            (byte)Stages.RoomY
        );
        NetworkManager.SendPacket(new NetworkPacket(PacketOpCode.PlayerDowned, payload));

        ToastNotifications.ShowText(
            "VOCE FOI DERRUBADO!",
            "Voce caiu em combate! Aguarde seu companheiro de equipe para reanima-lo! (30s)",
            null,
            6.0f
        );
    }

    public static void HandleRemotePlayerDowned(byte[] payload)
    {
        if (NetworkPacket.ReadPlayerDowned(payload, out string downedName, out byte stage, out byte rx, out byte ry))
        {
            IsRemoteDowned = true;
            RemoteBleedoutTimer = MaxBleedoutSeconds;

            ToastNotifications.ShowText(
                "ALERTA: COMPANHEIRO CAIDO!",
                $"{downedName} foi derrubado na sala ({rx}, {ry})! Aproxime-se para reanima-lo!",
                null,
                7.0f
            );
        }
    }

    public static void RequestReviveRemotePlayer()
    {
        if (!IsRemoteDowned || !NetworkManager.IsConnected) return;

        ushort healAmount = (ushort)Math.Max(50, RemotePuppet.MaxHp * 35 / 100);
        byte[] payload = NetworkPacket.CreatePlayerRevived(NetworkManager.LocalPlayerName, healAmount);
        NetworkManager.SendPacket(new NetworkPacket(PacketOpCode.PlayerRevived, payload));

        IsRemoteDowned = false;
        ToastNotifications.ShowText(
            "Companheiro Reanimado!",
            $"Voce reanimou {NetworkManager.RemotePlayerName}! A aventura continua!",
            null,
            4.0f
        );
    }

    public static void HandleRemotePlayerRevived(byte[] payload)
    {
        if (NetworkPacket.ReadPlayerRevived(payload, out string reviverName, out ushort restoredHp))
        {
            IsLocalDowned = false;
            Player.Hp = Math.Max(Player.Hp, (int)restoredHp);

            ToastNotifications.ShowText(
                "RESSURREICAO CO-OP!",
                $"{reviverName} reanimou voce com a luz sagrada! (+{restoredHp} HP)",
                null,
                5.0f
            );
        }
    }

    public static bool CanReviveRemote()
    {
        if (!IsRemoteDowned || !NetworkManager.IsConnected || !RemotePuppet.IsActive) return false;

        bool sameStage = RemotePuppet.StageId == (byte)Stages.Current;
        bool sameRoom = RemotePuppet.RoomX == (byte)Stages.RoomX && RemotePuppet.RoomY == (byte)Stages.RoomY;
        if (!sameStage || !sameRoom) return false;

        float dx = Math.Abs(Player.PosX - RemotePuppet.PosX);
        float dy = Math.Abs(Player.PosY - RemotePuppet.PosY);
        return dx <= 70f && dy <= 50f;
    }

    public static void Update(float dt)
    {
        if (!NetworkManager.IsConnected)
        {
            if (IsLocalDowned || IsRemoteDowned) Reset();
            return;
        }

        // Timer de sangramento do jogador local
        if (IsLocalDowned)
        {
            Player.Hp = 1; // Preserva o estado de espera
            LocalBleedoutTimer -= dt;

            if (LocalBleedoutTimer <= 0f)
            {
                IsLocalDowned = false;
                Player.Hp = 0; // Sucumbe: Game Over genuíno
                ToastNotifications.ShowText("Fim de Jogo", "Voce sucumbiu aos ferimentos em batalha.", null, 5.0f);
            }
        }

        // Timer de sangramento do parceiro remoto
        if (IsRemoteDowned)
        {
            RemoteBleedoutTimer -= dt;
            if (RemoteBleedoutTimer <= 0f)
            {
                IsRemoteDowned = false;
            }
        }
    }
}
