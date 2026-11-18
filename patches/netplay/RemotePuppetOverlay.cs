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
        _initialized = true;
    }

    public static void Render(Vector2 imageMin, Vector2 imageSize)
    {
        if (!NetworkManager.IsConnected || !RemotePuppet.IsActive) return;
        if (!Game.Available || !Game.InGame || Game.IsLoading) return;

        var m = RecompOne.Runtime.Runtime.Mem;
        if (m == null) return;

        // Native PSX resolution is 256x224
        float scaleX = imageSize.X / 256.0f;
        float scaleY = imageSize.Y / 224.0f;
        var dl = ImGui.GetWindowDrawList();
        float time = (float)ImGui.GetTime();

        // 1. Normal In-Game Pause Map (Select map): render partner position directly on the normal PS1 map!
        if (Game.MapOpen)
        {
            DrawPartnerOnPauseMap(dl, imageMin, imageSize, scaleX, scaleY);
            return;
        }

        bool sameStage = RemotePuppet.StageId == (byte)Stages.Current;
        bool sameRoom = RemotePuppet.RoomX == (byte)Stages.RoomX && RemotePuppet.RoomY == (byte)Stages.RoomY;

        // 2. Different room: render directional edge indicator to partner's room!
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

        float gameX;
        if (posX >= localScrollX && posX <= localScrollX + 450)
            gameX = posX - localScrollX;
        else
            gameX = posX;

        float gameY;
        if (posY >= localScrollY && posY <= localScrollY + 350)
            gameY = posY - localScrollY;
        else
            gameY = posY;

        Vector2 screenPos = imageMin + new Vector2(gameX * scaleX, gameY * scaleY);

        bool isOffScreen = gameX < -15 || gameX > 271 || gameY < -15 || gameY > 239;

        if (isOffScreen)
        {
            DrawOffScreenIndicator(dl, imageMin, imageSize, screenPos, scaleX, scaleY, time);
        }
        else
        {
            DrawInGamePuppet(dl, screenPos, scaleX, scaleY, time);
        }

        // Render any active tactical ping in this room
        DrawTacticalPing(dl, imageMin, imageSize, scaleX, scaleY, localScrollX, localScrollY, time);
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

        // Pulsing neon chevron
        float pulse = (MathF.Sin(time * 6f) + 1f) * 0.5f;
        uint arrowCol = CoopRevivalManager.IsRemoteDowned ? 0xFF2020FF : 0xFF40D0F5;

        Vector2 p1 = edgePos + dir * (14f + pulse * 3f);
        Vector2 p2 = edgePos + new Vector2(-dir.Y, dir.X) * 8f - dir * 6f;
        Vector2 p3 = edgePos + new Vector2(dir.Y, -dir.X) * 8f - dir * 6f;

        dl.AddTriangleFilled(p1, p2, p3, arrowCol);
        dl.AddTriangle(p1, p2, p3, 0xFFFFFFFF, 1.5f);

        // Badge with distance
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
        float time)
    {
        // 1. Co-op Downed State
        if (CoopRevivalManager.IsRemoteDowned)
        {
            DrawDownedPuppet(dl, screenPos, scaleX, scaleY, time);
            return;
        }

        // 2. Ground Shadow under feet (rounded pill)
        Vector2 shadowMin = new(screenPos.X - 13f * scaleX, screenPos.Y - 2.5f * scaleY);
        Vector2 shadowMax = new(screenPos.X + 13f * scaleX, screenPos.Y + 2.5f * scaleY);
        dl.AddRectFilled(shadowMin, shadowMax, 0x65000000, 3f);

        // 3. Body animation parameters
        float dir = RemotePuppet.FacingLeft ? -1f : 1f;
        float walkCycle = Math.Abs(RemotePuppet.VelX) > 10 ? MathF.Sin(time * 14f) : 0f;
        float breath = MathF.Sin(time * 3.5f) * 1.2f * scaleY;

        // Character center of mass (feet anchor to body center)
        Vector2 cPos = new(screenPos.X, screenPos.Y - 18f * scaleY + breath);

        // 4. Character Sprite Rendering: Authentic Pixel-Art Sprite with Procedural Fallback
        uint texId = NetplaySpriteManager.GetHunterSprite(
            RemotePuppet.Character,
            RemotePuppet.AttackTrigger != 0,
            Math.Abs(RemotePuppet.VelY) > 8,
            out Vector2 baseSize);

        if (texId != 0)
        {
            float drawW = baseSize.X * scaleX;
            float drawH = baseSize.Y * scaleY;
            Vector2 pMin = new(cPos.X - drawW * 0.5f, cPos.Y - drawH * 0.5f);
            Vector2 pMax = new(cPos.X + drawW * 0.5f, cPos.Y + drawH * 0.5f);

            Vector2 uv0 = RemotePuppet.FacingLeft ? new Vector2(1, 0) : Vector2.Zero;
            Vector2 uv1 = RemotePuppet.FacingLeft ? new Vector2(0, 1) : Vector2.One;

            dl.AddImage((nint)texId, pMin, pMax, uv0, uv1);
        }
        else
        {
            switch (RemotePuppet.Character)
            {
                case 1: // Richter Classic
                case 2: // Richter Saturn Coat
                case 5: // Richter Brown Coat
                    DrawRichterSprite(dl, cPos, scaleX, scaleY, dir, time, walkCycle, RemotePuppet.Character);
                    break;

                case 3: // Saturn Maria
                case 4: // PSP Maria
                    DrawMariaSprite(dl, cPos, scaleX, scaleY, dir, time, walkCycle, RemotePuppet.Character);
                    break;

                case 0: // Alucard
                default:
                    DrawAlucardSprite(dl, cPos, scaleX, scaleY, dir, time, walkCycle);
                    break;
            }
        }

        // 5. Weapon Slash effect
        if (RemotePuppet.AttackTrigger != 0)
        {
            DrawAttackEffect(dl, cPos, scaleX, scaleY, dir);
        }

        // 6. Overhead Tactical Nameplate & Health Bar
        DrawOverheadHUD(dl, screenPos, scaleX, scaleY);
    }

    private static void DrawDownedPuppet(
        ImDrawListPtr dl,
        Vector2 screenPos,
        float scaleX,
        float scaleY,
        float time)
    {
        // Pulsing bleedout aura
        float auraPulse = (MathF.Sin(time * 7f) + 1f) * 0.5f;
        dl.AddCircleFilled(screenPos, (18f + auraPulse * 8f) * scaleX, 0x40201080);
        dl.AddCircle(screenPos, (18f + auraPulse * 8f) * scaleX, 0xAA2020FF, 24, 2f);

        // Body lying on the floor
        Vector2 bodyMin = new(screenPos.X - 16f * scaleX, screenPos.Y - 8f * scaleY);
        Vector2 bodyMax = new(screenPos.X + 16f * scaleX, screenPos.Y);
        dl.AddRectFilled(bodyMin, bodyMax, 0xDD20202E, 3f);
        dl.AddRect(bodyMin, bodyMax, 0xAA802020, 3f, ImDrawFlags.None, 1f);

        // Head
        dl.AddCircleFilled(new Vector2(screenPos.X - 14f * scaleX, screenPos.Y - 4f * scaleY), 5f * scaleY, 0xFFD0D5E0);

        // Downed SOS Tag
        Vector2 sosPos = new(screenPos.X, screenPos.Y - 32f * scaleY);
        string sosText = $"[CAÍDO! {CoopRevivalManager.RemoteBleedoutTimer:0}s]";
        Vector2 sz = ImGui.CalcTextSize(sosText);
        Vector2 bMin = sosPos + new Vector2(-sz.X * 0.5f - 6, -sz.Y - 4);
        Vector2 bMax = sosPos + new Vector2(sz.X * 0.5f + 6, 2);

        dl.AddRectFilled(bMin, bMax, 0xEE1A0A0A, 4f);
        dl.AddRect(bMin, bMax, 0xFF2020FF, 4f, ImDrawFlags.None, 1.5f);
        dl.AddText(bMin + new Vector2(6, 2), 0xFF3540FF, sosText);
    }

    private static void DrawAlucardSprite(
        ImDrawListPtr dl,
        Vector2 cPos,
        float scaleX,
        float scaleY,
        float dir,
        float time,
        float walkCycle)
    {
        // Capa esvoaçante (exterior escuro, interior carmesim)
        float capeWave = MathF.Sin(time * 3.5f) * 3f;
        Vector2 capeBackP1 = cPos + new Vector2((-14 * dir - capeWave * dir) * scaleX, 18 * scaleY);
        Vector2 capeBackP2 = cPos + new Vector2(0, -9 * scaleY);
        Vector2 capeBackP3 = cPos + new Vector2(-6 * dir * scaleX, 20 * scaleY);
        dl.AddTriangleFilled(capeBackP1, capeBackP2, capeBackP3, 0xFF14141E);

        // Forro carmesim
        Vector2 capeFrontP1 = cPos + new Vector2((-9 * dir) * scaleX, 17 * scaleY);
        Vector2 capeFrontP2 = cPos + new Vector2(0, -7 * scaleY);
        Vector2 capeFrontP3 = cPos + new Vector2(-2 * dir * scaleX, 19 * scaleY);
        dl.AddTriangleFilled(capeFrontP1, capeFrontP2, capeFrontP3, 0xFF181085);

        // Tronco gótico com detalhes dourados
        Vector2 torsoMin = cPos + new Vector2(-4.5f * scaleX, -8 * scaleY);
        Vector2 torsoMax = cPos + new Vector2(4.5f * scaleX, 9 * scaleY);
        dl.AddRectFilled(torsoMin, torsoMax, 0xFF222228, 2f);
        dl.AddLine(cPos + new Vector2(0, -6 * scaleY), cPos + new Vector2(0, 7 * scaleY), 0xFF35B0D4, 1.5f);

        // Pernas e botas em movimento
        dl.AddLine(cPos + new Vector2(-2.5f * scaleX, 9 * scaleY), cPos + new Vector2((-2.5f + walkCycle * 3.5f) * scaleX, 18 * scaleY), 0xFF18181E, 2.5f * scaleX);
        dl.AddLine(cPos + new Vector2(2.5f * scaleX, 9 * scaleY), cPos + new Vector2((2.5f - walkCycle * 3.5f) * scaleX, 18 * scaleY), 0xFF18181E, 2.5f * scaleX);

        // Rosto pálido e cabelos prateados
        dl.AddCircleFilled(cPos + new Vector2(0, -14 * scaleY), 5.5f * scaleY, 0xFFE0E5EC);
        dl.AddLine(cPos + new Vector2(-3 * dir * scaleX, -16 * scaleY), cPos + new Vector2(-6 * dir * scaleX, -1 * scaleY), 0xFFD8EEF5, 2.0f * scaleX);
        dl.AddLine(cPos + new Vector2(3 * dir * scaleX, -16 * scaleY), cPos + new Vector2(6 * dir * scaleX, -1 * scaleY), 0xFFD8EEF5, 2.0f * scaleX);

        // Espada desembainhada com brilho
        Vector2 swordHilt = cPos + new Vector2(5 * dir * scaleX, 2 * scaleY);
        Vector2 swordTip = cPos + new Vector2(17 * dir * scaleX, 13 * scaleY);
        dl.AddLine(swordHilt, swordTip, 0xFFE0E0E0, 2.0f * scaleX);
        dl.AddLine(cPos + new Vector2(3 * dir * scaleX, 4 * scaleY), cPos + new Vector2(7 * dir * scaleX, 0), 0xFF20A0D0, 2.0f * scaleX);

        float spark = MathF.Sin(time * 6.0f);
        if (spark > 0.3f)
        {
            dl.AddCircleFilled(swordTip, 2.0f * scaleY, 0xFFFFFFFF);
        }
    }

    private static void DrawRichterSprite(
        ImDrawListPtr dl,
        Vector2 cPos,
        float scaleX,
        float scaleY,
        float dir,
        float time,
        float walkCycle,
        byte charId)
    {
        // Fita da bandana branca esvoaçando
        float ribbon = MathF.Sin(time * 5.5f) * 3f;
        dl.AddLine(cPos + new Vector2(-4 * dir * scaleX, -16 * scaleY), cPos + new Vector2((-14 * dir - ribbon * dir) * scaleX, -14 * scaleY), 0xFFFFFFFF, 2f * scaleX);

        // Cabeça e bandana
        dl.AddCircleFilled(cPos + new Vector2(0, -14 * scaleY), 5.5f * scaleY, 0xFFD5DDF0);
        dl.AddRectFilled(cPos + new Vector2(-5 * scaleX, -19 * scaleY), cPos + new Vector2(5 * scaleX, -14 * scaleY), 0xFF253555, 1.5f);
        dl.AddLine(cPos + new Vector2(-5 * scaleX, -15 * scaleY), cPos + new Vector2(5 * scaleX, -15 * scaleY), 0xFFFFFFFF, 1.6f);

        // Túnica ou Sobretudo
        uint tunicCol = charId switch
        {
            2 => 0xFF653010, // Saturn Coat (Azul Escuro)
            5 => 0xFF204565, // Brown Leather
            _ => 0xFFB05020  // Classic Royal Belmont Blue
        };

        dl.AddRectFilled(cPos + new Vector2(-6 * scaleX, -8 * scaleY), cPos + new Vector2(6 * scaleX, 10 * scaleY), tunicCol, 2f);
        dl.AddLine(cPos + new Vector2(-6 * scaleX, 3 * scaleY), cPos + new Vector2(6 * scaleX, 3 * scaleY), 0xFF20A0D0, 1.8f);

        // Pernas / Botas
        dl.AddLine(cPos + new Vector2(-2.5f * scaleX, 10 * scaleY), cPos + new Vector2((-2.5f + walkCycle * 3.5f) * scaleX, 18 * scaleY), 0xFF182030, 2.5f * scaleX);
        dl.AddLine(cPos + new Vector2(2.5f * scaleX, 10 * scaleY), cPos + new Vector2((2.5f - walkCycle * 3.5f) * scaleX, 18 * scaleY), 0xFF182030, 2.5f * scaleX);

        // Vampire Killer em arco
        dl.AddBezierCubic(
            cPos + new Vector2(6 * dir * scaleX, 3 * scaleY),
            cPos + new Vector2(13 * dir * scaleX, 8 * scaleY),
            cPos + new Vector2(15 * dir * scaleX, 15 * scaleY),
            cPos + new Vector2(20 * dir * scaleX, 13 * scaleY),
            0xFF80C0D0,
            2f * scaleX
        );

        // Cruz sagrada brilhante no peito
        Vector2 crossPos = cPos + new Vector2(-10 * dir * scaleX, -8 * scaleY);
        dl.AddLine(crossPos + new Vector2(0, -4 * scaleY), crossPos + new Vector2(0, 4 * scaleY), 0xFF40D0F0, 1.5f);
        dl.AddLine(crossPos + new Vector2(-2.5f * scaleX, -1 * scaleY), crossPos + new Vector2(2.5f * scaleX, -1 * scaleY), 0xFF40D0F0, 1.5f);
    }

    private static void DrawMariaSprite(
        ImDrawListPtr dl,
        Vector2 cPos,
        float scaleX,
        float scaleY,
        float dir,
        float time,
        float walkCycle,
        byte charId)
    {
        // Cabeça e cachos dourados
        dl.AddCircleFilled(cPos + new Vector2(0, -14 * scaleY), 5.5f * scaleY, 0xFFD8E5F0);
        dl.AddCircleFilled(cPos + new Vector2(-4 * dir * scaleX, -11 * scaleY), 3.0f * scaleY, 0xFF40D0F5);
        dl.AddCircleFilled(cPos + new Vector2(4 * dir * scaleX, -11 * scaleY), 3.0f * scaleY, 0xFF40D0F5);

        // Laço de fita no topo da cabeça
        dl.AddTriangleFilled(cPos + new Vector2(-4 * dir * scaleX, -19 * scaleY), cPos + new Vector2(0, -16 * scaleY), cPos + new Vector2(-1 * scaleX, -18 * scaleY), 0xFF8020E0);
        dl.AddTriangleFilled(cPos + new Vector2(4 * dir * scaleX, -19 * scaleY), cPos + new Vector2(0, -16 * scaleY), cPos + new Vector2(1 * scaleX, -18 * scaleY), 0xFF8020E0);

        // Vestido (Esmeralda para Saturn, Carmesim para PSP)
        uint dressCol = charId == 4 ? 0xFF401080 : 0xFF358525;
        dl.AddTriangleFilled(cPos + new Vector2(-8 * scaleX, 12 * scaleY), cPos + new Vector2(0, -7 * scaleY), cPos + new Vector2(8 * scaleX, 12 * scaleY), dressCol);
        dl.AddRectFilled(cPos + new Vector2(-2.5f * scaleX, -6 * scaleY), cPos + new Vector2(2.5f * scaleX, 1 * scaleY), 0xFFF0F5FA, 1f);

        // Pernas / Meias
        dl.AddLine(cPos + new Vector2(-2f * scaleX, 12 * scaleY), cPos + new Vector2((-2f + walkCycle * 3.0f) * scaleX, 18 * scaleY), 0xFFD0D5E0, 2.0f * scaleX);
        dl.AddLine(cPos + new Vector2(2f * scaleX, 12 * scaleY), cPos + new Vector2((2f - walkCycle * 3.0f) * scaleX, 18 * scaleY), 0xFFD0D5E0, 2.0f * scaleX);

        // Pássaro Celestial Suzaku batendo asas
        float birdFlap = MathF.Sin(time * 12.0f) * 4.0f * scaleY;
        Vector2 bPos = cPos + new Vector2(16 * dir * scaleX, -14 * scaleY + MathF.Sin(time * 3.5f) * 2f * scaleY);

        dl.AddCircleFilled(bPos, 3.0f * scaleY, 0xFFFFFFFF);
        dl.AddTriangleFilled(bPos + new Vector2(-2.5f * dir * scaleX, 0), bPos + new Vector2(0, -3.5f * scaleY - birdFlap), bPos + new Vector2(2f * dir * scaleX, 0), 0xFFF0F5FA);
        dl.AddLine(bPos + new Vector2(1.5f * dir * scaleX, 0), bPos + new Vector2(4f * dir * scaleX, 1 * scaleY), 0xFF20A0F0, 1.5f * scaleX);
    }

    private static void DrawAttackEffect(
        ImDrawListPtr dl,
        Vector2 cPos,
        float scaleX,
        float scaleY,
        float dir)
    {
        // Glowing slashing arc in front of the puppet (cubic bezier curve)
        Vector2 arcStart = cPos + new Vector2(4 * dir * scaleX, -16 * scaleY);
        Vector2 c1 = cPos + new Vector2(24 * dir * scaleX, -8 * scaleY);
        Vector2 c2 = cPos + new Vector2(24 * dir * scaleX, 8 * scaleY);
        Vector2 arcEnd = cPos + new Vector2(4 * dir * scaleX, 16 * scaleY);

        dl.AddBezierCubic(arcStart, c1, c2, arcEnd, 0xEE40FFFF, 3.0f * scaleX);
        dl.AddBezierCubic(arcStart, c1, c2, arcEnd, 0xAAFFFFFF, 1.5f * scaleX);
    }

    private static void DrawOverheadHUD(
        ImDrawListPtr dl,
        Vector2 screenPos,
        float scaleX,
        float scaleY)
    {
        Vector2 overhead = new(screenPos.X, screenPos.Y - 38f * scaleY);
        string name = string.IsNullOrWhiteSpace(NetworkManager.RemotePlayerName) ? "Parceiro" : NetworkManager.RemotePlayerName;
        string label = $"[P2] {name} ({RemotePuppet.CharacterName})";

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

        // Use puppet pos if ping was sent in this room
        float pGameX = RemotePuppet.InterpX - localScrollX;
        float pGameY = RemotePuppet.InterpY - localScrollY;

        if (pGameX < -20 || pGameX > 276 || pGameY < -20 || pGameY > 244) return;

        Vector2 pingScreen = imageMin + new Vector2(pGameX * scaleX, pGameY * scaleY);

        // Sonar ripple rings
        float ripple = (time * 2.5f) % 1.0f;
        float rRadius = (10f + ripple * 30f) * scaleX;
        uint rAlpha = (uint)((1f - ripple) * 180f);
        uint rCol = (rAlpha << 24) | 0x00FFFF;

        dl.AddCircle(pingScreen, rRadius, rCol, 24, 2f);
        dl.AddCircleFilled(pingScreen, 5f * scaleX, 0xFF40FFFF);

        // Ping text tag
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

        // If partner is in the opposite castle, display status banner at top
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

        // Pulsing neon reticle over the partner's room on the map
        Vector2 rMin = cellPos - new Vector2(2f + pulse * 2f, 2f + pulse * 2f);
        Vector2 rMax = cellPos + new Vector2(roomTileW + 2f + pulse * 2f, roomTileH + 2f + pulse * 2f);

        dl.AddRectFilled(cellPos, cellPos + new Vector2(roomTileW, roomTileH), 0xAA20D040);
        dl.AddRect(rMin, rMax, 0xFF40FFFF, 2f, ImDrawFlags.None, 1.8f);

        // Overhead Name badge
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
        string name = string.IsNullOrWhiteSpace(NetworkManager.RemotePlayerName) ? "Parceiro" : NetworkManager.RemotePlayerName;
        string dirStr = (dx > 0) ? "Direita →" : (dx < 0) ? "← Esquerda" : (dy > 0) ? "Abaixo ↓" : "Acima ↑";
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
