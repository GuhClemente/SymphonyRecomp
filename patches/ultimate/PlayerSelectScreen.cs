using System;
using System.IO;
using System.Numerics;
using ImGuiNET;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Events;
using RecompOne.Runtime.Host;
using RecompOne.Runtime.Host.Window;
using RecompOne.Runtime.Memory;
using Recompiled.Netplay;
using Sotn;

namespace Recompiled.Ultimate;

/// <summary>
/// Tela clássica e interativa de Seleção de Personagem (Player Select) do SOTN Ultimate.
/// Reproduz fielmente a estética gótica da Versão Ultimate do PS1 (Renan Bianchi):
/// Molduras filigranadas em turquesa/ouro, retratos nobres de Richter, Alucard e Maria,
/// e a caixa dourada de configuração (Version: Saturn/PSP, Balance: PS1/Saturn, Dub: PSP/JP).
/// </summary>
public static class PlayerSelectScreen
{
    public static bool IsOpen = false;
    private static bool _initialized = false;
    private static int _selectedCard = 1; // 0 = Richter, 1 = Alucard, 2 = Maria
    private static int _mariaRow = 0;     // 0 = Version, 1 = Balance, 2 = Dub
    private static int _richterCostumeIndex = 0; // 0 = Classic, 1 = SaturnCoat, 2 = BrownCoat

    // Texturas de alta fidelidade
    private static uint _texHeader;
    private static uint _texRichter;
    private static uint _texAlucard;
    private static uint _texMaria;
    private static uint _texConfigBox;
    private static Vector2 _szHeader;
    private static Vector2 _szRichter;
    private static Vector2 _szAlucard;
    private static Vector2 _szMaria;
    private static Vector2 _szConfigBox;
    private static bool _texturesLoaded = false;

    // Cores temáticas fiéis ao PS1 SotN Ultimate
    private static readonly uint ColBgDark = ImGui.ColorConvertFloat4ToU32(new Vector4(0.04f, 0.05f, 0.09f, 0.94f));
    private static readonly uint ColGold = ImGui.ColorConvertFloat4ToU32(new Vector4(0.95f, 0.80f, 0.32f, 1.0f));
    private static readonly uint ColGoldBorder = ImGui.ColorConvertFloat4ToU32(new Vector4(0.85f, 0.68f, 0.22f, 0.95f));
    private static readonly uint ColCyanBorder = ImGui.ColorConvertFloat4ToU32(new Vector4(0.25f, 0.85f, 0.90f, 0.95f));
    private static readonly uint ColActiveBlue = ImGui.ColorConvertFloat4ToU32(new Vector4(0.20f, 0.45f, 0.88f, 0.95f));
    private static readonly uint ColInactiveBtn = ImGui.ColorConvertFloat4ToU32(new Vector4(0.10f, 0.12f, 0.18f, 0.90f));

    public static void Initialize()
    {
        if (_initialized) return;

        OutputPanel.OnRenderGameOverlay += Render;

        Event.AddListener<KeyboardEvent>(e =>
        {
            if (!e.Pressed || e.Repeat) return;

            // Tecla 'P' ou 'F3' abre/fecha o seletor visual
            if (e.Key == (int)Silk.NET.Input.Key.P || e.Key == (int)Silk.NET.Input.Key.F3)
            {
                IsOpen = !IsOpen;
                if (IsOpen) SyncFromOptions();
            }
            else if (IsOpen)
            {
                HandleInput(e.Key);
            }
        });

        _initialized = true;
    }

    private static void EnsureTextures()
    {
        if (_texturesLoaded) return;
        _texturesLoaded = true;

        _texHeader = LoadTexture("assets/ui/charsel/charsel_header.png", out _szHeader);
        _texRichter = LoadTexture("assets/ui/charsel/charsel_card_richter.png", out _szRichter);
        _texAlucard = LoadTexture("assets/ui/charsel/charsel_card_alucard.png", out _szAlucard);
        _texMaria = LoadTexture("assets/ui/charsel/charsel_card_maria.png", out _szMaria);
        _texConfigBox = LoadTexture("assets/ui/charsel/charsel_config_box.png", out _szConfigBox);
    }

