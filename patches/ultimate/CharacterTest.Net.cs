using System;
using System.IO;
using Recompiled.Netplay;
using Sotn;

namespace Recompiled.Ultimate;

/// <summary>
/// SOTN_TEST_NET=host|client (with SOTN_TEST_MARIA=1, port SOTN_TEST_NETPORT, default 7790): two games
/// on this machine. The client plays Richter and goes through some poses, saving each frame it sends
/// (NN_..._sprite.bmp); the host stays as Maria, logs every second which partner frame it is showing and
/// whether its pixels arrived, saves that received frame (host_..._recebido.bmp) and asks for a window
/// capture ("[NETTEST] SHOT name pid") of what it draws.
/// </summary>
public static partial class CharacterTest
{
    private static int NetPort => int.TryParse(Environment.GetEnvironmentVariable("SOTN_TEST_NETPORT"), out int p) ? p : 7790;

    private static void WaitConnected(int timeout = 3600)
    {
        _script!.Add((_t, null, _ => NetworkManager.IsConnected, timeout));
        Wait(10);
    }

    private static void BuildNetScript(bool host)
    {
        if (host)
        {
            Do((_, _) => { _who = "host"; NetworkManager.StartHost(NetPort); });
            WaitConnected();
            Do((_, _) => Console.WriteLine($"[NETTEST] host conectado com '{NetworkManager.RemotePlayerName}'"));
            for (int i = 0; i < 24; i++)
            {
                int n = i;
                Wait(45);
                Do((_, m) =>
                {
                    ulong h = SpriteSync.RemoteHash;
                    var f = SpriteSync.FindRemoteFrame(h);
                    Console.WriteLine($"[NETTEST] host t={n} parceiro={RemotePuppet.CharacterName} sala=({RemotePuppet.RoomX},{RemotePuppet.RoomY}) " +
                                      $"pos=({RemotePuppet.PosX},{RemotePuppet.PosY}) mostrando=0x{h:X16} temPixels={(f != null)} " +
                                      $"recebidos={SpriteSync.FramesReceived} conectado={NetworkManager.IsConnected}");
                    if (f != null) SaveSpriteBmp(Path.Combine(Dir, $"host_{n:00}_recebido_{h:X16}.bmp"), f, 4);
                    Console.WriteLine($"[NETTEST] SHOT host_{n:00} {Environment.ProcessId}");
                });
            }
            Do((_, _) => Console.WriteLine($"[NETTEST] RESULTADO host: recebidos={SpriteSync.FramesReceived} enviados={SpriteSync.FramesSent}"));
            Wait(30);
            Do((_, _) => Environment.Exit(0));
            return;
        }

        // client: Richter, connected, then the poses twice (the second pass only repeats frames)
        Switch(CharacterSelection.RichterClassic, "cliente_richter");
        Do((_, _) => { _who = "cliente"; NetworkManager.ConnectTo("127.0.0.1", NetPort); });
        WaitConnected();
        Do((_, _) => { PlayerSpriteCapture.Active = true; Console.WriteLine("[NETTEST] cliente conectado"); });
        Wait(60);
        for (int pass = 0; pass < 3; pass++)
        {
            SpritePoses();
            Do((_, _) => Console.WriteLine($"[NETTEST] cliente passada: enviados={SpriteSync.FramesSent}"));
        }
        Do((_, _) => Console.WriteLine($"[NETTEST] RESULTADO cliente: enviados={SpriteSync.FramesSent} recebidos={SpriteSync.FramesReceived}"));
        Wait(600); // stay connected while the host finishes
        Do((_, _) => Environment.Exit(0));
    }
}
