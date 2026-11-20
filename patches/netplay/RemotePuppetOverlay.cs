using System;
using System.Numerics;
using ImGuiNET;
using RecompOne.Runtime.Host.Window;
using Sotn;

namespace Recompiled.Netplay;

public static class RemotePuppetOverlay
{
    private static bool _initialized = false;

    public static void Initialize()
    {
        if (_initialized) return;
        OutputPanel.OnRenderGameOverlay += Render;
        // where the game's drawing origin sits inside the area the GPU displays (in game: drawn from
        // y 0, displayed from y 20, so PS1 y 0 is 20 lines above the top of the image)
        RecompOne.Runtime.Events.Event.AddListener<RecompOne.Runtime.Events.DrawEnvEvent>(e =>
        {
            _originDx = e.OfsX - e.ClipX;
            _originDy = e.OfsY - e.ClipY;
        });
        _initialized = true;
    }

    private static volatile int _originDx, _originDy;

    /// <summary>Test aid: draw the puppet without a netplay session (the test feeds it itself).</summary>
    public static bool TestForce;

    /// <summary>How PS1 screen coordinates land on the game image: the image shows the GPU's display
    /// area (256x207 in game, not 256x224), which starts where the draw clip starts, while the game
    /// draws from its draw offset (20 lines higher), plus, in widescreen, Margin extra pixels on each
    /// side. Returns the scale and where PS1 (0,0) is on screen.</summary>
    public static (float ScaleX, float ScaleY, Vector2 Origin) GameScreen(Vector2 imageMin, Vector2 imageSize)
    {
        var gpu = RecompOne.Runtime.Runtime.Gpu;
        int dispW = gpu != null && gpu.DisplayWidth > 0 ? gpu.DisplayWidth : 256;
        int dispH = gpu != null && gpu.DisplayHeight > 0 ? gpu.DisplayHeight : 240;
        int margin = WidescreenPatch.Margin;
        float scaleX = imageSize.X / (dispW + 2 * margin);
        float scaleY = imageSize.Y / dispH;
        return (scaleX, scaleY, imageMin + new Vector2((margin + _originDx) * scaleX, _originDy * scaleY));
    }

    public static void Render(Vector2 imageMin, Vector2 imageSize)
    {
        if (!(NetworkManager.IsConnected || TestForce) || !RemotePuppet.IsActive) return;
        if (!Game.Available || !Game.InGame || Game.IsLoading) return;

        var m = RecompOne.Runtime.Runtime.Mem;
        if (m == null) return;

        var (scaleX, scaleY, gameOrigin) = GameScreen(imageMin, imageSize);
        int margin = WidescreenPatch.Margin;
        float viewH = imageSize.Y / scaleY;
        var dl = ImGui.GetWindowDrawList();
        float time = (float)ImGui.GetTime();

        // 1. In-Game Pause Map: render partner position on the PS1 map
        if (Game.MapOpen)
        {
            DrawPartnerOnPauseMap(dl, gameOrigin, imageSize, scaleX, scaleY);
            return;
        }

        bool sameStage = RemotePuppet.StageId == (byte)Stages.Current;
        bool sameRoom = sameStage && RemotePuppet.RoomX == (byte)Stages.RoomX && RemotePuppet.RoomY == (byte)Stages.RoomY;

        // 2. Different room: render directional edge indicator to partner's room
        if (!sameRoom)
        {
            if (sameStage)
            {
                int dx = RemotePuppet.RoomX - Stages.RoomX;
                int dy = RemotePuppet.RoomY - Stages.RoomY;
                DrawAdjacentRoomIndicator(dl, imageMin, imageSize, scaleX, scaleY, dx, dy, time);
            }
            return;
        }

        // Camera scroll offsets in pixels
        short localScrollX = (short)m.ReadU16(0x8007308E);
        short localScrollY = (short)m.ReadU16(0x80073092);

        float posX = RemotePuppet.InterpX;
        float posY = RemotePuppet.InterpY;

        // The partner's position is in room world space (NetworkManager sends 0x800973F0/F4), so it is
        // always relative to our camera; outside the view it becomes the off-screen indicator below.
        float gameX = HudLogic.WorldToScreen(posX, localScrollX);
        float gameY = HudLogic.WorldToScreen(posY, localScrollY);

        Vector2 screenPos = gameOrigin + new Vector2(gameX * scaleX, gameY * scaleY);
        bool isOffScreen = gameX < -margin - 20 || gameX > 256 + margin + 20 || gameY < -20 || gameY > viewH + 20;

        if (isOffScreen)
        {
            DrawOffScreenIndicator(dl, imageMin, imageSize, screenPos, scaleX, scaleY, time);
        }
        else
        {
            DrawInGamePuppet(dl, screenPos, scaleX, scaleY, time, localScrollX, localScrollY, gameOrigin);
        }

        // Render any active tactical ping in this room
        DrawTacticalPing(dl, gameOrigin, imageSize, scaleX, scaleY, localScrollX, localScrollY, time);
    }

