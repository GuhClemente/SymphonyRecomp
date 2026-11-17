using System;
using RecompOne.Runtime.Host.Window;
using Sotn;

namespace Recompiled.Netplay;

/// <summary>
/// Sistema de Pings Táticos e Marcadores de Cenário / Mapa (Co-op Ping System).
/// Permite que os caçadores coordenem exploração, avisem sobre chefes e itens secretos.
/// </summary>
public static class CoopPingManager
{
    public static readonly (string Label, string Description, string Icon)[] PingPresets =
    [
        ("Item / Segredo", "Achei uma passagem secreta ou item raro aqui!", "[ITEM]"),
        ("Perigo!", "Cuidado! Inimigo perigoso ou armadilha nesta sala!", "[PERIGO]"),
        ("Teleporte / Ajuda", "Preciso de ajuda urgente! Teleporte ate mim com Buddy Warp!", "[AJUDA]"),
        ("Caminho Certo", "Venha por aqui! Este e o caminho para o objetivo!", "[CAMINHO]")
    ];

    public static bool HasActivePing => PingTimer > 0f;
    public static byte ActivePingType { get; private set; } = 0;
    public static byte PingStage { get; private set; } = 0;
    public static byte PingRoomX { get; private set; } = 0;
    public static byte PingRoomY { get; private set; } = 0;
    public static string PingSender { get; private set; } = "";
    public static float PingTimer { get; private set; } = 0f;

    public static void SendPing(byte pingType)
    {
        if (!NetworkManager.IsConnected || !Game.Available || !Game.InGame) return;

        pingType = (byte)Math.Clamp((int)pingType, 0, PingPresets.Length - 1);
        byte stage = (byte)Stages.Current;
        byte rx = (byte)Stages.RoomX;
        byte ry = (byte)Stages.RoomY;

        byte[] payload = NetworkPacket.CreateCoopPing(pingType, stage, rx, ry, NetworkManager.LocalPlayerName);
        NetworkManager.SendPacket(new NetworkPacket(PacketOpCode.CoopPing, payload));

        SetPing(pingType, stage, rx, ry, NetworkManager.LocalPlayerName);
        ToastNotifications.ShowText(
            $"Marcacao: {PingPresets[pingType].Label}",
            $"Voce marcou a sala ({rx}, {ry}) para o parceiro!",
            null,
            4.0f
        );
    }

    public static void ReceivePing(byte[] payload)
    {
        if (NetworkPacket.ReadCoopPing(payload, out byte type, out byte stage, out byte rx, out byte ry, out string sender))
        {
            type = (byte)Math.Clamp((int)type, 0, PingPresets.Length - 1);
            SetPing(type, stage, rx, ry, sender);

            ToastNotifications.ShowText(
                $"{PingPresets[type].Icon} Ping de {sender}: {PingPresets[type].Label}",
                $"{PingPresets[type].Description} [Sala: {rx}, {ry}]",
                null,
                5.0f
            );
        }
    }

    private static void SetPing(byte type, byte stage, byte rx, byte ry, string sender)
    {
        ActivePingType = type;
        PingStage = stage;
        PingRoomX = rx;
        PingRoomY = ry;
        PingSender = sender;
        PingTimer = 10.0f; // Exibe o marcador por 10 segundos
    }

    public static void Update(float dt)
    {
        if (PingTimer > 0f)
        {
            PingTimer -= dt;
            if (PingTimer < 0f) PingTimer = 0f;
        }
    }
}
