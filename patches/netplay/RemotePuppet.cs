using System;
using System.Collections.Generic;
using System.Numerics;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Memory;
using Sotn;

namespace Recompiled.Netplay;

public enum PuppetAction
{
    Idle,
    Walk,
    Run,
    Backdash,
    JumpAscend,
    JumpApex,
    Fall,
    Crouch,
    AttackMain,
    AttackSubweapon,
    Hurt,
    Downed
}

public struct GhostSample
{
    public Vector2 Pos;
    public bool FacingLeft;
    public float Alpha;
    public uint Tint;
    public PuppetAction Action;
    public float WalkPhase;
}

public struct PuppetParticle
{
    public Vector2 Pos;
    public Vector2 Vel;
    public float Life;
    public float MaxLife;
    public uint Color;
    public float Size;
}

public struct PuppetProjectile
{
    public Vector2 Pos;
    public Vector2 Vel;
    public float Gravity;
    public byte Kind; // 1=Dagger, 2=Axe, 3=Cross, 4=HolyWater, 5=MariaDove
    public float Life;
    public float Rotation;
    public bool FacingLeft;
}

public static class RemotePuppet
{
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

    public static float InterpX => _interpX;
    public static float InterpY => _interpY;
    public static float WalkPhase => _walkPhase;
    public static float AttackTimer => _attackTimer;
    public static float SubweaponTimer => _subweaponTimer;
    public static float HurtTimer => _hurtTimer;

    public static readonly List<GhostSample> GhostTrails = [];
    public static readonly List<PuppetParticle> Particles = [];
    public static readonly List<PuppetProjectile> Projectiles = [];

    public static string CharacterName => Character switch
    {
        1 => "Richter (Clássico)",
        2 => "Richter (Saturn Coat)",
        5 => "Richter (Couro)",
        3 => "Maria (Saturn)",
        4 => "Maria (PSP)",
        _ => "Alucard"
    };

    public static PuppetAction CurrentAction
    {
        get
        {
            if (CoopRevivalManager.IsRemoteDowned) return PuppetAction.Downed;
            if (_hurtTimer > 0f) return PuppetAction.Hurt;
            if (_attackTimer > 0f) return PuppetAction.AttackMain;
            if (_subweaponTimer > 0f) return PuppetAction.AttackSubweapon;
            if (AttackTrigger == 3 || AnimId == 0x13) return PuppetAction.Backdash;
            if (AnimId == 0x02) return PuppetAction.Crouch;
            if (AnimId == 0x04 || Math.Abs(VelY) > 8)
            {
                if (VelY < -2) return PuppetAction.JumpAscend;
                if (VelY > 2) return PuppetAction.Fall;
                return PuppetAction.JumpApex;
            }
            if (Math.Abs(VelX) > 14) return PuppetAction.Run;
            if (Math.Abs(VelX) > 1) return PuppetAction.Walk;
            return PuppetAction.Idle;
        }
    }

    private static float _interpX = 0f;
    private static float _interpY = 0f;
    private static float _walkPhase = 0f;
    private static float _attackTimer = 0f;
    private static float _subweaponTimer = 0f;
    private static float _hurtTimer = 0f;
    private static float _ghostSpawnTimer = 0f;
    private static float _dustSpawnTimer = 0f;
    private static ushort _prevHp = 100;
    private static byte _prevAttackTrigger = 0;
    private static bool _wasRendered = false;

    public static void Reset()
    {
        IsActive = false;
        StageId = RoomX = RoomY = 0;
        PosX = PosY = VelX = VelY = 0;
        FacingLeft = false;
        AnimId = FrameId = 0;
        CurHp = MaxHp = 100;
        _prevHp = 100;
        Character = PaletteId = AttackTrigger = SubWeapon = 0;
        _prevAttackTrigger = 0;
        _attackTimer = _subweaponTimer = _hurtTimer = 0f;
        _walkPhase = 0f;

        GhostTrails.Clear();
        Particles.Clear();
        Projectiles.Clear();

        DespawnPuppet();
    }