    private static void DrawOffScreenIndicator(
        ImDrawListPtr dl,
        Vector2 imageMin,
        Vector2 imageSize,
        Vector2 screenPos,
        float scaleX,
        float scaleY,
        float time)
    {
        float borderPadding = 36f;
        Vector2 center = imageMin + imageSize * 0.5f;
        Vector2 diff = screenPos - center;
        if (diff.LengthSquared() < 0.001f) return;

        Vector2 dir = Vector2.Normalize(diff);
        float edgeX = Math.Clamp(screenPos.X, imageMin.X + borderPadding, imageMin.X + imageSize.X - borderPadding);
        float edgeY = Math.Clamp(screenPos.Y, imageMin.Y + borderPadding, imageMin.Y + imageSize.Y - borderPadding);
        Vector2 edgePos = new(edgeX, edgeY);

        float pulse = (MathF.Sin(time * 6f) + 1f) * 0.5f;
        uint arrowCol = CoopRevivalManager.IsRemoteDowned ? 0xFF2020FF : 0xFF40D0F5;

        Vector2 p1 = edgePos + dir * (14f + pulse * 3f);
        Vector2 p2 = edgePos + new Vector2(-dir.Y, dir.X) * 8f - dir * 6f;
        Vector2 p3 = edgePos + new Vector2(dir.Y, -dir.X) * 8f - dir * 6f;

        dl.AddTriangleFilled(p1, p2, p3, arrowCol);
        dl.AddTriangle(p1, p2, p3, 0xFFFFFFFF, 1.5f);

        float distPx = diff.Length() / Math.Max(scaleX, scaleY);
        string name = string.IsNullOrWhiteSpace(NetworkManager.RemotePlayerName) ? "Parceiro" : NetworkManager.RemotePlayerName;
        string badgeText = $"{name} ({(int)distPx}px)";
        Vector2 txtSz = ImGui.CalcTextSize(badgeText);

        Vector2 bMin = edgePos + new Vector2(-txtSz.X * 0.5f - 4, 12);
        Vector2 bMax = bMin + txtSz + new Vector2(8, 4);
        dl.AddRectFilled(bMin, bMax, 0xDD12141C, 3f);
        dl.AddRect(bMin, bMax, arrowCol, 3f, ImDrawFlags.None, 1f);
        dl.AddText(bMin + new Vector2(4, 2), 0xFFFFFFFF, badgeText);
    }

    private static void DrawInGamePuppet(
        ImDrawListPtr dl,
        Vector2 screenPos,
        float scaleX,
        float scaleY,
        float time,
        short localScrollX,
        short localScrollY,
        Vector2 imageMin)
    {
        // 1. Co-op Downed State
        if (CoopRevivalManager.IsRemoteDowned)
        {
            DrawDownedPuppet(dl, screenPos, scaleX, scaleY, time);
            return;
        }

        // The partner's own sprite, pixel for pixel as their game drew it (SpriteSync). Only what the
        // game draws: no invented shadow, dust or projectiles; the trail reuses their earlier frames.
        if (SpriteSync.TryGetTexture(SpriteSync.RemoteHash, out uint tex, out SpriteFrame frame))
        {
            foreach (var ghost in RemotePuppet.GhostTrails)
            {
                if (!SpriteSync.TryGetTexture(ghost.SpriteHash, out uint gTex, out SpriteFrame gFrame)) continue;
                Vector2 gScr = imageMin + new Vector2((ghost.Pos.X - localScrollX) * scaleX, (ghost.Pos.Y - localScrollY) * scaleY);
                uint gCol = (ghost.Tint & 0x00FFFFFF) | ((uint)(Math.Clamp(ghost.Alpha, 0f, 1f) * 150f) << 24);
                DrawSpriteFrame(dl, gScr, scaleX, scaleY, gTex, gFrame, gCol);
            }

            bool flash = RemotePuppet.HurtTimer > 0f && ((int)(time * 28f) % 2 == 0);
            DrawSpriteFrame(dl, screenPos, scaleX, scaleY, tex, frame, flash ? 0xFFB0B0FFu : 0xFFFFFFFFu);
            DrawOverheadHUD(dl, screenPos, scaleX, scaleY);
            return;
        }

        // 2. Render Particles (Running dust & hit sparks)
        foreach (var p in RemotePuppet.Particles)
        {
            Vector2 pScr = imageMin + new Vector2((p.Pos.X - localScrollX) * scaleX, (p.Pos.Y - localScrollY) * scaleY);
            float alpha = Math.Clamp(p.Life / p.MaxLife, 0f, 1f);
            uint col = (p.Color & 0x00FFFFFF) | ((uint)(alpha * 220) << 24);
            dl.AddCircleFilled(pScr, p.Size * scaleX, col);
        }

        // 3. Render Active Subweapon Projectiles
        foreach (var pr in RemotePuppet.Projectiles)
        {
            Vector2 prScr = imageMin + new Vector2((pr.Pos.X - localScrollX) * scaleX, (pr.Pos.Y - localScrollY) * scaleY);
            DrawProjectile(dl, prScr, scaleX, scaleY, pr);
        }

        // 4. Dynamic Ground Shadow (scales with jump height)
        float jumpDist = Math.Max(0f, Math.Abs(RemotePuppet.VelY) * 0.6f);
        float shadowAlpha = Math.Clamp(0.55f - jumpDist * 0.02f, 0.12f, 0.55f);
        float shadowW = Math.Clamp((14f - jumpDist * 0.25f) * scaleX, 5f * scaleX, 16f * scaleX);
        uint shadowCol = ((uint)(shadowAlpha * 255) << 24);
        dl.AddEllipseFilled(new Vector2(screenPos.X, screenPos.Y), new Vector2(shadowW, 3f * scaleY), shadowCol);

        // 5. Ghost Trails (Afterimages) for Sprint / Dash
        foreach (var ghost in RemotePuppet.GhostTrails)
        {
            Vector2 gScr = imageMin + new Vector2((ghost.Pos.X - localScrollX) * scaleX, (ghost.Pos.Y - localScrollY) * scaleY);
            DrawGhostSilhouette(dl, gScr, scaleX, scaleY, ghost.FacingLeft ? -1f : 1f, ghost.Alpha, ghost.Tint, RemotePuppet.Character, ghost.Action, ghost.WalkPhase);
        }

        // 6. Character Motion Parameters
        float dir = RemotePuppet.FacingLeft ? -1f : 1f;
        PuppetAction action = RemotePuppet.CurrentAction;
        float walkPhase = RemotePuppet.WalkPhase;
        bool hurtFlash = RemotePuppet.HurtTimer > 0f && ((int)(time * 28f) % 2 == 0);

        // Center of mass (cPos)
        float crouchOffset = (action == PuppetAction.Crouch) ? 7f * scaleY : 0f;
        float breath = (action == PuppetAction.Idle) ? MathF.Sin(time * 3.5f) * 1.2f * scaleY : 0f;
        Vector2 cPos = new(screenPos.X, screenPos.Y - 18f * scaleY + crouchOffset + breath);

        // 7. Render State-of-the-Art Character Poses
        switch (RemotePuppet.Character)
        {
            case 1: // Richter Classic
            case 2: // Richter Saturn Coat
            case 5: // Richter Brown Coat
                DrawRichterState(dl, cPos, scaleX, scaleY, dir, time, walkPhase, action, hurtFlash, RemotePuppet.Character);
                break;

            case 3: // Maria Saturn
            case 4: // Maria PSP
                DrawMariaState(dl, cPos, scaleX, scaleY, dir, time, walkPhase, action, hurtFlash, RemotePuppet.Character);
                break;

            case 0: // Alucard
            default:
                DrawAlucardState(dl, cPos, scaleX, scaleY, dir, time, walkPhase, action, hurtFlash);
                break;
        }

        // 8. Overhead Tactical Nameplate & Smooth Animated HP Bar
        DrawOverheadHUD(dl, screenPos, scaleX, scaleY);
    }

