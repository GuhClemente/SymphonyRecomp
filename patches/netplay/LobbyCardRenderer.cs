using System;
using System.Numerics;
using ImGuiNET;
using Recompiled.Ultimate;

namespace Recompiled.Netplay;

/// <summary>
/// Renderer visual dos Cards de Caçadores do Lobby Pré-Jogo (Showcase Co-op estilo Versus/Arcade).
/// Desenha a silhueta estilizada, auras temáticas e animação de respiração (idle) de cada caçador.
/// </summary>
public static class LobbyCardRenderer
{
    public static void DrawHunterCard(
        ImDrawListPtr dl,
        Vector2 pos,
        Vector2 size,
        string playerName,
        string roleText,
        CharacterSelection character,
        bool isReady,
        bool isLocal,
        float time)
    {
        Vector2 cardMin = pos;
        Vector2 cardMax = pos + size;

        // 1. FUNDO E BORDA DO CARD
        uint bgCol = ImGui.ColorConvertFloat4ToU32(new Vector4(0.07f, 0.09f, 0.12f, 0.96f));
        dl.AddRectFilled(cardMin, cardMax, bgCol, 8.0f);

        if (isReady)
        {
            // Borda pulsante verde néon para jogador Pronto
            float pulse = (MathF.Sin(time * 5.0f) + 1.0f) * 0.5f;
            Vector4 bCol = Vector4.Lerp(new Vector4(0.18f, 0.78f, 0.38f, 1f), new Vector4(0.35f, 1.0f, 0.55f, 1f), pulse);
            dl.AddRect(cardMin, cardMax, ImGui.ColorConvertFloat4ToU32(bCol), 8.0f, ImDrawFlags.None, 2.5f);
        }
        else
        {
            // Borda suave em aço/âmbar enquanto escolhe
            dl.AddRect(cardMin, cardMax, ImGui.ColorConvertFloat4ToU32(new Vector4(0.28f, 0.32f, 0.40f, 0.85f)), 8.0f, ImDrawFlags.None, 1.5f);
        }

        // 2. CABEÇALHO DO JOGADOR
        string headerTitle = isLocal ? $"[VOCÊ] {playerName}" : $"[PARCEIRO] {playerName}";
        Vector4 titleCol = isLocal ? new Vector4(0.35f, 0.85f, 1.0f, 1f) : new Vector4(1.0f, 0.78f, 0.28f, 1f);
        dl.AddText(cardMin + new Vector2(12, 8), ImGui.ColorConvertFloat4ToU32(titleCol), headerTitle);

        string roleBadge = $"({roleText})";
        float roleWidth = ImGui.CalcTextSize(roleBadge).X;
        dl.AddText(cardMin + new Vector2(size.X - roleWidth - 12, 8), ImGui.ColorConvertFloat4ToU32(new Vector4(0.6f, 0.65f, 0.75f, 1f)), roleBadge);

        // 3. CAIXA DE PREVIEW GRÁFICA DO SPRITE
        Vector2 boxMin = cardMin + new Vector2(10, 28);
        Vector2 boxSize = new Vector2(size.X - 20, 120);
        Vector2 boxMax = boxMin + boxSize;

        dl.AddRectFilled(boxMin, boxMax, ImGui.ColorConvertFloat4ToU32(new Vector4(0.03f, 0.04f, 0.06f, 1f)), 6.0f);
        dl.AddRect(boxMin, boxMax, ImGui.ColorConvertFloat4ToU32(new Vector4(0.18f, 0.22f, 0.30f, 0.7f)), 6.0f);

        // 4. AURA TEMÁTICA ANIMADA ATRÁS DO CAÇADOR
        Vector2 center = boxMin + boxSize * 0.5f;
        Vector4 auraCol;
        float auraRadius;

        switch (character)
        {
            case CharacterSelection.SaturnMaria:
            case CharacterSelection.PSPMaria:
                auraCol = new Vector4(0.15f, 0.85f, 0.45f, 0.22f);
                auraRadius = 36.0f + MathF.Sin(time * 3.8f) * 4.0f;
                break;
            case CharacterSelection.RichterClassic:
            case CharacterSelection.RichterSaturnCoat:
            case CharacterSelection.RichterBrownCoat:
                auraCol = new Vector4(0.20f, 0.50f, 0.95f, 0.22f);
                auraRadius = 36.0f + MathF.Sin(time * 3.5f) * 4.0f;
                break;
            default: // Alucard
                auraCol = new Vector4(0.75f, 0.15f, 0.20f, 0.22f);
                auraRadius = 36.0f + MathF.Sin(time * 3.2f) * 4.0f;
                break;
        }

        dl.AddCircleFilled(center, auraRadius, ImGui.ColorConvertFloat4ToU32(auraCol));

        // 5. ANIMAÇÃO DE RESPIRAÇÃO (IDLE BOBBING) DO SPRITE REAL
        float bob = MathF.Sin(time * 3.5f + (isLocal ? 0f : 1.7f)) * 3.2f;
        Vector2 cPos = center + new Vector2(0f, bob + 4f);

        uint texId = NetplaySpriteManager.GetHunterSprite((byte)character, isAttacking: false, isJumping: false, out Vector2 sz);
        if (texId != 0)
        {
            float drawH = 64f;
            float drawW = (sz.Y > 0) ? sz.X * (drawH / sz.Y) : 48f;
            Vector2 pMin = new(center.X - drawW * 0.5f, center.Y - drawH * 0.5f + bob);
            Vector2 pMax = new(center.X + drawW * 0.5f, center.Y + drawH * 0.5f + bob);
            dl.AddImage((nint)texId, pMin, pMax);
        }
        else
        {
            switch (character)
            {
                case CharacterSelection.SaturnMaria:
                case CharacterSelection.PSPMaria:
                    DrawMariaSprite(dl, cPos, character, time);
                    break;
                case CharacterSelection.RichterClassic:
                case CharacterSelection.RichterSaturnCoat:
                case CharacterSelection.RichterBrownCoat:
                    DrawRichterSprite(dl, cPos, character, time);
                    break;
                default:
                    DrawAlucardSprite(dl, cPos, time);
                    break;
            }
        }

        // 6. NOME E ARQUÉTIPO DO CAÇADOR
        string charTitle = character switch
        {
            CharacterSelection.RichterClassic => "Richter Belmont (Clássico)",
            CharacterSelection.RichterSaturnCoat => "Richter Belmont (Sobretudo Azul)",
            CharacterSelection.RichterBrownCoat => "Richter Belmont (Sobretudo Couro)",
            CharacterSelection.SaturnMaria => "Maria Renard (Sega Saturn)",
            CharacterSelection.PSPMaria => "Maria Renard (PSP Edition)",
            _ => "Alucard Tepes (Dhampir)"
        };

        string charSubtitle = character switch
        {
            CharacterSelection.RichterClassic => "Chicote Sagrado • Item Crash",
            CharacterSelection.RichterSaturnCoat => "Vampire Killer • Sobretudo Real",
            CharacterSelection.RichterBrownCoat => "Chicote Sagrado • Duster Couro",
            CharacterSelection.SaturnMaria => "4 Deuses Celestiais • Saltos Ágeis",
            CharacterSelection.PSPMaria => "Disparos Velozes • Ataques Rápidos",
            _ => "Espadas Mágicas • Metamorfose"
        };

        dl.AddText(cardMin + new Vector2(12, 154), ImGui.ColorConvertFloat4ToU32(new Vector4(1f, 1f, 1f, 1f)), charTitle);
        dl.AddText(cardMin + new Vector2(12, 172), ImGui.ColorConvertFloat4ToU32(new Vector4(0.65f, 0.70f, 0.80f, 0.9f)), charSubtitle);

        // 7. BADGE INFERIOR DE STATUS DE PRONTIDÃO (READY CHECK)
        Vector2 badgeMin = cardMin + new Vector2(10, size.Y - 28);
        Vector2 badgeMax = badgeMin + new Vector2(size.X - 20, 22);

        if (isReady)
        {
            dl.AddRectFilled(badgeMin, badgeMax, ImGui.ColorConvertFloat4ToU32(new Vector4(0.12f, 0.55f, 0.25f, 0.85f)), 4.0f);
            dl.AddRect(badgeMin, badgeMax, ImGui.ColorConvertFloat4ToU32(new Vector4(0.25f, 0.95f, 0.45f, 1f)), 4.0f);
            string readyText = "[✓ PRONTO PARA A AVENTURA]";
            float txtW = ImGui.CalcTextSize(readyText).X;
            dl.AddText(badgeMin + new Vector2((badgeMax.X - badgeMin.X - txtW) * 0.5f, 3), 0xFFFFFFFF, readyText);
        }
        else
        {
            dl.AddRectFilled(badgeMin, badgeMax, ImGui.ColorConvertFloat4ToU32(new Vector4(0.40f, 0.28f, 0.08f, 0.75f)), 4.0f);
            dl.AddRect(badgeMin, badgeMax, ImGui.ColorConvertFloat4ToU32(new Vector4(0.85f, 0.60f, 0.15f, 0.9f)), 4.0f);
            string choosingText = "[ESCOLHENDO CAÇADOR...]";
            float txtW = ImGui.CalcTextSize(choosingText).X;
            dl.AddText(badgeMin + new Vector2((badgeMax.X - badgeMin.X - txtW) * 0.5f, 3), 0xFFE0E0E0, choosingText);
        }
    }

