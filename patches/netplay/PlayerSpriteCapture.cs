using System;
using RecompOne.Runtime.Events;
using Sotn;

namespace Recompiled.Netplay;

/// <summary>One captured frame of the player's sprite in a box placed at (OffsetX, OffsetY) from the
/// player entity's screen position. Each pixel: 0 = transparent; bit 15 set = opaque, bit 15 clear =
/// semi-transparent (drawn at 50%, like the PS1's average blend); bits 0-14 the 15-bit color.</summary>
public sealed class SpriteFrame
{
    public ulong Hash;
    public int OffsetX, OffsetY, Width, Height;
    public ushort[] Pixels = [];

    /// <summary>RGBA bytes for an ImGui texture; color 0 is transparent, as on the PS1.</summary>
    public byte[] ToRgba()
    {
        var rgba = new byte[Width * Height * 4];
        for (int i = 0; i < Pixels.Length; i++)
        {
            ushort c = Pixels[i];
            if (c == 0) continue;
            rgba[i * 4 + 0] = (byte)((c & 0x1F) << 3);
            rgba[i * 4 + 1] = (byte)(((c >> 5) & 0x1F) << 3);
            rgba[i * 4 + 2] = (byte)(((c >> 10) & 0x1F) << 3);
            rgba[i * 4 + 3] = (c & 0x8000) != 0 ? (byte)255 : (byte)128;
        }
        return rgba;
    }
}

/// <summary>
/// Captures the local player's sprite exactly as the game draws it, rasterized from the GPU's VRAM copy
/// at the moment each prim is drawn (the player's graphics are re-uploaded to VRAM every frame, so later
/// would be too late), into a screen-space canvas. Taken, as seen in game with a prim probe:
///  - the body: the player entity's CLUT drawn unmodulated (raw); the same CLUT drawn colour-modulated
///    is the afterimage trail, left out (the partner draws its own trail from earlier real frames);
///  - weapons and attack effects: the player's weapon texture page (576,256) near the player.
/// <see cref="TakeFrame"/>, once per game frame, cuts the drawn box out and returns it.
/// </summary>
public static class PlayerSpriteCapture
{
    /// <summary>Capture runs only while something needs it (a netplay session or a test).</summary>
    public static volatile bool Active;
    /// <summary>Test aid: frames left in which every textured prim near the player is logged.</summary>
    public static int ProbeFrames;

    // PS1 screen space with room for widescreen prims to the left and right
    private const int CanvasW = 640, CanvasH = 320, OriginX = 192, OriginY = 48;
    private const int MaxReach = 128; // prims farther than this from the player are not the player
    private const int MaxSize = 160;
    private const int WeaponPageX = 576, WeaponPageY = 256;

    private static readonly ushort[] Canvas = new ushort[CanvasW * CanvasH];
    private static readonly object Lock = new();
    private static int _minX = int.MaxValue, _minY = int.MaxValue, _maxX = int.MinValue, _maxY = int.MinValue;
    private static int _playerX, _playerY;
    private static ushort _clut;
    private static bool _frameStarted;
    private static bool _initialized;

    /// <summary>Prims taken into the last frame (for the test log).</summary>
    public static int LastPrimCount { get; private set; }
    private static int _primCount;

    public static void Initialize()
    {
        if (_initialized) return;
        _initialized = true;
        Event.AddListener<RenderPrimEvent>(OnPrim);
        // The game sets its draw environment once per frame right before drawing that frame's ordering
        // table, so that is where the previous frame's capture is complete.
        Event.AddListener<DrawEnvEvent>(_ => Pump());
    }