    /// <summary>A captured frame placed like the game placed it: its box starts (OffsetX, OffsetY) PS1
    /// pixels from the player's position.</summary>
    private static void DrawSpriteFrame(ImDrawListPtr dl, Vector2 screenPos, float scaleX, float scaleY,
        uint tex, SpriteFrame frame, uint tint)
    {
        Vector2 min = screenPos + new Vector2(frame.OffsetX * scaleX, frame.OffsetY * scaleY);
        Vector2 max = min + new Vector2(frame.Width * scaleX, frame.Height * scaleY);
        dl.AddImage((nint)tex, min, max, Vector2.Zero, Vector2.One, tint);
    }

    private static void DrawDownedPuppet(
        ImDrawListPtr dl,
        Vector2 screenPos,
        float scaleX,
        float scaleY,
        float time)
    {
        float auraPulse = (MathF.Sin(time * 7f) + 1f) * 0.5f;
        dl.AddCircleFilled(screenPos, (18f + auraPulse * 8f) * scaleX, 0x40201080);
        dl.AddCircle(screenPos, (18f + auraPulse * 8f) * scaleX, 0xAA2020FF, 24, 2f);

        Vector2 bodyMin = new(screenPos.X - 16f * scaleX, screenPos.Y - 8f * scaleY);
        Vector2 bodyMax = new(screenPos.X + 16f * scaleX, screenPos.Y);
        dl.AddRectFilled(bodyMin, bodyMax, 0xDD20202E, 3f);
        dl.AddRect(bodyMin, bodyMax, 0xAA802020, 3f, ImDrawFlags.None, 1f);

        dl.AddCircleFilled(new Vector2(screenPos.X - 14f * scaleX, screenPos.Y - 4f * scaleY), 5f * scaleY, 0xFFD0D5E0);

        Vector2 sosPos = new(screenPos.X, screenPos.Y - 32f * scaleY);
        string sosText = $"[CAÍDO! {CoopRevivalManager.RemoteBleedoutTimer:0}s]";
        Vector2 sz = ImGui.CalcTextSize(sosText);
        Vector2 bMin = sosPos + new Vector2(-sz.X * 0.5f - 6, -sz.Y - 4);
        Vector2 bMax = sosPos + new Vector2(sz.X * 0.5f + 6, 2);

        dl.AddRectFilled(bMin, bMax, 0xEE1A0A0A, 4f);
        dl.AddRect(bMin, bMax, 0xFF2020FF, 4f, ImDrawFlags.None, 1.5f);
        dl.AddText(bMin + new Vector2(6, 2), 0xFF3540FF, sosText);
    }

    private static void DrawGhostSilhouette(
        ImDrawListPtr dl,
        Vector2 screenPos,
        float scaleX,
        float scaleY,
        float dir,
        float alpha,
        uint tint,
        byte charId,
        PuppetAction action,
        float walkPhase)
    {
        uint col = (tint & 0x00FFFFFF) | ((uint)(alpha * 200f) << 24);
        Vector2 cPos = new(screenPos.X, screenPos.Y - 18f * scaleY);

        // Ghost body silhouette
        dl.AddRectFilled(cPos + new Vector2(-5f * scaleX, -8f * scaleY), cPos + new Vector2(5f * scaleX, 10f * scaleY), col, 3f);
        dl.AddCircleFilled(cPos + new Vector2(0, -14f * scaleY), 5.5f * scaleY, col);

        // Cape / Coat ghost trail
        Vector2 p1 = cPos + new Vector2(-12f * dir * scaleX, 18f * scaleY);
        Vector2 p2 = cPos + new Vector2(0, -7f * scaleY);
        Vector2 p3 = cPos + new Vector2(-4f * dir * scaleX, 19f * scaleY);
        dl.AddTriangleFilled(p1, p2, p3, col);
    }