    private static void DrawAlucardSprite(ImDrawListPtr dl, Vector2 cPos, float time)
    {
        // Capa esvoaçante (exterior escuro, interior carmesim)
        float capeWave = MathF.Sin(time * 3.0f) * 3.5f;
        dl.AddTriangleFilled(cPos + new Vector2(-15 - capeWave, 22), cPos + new Vector2(0, -10), cPos + new Vector2(-8, 25), 0xFF14141E);
        dl.AddTriangleFilled(cPos + new Vector2(15 + capeWave, 22), cPos + new Vector2(0, -10), cPos + new Vector2(8, 25), 0xFF14141E);

        // Forro carmesim
        dl.AddTriangleFilled(cPos + new Vector2(-10, 21), cPos + new Vector2(0, -8), cPos + new Vector2(-3, 23), 0xFF181085);
        dl.AddTriangleFilled(cPos + new Vector2(10, 21), cPos + new Vector2(0, -8), cPos + new Vector2(3, 23), 0xFF181085);

        // Tronco gótico com detalhes dourados
        dl.AddRectFilled(cPos + new Vector2(-5, -8), cPos + new Vector2(5, 10), 0xFF222228, 2f);
        dl.AddLine(cPos + new Vector2(0, -6), cPos + new Vector2(0, 8), 0xFF35B0D4, 1.5f);

        // Rosto pálido e cabelos prateados
        dl.AddCircleFilled(cPos + new Vector2(0, -15), 6.5f, 0xFFE0E5EC);
        dl.AddLine(cPos + new Vector2(-4, -17), cPos + new Vector2(-7, -1), 0xFFD8EEF5, 2.2f);
        dl.AddLine(cPos + new Vector2(4, -17), cPos + new Vector2(7, -1), 0xFFD8EEF5, 2.2f);

        // Espada desembainhada com brilho na ponta
        dl.AddLine(cPos + new Vector2(6, 2), cPos + new Vector2(19, 15), 0xFFE0E0E0, 2.0f);
        dl.AddLine(cPos + new Vector2(4, 4), cPos + new Vector2(8, 0), 0xFF20A0D0, 2.0f);

        float spark = MathF.Sin(time * 6.0f);
        if (spark > 0.3f)
        {
            dl.AddCircleFilled(cPos + new Vector2(19, 15), 2.0f, 0xFFFFFFFF);
        }
    }