    private static void OnPrim(RenderPrimEvent e)
    {
        if (!Active || !e.Textured) return;
        var gpu = RecompOne.Runtime.Runtime.Gpu;
        if (gpu == null || !Game.Available || !Game.InGame) return;

        lock (Lock)
        {
            if (!_frameStarted)
            {
                // the player's palette and position as of this frame's draw
                var pl = Player.Entity;
                _clut = pl.ClutId;
                _playerX = pl.PosX;
                _playerY = pl.PosY;
                _frameStarted = true;
            }
            if (ProbeFrames > 0)
            {
                int mx = e.X[0], my = e.Y[0];
                if (Math.Abs(mx - _playerX) < 80 && Math.Abs(my - _playerY) < 80)
                    Console.WriteLine($"[SPRITEPROBE] clut=0x{e.Clut:X4} player=0x{_clut:X4} semi={e.SemiTransparent} raw={e.Raw} n={e.Count} " +
                                      $"tp=({e.TexPageX},{e.TexPageY},{e.TexDepth}) xy=({e.X[0]},{e.Y[0]}) uv=({e.U[0]},{e.V[0]})");
            }
            bool body = _clut != 0 && e.Clut == _clut && e.Raw;
            bool weapon = e.TexPageX == WeaponPageX && e.TexPageY == WeaponPageY;
            if (!body && !weapon) return;

            int n = e.Count == 2 ? 2 : e.Count;
            int bx0 = int.MaxValue, by0 = int.MaxValue, bx1 = int.MinValue, by1 = int.MinValue;
            for (int i = 0; i < n; i++)
            {
                bx0 = Math.Min(bx0, e.X[i]); bx1 = Math.Max(bx1, e.X[i]);
                by0 = Math.Min(by0, e.Y[i]); by1 = Math.Max(by1, e.Y[i]);
            }
            // a prim far from the player with the same palette (menus, portraits) is not the sprite
            if (bx1 < _playerX - MaxReach || bx0 > _playerX + MaxReach ||
                by1 < _playerY - MaxReach || by0 > _playerY + MaxReach) return;

            var shadow = gpu.Shadow;
            if (e.Count == 2)
                RasterRect(shadow, e, e.X[0], e.Y[0], e.X[1] - e.X[0], e.Y[1] - e.Y[0]);
            else
            {
                RasterTri(shadow, e, 0, 1, 2);
                if (e.Count == 4) RasterTri(shadow, e, 1, 2, 3);
            }
            _primCount++;
        }
    }

    private static ushort Texel(RecompOne.Runtime.Hle.VramShadow vram, RenderPrimEvent e, int u, int v)
    {
        int x = e.TexPageX, y = e.TexPageY + v;
        int clut = e.Clut;
        switch (e.TexDepth)
        {
            case 0:
            {
                var word = vram[(x + (u >> 2)) & 1023, y & 511];
                var index = (word >> ((u & 3) << 2)) & 0xF;
                return vram[((clut & 0x3F) * 16 + index) & 1023, (clut >> 6) & 511];
            }
            case 1:
            {
                var word = vram[(x + (u >> 1)) & 1023, y & 511];
                var index = (word >> ((u & 1) << 3)) & 0xFF;
                return vram[((clut & 0x3F) * 16 + index) & 1023, (clut >> 6) & 511];
            }
            default:
                return vram[(x + u) & 1023, y & 511];
        }
    }

    private static void Plot(int sx, int sy, ushort t, bool semiPrim)
    {
        if (t == 0) return; // PS1: texel 0 is transparent
        int cx = sx + OriginX, cy = sy + OriginY;
        if ((uint)cx >= CanvasW || (uint)cy >= CanvasH) return;
        // semi-transparency applies to texels with the STP bit in a semi-transparent prim
        bool semi = semiPrim && (t & 0x8000) != 0;
        ushort color = (ushort)(t & 0x7FFF);
        ushort stored = semi ? (color == 0 ? (ushort)1 : color) : (ushort)(color | 0x8000);
        int i = cy * CanvasW + cx;
        if (semi && (Canvas[i] & 0x8000) != 0) return; // a see-through effect does not cover the body
        Canvas[i] = stored;
        if (sx < _minX) _minX = sx;
        if (sy < _minY) _minY = sy;
        if (sx > _maxX) _maxX = sx;
        if (sy > _maxY) _maxY = sy;
    }

    private static void RasterRect(RecompOne.Runtime.Hle.VramShadow vram, RenderPrimEvent e, int x, int y, int w, int h)
    {
        if (w <= 0 || h <= 0 || w > 256 || h > 256) return;
        for (int dy = 0; dy < h; dy++)
            for (int dx = 0; dx < w; dx++)
                Plot(x + dx, y + dy, Texel(vram, e, (e.U[0] + dx) & 0xFF, (e.V[0] + dy) & 0xFF), e.SemiTransparent);
    }