    private static uint LoadTexture(string relativePath, out Vector2 size)
    {
        size = Vector2.Zero;
        try
        {
            string fullPath = Path.Combine(AppContext.BaseDirectory, relativePath);
            if (!File.Exists(fullPath)) fullPath = relativePath;
            if (!File.Exists(fullPath)) return 0;

            byte[] bytes = File.ReadAllBytes(fullPath);
            var img = RgbaImage.Load(bytes);
            uint texId = HostWindow.UploadTexture(img.Pixels, img.Width, img.Height);
            if (texId != 0)
            {
                size = new Vector2(img.Width, img.Height);
                return texId;
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[PlayerSelectScreen] Erro ao carregar {relativePath}: {ex.Message}");
        }
        return 0;
    }

    private static void SyncFromOptions()
    {
        switch (UltimateOptions.CharacterMode)
        {
            case CharacterSelection.RichterClassic:
                _selectedCard = 0;
                _richterCostumeIndex = 0;
                break;
            case CharacterSelection.RichterSaturnCoat:
                _selectedCard = 0;
                _richterCostumeIndex = 1;
                break;
            case CharacterSelection.RichterBrownCoat:
                _selectedCard = 0;
                _richterCostumeIndex = 2;
                break;
            case CharacterSelection.SaturnMaria:
            case CharacterSelection.PSPMaria:
                _selectedCard = 2;
                break;
            default:
                _selectedCard = 1; // Alucard
                break;
        }
    }

    public static void Open(int initialCard = -1)
    {
        IsOpen = true;
        if (initialCard >= 0 && initialCard <= 2)
            _selectedCard = initialCard;
        else
            SyncFromOptions();
    }

    public static void Close()
    {
        IsOpen = false;
    }

    private static void HandleInput(int key)
    {
        switch ((Silk.NET.Input.Key)key)
        {
            case Silk.NET.Input.Key.Left:
                if (_selectedCard == 2)
                {
                    // Na Maria, Left/Right pode navegar opções da caixa de configuração
                    ToggleConfigRow(_mariaRow, left: true);
                }
                else if (_selectedCard == 0)
                {
                    if (_richterCostumeIndex > 0) _richterCostumeIndex--;
                    else _selectedCard = 0;
                }
                else if (_selectedCard > 0)
                {
                    _selectedCard--;
                }
                break;

            case Silk.NET.Input.Key.Right:
                if (_selectedCard == 2)
                {
                    ToggleConfigRow(_mariaRow, left: false);
                }
                else if (_selectedCard == 0)
                {
                    if (_richterCostumeIndex < 2) _richterCostumeIndex++;
                    else if (_selectedCard < 2) _selectedCard++;
                }
                else if (_selectedCard < 2)
                {
                    _selectedCard++;
                }
                break;

            case Silk.NET.Input.Key.Up:
                if (_selectedCard == 2 && _mariaRow > 0) _mariaRow--;
                else if (_selectedCard == 0 && _richterCostumeIndex > 0) _richterCostumeIndex--;
                else if (_selectedCard > 0 && _selectedCard != 2) _selectedCard--;
                break;

            case Silk.NET.Input.Key.Down:
                if (_selectedCard == 2 && _mariaRow < 2) _mariaRow++;
                else if (_selectedCard == 0 && _richterCostumeIndex < 2) _richterCostumeIndex++;
                else if (_selectedCard < 2 && _selectedCard != 2) _selectedCard++;
                break;

            case Silk.NET.Input.Key.Enter:
            case Silk.NET.Input.Key.Space:
                ConfirmSelection();
                break;

            case Silk.NET.Input.Key.Escape:
                Close();
                break;
        }
    }

    private static void ToggleConfigRow(int row, bool left)
    {
        switch (row)
        {
            case 0: // Version (Saturn / PSP)
                UltimateOptions.CharacterMode = left ? CharacterSelection.SaturnMaria : CharacterSelection.PSPMaria;
                UltimateOptions.Save();
                break;
            case 1: // Balance (PS1 / Saturn)
                UltimateOptions.Balance = left ? SaturnBalanceMode.PS1 : SaturnBalanceMode.Saturn;
                UltimateOptions.Save();
                if (Game.Available && RecompOne.Runtime.Runtime.Mem != null)
                    SaturnBalance.ForceReapply(RecompOne.Runtime.Runtime.Mem);
                break;
            case 2: // Dub (PSP / JP)
                UltimateOptions.Dub = left ? DubMode.PSPEnglish : DubMode.Japanese;
                UltimateOptions.Save();
                break;
        }
    }

    public static void ConfirmSelection()
    {
        CharacterSelection target = _selectedCard switch
        {
            0 => _richterCostumeIndex switch
            {
                1 => CharacterSelection.RichterSaturnCoat,
                2 => CharacterSelection.RichterBrownCoat,
                _ => CharacterSelection.RichterClassic
            },
            2 => UltimateOptions.CharacterMode == CharacterSelection.PSPMaria
                ? CharacterSelection.PSPMaria
                : CharacterSelection.SaturnMaria,
            _ => CharacterSelection.Alucard
        };

        UltimateOptions.CharacterMode = target;
        UltimateOptions.Save();

        // Se estiver em jogo ativo, efetua a troca imediata
        if (Game.Available && Game.InGame && Player.HasControl)
        {
            MariaCharacter.RequestCharacterSwitch(target);
            ToastNotifications.ShowText(
                "Player Select",
                $"Personagem escolhido: {target}",
                null,
                3.0f);
        }

        Close();
    }

    public static void Render(Vector2 imageMin, Vector2 imageSize)
    {
        if (!IsOpen) return;

        EnsureTextures();

        // Renderiza no topo absoluto de toda a janela da aplicação (1280x720 ou Widescreen)
        var vp = ImGui.GetMainViewport();
        Vector2 screenMin = vp.Pos;
        Vector2 screenSize = vp.Size;
        Vector2 screenMax = screenMin + screenSize;
        var dl = ImGui.GetForegroundDrawList();
        float time = (float)ImGui.GetTime();

        // 1. FUNDO TRANSLÚCIDO E VIGNETTE GÓTICA TOTAL
        dl.AddRectFilled(screenMin, screenMax, ColBgDark);

        // Moldura externa nobre da tela
        dl.AddRect(screenMin + new Vector2(10, 10), screenMax - new Vector2(10, 10), ColGoldBorder, 6.0f, ImDrawFlags.None, 2.5f);
        dl.AddRect(screenMin + new Vector2(14, 14), screenMax - new Vector2(14, 14), ColCyanBorder, 4.0f, ImDrawFlags.None, 1.0f);

        // 2. CABEÇALHO GÓTICO: "Player Select" • "Select a player"
        Vector2 titlePos = screenMin + new Vector2(40, 28);
        if (_texHeader != 0)
        {
            float hdrH = 46f;
            float hdrW = _szHeader.Y > 0 ? _szHeader.X * (hdrH / _szHeader.Y) : 210f;
            dl.AddImage((nint)_texHeader, titlePos, titlePos + new Vector2(hdrW, hdrH));
            Vector2 subPos = titlePos + new Vector2(hdrW + 24f, 14f);
            dl.AddText(subPos, 0xFFFFFFFF, "Select a player");
        }
        else
        {
            dl.AddText(titlePos + new Vector2(2, 2), 0xFF000000, "Player Select");
            dl.AddText(titlePos, ColGold, "Player Select");
            Vector2 subPos = titlePos + new Vector2(190, 4);
            dl.AddText(subPos, 0xFFE0E0E0, "Select a player");
        }

        // 3. DIMENSÕES DOS CARDS DE PERSONAGEM
        bool showMariaConfig = (_selectedCard == 2);
        float cardW = MathF.Min(220f, (screenSize.X - 100f) / (showMariaConfig ? 4.2f : 3.4f));
        float cardH = cardW * 1.80f; // Proporção áurea dos retratos do Saturn
        if (cardH > screenSize.Y - 170f)
        {
            cardH = screenSize.Y - 170f;
            cardW = cardH / 1.80f;
        }

        float gap = 24f;
        float totalCardsW = cardW * 3 + gap * 2;
        float startX = showMariaConfig
            ? screenMin.X + 45f
            : screenMin.X + (screenSize.X - totalCardsW) * 0.5f;
        float cardY = screenMin.Y + 90f;

        // Desenhar os 3 Cards com Texturas Oficiais
        DrawCard(dl, new Vector2(startX, cardY), new Vector2(cardW, cardH), 0, "RICHTER", _texRichter, time);
        DrawCard(dl, new Vector2(startX + cardW + gap, cardY), new Vector2(cardW, cardH), 1, "ALUCARD", _texAlucard, time);
        DrawCard(dl, new Vector2(startX + (cardW + gap) * 2, cardY), new Vector2(cardW, cardH), 2, "MARIA", _texMaria, time);

        // 4. CAIXA DE CONFIGURAÇÃO DA MARIA (Idêntica à Foto 3 do Vídeo)
        if (showMariaConfig)
        {
            float configX = startX + (cardW + gap) * 3 + 15f;
            float configW = MathF.Min(400f, screenMax.X - configX - 35f);
            if (configW > 240f)
            {
                DrawMariaConfigBox(dl, new Vector2(configX, cardY + 10f), new Vector2(configW, cardH - 20f), time);
            }
        }

        // 5. RODAPÉ INFORMATIVO: Controles e Assinatura
        Vector2 footPos = new Vector2(screenMin.X + 40f, screenMax.Y - 45f);
        dl.AddText(footPos + new Vector2(1, 1), 0xFF000000, "Versão Ultimate do PS1");
        dl.AddText(footPos, 0xFFB0B8C8, "Versão Ultimate do PS1");

        string controlsPrompt = "<- -> Selecionar   |   [ENTER / X] Decidir   |   [ESC / Triangle] Cancelar";
        float cW = ImGui.CalcTextSize(controlsPrompt).X;
        Vector2 cPromptPos = new Vector2(screenMax.X - cW - 40f, footPos.Y);
        dl.AddText(cPromptPos + new Vector2(1, 1), 0xFF000000, controlsPrompt);
        dl.AddText(cPromptPos, ColGold, controlsPrompt);
    }

    private static void DrawCard(
        ImDrawListPtr dl,
        Vector2 pos,
        Vector2 size,
        int cardIndex,
        string name,
        uint texId,
        float time)
    {
        bool isSelected = (_selectedCard == cardIndex);
        Vector2 cardMin = pos;
        Vector2 cardMax = pos + size;

        // Fundo do card
        uint cardBg = isSelected
            ? ImGui.ColorConvertFloat4ToU32(new Vector4(0.08f, 0.12f, 0.22f, 0.96f))
            : ImGui.ColorConvertFloat4ToU32(new Vector4(0.05f, 0.07f, 0.12f, 0.88f));
        dl.AddRectFilled(cardMin, cardMax, cardBg, 6.0f);

        // Efeito de pulso dourado se selecionado
        uint borderCol = isSelected ? ColGoldBorder : ColCyanBorder;
        float borderThickness = isSelected ? 3.5f : 1.5f;

        if (isSelected)
        {
            float pulse = (MathF.Sin(time * 4.5f) + 1.0f) * 0.5f;
            Vector4 glowCol = Vector4.Lerp(new Vector4(0.85f, 0.68f, 0.22f, 1f), new Vector4(1.0f, 0.95f, 0.45f, 1f), pulse);
            borderCol = ImGui.ColorConvertFloat4ToU32(glowCol);
        }

        // Se tiver a textura oficial com a moldura e retrato, desenha ela diretamente
        if (texId != 0)
        {
            float bob = isSelected ? MathF.Sin(time * 3.8f) * 3.5f : 0f;
            Vector2 imgMin = new Vector2(cardMin.X + 6f, cardMin.Y + 6f + bob);
            Vector2 imgMax = new Vector2(cardMax.X - 6f, cardMax.Y - 36f + bob);
            dl.AddImage((nint)texId, imgMin, imgMax);
        }

        dl.AddRect(cardMin, cardMax, borderCol, 6.0f, ImDrawFlags.None, borderThickness);

        // Cruz ou adorno no topo da moldura
        Vector2 crestTop = new Vector2(cardMin.X + size.X * 0.5f, cardMin.Y - 6f);
        dl.AddRectFilled(crestTop - new Vector2(8, 6), crestTop + new Vector2(8, 2), borderCol, 2.0f);
        dl.AddRectFilled(crestTop - new Vector2(2, 10), crestTop + new Vector2(2, 4), borderCol, 1.0f);

        // Subtítulo de Estilo/Traje no rodapé do card
        string subInfo = cardIndex switch
        {
            0 => _richterCostumeIndex switch
            {
                1 => "< Saturn Azul >",
                2 => "< Duster Couro >",
                _ => "< Clássico PS1 >"
            },
            1 => "Dhampir • Espadas",
            _ => UltimateOptions.CharacterMode == CharacterSelection.PSPMaria ? "PSP Chronicles" : "Sega Saturn"
        };
        Vector2 subPos = new Vector2(cardMin.X + size.X * 0.5f - ImGui.CalcTextSize(subInfo).X * 0.5f, cardMax.Y - 26f);
        dl.AddText(subPos, isSelected ? 0xFF80D8FF : 0xFF9098A8, subInfo);
    }

    private static void DrawMariaConfigBox(ImDrawListPtr dl, Vector2 pos, Vector2 size, float time)
    {
        Vector2 boxMin = pos;
        Vector2 boxMax = pos + size;

        // Fundo e moldura dourada idêntica à Foto 3
        dl.AddRectFilled(boxMin, boxMax, ColBgDark, 6.0f);
        dl.AddRect(boxMin, boxMax, ColGoldBorder, 6.0f, ImDrawFlags.None, 2.5f);

        // Linhas decorativas filigranadas internas
        float contentW = size.X - 36f;
        float rowSpacing = (size.Y - 40f) / 3f;
        float startY = boxMin.Y + 22f;

        // 1. LINHA: VERSION (Saturn / PSP)
        DrawConfigOptionRow(dl, new Vector2(boxMin.X + 18f, startY), contentW, "Version",
            "Saturn", "PSP",
            isSelectedRow: (_mariaRow == 0),
            isOptionAActive: (UltimateOptions.CharacterMode != CharacterSelection.PSPMaria),
            onToggle: isA =>
            {
                _mariaRow = 0;
                ToggleConfigRow(0, isA);
            });

        // 2. LINHA: BALANCE (PS1 / Saturn)
        startY += rowSpacing;
        DrawConfigOptionRow(dl, new Vector2(boxMin.X + 18f, startY), contentW, "Balance",
            "PS1", "Saturn",
            isSelectedRow: (_mariaRow == 1),
            isOptionAActive: (UltimateOptions.Balance == SaturnBalanceMode.PS1),
            onToggle: isA =>
            {
                _mariaRow = 1;
                ToggleConfigRow(1, isA);
            });

        // 3. LINHA: DUB (PSP / JP)
        startY += rowSpacing;
        DrawConfigOptionRow(dl, new Vector2(boxMin.X + 18f, startY), contentW, "Dub",
            "PSP", "JP",
            isSelectedRow: (_mariaRow == 2),
            isOptionAActive: (UltimateOptions.Dub != DubMode.Japanese),
            onToggle: isA =>
            {
                _mariaRow = 2;
                ToggleConfigRow(2, isA);
            });
    }

    private static void DrawConfigOptionRow(
        ImDrawListPtr dl,
        Vector2 pos,
        float width,
        string label,
        string optionA,
        string optionB,
        bool isSelectedRow,
        bool isOptionAActive,
        Action<bool> onToggle)
    {
        // Rótulo da Opção (ex: "Version", "Balance", "Dub")
        uint labelCol = isSelectedRow ? ColGold : 0xFFD0D8E8;
        dl.AddText(pos, labelCol, label);

        float btnW = (width - 16f) * 0.5f;
        float btnH = 34f;
        Vector2 btnAPos = pos + new Vector2(0f, 22f);
        Vector2 btnBPos = pos + new Vector2(btnW + 16f, 22f);

        // Botão Opção A
        uint colBtnA = isOptionAActive ? ColActiveBlue : ColInactiveBtn;
        uint borderA = isOptionAActive ? ColGold : ImGui.ColorConvertFloat4ToU32(new Vector4(0.3f, 0.35f, 0.45f, 0.8f));
        dl.AddRectFilled(btnAPos, btnAPos + new Vector2(btnW, btnH), colBtnA, 4.0f);
        dl.AddRect(btnAPos, btnAPos + new Vector2(btnW, btnH), borderA, 4.0f, ImDrawFlags.None, isOptionAActive ? 2.0f : 1.0f);

        Vector2 txtPosA = btnAPos + new Vector2((btnW - ImGui.CalcTextSize(optionA).X) * 0.5f, 8f);
        dl.AddText(txtPosA, isOptionAActive ? 0xFFFFFFFF : 0xFFA0A8B8, optionA);

        // Botão Opção B
        uint colBtnB = !isOptionAActive ? ColActiveBlue : ColInactiveBtn;
        uint borderB = !isOptionAActive ? ColGold : ImGui.ColorConvertFloat4ToU32(new Vector4(0.3f, 0.35f, 0.45f, 0.8f));
        dl.AddRectFilled(btnBPos, btnBPos + new Vector2(btnW, btnH), colBtnB, 4.0f);
        dl.AddRect(btnBPos, btnBPos + new Vector2(btnW, btnH), borderB, 4.0f, ImDrawFlags.None, !isOptionAActive ? 2.0f : 1.0f);

        Vector2 txtPosB = btnBPos + new Vector2((btnW - ImGui.CalcTextSize(optionB).X) * 0.5f, 8f);
        dl.AddText(txtPosB, !isOptionAActive ? 0xFFFFFFFF : 0xFFA0A8B8, optionB);
    }
}