    public static void DespawnPuppet()
    {
        _wasRendered = false;
        GhostTrails.Clear();
        Particles.Clear();
        Projectiles.Clear();
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

            // Damage reaction detection
            if (hp < CurHp && CurHp > 0)
            {
                _hurtTimer = 0.32f;
                SpawnHitSparks(new Vector2(px, py));
            }
            _prevHp = hp;
            CurHp = hp;
            MaxHp = maxHp;
            Character = ch;
            PaletteId = pal;

            // Attack trigger transition detection
            if (atk != 0 && atk != _prevAttackTrigger)
            {
                OnAttackTriggered(atk, subw, px, py, facing, ch);
            }
            _prevAttackTrigger = atk;
            AttackTrigger = atk;
            SubWeapon = subw;
        }
    }

    private static void OnAttackTriggered(byte atk, byte subw, short px, short py, bool facing, byte ch)
    {
        float dir = facing ? -1f : 1f;
        Vector2 origin = new(px, py - 18);

        if (atk == 1) // Main Attack (Sword / Whip / Dove)
        {
            _attackTimer = 0.28f;
            try
            {
                if (ch == 1 || ch == 2 || ch == 5) GameApi.PlaySfx(0x6FC); // Richter Whip Crack
                else if (ch == 3 || ch == 4) GameApi.PlaySfx(0x6FD);       // Maria Dove
                else GameApi.PlaySfx(0x6F8);                               // Alucard Sword Swing
            }
            catch { }
        }
        else if (atk == 2) // Subweapon
        {
            _subweaponTimer = 0.35f;
            try { GameApi.PlaySfx(0x678); } catch { }

            // Spawn subweapon projectile
            byte kind = subw switch
            {
                1 => 1, // Dagger
                2 => 2, // Axe
                3 => 3, // Cross
                4 => 4, // Holy Water
                _ => (byte)((ch == 3 || ch == 4) ? 5 : 1)
            };

            float vx = dir * (kind == 1 ? 400f : (kind == 2 ? 140f : 240f));
            float vy = kind == 2 ? -280f : (kind == 4 ? -160f : 0f);
            float grav = kind == 2 ? 650f : (kind == 4 ? 500f : 0f);

            Projectiles.Add(new PuppetProjectile
            {
                Pos = origin + new Vector2(dir * 16f, 0),
                Vel = new Vector2(vx, vy),
                Gravity = grav,
                Kind = kind,
                Life = kind == 4 ? 2.0f : 1.6f,
                Rotation = 0f,
                FacingLeft = facing
            });
        }
    }

    private static void SpawnHitSparks(Vector2 pos)
    {
        for (int i = 0; i < 8; i++)
        {
            float angle = (float)(i * Math.PI / 4.0);
            float spd = 70f + (i % 3) * 35f;
            Particles.Add(new PuppetParticle
            {
                Pos = pos + new Vector2(0, -16),
                Vel = new Vector2(MathF.Cos(angle) * spd, MathF.Sin(angle) * spd),
                Life = 0.25f,
                MaxLife = 0.25f,
                Color = 0xFF4080FF,
                Size = 3f
            });
        }
    }

    public static void Update(CpuContext c, IMemory m)
    {
        if (!Game.Available || !Game.InGame || Game.IsLoading) return;

        float dt = 1f / 60f;

        // Timers decay
        if (_attackTimer > 0f) _attackTimer = Math.Max(0f, _attackTimer - dt);
        if (_subweaponTimer > 0f) _subweaponTimer = Math.Max(0f, _subweaponTimer - dt);
        if (_hurtTimer > 0f) _hurtTimer = Math.Max(0f, _hurtTimer - dt);

        if (!IsActive)
        {
            if (_wasRendered) DespawnPuppet();
            return;
        }

        bool sameStage = StageId == (byte)Stages.Current;
        bool sameRoom = RoomX == (byte)Stages.RoomX && RoomY == (byte)Stages.RoomY;

        if (sameStage && sameRoom)
        {
            if (Math.Abs(_interpX - PosX) > 140 || Math.Abs(_interpY - PosY) > 140 || !_wasRendered)
            {
                _interpX = PosX;
                _interpY = PosY;
            }
            else
            {
                // Smooth position interpolation (Lerp 0.35)
                _interpX += (PosX - _interpX) * 0.35f;
                _interpY += (PosY - _interpY) * 0.35f;
            }

            // Walk cycle phase accumulation
            float speedX = Math.Abs(VelX);
            if (speedX > 1f)
            {
                _walkPhase += speedX * 0.08f * dt * 60f;
                if (_walkPhase > MathF.PI * 2f) _walkPhase -= MathF.PI * 2f;
            }

            // Ghost Trails (Afterimages) for Sprint / Dash / High Speed
            bool isHighSpeed = speedX > 14f || AttackTrigger == 3 || AnimId == 0x13;
            if (isHighSpeed)
            {
                _ghostSpawnTimer += dt;
                if (_ghostSpawnTimer >= 0.045f) // Every ~3 frames
                {
                    _ghostSpawnTimer = 0f;
                    uint tint = Character switch
                    {
                        1 or 2 or 5 => 0x9040C0FF, // Richter holy blue trail
                        3 or 4 => 0x9040FFA0,      // Maria emerald aura trail
                        _ => 0x90C040FF            // Alucard spectral purple/crimson trail
                    };

                    if (GhostTrails.Count >= 6) GhostTrails.RemoveAt(0);
                    GhostTrails.Add(new GhostSample
                    {
                        Pos = new Vector2(_interpX, _interpY),
                        FacingLeft = FacingLeft,
                        Alpha = 0.65f,
                        Tint = tint,
                        Action = CurrentAction,
                        WalkPhase = _walkPhase
                    });
                }
            }

            // Dust Particles on Ground Run
            if (speedX > 10f && Math.Abs(VelY) < 4)
            {
                _dustSpawnTimer += dt;
                if (_dustSpawnTimer >= 0.07f)
                {
                    _dustSpawnTimer = 0f;
                    float dir = FacingLeft ? 1f : -1f; // Behind feet
                    Particles.Add(new PuppetParticle
                    {
                        Pos = new Vector2(_interpX + dir * 8f, _interpY),
                        Vel = new Vector2(dir * 15f, -12f),
                        Life = 0.22f,
                        MaxLife = 0.22f,
                        Color = 0x808090A0,
                        Size = 2.5f
                    });
                }
            }

            _wasRendered = true;
        }
        else
        {
            if (_wasRendered) DespawnPuppet();
            _interpX = PosX;
            _interpY = PosY;
        }

        // Update Ghost Trails
        for (int i = GhostTrails.Count - 1; i >= 0; i--)
        {
            var g = GhostTrails[i];
            g.Alpha -= dt * 3.5f;
            if (g.Alpha <= 0f) GhostTrails.RemoveAt(i);
            else GhostTrails[i] = g;
        }

        // Update Particles
        for (int i = Particles.Count - 1; i >= 0; i--)
        {
            var p = Particles[i];
            p.Life -= dt;
            p.Pos += p.Vel * dt;
            if (p.Life <= 0f) Particles.RemoveAt(i);
            else Particles[i] = p;
        }

        // Update Subweapon Projectiles
        for (int i = Projectiles.Count - 1; i >= 0; i--)
        {
            var pr = Projectiles[i];
            pr.Life -= dt;
            pr.Vel = new Vector2(pr.Vel.X, pr.Vel.Y + pr.Gravity * dt);
            pr.Pos += pr.Vel * dt;
            pr.Rotation += dt * 14f;

            if (pr.Life <= 0f) Projectiles.RemoveAt(i);
            else Projectiles[i] = pr;
        }
    }
}