    private static void DrawRichterSprite(ImDrawListPtr dl, Vector2 cPos, CharacterSelection character, float time)
    {
        // Fita da bandana branca esvoaçando ao vento
        float ribbon = MathF.Sin(time * 5.5f) * 3.5f;
        dl.AddLine(cPos + new Vector2(-5, -17), cPos + new Vector2(-16 - ribbon, -15), 0xFFFFFFFF, 2f);

        // Cabeça e bandana
        dl.AddCircleFilled(cPos + new Vector2(0, -15), 6.5f, 0xFFD5DDF0);
        dl.AddRectFilled(cPos + new Vector2(-6, -20), cPos + new Vector2(6, -15), 0xFF253555, 2f);
        dl.AddLine(cPos + new Vector2(-6, -16), cPos + new Vector2(6, -16), 0xFFFFFFFF, 1.8f);

        // Túnica ou Sobretudo
        uint tunicCol = character switch
        {
            CharacterSelection.RichterSaturnCoat => 0xFF653010, // Azul marinho
            CharacterSelection.RichterBrownCoat => 0xFF204565,  // Couro marrom
            _ => 0xFFB05020 // Azul Belmont Real
        };

        dl.AddRectFilled(cPos + new Vector2(-7, -8), cPos + new Vector2(7, 11), tunicCol, 2f);
        dl.AddLine(cPos + new Vector2(-7, 4), cPos + new Vector2(7, 4), 0xFF20A0D0, 1.8f);

        // Chicote Sagrado (Vampire Killer) em arco
        dl.AddBezierCubic(
            cPos + new Vector2(7, 4),
            cPos + new Vector2(14, 10),
            cPos + new Vector2(16, 18),
            cPos + new Vector2(22, 16),
            0xFF80C0D0,
            2f
        );

        // Cruz sagrada brilhante sobre o ombro
        Vector2 crossPos = cPos + new Vector2(-13, -9);
        dl.AddLine(crossPos + new Vector2(0, -5), crossPos + new Vector2(0, 5), 0xFF40D0F0, 1.5f);
        dl.AddLine(crossPos + new Vector2(-3, -2), crossPos + new Vector2(3, -2), 0xFF40D0F0, 1.5f);
    }

