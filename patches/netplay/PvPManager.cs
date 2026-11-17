using System;
using RecompOne.Runtime.Host.Window;
using Sotn;

namespace Recompiled.Netplay;

public static class PvPManager
{
    public static bool IsPvPActive { get; private set; } = false;
    public static int LocalScore { get; set; } = 0;
    public static int RemoteScore { get; set; } = 0;

    private static bool _wasInColosseum = false;
    private static float _attackCooldownTimer = 0f;
    private static float _roundResetTimer = 0f;

    public static void ResetScores()
    {
        LocalScore = 0;
        RemoteScore = 0;
        _roundResetTimer = 0f;
        _attackCooldownTimer = 0f;
    }

    public static void Update()
    {
        if (!NetworkManager.IsConnected || !RemotePuppet.IsActive)
        {
            IsPvPActive = false;
            _wasInColosseum = false;
            _roundResetTimer = 0f;
            return;
        }

        bool localInColosseum = Stages.Current == Stage.Colosseum || Stages.Current == Stage.ReverseColosseum;
        bool remoteInColosseum = (Stage)RemotePuppet.StageId == Stage.Colosseum || (Stage)RemotePuppet.StageId == Stage.ReverseColosseum;
        bool sameRoom = RemotePuppet.RoomX == Stages.RoomX && RemotePuppet.RoomY == Stages.RoomY;

        IsPvPActive = localInColosseum && remoteInColosseum && sameRoom;

        if (IsPvPActive && !_wasInColosseum)
        {
            ToastNotifications.ShowText("ARENA DO COLISEU!", "Duelo PvP Ativado! 3.. 2.. 1.. LUTEM!", null, 5.0f);
            _roundResetTimer = 0f;
        }

        _wasInColosseum = IsPvPActive;

        if (!IsPvPActive) return;

        // Process round reset timer countdown (after a knockout)
        if (_roundResetTimer > 0f)
        {
            _roundResetTimer -= 1f / 60f;
            if (_roundResetTimer <= 0f)
            {
                _roundResetTimer = 0f;
                if (Game.Available && Game.InGame)
                {
                    Player.FullHeal();
                    ToastNotifications.ShowText(
                        "NOVO ROUND!",
                        $"Lutem! Placar: {NetworkManager.LocalPlayerName} {LocalScore} x {RemoteScore} {NetworkManager.RemotePlayerName}",
                        null, 4.0f
                    );
                }
            }
            return; // In round transition, players cannot attack or take damage
        }

        // Process attack cooldown timer
        if (_attackCooldownTimer > 0f)
        {
            _attackCooldownTimer -= 1f / 60f;
        }

        // Check for local player attack hits in arena
        if (!Game.Available || !Game.InGame || Game.IsLoading) return;
        if (Player.Hp <= 0) return;

        // Attack input check: Square, Circle, Triangle, or Up+Attack (subweapon)
        bool attackPressed = Game.IsTapped(Button.Square) || Game.IsTapped(Button.Circle) || Game.IsTapped(Button.Triangle);
        bool subweaponPressed = Game.IsPressed(Button.Up) && (Game.IsTapped(Button.Square) || Game.IsTapped(Button.Circle));

        if ((attackPressed || subweaponPressed) && _attackCooldownTimer <= 0f)
        {
            CheckAndDispatchAttackHit();
        }
    }

    public static bool CheckHitRange(int px, int py, bool facingLeft, int rx, int ry)
    {
        int dx = rx - px;
        int dy = Math.Abs(ry - py);
        if (dy > 48) return false;
        return facingLeft ? (dx <= 15 && dx >= -80) : (dx >= -15 && dx <= 80);
    }

    public static ushort CalculateDamage(int strengthTotal)
    {
        if (strengthTotal > 0)
        {
            return (ushort)Math.Clamp(15 + (strengthTotal / 5), 15, 60);
        }
        return 25;
    }

    public static bool ProcessPvPHit(int currentHp, ushort damage, bool isInvincible, out int newHp, out bool isKnockout)
    {
        if (isInvincible)
        {
            newHp = currentHp;
            isKnockout = false;
            return false;
        }

        newHp = Math.Max(0, currentHp - damage);
        isKnockout = newHp <= 0;
        return true;
    }

    private static void CheckAndDispatchAttackHit()
    {
        short px = (short)Player.PosX;
        short py = (short)Player.PosY;
        short rx = RemotePuppet.PosX;
        short ry = RemotePuppet.PosY;
        bool facingLeft = Player.FacingLeft;

        if (!CheckHitRange(px, py, facingLeft, rx, ry)) return;

        // Set cooldown so a single swing only triggers 1 hit (approx 21 frames = 0.35s)
        _attackCooldownTimer = 0.35f;

        ushort damage = CalculateDamage(Player.StrengthTotal);
        byte hitType = (byte)(Game.IsTapped(Button.Triangle) ? 1 : 0);
        short knockbackX = (short)(rx >= px ? 4 : -4);

        byte[] payload = NetworkPacket.CreatePvPHit(damage, hitType, knockbackX, NetworkManager.LocalPlayerName);
        NetworkManager.SendPacket(new NetworkPacket(PacketOpCode.PvPHit, payload));
    }

    public static void ApplyPvPHit(ushort damage, byte hitType, short knockbackX, string attackerName)
    {
        if (!IsPvPActive || _roundResetTimer > 0f) return;
        if (!Game.Available || !Game.InGame || Game.IsLoading) return;

        if (!ProcessPvPHit(Player.Hp, damage, Player.IsInvincible, out int newHp, out bool isKnockout))
        {
            return;
        }

        Player.Hp = newHp;
        Player.KnockbackInvincibilityTimer = 45; // ~0.75s i-frames
        Player.DamagePalette = 0x8100; // Red damage flash
        Player.VelocityX = knockbackX * 3;

        try
        {
            GameApi.PlaySfx(0x6F8);
        }
        catch { }

        if (isKnockout)
        {
            RemoteScore++;
            _roundResetTimer = 3.5f;

            byte[] koPayload = NetworkPacket.CreatePvPKnockout(
                winnerName: attackerName,
                loserName: NetworkManager.LocalPlayerName,
                round: (ushort)(LocalScore + RemoteScore)
            );
            NetworkManager.SendPacket(new NetworkPacket(PacketOpCode.PvPKnockout, koPayload));

            ToastNotifications.ShowText(
                "🏆 K.O. NO COLISEU!",
                $"{attackerName} venceu o round! Placar: Você {LocalScore} x {RemoteScore} {NetworkManager.RemotePlayerName}",
                null, 5.0f
            );
        }
    }

    public static void OnRemoteKnockout(string winnerName, string loserName, ushort round)
    {
        if (!IsPvPActive) return;

        LocalScore++;
        _roundResetTimer = 3.5f;

        ToastNotifications.ShowText(
            "🏆 VITÓRIA NO COLISEU!",
            $"Você derrotou {NetworkManager.RemotePlayerName}! Placar: Você {LocalScore} x {RemoteScore} {NetworkManager.RemotePlayerName}",
            null, 5.0f
        );
    }
}
