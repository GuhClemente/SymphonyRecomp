using System;
using System.Collections.Generic;
using RecompOne.Runtime.Host;

namespace Recompiled.Netplay;

/// <summary>
/// Sends the local player's real sprite (PlayerSpriteCapture) to the partner and keeps the partner's.
/// Each frame sends only which frame is on screen (SpriteShow, a hash); the pixels of a frame go once
/// (SpriteFrame) the first time it appears, and again if the partner asks (SpriteRequest) because UDP
/// lost it. Frames repeat (an animation is a few dozen of them), so after a few seconds nearly every
/// frame is already on the other side.
/// </summary>
public static class SpriteSync
{
    private const int MaxFrames = 2048;

    // ---- sending (game thread) ----
    private static readonly Dictionary<ulong, SpriteFrame> _mine = [];
    private static readonly Queue<ulong> _mineOrder = new();
    private static readonly HashSet<ulong> _sent = [];
    private static ulong _lastShown;

    // ---- receiving: game thread writes, render thread uploads and draws ----
    private sealed class Remote
    {
        public SpriteFrame Frame = null!;
        public byte[]? Rgba;   // waiting for upload on the render thread
        public uint Texture;
    }

    private static readonly object Lock = new();
    private static readonly Dictionary<ulong, Remote> _theirs = [];
    private static readonly Dictionary<ulong, int> _requestedAt = [];
    private static int _frame;

    /// <summary>The frame the partner shows now (0 = none yet).</summary>
    public static ulong RemoteHash { get; private set; }

    /// <summary>Frames sent / received so far (for logs and the test).</summary>
    public static int FramesSent { get; private set; }
    public static int FramesReceived { get; private set; }

    public static void Reset()
    {
        lock (Lock)
        {
            _sent.Clear();
            _lastShown = 0;
            RemoteHash = 0;
            _requestedAt.Clear();
            // textures stay valid: if the same partner reconnects their frames are reused
        }
    }

    /// <summary>Once per game frame while connected (NetworkManager.Update).</summary>
    public static void Update()
    {
        _frame++;
        PlayerSpriteCapture.Active = true;
        var f = PlayerSpriteCapture.Latest;
        if (f == null) return;

        if (!_mine.ContainsKey(f.Hash))
        {
            _mine[f.Hash] = f;
            _mineOrder.Enqueue(f.Hash);
            while (_mineOrder.Count > MaxFrames) _mine.Remove(_mineOrder.Dequeue());
        }

        if (_sent.Add(f.Hash)) SendFrame(f);

        // the hash every frame is 9 bytes; it is also what tells the partner a frame it lacks
        NetworkManager.SendPacket(new NetworkPacket(PacketOpCode.SpriteShow, NetworkPacket.CreateSpriteShow(f.Hash)));
        _lastShown = f.Hash;
    }

    private static void SendFrame(SpriteFrame f)
    {
        byte[] payload = NetworkPacket.CreateSpriteFrame(f.Hash, f.OffsetX, f.OffsetY, f.Width, f.Height, f.Pixels);
        NetworkManager.SendPacket(new NetworkPacket(PacketOpCode.SpriteFrame, payload));
        FramesSent++;
    }

    public static void HandleShow(byte[] payload)
    {
        if (!NetworkPacket.ReadSpriteShow(payload, out ulong hash)) return;
        bool have;
        lock (Lock)
        {
            RemoteHash = hash;
            have = _theirs.ContainsKey(hash);
            // ask again at most every half second for the same frame
            if (!have && _requestedAt.TryGetValue(hash, out int at) && _frame - at < 30) return;
            if (!have) _requestedAt[hash] = _frame;
        }
        if (!have)
            NetworkManager.SendPacket(new NetworkPacket(PacketOpCode.SpriteRequest, NetworkPacket.CreateSpriteRequest(hash)));
    }

    public static void HandleRequest(byte[] payload)
    {
        if (NetworkPacket.ReadSpriteRequest(payload, out ulong hash) && _mine.TryGetValue(hash, out var f))
            SendFrame(f);
    }

    public static void HandleFrame(byte[] payload)
    {
        if (!NetworkPacket.ReadSpriteFrame(payload, out ulong hash, out int ox, out int oy, out int w, out int h, out ushort[] px))
            return;
        var frame = new SpriteFrame { Hash = hash, OffsetX = ox, OffsetY = oy, Width = w, Height = h, Pixels = px };
        lock (Lock)
        {
            if (_theirs.ContainsKey(hash)) return;
            _theirs[hash] = new Remote { Frame = frame, Rgba = frame.ToRgba() };
            _requestedAt.Remove(hash);
            FramesReceived++;
        }
    }

    /// <summary>Render thread: the partner's frame <paramref name="hash"/> as a texture, uploading it on
    /// first use. False while that frame has not arrived.</summary>
    public static bool TryGetTexture(ulong hash, out uint texture, out SpriteFrame frame)
    {
        texture = 0;
        frame = null!;
        if (hash == 0) return false;
        lock (Lock)
        {
            if (!_theirs.TryGetValue(hash, out var r)) return false;
            if (r.Texture == 0 && r.Rgba != null)
            {
                r.Texture = HostWindow.UploadTexture(r.Rgba, r.Frame.Width, r.Frame.Height);
                if (r.Texture != 0) r.Rgba = null;
            }
            if (r.Texture == 0) return false;
            texture = r.Texture;
            frame = r.Frame;
            return true;
        }
    }

    /// <summary>The partner's frame <paramref name="hash"/> if its pixels arrived (any thread).</summary>
    public static SpriteFrame? FindRemoteFrame(ulong hash)
    {
        lock (Lock) return _theirs.TryGetValue(hash, out var r) ? r.Frame : null;
    }

    /// <summary>Test aid: puts a frame on the receiving side as if it came from the partner.</summary>
    public static void InjectRemote(SpriteFrame f)
    {
        HandleFrame(NetworkPacket.CreateSpriteFrame(f.Hash, f.OffsetX, f.OffsetY, f.Width, f.Height, f.Pixels));
        lock (Lock) RemoteHash = f.Hash;
    }
}