    private static void DrawProjectile(
        ImDrawListPtr dl,
        Vector2 prScr,
        float scaleX,
        float scaleY,
        PuppetProjectile pr)
    {
        float dir = pr.FacingLeft ? -1f : 1f;

        switch (pr.Kind)
        {
            case 1: // Dagger
                Vector2 tip = prScr + new Vector2(dir * 12f * scaleX, 0);
                Vector2 hilt = prScr - new Vector2(dir * 10f * scaleX, 0);
                dl.AddLine(hilt, tip, 0xFFFFFFFF, 2.5f * scaleX);
                dl.AddLine(hilt, prScr - new Vector2(dir * 18f * scaleX, 0), 0x60A0D0FF, 1.5f * scaleX);
                break;

            case 2: // Axe (Spinning)
                float cos = MathF.Cos(pr.Rotation);
                float sin = MathF.Sin(pr.Rotation);
                Vector2 a1 = prScr + new Vector2(cos * 10f * scaleX, sin * 10f * scaleY);
                Vector2 a2 = prScr - new Vector2(cos * 10f * scaleX, sin * 10f * scaleY);
                dl.AddLine(a1, a2, 0xFF704020, 2.5f * scaleX); // Wooden haft
                dl.AddCircleFilled(a1, 5f * scaleX, 0xFFD0D5E0); // Blade head
                dl.AddCircle(a1, 5f * scaleX, 0xFFFFFFFF, 12, 1.2f);
                break;

            case 3: // Cross (Spinning golden crucifix)
                float cCos = MathF.Cos(pr.Rotation * 1.5f);
                float cSin = MathF.Sin(pr.Rotation * 1.5f);
                Vector2 v1 = prScr + new Vector2(cCos * 12f * scaleX, cSin * 12f * scaleY);
                Vector2 v2 = prScr - new Vector2(cCos * 12f * scaleX, cSin * 12f * scaleY);
                Vector2 h1 = prScr + new Vector2(-cSin * 8f * scaleX, cCos * 8f * scaleY);
                Vector2 h2 = prScr - new Vector2(-cSin * 8f * scaleX, cCos * 8f * scaleY);
                dl.AddLine(v1, v2, 0xFF30D0FF, 3.5f * scaleX);
                dl.AddLine(h1, h2, 0xFF30D0FF, 3.5f * scaleX);
                dl.AddCircle(prScr, 10f * scaleX, 0x8040FFFF, 16, 1.5f);
                break;

            case 4: // Holy Water (Sacred blue fire)
                float flameP = MathF.Sin(pr.Life * 16f) * 3f;
                dl.AddCircleFilled(prScr, (8f + flameP) * scaleX, 0x80FFA030);
                dl.AddCircleFilled(prScr + new Vector2(0, -6f * scaleY), 5f * scaleX, 0xCCFFD040);
                break;

            case 5: // Maria Dove
            default:
                float wingFlap = MathF.Sin(pr.Life * 22f) * 6f;
                dl.AddCircleFilled(prScr, 4f * scaleX, 0xFFFFFFFF);
                dl.AddLine(prScr, prScr + new Vector2(-dir * 8f * scaleX, -wingFlap * scaleY), 0xFFFFFFFF, 2f * scaleX);
                dl.AddLine(prScr, prScr + new Vector2(-dir * 8f * scaleX, wingFlap * scaleY), 0xFFFFFFFF, 2f * scaleX);
                break;
        }
    }

