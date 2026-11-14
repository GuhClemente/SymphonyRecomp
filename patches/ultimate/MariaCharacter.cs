using System;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Host;
using RecompOne.Runtime.Host.Window;
using RecompOne.Runtime.Memory;
using Sotn;

namespace Recompiled.Ultimate;

public static class MariaCharacter
{
    private const uint CostumeMagicAddr = Game.StatusAddr + 0x240u; // D_80097C40
    public const uint RicSaturnCoatMagic = 0x32434952u; // "RIC2"

    private static int _aerialJumpsRemaining = 2; // Allows triple jump
    private static bool _autoTransformed = false;
    private static int _autoPadFrame = 0;

    static MariaCharacter()
    {
        if (Environment.GetEnvironmentVariable("SOTN_TEST_MARIA") == "1")
        {
            RecompOne.Runtime.Events.Event.AddListener<RecompOne.Runtime.Events.PadReadEvent>(e =>
            {
                if (e.Port == 0 && !Game.InGame)
                {
                    _autoPadFrame++;
                    // In Title/Select screen, pulse Start / Confirm every 20 frames
                    if ((_autoPadFrame % 20) < 6)
                    {
                        // Active-low in swapped format:
                        // Start = 0x0800, Cross = 0x0040, Circle = 0x0020
                        e.Buttons = (ushort)(e.Buttons & ~(0x0800 | 0x0040 | 0x0020));
                    }
                }
            });
        }
    }

    public static void ResetState()
    {
        _autoTransformed = false;
    }

    public static bool IsSaturnMaria =>
        UltimateOptions.UltimateEnabled &&
        (UltimateOptions.CharacterMode == CharacterSelection.SaturnMaria || Player.IsMaria);

    public static bool IsPSPMaria =>
        UltimateOptions.UltimateEnabled &&
        UltimateOptions.CharacterMode == CharacterSelection.PSPMaria;

    public static bool IsSaturnRichter =>
        UltimateOptions.UltimateEnabled &&
        UltimateOptions.CharacterMode == CharacterSelection.RichterSaturnCoat;

    public static void Update(CpuContext c, IMemory m)
    {
        if (!UltimateOptions.UltimateEnabled) return;
        if (!Game.Available || !Game.InGame || Game.IsLoading) return;

        // Auto-transform to Maria if SaturnMaria is chosen or test flag is active
        if ((UltimateOptions.CharacterMode == CharacterSelection.SaturnMaria ||
             Environment.GetEnvironmentVariable("SOTN_TEST_MARIA") == "1") &&
            !_autoTransformed && !MariaLoader.IsLoaded && Game.InGame && !Game.InPrologue())
        {
            _autoTransformed = true;
            MariaLoader.TransformToMaria(m);
            string? capturePath = Environment.GetEnvironmentVariable("SOTN_SCREENSHOT_PATH");
            if (!string.IsNullOrEmpty(capturePath))
            {
                HostWindow.RequestScreenshot(capturePath, 60);
            }
        }

        // 1. Richter Saturn Coat Look
        if (Player.IsRichter && IsSaturnRichter)
        {
            try
            {
                // Set Saturn coat magic in D_80097C40
                m.WriteU32(CostumeMagicAddr, RicSaturnCoatMagic);
            }
            catch { }
        }

        // 2. Maria Triple Jump & Aerial Mechanics
        if (IsSaturnMaria || IsPSPMaria)
        {
            UpdateMariaJumps();
        }
    }

    private static void UpdateMariaJumps()
    {
        // If grounded, reset aerial jumps
        if (Player.Step == PlayerStep.Standing || Player.Step == PlayerStep.Walk)
        {
            _aerialJumpsRemaining = 2;
        }

        ushort tapped = Game.Tapped;
        bool jumpTapped = (tapped & (ushort)Button.Cross) != 0;

        // If in air and jump is tapped
        if (jumpTapped && _aerialJumpsRemaining > 0 &&
            (Player.Step == PlayerStep.AirJump || Player.Step == PlayerStep.Falling || Player.Step == PlayerStep.Jump))
        {
            _aerialJumpsRemaining--;
            // Perform aerial jump impulse
            Player.VelocityY = -0x00060000; // Jump upward impulse
            Player.Step = PlayerStep.AirJump;

            // Optional flip effect
            try
            {
                RecompOne.Runtime.Runtime.Mem!.WriteU16(Player.EffectActivatorAddr, 1);
            }
            catch { }
        }
    }
}
