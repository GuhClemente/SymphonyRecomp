using System;
using System.Collections.Generic;
using System.IO;
using System.Numerics;
using System.Reflection;
using RecompOne.Runtime.Host;
using RecompOne.Runtime.Host.Window;

namespace Recompiled.Netplay;

public static class NetplaySpriteManager
{
    private static readonly Dictionary<string, (uint TexId, Vector2 Size)> _textures = new();
    private static readonly HashSet<string> _failed = new();
    private static string[]? _resourceNames;

    public static uint GetHunterSprite(
        byte character,
        bool isAttacking,
        bool isJumping,
        out Vector2 size)
    {
        string spriteName = SelectSpriteName(character, isAttacking, isJumping);
        return GetTexture(spriteName, out size);
    }

    private static string SelectSpriteName(byte character, bool isAttacking, bool isJumping)
    {
        return character switch
        {
            1 => "richter_classic",
            2 => "richter_saturn_coat",
            5 => "richter_brown_coat",
            3 => "maria_saturn_idle",
            4 => "maria_psp_idle",
            _ => "alucard_idle"
        };
    }

    public static uint GetTexture(string name, out Vector2 size)
    {
        if (_textures.TryGetValue(name, out var cached))
        {
            size = cached.Size;
            return cached.TexId;
        }

        size = new Vector2(32, 48);
        if (_failed.Contains(name)) return 0;

        byte[]? bytes = LoadAssetBytes(name);
        if (bytes == null || bytes.Length == 0)
        {
            _failed.Add(name);
            return 0;
        }

        try
        {
            var img = RgbaImage.Load(bytes);
            uint texId = HostWindow.UploadTexture(img.Pixels, img.Width, img.Height);
            if (texId != 0)
            {
                // Normalize displayed size keeping original aspect ratio
                float h = 44f;
                float w = (img.Height > 0) ? (img.Width * (h / img.Height)) : 32f;
                Vector2 sz = new(w, h);

                _textures[name] = (texId, sz);
                size = sz;
                return texId;
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[NetplaySpriteManager] Erro ao carregar sprite {name}: {ex.Message}");
        }

        _failed.Add(name);
        return 0;
    }

    private static byte[]? LoadAssetBytes(string name)
    {
        // 1. Try direct disk paths
        string[] candidates = [
            Path.Combine(AppContext.BaseDirectory, "assets", "netplay", $"{name}.png"),
            Path.Combine("assets", "netplay", $"{name}.png")
        ];

        foreach (var path in candidates)
        {
            if (File.Exists(path))
            {
                try { return File.ReadAllBytes(path); } catch { }
            }
        }

        // 2. Try Embedded Resource
        try
        {
            var asm = Assembly.GetExecutingAssembly();
            _resourceNames ??= asm.GetManifestResourceNames();
            string suffix = $".{name}.png";

            foreach (var res in _resourceNames)
            {
                if (res.EndsWith(suffix, StringComparison.OrdinalIgnoreCase))
                {
                    using var s = asm.GetManifestResourceStream(res);
                    if (s != null)
                    {
                        using var ms = new MemoryStream();
                        s.CopyTo(ms);
                        return ms.ToArray();
                    }
                }
            }
        }
        catch { }

        return null;
    }
}
