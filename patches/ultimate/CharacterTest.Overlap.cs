using System;
using RecompOne.Runtime.Host.Window;
using Recompiled.Netplay;
using Sotn;

namespace Recompiled.Ultimate;

/// <summary>
/// SOTN_TEST_OVERLAP=1 (with SOTN_TEST_MARIA=1): the netplay puppet is fed the local player's own
/// position and real sprite, so where the overlay draws it can be compared with where the game draws
/// the player: "mesmo_lugar" must show one figure, "lado_a_lado" two figures standing on the same floor.
/// Window captures via "[HUDTEST] SHOT name" (capture.ps1).
/// </summary>
public static partial class CharacterTest
{
    private static int _overlapDx;
    private static int _envLogs = 8;

    private static void FeedPuppet(RecompOne.Runtime.Context.CpuContext c, RecompOne.Runtime.Memory.IMemory m)
    {
        byte[] payload = NetworkPacket.CreatePlayerTransform(
            (byte)Stages.Current, (byte)Stages.RoomX, (byte)Stages.RoomY,
            (short)(WorldX(m) + _overlapDx), (short)WorldY(m), 0, 0, Player.FacingLeft, 0, 0,
            (ushort)Player.Hp, (ushort)Player.HpMax, 3, 1, 0, 0);
        RemotePuppet.ReceiveTransform(payload);
        RemotePuppet.Update(c, m);
        var f = PlayerSpriteCapture.Latest;
        if (f != null) SpriteSync.InjectRemote(f);
    }

    private static void BuildOverlapScript()
    {
        Do((_, _) =>
        {
            _who = "overlap"; PlayerSpriteCapture.Active = true; RemotePuppetOverlay.TestForce = true;
            RecompOne.Runtime.Events.Event.AddListener<RecompOne.Runtime.Events.DrawEnvEvent>(e =>
            {
                if (_envLogs-- > 0)
                    Console.WriteLine($"[HUDTEST] drawenv clip=({e.ClipX},{e.ClipY},{e.ClipW},{e.ClipH}) ofs=({e.OfsX},{e.OfsY}) bg={e.IsBackground} " +
                                      $"display=({RecompOne.Runtime.Runtime.Gpu?.DisplayX},{RecompOne.Runtime.Runtime.Gpu?.DisplayY})");
            });
        });
        Wait(60);
        WaitIdle();
        foreach (var (dx, name) in new[] { (0, "mesmo_lugar"), (40, "lado_a_lado"), (-40, "lado_a_lado_esq") })
        {
            Do((_, _) => _overlapDx = dx);
            for (int i = 0; i < 40; i++) { Do(FeedPuppet); Wait(1); }
            Do((_, _) =>
            {
                var gpu = RecompOne.Runtime.Runtime.Gpu;
                Console.WriteLine($"[HUDTEST] geometria: display={gpu?.DisplayWidth}x{gpu?.DisplayHeight} margem={WidescreenPatch.Margin} " +
                                  $"imagem={OutputPanel.ScreenImageSize} puppet=({RemotePuppet.InterpX},{RemotePuppet.InterpY}) jogador_tela=({Player.PosX},{Player.PosY})");
                Console.WriteLine($"[HUDTEST] SHOT {name}");
                RecompOne.Runtime.Host.HostWindow.RequestScreenshot(System.IO.Path.Combine(Dir, $"{name}_jogo.bmp"), 2);
            });
            // keep feeding while the window is captured
            for (int i = 0; i < 150; i++) { Do(FeedPuppet); Wait(1); }
        }
        Do((_, _) => { RemotePuppetOverlay.TestForce = false; RemotePuppet.Reset(); Console.WriteLine("[HUDTEST] RESULTADO: capturas prontas"); });
        Wait(30);
        Do((_, _) => Environment.Exit(0));
    }
}
