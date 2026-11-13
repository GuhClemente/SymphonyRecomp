using System;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Host.Window;
using RecompOne.Runtime.Memory;
using Sotn;

namespace Recompiled.Ultimate;

public static class GodSpeedShoes
{
    public const int RelicId = 30;
    public const float SprintMultiplier = 2.0f; // 4.5 px/frame (Saturn GODSPEED_SPEED)

    private static int _doubleTapTimer = 0;
    private static int _lastTapDir = 0; // -1: Left, 1: Right
    private static bool _isSprinting = false;
    private static int _sprintEffectTimer = 0;

    public static bool HasShoes => Game.Available && Inventory.HasRelic(RelicId);
    public static bool IsActive => Game.Available && Inventory.IsRelicActive(RelicId);

    public static void Grant(bool active = true)
    {
        Inventory.SetRelic(RelicId, (byte)(active ? 3 : 1));
        ToastNotifications.ShowText(
            "👟 Botas da Velocidade (God Speed Shoes)!",
            "Relíquia 30 do Sega Saturn obtida! Toque duas vezes para frente para correr em alta velocidade.",
            null,
            4.0f);
    }

    public static void Revoke()
    {
        Inventory.SetRelic(RelicId, 0);
    }

    public static void ToggleActive()
    {
        byte current = Inventory.GetRelic(RelicId);
        if (current == 0) Grant(true);
        else if (current == 3) Inventory.SetRelic(RelicId, 1);
        else Inventory.SetRelic(RelicId, 3);
    }

    public static void Update(CpuContext c, IMemory m)
    {
        if (!UltimateOptions.UltimateEnabled || !UltimateOptions.GodSpeedShoes)
        {
            _isSprinting = false;
            return;
        }

        if (!Game.Available || !Game.InGame || Game.IsLoading)
        {
            _isSprinting = false;
            _doubleTapTimer = 0;
            return;
        }

        if (!Player.IsAlucard || !Player.HasControl)
        {
            _isSprinting = false;
            return;
        }

        // Must have relic obtained and active (or option allows it)
        if (!IsActive)
        {
            _isSprinting = false;
            return;
        }

        ushort pressed = Game.Pressed;
        ushort tapped = Game.Tapped;

        bool tapRight = (tapped & (ushort)Button.Right) != 0;
        bool tapLeft = (tapped & (ushort)Button.Left) != 0;
        bool holdRight = (pressed & (ushort)Button.Right) != 0;
        bool holdLeft = (pressed & (ushort)Button.Left) != 0;
        bool holdL1 = (pressed & (ushort)Button.L1) != 0;
        bool holdR1 = (pressed & (ushort)Button.R1) != 0;

        if (_doubleTapTimer > 0) _doubleTapTimer--;

        // Double-tap forward detection
        if (tapRight)
        {
            if (_lastTapDir == 1 && _doubleTapTimer > 0)
                _isSprinting = true;
            else
            {
                _lastTapDir = 1;
                _doubleTapTimer = 18;
            }
        }
        else if (tapLeft)
        {
            if (_lastTapDir == -1 && _doubleTapTimer > 0)
                _isSprinting = true;
            else
            {
                _lastTapDir = -1;
                _doubleTapTimer = 18;
            }
        }

        // Optional shoulder button hold trigger
        if (holdL1 || holdR1)
        {
            if (holdRight || holdLeft) _isSprinting = true;
        }

        // Stop sprinting when direction is released or state changes
        if (!holdRight && !holdLeft)
        {
            _isSprinting = false;
        }

        if (Player.Step != PlayerStep.Walk && Player.Step != PlayerStep.Standing && Player.Step != PlayerStep.FlameWhip)
        {
            _isSprinting = false;
        }

        // Apply Godspeed sprint mechanics
        if (_isSprinting)
        {
            int velX = Player.VelocityX;
            if (holdRight && velX > 0)
            {
                Player.VelocityX = (int)(velX * SprintMultiplier);
            }
            else if (holdLeft && velX < 0)
            {
                Player.VelocityX = (int)(velX * SprintMultiplier);
            }

            // Visual afterimage effect
            _sprintEffectTimer++;
            if (_sprintEffectTimer % 4 == 0)
            {
                try
                {
                    // Create afterimage factory (BP_1, 5) at Alucard's coordinates
                    m.WriteU16(Player.EffectActivatorAddr, 1);
                }
                catch { }
            }
        }
    }
}
