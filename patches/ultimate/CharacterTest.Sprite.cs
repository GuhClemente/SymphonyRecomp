using System;
using System.IO;
using Recompiled.Netplay;
using Sotn;

namespace Recompiled.Ultimate;

/// <summary>
/// SOTN_TEST_SPRITE=1 (with SOTN_TEST_MARIA=1): every character in a few poses; each pose saves the
/// game's own screenshot (NN_who_pose.bmp) and the sprite PlayerSpriteCapture cut out of the same frame
/// (NN_who_pose_sprite.bmp, 4x, magenta = transparent), to compare pixel for pixel.
/// </summary>
public static partial class CharacterTest
{
    private static void SpriteShot(string step)
    {
        Shot(step);
        Do((_, _) =>
        {
            var f = PlayerSpriteCapture.Latest;
            if (f == null)
            {
                Console.WriteLine($"[SPRITE] {_who}_{step}: nada capturado");
                return;
            }
            int opaque = 0;
            foreach (var c in f.Pixels) if (c != 0) opaque++;
            Console.WriteLine($"[SPRITE] {_who}_{step}: {f.Width}x{f.Height} offset=({f.OffsetX},{f.OffsetY}) prims={PlayerSpriteCapture.LastPrimCount} " +
                              $"opacos={opaque} hash={f.Hash:X16} clut=0x{Player.Entity.ClutId:X4}");
            SaveSpriteBmp(Path.Combine(Dir, $"{_shot:000}_{_who}_{step}_sprite.bmp"), f, 4);
        });
        Wait(5);
    }

    private static void SaveSpriteBmp(string path, SpriteFrame f, int scale)
    {
        int w = f.Width * scale, h = f.Height * scale;
        int stride = (w * 3 + 3) & ~3;
        var data = new byte[54 + stride * h];
        void U32(int o, int v) { data[o] = (byte)v; data[o + 1] = (byte)(v >> 8); data[o + 2] = (byte)(v >> 16); data[o + 3] = (byte)(v >> 24); }
        data[0] = (byte)'B'; data[1] = (byte)'M';
        U32(2, data.Length); U32(10, 54); U32(14, 40); U32(18, w); U32(22, h);
        data[26] = 1; data[28] = 24; U32(34, stride * h);
        for (int y = 0; y < h; y++)
        {
            int row = 54 + (h - 1 - y) * stride;
            for (int x = 0; x < w; x++)
            {
                ushort c = f.Pixels[(y / scale) * f.Width + x / scale];
                byte r = 255, g = 0, b = 255;
                if (c != 0)
                {
                    r = (byte)((c & 0x1F) << 3); g = (byte)(((c >> 5) & 0x1F) << 3); b = (byte)(((c >> 10) & 0x1F) << 3);
                    if ((c & 0x8000) == 0) { r = (byte)((r + 255) / 2); g = (byte)(g / 2); b = (byte)((b + 255) / 2); } // 50% over magenta
                }
                data[row + x * 3] = b; data[row + x * 3 + 1] = g; data[row + x * 3 + 2] = r;
            }
        }
        File.WriteAllBytes(path, data);
    }

    private static void SpritePoses()
    {
        WaitIdle();
        SpriteShot("parado");
        Press(Button.Right); Wait(20); SpriteShot("andando"); Release(Button.Right); Wait(20);
        WaitIdle();
        Press(Button.Square); Wait(7); Do((_, _) => { Console.WriteLine($"[SPRITEPROBE] ===== {_who} ataque ====="); PlayerSpriteCapture.ProbeFrames = 2; }); Wait(1);
        SpriteShot("ataque"); Release(Button.Square); Wait(50);
        WaitIdle();
        Press(Button.Cross); Wait(11); Do((_, _) => { Console.WriteLine($"[SPRITEPROBE] ===== {_who} pulo ====="); PlayerSpriteCapture.ProbeFrames = 2; }); Wait(1);
        SpriteShot("pulo"); Release(Button.Cross); Wait(60);
        WaitIdle();
        Press(Button.Left); Wait(20); SpriteShot("andando_esq"); Release(Button.Left); Wait(20);
    }

    private static void BuildSpriteScript()
    {
        Do((_, _) => { PlayerSpriteCapture.Active = true; _who = "maria_saturn"; });
        Wait(30);
        SpritePoses();
        foreach (var (who, name) in new[]
                 {
                     (CharacterSelection.PSPMaria, "maria_psp"),
                     (CharacterSelection.RichterClassic, "richter"),
                     (CharacterSelection.Alucard, "alucard"),
                 })
        {
            Switch(who, name);
            SpritePoses();
        }
        Do((_, _) => Console.WriteLine("[MariaTest] SEQUÊNCIA COMPLETA"));
        Wait(30);
        Do((_, _) => Environment.Exit(0));
    }
}