    // =========================================================================
    // 1. ALUCARD TEPES (STATE-OF-THE-ART DETAILED RENDERING)
    // =========================================================================
    private static void DrawAlucardState(
        ImDrawListPtr dl,
        Vector2 cPos,
        float scaleX,
        float scaleY,
        float dir,
        float time,
        float walkPhase,
        PuppetAction action,
        bool hurtFlash)
    {
        uint tintHurt = hurtFlash ? 0xFF3030FF : 0xFFFFFFFF;
        float stride = (action == PuppetAction.Walk || action == PuppetAction.Run) ? MathF.Sin(walkPhase) : 0f;
        float capeWind = MathF.Sin(time * 4f) * 3f;

        if (action == PuppetAction.Run) capeWind += 7f;
        else if (action == PuppetAction.Backdash) capeWind += 12f;

        // Capa Exterior (Obsidian Black)
        Vector2 capeBackP1 = cPos + new Vector2((-14 * dir - capeWind * dir) * scaleX, 18 * scaleY);
        Vector2 capeBackP2 = cPos + new Vector2(0, -9 * scaleY);
        Vector2 capeBackP3 = cPos + new Vector2(-5 * dir * scaleX, 20 * scaleY);
        dl.AddTriangleFilled(capeBackP1, capeBackP2, capeBackP3, hurtFlash ? 0xFF2020A0 : 0xFF14141E);

        // Capa Forro Interior (Royal Crimson Velvet)
        Vector2 capeFrontP1 = cPos + new Vector2((-9 * dir - (capeWind * 0.5f) * dir) * scaleX, 17 * scaleY);
        Vector2 capeFrontP2 = cPos + new Vector2(0, -7 * scaleY);
        Vector2 capeFrontP3 = cPos + new Vector2(-2 * dir * scaleX, 19 * scaleY);
        dl.AddTriangleFilled(capeFrontP1, capeFrontP2, capeFrontP3, hurtFlash ? 0xFF4040FF : 0xFF181085);

        // Tronco Nobre & Detalhes Dourados
        Vector2 torsoMin = cPos + new Vector2(-4.5f * scaleX, -8 * scaleY);
        Vector2 torsoMax = cPos + new Vector2(4.5f * scaleX, 9 * scaleY);
        dl.AddRectFilled(torsoMin, torsoMax, hurtFlash ? 0xFF8080C0 : 0xFF222228, 2f);
        dl.AddLine(cPos + new Vector2(0, -6 * scaleY), cPos + new Vector2(0, 7 * scaleY), hurtFlash ? 0xFFFFFFFF : 0xFF35B0D4, 1.5f);

        // Pernas & Botas em Movimento Articulado
        float leg1Off = stride * 4f;
        float leg2Off = -stride * 4f;
        dl.AddLine(cPos + new Vector2(-2.5f * scaleX, 9 * scaleY), cPos + new Vector2((-2.5f + leg1Off) * scaleX, 18 * scaleY), hurtFlash ? 0xFFFFFFFF : 0xFF18181E, 2.5f * scaleX);
        dl.AddLine(cPos + new Vector2(2.5f * scaleX, 9 * scaleY), cPos + new Vector2((2.5f + leg2Off) * scaleX, 18 * scaleY), hurtFlash ? 0xFFFFFFFF : 0xFF18181E, 2.5f * scaleX);

        // Rosto Pálido & Cabelos Prateados
        dl.AddCircleFilled(cPos + new Vector2(0, -14 * scaleY), 5.5f * scaleY, hurtFlash ? 0xFFFF8080 : 0xFFE0E5EC);
        float hairWave = MathF.Sin(time * 5f) * 2f;
        dl.AddLine(cPos + new Vector2(-3 * dir * scaleX, -16 * scaleY), cPos + new Vector2((-6 * dir - hairWave * dir) * scaleX, -1 * scaleY), hurtFlash ? 0xFFFFFFFF : 0xFFD8EEF5, 2.0f * scaleX);
        dl.AddLine(cPos + new Vector2(3 * dir * scaleX, -16 * scaleY), cPos + new Vector2((6 * dir - hairWave * dir) * scaleX, -1 * scaleY), hurtFlash ? 0xFFFFFFFF : 0xFFD8EEF5, 2.0f * scaleX);

        // Espada desembainhada & Corte Arc
        if (action == PuppetAction.AttackMain)
        {
            // Dynamic Crescent Sword Slash Arc
            float atkProgress = 1f - Math.Clamp(RemotePuppet.AttackTimer / 0.28f, 0f, 1f);
            float arcAngle = -0.4f + atkProgress * 1.8f;
            Vector2 slashCenter = cPos + new Vector2(dir * 8f * scaleX, 0);
            Vector2 tip = slashCenter + new Vector2(MathF.Cos(arcAngle) * dir * 26f * scaleX, MathF.Sin(arcAngle) * 22f * scaleY);

            dl.AddLine(cPos + new Vector2(dir * 4f * scaleX, 0), tip, 0xFFFFFFFF, 2.5f * scaleX);
            dl.AddCircle(slashCenter, 24f * scaleX, 0x80D0E8FF, 16, 2.2f);
            dl.AddCircleFilled(tip, 3.5f * scaleX, 0xFFFFFFFF);
        }
        else
        {
            // Espada em repouso
            Vector2 swordHilt = cPos + new Vector2(5 * dir * scaleX, 2 * scaleY);
            Vector2 swordTip = cPos + new Vector2(17 * dir * scaleX, 13 * scaleY);
            dl.AddLine(swordHilt, swordTip, 0xFFE0E0E0, 2.0f * scaleX);
            dl.AddLine(cPos + new Vector2(3 * dir * scaleX, 4 * scaleY), cPos + new Vector2(7 * dir * scaleX, 0), 0xFF20A0D0, 2.0f * scaleX);
        }
    }