    // Affine UV over the triangle, sampled at pixel centres with a top-left fill rule like the GPU.
    private static void RasterTri(RecompOne.Runtime.Hle.VramShadow vram, RenderPrimEvent e, int a, int b, int c)
    {
        int x0 = e.X[a], y0 = e.Y[a], x1 = e.X[b], y1 = e.Y[b], x2 = e.X[c], y2 = e.Y[c];
        long area = (long)(x1 - x0) * (y2 - y0) - (long)(x2 - x0) * (y1 - y0);
        if (area == 0) return;
        int minX = Math.Min(x0, Math.Min(x1, x2)), maxX = Math.Max(x0, Math.Max(x1, x2));
        int minY = Math.Min(y0, Math.Min(y1, y2)), maxY = Math.Max(y0, Math.Max(y1, y2));
        if (maxX - minX > 256 || maxY - minY > 256) return;

        for (int py = minY; py < maxY; py++)
        {
            for (int px = minX; px < maxX; px++)
            {
                // barycentric weights at the pixel centre
                double cxp = px + 0.5, cyp = py + 0.5;
                double w0 = ((x1 - cxp) * (y2 - cyp) - (x2 - cxp) * (y1 - cyp)) / area;
                double w1 = ((x2 - cxp) * (y0 - cyp) - (x0 - cxp) * (y2 - cyp)) / area;
                double w2 = 1.0 - w0 - w1;
                if (w0 < 0 || w1 < 0 || w2 < 0) continue;
                int u = (int)(w0 * e.U[a] + w1 * e.U[b] + w2 * e.U[c]);
                int v = (int)(w0 * e.V[a] + w1 * e.V[b] + w2 * e.V[c]);
                Plot(px, py, Texel(vram, e, u & 0xFF, v & 0xFF), e.SemiTransparent);
            }
        }
    }

    /// <summary>The newest frame <see cref="Pump"/> produced (kept when a frame drew nothing).</summary>
    public static volatile SpriteFrame? Latest;

    /// <summary>Closes the frame being captured (on every PutDrawEnv).</summary>
    public static void Pump()
    {
        if (!Active) return;
        var f = TakeFrame();
        if (f != null) Latest = f;
    }

    /// <summary>Called once per game frame: returns what was drawn of the player since the last call
    /// (null when nothing was) and clears the canvas for the next frame.</summary>
    public static SpriteFrame? TakeFrame()
    {
        lock (Lock)
        {
            SpriteFrame? frame = null;
            LastPrimCount = _primCount;
            if (_maxX >= _minX && _maxY >= _minY)
            {
                int w = Math.Min(MaxSize, _maxX - _minX + 1);
                int h = Math.Min(MaxSize, _maxY - _minY + 1);
                var px = new ushort[w * h];
                ulong hash = 1469598103934665603UL; // FNV-1a over size + pixels
                hash = (hash ^ (ulong)w) * 1099511628211UL;
                hash = (hash ^ (ulong)h) * 1099511628211UL;
                for (int y = 0; y < h; y++)
                {
                    int row = (y + _minY + OriginY) * CanvasW + _minX + OriginX;
                    for (int x = 0; x < w; x++)
                    {
                        ushort c = Canvas[row + x];
                        px[y * w + x] = c;
                        hash = (hash ^ c) * 1099511628211UL;
                    }
                }
                frame = new SpriteFrame
                {
                    Hash = hash,
                    OffsetX = _minX - _playerX,
                    OffsetY = _minY - _playerY,
                    Width = w,
                    Height = h,
                    Pixels = px,
                };
                // offsets go into the hash too: same pixels drawn elsewhere is a different frame
                frame.Hash = (frame.Hash ^ (uint)(frame.OffsetX & 0xFFFF) ^ ((ulong)(uint)(frame.OffsetY & 0xFFFF) << 16)) * 1099511628211UL;

                for (int y = _minY; y <= _maxY; y++)
                {
                    int cy = y + OriginY;
                    if ((uint)cy >= CanvasH) continue;
                    Array.Clear(Canvas, cy * CanvasW + Math.Max(0, _minX + OriginX), Math.Min(CanvasW, _maxX + OriginX + 1) - Math.Max(0, _minX + OriginX));
                }
            }
            _minX = _minY = int.MaxValue;
            _maxX = _maxY = int.MinValue;
            _frameStarted = false;
            _primCount = 0;
            if (ProbeFrames > 0 && --ProbeFrames >= 0) Console.WriteLine("[SPRITEPROBE] ---- fim do quadro ----");
            return frame;
        }
    }
}
