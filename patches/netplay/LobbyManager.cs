using System;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Host.Window;
using RecompOne.Runtime.Memory;
using Recompiled.Ultimate;
using Sotn;

namespace Recompiled.Netplay;

public static class LobbyManager
{
    public static bool LocalReady { get; private set; } = false;
    public static bool RemoteReady { get; private set; } = false;
    public static CharacterSelection LocalCharacter { get; private set; } = CharacterSelection.Alucard;
    public static CharacterSelection RemoteCharacter { get; private set; } = CharacterSelection.Alucard;
    public static int SelectedLocationIndex { get; set; } = 0;

    public static readonly (string Name, Stage Stage, byte RoomX, byte RoomY, short OffsetX, short OffsetY)[] StartingLocations =
    [
        ("Entrada do Castelo (Save Room)", Stage.CastleEntrance, 17, 37, 72, 164),
        ("Portão Principal do Castelo", Stage.CastleEntrance, 0, 37, 80, 136),
        ("Prólogo: Confronto de 1792", Stage.Prologue, 0, 0, 128, 128)
    ];

    public static void Reset()
    {
        LocalReady = false;
        RemoteReady = false;
        LocalCharacter = CharacterSelection.Alucard;
        RemoteCharacter = CharacterSelection.Alucard;
        SelectedLocationIndex = 0;
    }

    public static void SetLocalCharacter(CharacterSelection character)
    {
        if (LocalCharacter == character) return;
        LocalCharacter = character;
        SendLobbySync();
    }

    public static void ToggleLocalReady()
    {
        LocalReady = !LocalReady;
        SendLobbySync();
    }

    public static void SetStartingLocation(int index)
    {
        if (index < 0 || index >= StartingLocations.Length) return;
        SelectedLocationIndex = index;
        SendLobbySync();
    }

    public static void SendLobbySync()
    {
        if (!NetworkManager.IsConnected) return;
        byte[] payload = NetworkPacket.CreateLobbySync(LocalReady, (byte)LocalCharacter, (byte)SelectedLocationIndex);
        NetworkManager.SendPacket(new NetworkPacket(PacketOpCode.LobbySync, payload));
    }

    public static void HandleLobbySync(byte[] payload)
    {
        if (NetworkPacket.ReadLobbySync(payload, out bool ready, out byte character, out byte stageIndex))
        {
            RemoteReady = ready;
            RemoteCharacter = (CharacterSelection)Math.Clamp((int)character, 0, 5);
            if (stageIndex < StartingLocations.Length && NetworkManager.Role == NetworkRole.Client)
            {
                SelectedLocationIndex = stageIndex;
            }
        }
    }

    public static void RequestStartGame()
    {
        if (NetworkManager.Role != NetworkRole.Host || !NetworkManager.IsConnected) return;

        var loc = StartingLocations[SelectedLocationIndex];
        byte[] payload = NetworkPacket.CreateStartGameSync(
            (byte)loc.Stage,
            loc.RoomX,
            loc.RoomY,
            loc.OffsetX,
            loc.OffsetY
        );

        NetworkManager.SendPacket(new NetworkPacket(PacketOpCode.StartGameSync, payload));
        ExecuteSynchronizedGameStart(loc.Stage, loc.RoomX, loc.RoomY, loc.OffsetX, loc.OffsetY, LocalCharacter);
    }

    public static void HandleStartGameSync(byte[] payload)
    {
        if (NetworkPacket.ReadStartGameSync(payload, out byte stage, out byte roomX, out byte roomY, out short posX, out short posY))
        {
            ExecuteSynchronizedGameStart((Stage)stage, roomX, roomY, posX, posY, LocalCharacter);
        }
    }

    private static void ExecuteSynchronizedGameStart(Stage stage, byte roomX, byte roomY, short posX, short posY, CharacterSelection character)
    {
        GameThread.Post((c, m) =>
        {
            if (Game.Available)
            {
                // 1. Switch character
                MariaCharacter.SwitchToCharacter(c, m, character);

                // 2. Teleport to synchronized starting spot
                Player.TeleportTo(stage, roomX, roomY, posX, posY);

                // 3. Restore full health
                Player.Hp = Player.HpMax;

                ToastNotifications.ShowText("Aventura Co-op Iniciada!", $"Caçadores unidos em {stage}! Que a caçada a Drácula comece!", null, 5.0f);
            }
        });
    }
}