    // =========================================================================
    // 2. RICHTER BELMONT (STATE-OF-THE-ART DETAILED RENDERING)
    // =========================================================================
    private static void DrawRichterState(
        ImDrawListPtr dl,
        Vector2 cPos,
        float scaleX,
        float scaleY,
        float dir,
        float time,
        float walkPhase,
        PuppetAction action,
        bool hurtFlash,
        byte charVariant)
    {
        float stride = (action == PuppetAction.Walk || action == PuppetAction.Run) ? MathF.Sin(walkPhase) : 0f;
        float ribbon = MathF.Sin(time * 6f) * 4f;
        if (action == PuppetAction.Run || action == PuppetAction.Backdash) ribbon += 8f;

        // Fita da Bandana Branca Esvoaçante
        dl.AddLine(cPos + new Vector2(-4 * dir * scaleX, -16 * scaleY), cPos + new Vector2((-15 * dir - ribbon * dir) * scaleX, -14 * scaleY), 0xFFFFFFFF, 2f * scaleX);

        // Cabeça & Bandana
        dl.AddCircleFilled(cPos + new Vector2(0, -14 * scaleY), 5.5f * scaleY, hurtFlash ? 0xFFFF8080 : 0xFFD5DDF0);
        dl.AddRectFilled(cPos + new Vector2(-5 * scaleX, -19 * scaleY), cPos + new Vector2(5 * scaleX, -14 * scaleY), 0xFF253555, 1.5f);
        dl.AddLine(cPos + new Vector2(-5 * scaleX, -15 * scaleY), cPos + new Vector2(5 * scaleX, -15 * scaleY), 0xFFFFFFFF, 1.6f);

        // Túnica ou Sobretudo
        uint tunicCol = charVariant switch
        {
            2 => 0xFF653010, // Saturn Coat (Azul Escuro Real)
            5 => 0xFF204565, // Brown Leather Duster
            _ => 0xFFB05020  // Classic Royal Belmont Blue
        };
        if (hurtFlash) tunicCol = 0xFFFFFFFF;

        dl.AddRectFilled(cPos + new Vector2(-6 * scaleX, -8 * scaleY), cPos + new Vector2(6 * scaleX, 10 * scaleY), tunicCol, 2f);
        dl.AddLine(cPos + new Vector2(-6 * scaleX, 3 * scaleY), cPos + new Vector2(6 * scaleX, 3 * scaleY), 0xFF20A0D0, 1.8f);

        // Cruz Sagrada no Peito
        Vector2 crossPos = cPos + new Vector2(-8 * dir * scaleX, -7 * scaleY);
        dl.AddLine(crossPos + new Vector2(0, -3.5f * scaleY), crossPos + new Vector2(0, 3.5f * scaleY), 0xFF40D0F0, 1.5f);
        dl.AddLine(crossPos + new Vector2(-2f * scaleX, -1 * scaleY), crossPos + new Vector2(2f * scaleX, -1 * scaleY), 0xFF40D0F0, 1.5f);

        // Pernas & Botas
        float leg1Off = stride * 4.5f;
        float leg2Off = -stride * 4.5f;
        dl.AddLine(cPos + new Vector2(-2.5f * scaleX, 10 * scaleY), cPos + new Vector2((-2.5f + leg1Off) * scaleX, 18 * scaleY), hurtFlash ? 0xFFFFFFFF : 0xFF182030, 2.5f * scaleX);
        dl.AddLine(cPos + new Vector2(2.5f * scaleX, 10 * scaleY), cPos + new Vector2((2.5f + leg2Off) * scaleX, 18 * scaleY), hurtFlash ? 0xFFFFFFFF : 0xFF182030, 2.5f * scaleX);

        // Vampire Killer Attack (Multi-Segment Articulated Chain Whip)
        if (action == PuppetAction.AttackMain)
        {
            float whipProgress = 1f - Math.Clamp(RemotePuppet.AttackTimer / 0.28f, 0f, 1f);
            float reach = 10f + whipProgress * 36f;
            Vector2 pStart = cPos + new Vector2(6 * dir * scaleX, 2 * scaleY);
            Vector2 pWave = cPos + new Vector2((reach * 0.5f) * dir * scaleX, -4 * scaleY);
            Vector2 pTip = cPos + new Vector2(reach * dir * scaleX, 2 * scaleY);

            dl.AddBezierCubic(pStart, pWave, pWave + new Vector2(dir * 8f * scaleX, 6 * scaleY), pTip, 0xFFFFFFFF, 2.2f * scaleX);
            dl.AddCircleFilled(pTip, 3.5f * scaleX, 0xFF40FFFF); // Spiked morningstar tip spark
        }
        else
        {
            // Vampire Killer recolhido no quadril
            dl.AddBezierCubic(
                cPos + new Vector2(6 * dir * scaleX, 3 * scaleY),
                cPos + new Vector2(12 * dir * scaleX, 7 * scaleY),
                cPos + new Vector2(14 * dir * scaleX, 13 * scaleY),
                cPos + new Vector2(18 * dir * scaleX, 11 * scaleY),
                0xFF80C0D0,
                2f * scaleX
            );
        }
    }

    // =========================================================================
    // 3. MARIA RENARD (STATE-OF-THE-ART DETAILED RENDERING)
    // =========================================================================
    private static void DrawMariaState(
        ImDrawListPtr dl,
        Vector2 cPos,
        float scaleX,
        float scaleY,
        float dir,
        float time,
        float walkPhase,
        PuppetAction action,
        bool hurtFlash,
        byte charVariant)
    {
        float stride = (action == PuppetAction.Walk || action == PuppetAction.Run) ? MathF.Sin(walkPhase) : 0f;
        float ribbon = MathF.Sin(time * 5.5f) * 3f;

        // Cabeça & Cachos Dourados
        dl.AddCircleFilled(cPos + new Vector2(0, -14 * scaleY), 5.5f * scaleY, hurtFlash ? 0xFFFF8080 : 0xFFD8E5F0);
        dl.AddCircleFilled(cPos + new Vector2(-4 * dir * scaleX, -11 * scaleY), 3.0f * scaleY, 0xFF40D0F5);
        dl.AddCircleFilled(cPos + new Vector2(4 * dir * scaleX, -11 * scaleY), 3.0f * scaleY, 0xFF40D0F5);

        // Tiara / Laço
        uint headAcc = (charVariant == 4) ? 0xFF4020D0 : 0xFF20D0F5; // Red bow (PSP) or Gold tiara (Saturn)
        dl.AddLine(cPos + new Vector2(-4 * scaleX, -18 * scaleY), cPos + new Vector2(4 * scaleX, -18 * scaleY), headAcc, 1.8f);

        // Vestido / Túnica (Emerald Green no Saturn, Crimson/Pink no PSP)
        uint dressCol = (charVariant == 4) ? 0xFF352085 : 0xFF257520;
        if (hurtFlash) dressCol = 0xFFFFFFFF;

        Vector2 dP1 = cPos + new Vector2(-6 * scaleX, 9 * scaleY);
        Vector2 dP2 = cPos + new Vector2(6 * scaleX, 9 * scaleY);
        Vector2 dP3 = cPos + new Vector2(0, -8 * scaleY);
        dl.AddTriangleFilled(dP1, dP2, dP3, dressCol);

        // Pernas & Sapatilhas
        float leg1Off = stride * 3.5f;
        float leg2Off = -stride * 3.5f;
        dl.AddLine(cPos + new Vector2(-2 * scaleX, 9 * scaleY), cPos + new Vector2((-2 + leg1Off) * scaleX, 17 * scaleY), 0xFFD8E5F0, 2f * scaleX);
        dl.AddLine(cPos + new Vector2(2 * scaleX, 9 * scaleY), cPos + new Vector2((2 + leg2Off) * scaleX, 17 * scaleY), 0xFFD8E5F0, 2f * scaleX);

        // Magic Dove Cast Attack
        if (action == PuppetAction.AttackMain)
        {
            Vector2 handPos = cPos + new Vector2(dir * 12f * scaleX, -4 * scaleY);
            dl.AddCircleFilled(handPos, 4f * scaleX, 0xFFFFFFFF);
            dl.AddCircle(handPos, 8f * scaleX, 0x8040FFFF, 12, 1.5f);
        }
    }

