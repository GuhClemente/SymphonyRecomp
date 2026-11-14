using System;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Host;
using RecompOne.Runtime.Host.Window;
using RecompOne.Runtime.Memory;
using Sotn;

namespace Recompiled.Ultimate;

public static class MariaCharacter
{
    // g_Status.D_80097C40 in the retail PlayerStatus layout (0x80097964 + 0x2DC). SOTN Ultimate keeps
    // Richter's costume magic there; the retail DRA this recomp runs has no RIC2 loader yet, so the
    // value is only stored for when that lands.
    private const uint CostumeMagicAddr = Game.StatusAddr + 0x2DCu;
    public const uint RicSaturnCoatMagic = 0x32434952u; // "RIC2"

    private static readonly bool TestMode = Environment.GetEnvironmentVariable("SOTN_TEST_MARIA") == "1";

    private static int _aerialJumpsRemaining = 2; // Allows triple jump
    private static bool _autoTransformFailed;
    private static bool _wasMaria;
    private static int _autoPadFrame = 0;

    static MariaCharacter()
    {
        if (TestMode)
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

    /// <summary>Lets the automatic transformation try again (e.g. after the option changes).</summary>
    public static void ResetState()
    {
        _autoTransformFailed = false;
    }

    private static bool WantsSaturnMaria =>
        UltimateOptions.CharacterMode == CharacterSelection.SaturnMaria || TestMode;

    public static bool IsSaturnMaria => UltimateOptions.UltimateEnabled && Player.IsMaria;

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

        bool isMaria = Player.IsMaria;

        // The game resets the playable character whenever a session starts (title, game over, file
        // load), so "is Maria loaded" is read from the game instead of being latched here: a new
        // session transforms again, and the retail sound priorities come back once she is gone.
        if (_wasMaria && !isMaria) MariaLoader.OnMariaLeft(m);
        _wasMaria = isMaria;

        if (WantsSaturnMaria && !isMaria && !_autoTransformFailed && !Game.InPrologue())
        {
            if (MariaLoader.TransformToMaria(c, m))
            {
                _wasMaria = true;
                isMaria = true;
                string? capturePath = Environment.GetEnvironmentVariable("SOTN_SCREENSHOT_PATH");
                if (TestMode && !string.IsNullOrEmpty(capturePath))
                    HostWindow.RequestScreenshot(capturePath, 60);
            }
            else
            {
                // Do not retry every frame; picking the character again or the panel button retries
                _autoTransformFailed = true;
            }
        }

        // 1. Richter Saturn Coat Look
        if (Player.IsRichter && IsSaturnRichter)
            m.WriteU32(CostumeMagicAddr, RicSaturnCoatMagic);

        // 2. Maria Triple Jump & Aerial Mechanics (only while actually playing as her)
        if (isMaria) UpdateMariaJumps(m);
    }

    private static void UpdateMariaJumps(IMemory m)
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
            m.WriteU16(Player.EffectActivatorAddr, 1);
        }
    }
}
