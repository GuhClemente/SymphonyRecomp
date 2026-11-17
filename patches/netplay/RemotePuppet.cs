using System;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Memory;
using Sotn;

namespace Recompiled.Netplay;

public static class RemotePuppet
{
    // Safe entity slot (top of player-owned entities 1..63, avoiding stage entities which start at 64)
    public const int PuppetEntitySlot = 63;

    public static bool IsActive { get; private set; } = false;
    public static byte StageId { get; private set; } = 0;
    public static byte RoomX { get; private set; } = 0;
    public static byte RoomY { get; private set; } = 0;
    public static short PosX { get; private set; } = 0;
    public static short PosY { get; private set; } = 0;
    public static short VelX { get; private set; } = 0;
    public static short VelY { get; private set; } = 0;
    public static bool FacingLeft { get; private set; } = false;
    public static byte AnimId { get; private set; } = 0;
    public static byte FrameId { get; private set; } = 0;
    public static ushort CurHp { get; private set; } = 100;
    public static ushort MaxHp { get; private set; } = 100;
    public static byte Character { get; private set; } = 0;
    public static byte PaletteId { get; private set; } = 0;
    public static byte AttackTrigger { get; private set; } = 0;
    public static byte SubWeapon { get; private set; } = 0;

    private static float _interpX = 0f;
    private static float _interpY = 0f;
    private static bool _wasRendered = false;

    public static void Reset()
    {
        IsActive = false;
        StageId = RoomX = RoomY = 0;
        PosX = PosY = VelX = VelY = 0;
        FacingLeft = false;
        AnimId = FrameId = 0;
        CurHp = MaxHp = 100;
        Character = PaletteId = AttackTrigger = SubWeapon = 0;

        DespawnPuppet();
    }

    public static void DespawnPuppet()
    {
        if (_wasRendered && Game.Available && Game.InGame)
        {
            try
            {
                var puppet = Entities.At(PuppetEntitySlot);
                puppet.Destroy();
            }
            catch { }
        }
        _wasRendered = false;
    }

    public static void ReceiveTransform(byte[] payload)
    {
        if (NetworkPacket.ReadPlayerTransform(
            payload,
            out byte stage, out byte rx, out byte ry,
            out short px, out short py, out short vx, out short vy,
            out bool facing, out byte anim, out byte frame,
            out ushort hp, out ushort maxHp,
            out byte ch, out byte pal, out byte atk, out byte subw))
        {
            IsActive = true;
            StageId = stage;
            RoomX = rx;
            RoomY = ry;
            PosX = px;
            PosY = py;
            VelX = vx;
            VelY = vy;
            FacingLeft = facing;
            AnimId = anim;
            FrameId = frame;
            CurHp = hp;
            MaxHp = maxHp;
            Character = ch;
            PaletteId = pal;
            AttackTrigger = atk;
            SubWeapon = subw;
        }
    }

    public static void Update(CpuContext c, IMemory m)
    {
        if (!Game.Available || !Game.InGame || Game.IsLoading) return;

        if (!IsActive)
        {
            if (_wasRendered) DespawnPuppet();
            return;
        }

        bool sameStage = StageId == (byte)Stages.Current;
        bool sameRoom = RoomX == (byte)Stages.RoomX && RoomY == (byte)Stages.RoomY;

        // Remote player is in the same room: render entity puppet
        if (sameStage && sameRoom)
        {
            // Smooth position interpolation (Lerp 0.35)
            _interpX = _interpX + (PosX - _interpX) * 0.35f;
            _interpY = _interpY + (PosY - _interpY) * 0.35f;

            var puppet = Entities.At(PuppetEntitySlot);
            puppet.PosX = (int)_interpX;
            puppet.PosY = (int)_interpY;
            puppet.FacingLeft = (ushort)(FacingLeft ? 1 : 0);
            puppet.Step = 1; // Active entity step
            _wasRendered = true;
        }
        else
        {
            if (_wasRendered) DespawnPuppet();
            _interpX = PosX;
            _interpY = PosY;
        }
    }
}