    private static void DrawOverheadHUD(
        ImDrawListPtr dl,
        Vector2 screenPos,
        float scaleX,
        float scaleY)
    {
        Vector2 overhead = new(screenPos.X, screenPos.Y - 38f * scaleY);
        string rawName = NetworkManager.RemotePlayerName;
        string name = (string.IsNullOrWhiteSpace(rawName) || rawName == "Alucard" || rawName == "Partner" || rawName == "Parceiro")
            ? "P2"
            : rawName;
        string charName = string.IsNullOrEmpty(RemotePuppet.CharacterName) ? "" : $" ({RemotePuppet.CharacterName})";
        string label = $"[{name}]{charName}";

        Vector2 txtSz = ImGui.CalcTextSize(label);
        Vector2 bMin = new(overhead.X - txtSz.X * 0.5f - 6f, overhead.Y - txtSz.Y - 9f);
        Vector2 bMax = new(overhead.X + txtSz.X * 0.5f + 6f, overhead.Y - 1f);

        // Nameplate Background pill
        dl.AddRectFilled(bMin, bMax, 0xDD12141C, 4f);
        dl.AddRect(bMin, bMax, 0xFF35B0D4, 4f, ImDrawFlags.None, 1.2f);

        // Text with sharp drop shadow
        dl.AddText(new Vector2(overhead.X - txtSz.X * 0.5f + 1, bMin.Y + 2), 0xFF000000, label);
        dl.AddText(new Vector2(overhead.X - txtSz.X * 0.5f, bMin.Y + 1), 0xFFFFFFFF, label);

        // Mini Health Bar
        float barW = Math.Max(48f * scaleX, txtSz.X + 8f);
        float barH = 5f * scaleY;
        Vector2 barMin = new(overhead.X - barW * 0.5f, overhead.Y);
        Vector2 barMax = new(overhead.X + barW * 0.5f, overhead.Y + barH);

        float hpRatio = Math.Clamp((float)RemotePuppet.CurHp / Math.Max(1, (int)RemotePuppet.MaxHp), 0f, 1f);
        uint hpCol = hpRatio > 0.5f ? 0xFF35D040 : (hpRatio > 0.25f ? 0xFF20D0E0 : 0xFF2525E5);

        dl.AddRectFilled(barMin, barMax, 0xCC1A1010, 2f);
        dl.AddRectFilled(barMin, new Vector2(barMin.X + barW * hpRatio, barMax.Y), hpCol, 2f);
        dl.AddRect(barMin, barMax, 0xFF505565, 2f, ImDrawFlags.None, 0.8f);
    }

    private static void DrawTacticalPing(
        ImDrawListPtr dl,
        Vector2 imageMin,
        Vector2 imageSize,
        float scaleX,
        float scaleY,
        short localScrollX,
        short localScrollY,
        float time)
    {
        if (!CoopPingManager.HasActivePing) return;

        byte currentStage = (byte)Stages.Current;
        byte currentRoomX = (byte)Stages.RoomX;
        byte currentRoomY = (byte)Stages.RoomY;

        if (CoopPingManager.PingStage != currentStage ||
            CoopPingManager.PingRoomX != currentRoomX ||
            CoopPingManager.PingRoomY != currentRoomY)
            return;

        float pGameX = RemotePuppet.InterpX - localScrollX;
        float pGameY = RemotePuppet.InterpY - localScrollY;

        if (pGameX < -20 || pGameX > 276 || pGameY < -20 || pGameY > 244) return;

        Vector2 pingScreen = imageMin + new Vector2(pGameX * scaleX, pGameY * scaleY);

        float ripple = (time * 2.5f) % 1.0f;
        float rRadius = (10f + ripple * 30f) * scaleX;
        uint rAlpha = (uint)((1f - ripple) * 180f);
        uint rCol = (rAlpha << 24) | 0x00FFFF;

        dl.AddCircle(pingScreen, rRadius, rCol, 24, 2f);
        dl.AddCircleFilled(pingScreen, 5f * scaleX, 0xFF40FFFF);

        int pType = Math.Clamp((int)CoopPingManager.ActivePingType, 0, CoopPingManager.PingPresets.Length - 1);
        string tag = $"{CoopPingManager.PingPresets[pType].Icon} {CoopPingManager.PingPresets[pType].Label}";
        Vector2 tSz = ImGui.CalcTextSize(tag);
        Vector2 tMin = pingScreen + new Vector2(-tSz.X * 0.5f - 4, -tSz.Y - 14);
        Vector2 tMax = tMin + tSz + new Vector2(8, 4);

        dl.AddRectFilled(tMin, tMax, 0xDD101018, 3f);
        dl.AddRect(tMin, tMax, 0xFF40FFFF, 3f, ImDrawFlags.None, 1f);
        dl.AddText(tMin + new Vector2(4, 2), 0xFFFFFFFF, tag);
    }