    private static void DrawMariaSprite(ImDrawListPtr dl, Vector2 cPos, CharacterSelection character, float time)
    {
        // Cabeça e cachos dourados
        dl.AddCircleFilled(cPos + new Vector2(0, -15), 6.5f, 0xFFD8E5F0);
        dl.AddCircleFilled(cPos + new Vector2(-5, -12), 3.5f, 0xFF40D0F5);
        dl.AddCircleFilled(cPos + new Vector2(5, -12), 3.5f, 0xFF40D0F5);

        // Laço de fita no topo da cabeça
        dl.AddTriangleFilled(cPos + new Vector2(-5, -22), cPos + new Vector2(0, -19), cPos + new Vector2(-1, -21), 0xFF8020E0);
        dl.AddTriangleFilled(cPos + new Vector2(5, -22), cPos + new Vector2(0, -19), cPos + new Vector2(1, -21), 0xFF8020E0);

        // Vestido (Esmeralda para Saturn, Carmesim para PSP)
        uint dressCol = character == CharacterSelection.PSPMaria ? 0xFF401080 : 0xFF358525;
        dl.AddTriangleFilled(cPos + new Vector2(-10, 13), cPos + new Vector2(0, -8), cPos + new Vector2(10, 13), dressCol);
        dl.AddRectFilled(cPos + new Vector2(-3, -7), cPos + new Vector2(3, 2), 0xFFF0F5FA, 1f);

        // Pássaro Celestial Companheiro (Suzaku / Pomba Branca) batendo asas
        float birdFlap = MathF.Sin(time * 12.0f) * 4.5f;
        Vector2 bPos = cPos + new Vector2(17, -15 + MathF.Sin(time * 3.5f) * 2.5f);

        dl.AddCircleFilled(bPos, 3.5f, 0xFFFFFFFF);
        dl.AddTriangleFilled(bPos + new Vector2(-3, 0), bPos + new Vector2(0, -4 - birdFlap), bPos + new Vector2(2, 0), 0xFFF0F5FA);
        dl.AddLine(bPos + new Vector2(2, 0), bPos + new Vector2(5, 1), 0xFF20A0F0, 1.5f);
    }
}