    private static void DrawPartnerOnPauseMap(
        ImDrawListPtr dl,
        Vector2 imageMin,
        Vector2 imageSize,
        float scaleX,
        float scaleY)
    {
        var m = RecompOne.Runtime.Runtime.Mem;
        if (m == null) return;

        bool localSecondCastle = Game.SecondCastle;
        bool remoteSecondCastle = (RemotePuppet.StageId & 0x20) != 0;

        if (localSecondCastle != remoteSecondCastle)
        {
            string otherCastleText = $"[Parceiro no {(remoteSecondCastle ? "2º Castelo (Invertido)" : "1º Castelo")}: Sala {RemotePuppet.RoomX}, {RemotePuppet.RoomY}]";
            Vector2 sz = ImGui.CalcTextSize(otherCastleText);
            Vector2 bMin = imageMin + new Vector2((imageSize.X - sz.X) * 0.5f - 8, 12);
            Vector2 bMax = bMin + sz + new Vector2(16, 8);
            dl.AddRectFilled(bMin, bMax, 0xEE14121A, 4f);
            dl.AddRect(bMin, bMax, 0xFF40D0F5, 4f, ImDrawFlags.None, 1.5f);
            dl.AddText(bMin + new Vector2(8, 4), 0xFFFFFFFF, otherCastleText);
            return;
        }

        int castleX = (int)m.ReadU8(0x800730B0);
        int castleY = (int)m.ReadU8(0x800730B4);
        int cellX = castleX + RemotePuppet.RoomX;
        int cellY = castleY + RemotePuppet.RoomY;

        float mapPixelX = cellX * 4f - (localSecondCastle ? 15f : 14f);
        float mapPixelY = cellY * 4f - (localSecondCastle ? 30f : 13f);

        Vector2 cellPos = imageMin + new Vector2(mapPixelX * scaleX, mapPixelY * scaleY);
        float roomTileW = 4f * scaleX;
        float roomTileH = 4f * scaleY;

        float pulse = (MathF.Sin((float)ImGui.GetTime() * 8f) + 1f) * 0.5f;

        Vector2 rMin = cellPos - new Vector2(2f + pulse * 2f, 2f + pulse * 2f);
        Vector2 rMax = cellPos + new Vector2(roomTileW + 2f + pulse * 2f, roomTileH + 2f + pulse * 2f);

        dl.AddRectFilled(cellPos, cellPos + new Vector2(roomTileW, roomTileH), 0xAA20D040);
        dl.AddRect(rMin, rMax, 0xFF40FFFF, 2f, ImDrawFlags.None, 1.8f);

        string name = string.IsNullOrWhiteSpace(NetworkManager.RemotePlayerName) ? "Parceiro" : NetworkManager.RemotePlayerName;
        string label = $"[P2] {name} ({RemotePuppet.CharacterName})";
        Vector2 txtSz = ImGui.CalcTextSize(label);
        Vector2 tagMin = new(cellPos.X - txtSz.X * 0.5f - 4, rMin.Y - txtSz.Y - 6);
        Vector2 tagMax = new(cellPos.X + txtSz.X * 0.5f + 4, rMin.Y - 1);

        dl.AddRectFilled(tagMin, tagMax, 0xEE10141C, 3f);
        dl.AddRect(tagMin, tagMax, 0xFF20E050, 3f, ImDrawFlags.None, 1f);
        dl.AddText(tagMin + new Vector2(4, 1), 0xFFFFFFFF, label);
    }

    private static void DrawAdjacentRoomIndicator(
        ImDrawListPtr dl,
        Vector2 imageMin,
        Vector2 imageSize,
        float scaleX,
        float scaleY,
        int dx,
        int dy,
        float time)
    {
        string rawName = NetworkManager.RemotePlayerName;
        string name = (string.IsNullOrWhiteSpace(rawName) || rawName == "Alucard" || rawName == "Partner" || rawName == "Parceiro")
            ? (string.IsNullOrEmpty(RemotePuppet.CharacterName) ? "P2" : $"P2 ({RemotePuppet.CharacterName})")
            : rawName;
        string dirStr = (dx > 0) ? "-> Direita" : (dx < 0) ? "<- Esquerda" : (dy > 0) ? "v Abaixo" : "^ Acima";
        int distRooms = Math.Abs(dx) + Math.Abs(dy);
        string label = distRooms == 1 ? $"[{name} na sala ao lado ({dirStr})]" : $"[{name} a {distRooms} salas ({dirStr})]";

        Vector2 txtSz = ImGui.CalcTextSize(label);
        Vector2 center = imageMin + imageSize * 0.5f;

        Vector2 edgePos;
        if (Math.Abs(dx) >= Math.Abs(dy))
        {
            float x = (dx > 0) ? (imageMin.X + imageSize.X - txtSz.X * 0.5f - 24f) : (imageMin.X + txtSz.X * 0.5f + 24f);
            edgePos = new(x, center.Y);
        }
        else
        {
            float y = (dy > 0) ? (imageMin.Y + imageSize.Y - 32f) : (imageMin.Y + 36f);
            edgePos = new(center.X, y);
        }

        float pulse = (MathF.Sin(time * 5f) + 1f) * 0.5f;
        Vector2 bMin = edgePos - txtSz * 0.5f - new Vector2(8, 4);
        Vector2 bMax = edgePos + txtSz * 0.5f + new Vector2(8, 4);

        dl.AddRectFilled(bMin, bMax, 0xEE12141C, 4f);
        dl.AddRect(bMin, bMax, 0xFF35B0D4, 4f, ImDrawFlags.None, 1.2f + pulse * 0.8f);
        dl.AddText(bMin + new Vector2(8, 4), 0xFF40FFFF, label);
    }
}
