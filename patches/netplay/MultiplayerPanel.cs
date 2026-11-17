using System;
using System.Collections.Generic;
using System.Linq;
using System.Numerics;
using ImGuiNET;
using RecompOne.Runtime.Host.Window;
using Sotn;

namespace Recompiled.Netplay;

public sealed class MultiplayerPanel : IPanel
{
    public string Name => "Multiplayer Co-op & PvP";
    public string TitleKey => "panel.multiplayer";
    public bool IsOpen { get; set; }

    private string _inputIp = "127.0.0.1";
    private int _inputPort = 7777;
    private string _inputName = "Alucard";
    private string _chatInput = "";
    private int _selectedPresetIndex = 0;
    private bool _autoScrollChat = true;

    public void Draw()
    {
        ImGui.SetNextWindowSize(new Vector2(620, 640), ImGuiCond.FirstUseEver);
        bool open = IsOpen;
        if (!ImGui.Begin(this.Title(), ref open))
        {
            IsOpen = open;
            ImGui.End();
            return;
        }

        if (ImGui.BeginTabBar("multiplayer_tabs"))
        {
            // TAB 1: CONEXÃO & LOBBY
            if (ImGui.BeginTabItem("Lobby & Conexão"))
            {
                DrawLobbyTab();
                ImGui.EndTabItem();
            }

            // TAB 2: MAPA CO-OP & RADAR AO VIVO
            if (ImGui.BeginTabItem("Radar & Mapa Co-op"))
            {
                DrawCoopMapTab();
                ImGui.EndTabItem();
            }

            // TAB 3: JOGADORES & CO-OP
            if (ImGui.BeginTabItem("Jogadores & Status"))
            {
                DrawPlayersTab();
                ImGui.EndTabItem();
            }

            // TAB 4: CHAT & AVISOS
            if (ImGui.BeginTabItem("Chat & Avisos"))
            {
                DrawChatTab();
                ImGui.EndTabItem();
            }

            ImGui.EndTabBar();
        }

        IsOpen = open;
        ImGui.End();
    }

    private List<(string Label, string Ip)> GetIpPresetOptions()
    {
        var options = new List<(string Label, string Ip)>
        {
            ("Mesmo PC (127.0.0.1 - Loopback)", "127.0.0.1")
        };

        if (!string.IsNullOrEmpty(NatHelper.LocalIp) && NatHelper.LocalIp != "127.0.0.1")
        {
            options.Add(($"Rede Local / Wi-Fi ({NatHelper.LocalIp})", NatHelper.LocalIp));
        }

        foreach (var adapter in NatHelper.LocalAdapters)
        {
            if (adapter.Ip != "127.0.0.1" && adapter.Ip != NatHelper.LocalIp)
            {
                options.Add(($"{adapter.Description} ({adapter.Ip})", adapter.Ip));
            }
        }

        if (!string.IsNullOrWhiteSpace(NetworkManager.TargetIp) &&
            !options.Exists(o => o.Ip == NetworkManager.TargetIp))
        {
            options.Add(($"Último Utilizado ({NetworkManager.TargetIp})", NetworkManager.TargetIp));
        }

        options.Add(("Digitar Manualmente...", ""));
        return options;
    }

    private void DrawLobbyTab()
    {
        ImGui.TextColored(new Vector4(1f, 0.85f, 0.2f, 1f), "Castlevania Netplay P2P 60Hz:");
        ImGui.Separator();

        ImGui.InputText("Seu Nome", ref _inputName, 32);
        NetworkManager.LocalPlayerName = _inputName;

        ImGui.Spacing();

        if (!NetworkManager.IsConnected)
        {
            // BLOCO 1: HOSPEDAR SESSÃO (HOST)
            ImGui.BeginChild("host_join_child", new Vector2(-1, 410), ImGuiChildFlags.Borders);

            ImGui.TextColored(new Vector4(0.3f, 0.8f, 1f, 1f), "[1] Hospedar Sessão (Host):");
            ImGui.InputInt("Porta UDP", ref _inputPort);

            // Informações de IP Local (LAN / Wi-Fi)
            ImGui.Spacing();
            ImGui.TextColored(new Vector4(0.2f, 1f, 0.4f, 1f), $"IP Local (Wi-Fi / LAN): {NatHelper.LocalIp}");
            ImGui.SameLine();
            if (ImGui.Button("Copiar IP Local"))
            {
                ImGui.SetClipboardText(NatHelper.LocalIp);
                ToastNotifications.ShowText("IP Local Copiado!", $"IP {NatHelper.LocalIp} copiado para a área de transferência!", null, 3.0f);
            }
            ImGui.TextDisabled("-> Para jogar no mesmo Wi-Fi/rede local (ex: Mac e Windows na mesma casa).");

            if (NatHelper.LocalAdapters.Count > 1)
            {
                if (ImGui.TreeNode("Ver outros adaptadores de rede locais"))
                {
                    foreach (var adapter in NatHelper.LocalAdapters)
                    {
                        ImGui.BulletText($"{adapter.Description}: {adapter.Ip} ({adapter.Name})");
                        ImGui.SameLine();
                        if (ImGui.SmallButton($"Copiar##{adapter.Ip}"))
                        {
                            ImGui.SetClipboardText(adapter.Ip);
                            ToastNotifications.ShowText("IP Copiado!", $"{adapter.Ip} copiado!", null, 3.0f);
                        }
                    }
                    ImGui.TreePop();
                }
            }

            // Informações de IP Público (Internet)
            ImGui.Spacing();
            ImGui.TextColored(new Vector4(1f, 0.75f, 0.2f, 1f), $"IP Público (Internet): {NatHelper.PublicIp}");
            ImGui.SameLine();
            if (ImGui.Button("Copiar IP Público"))
            {
                ImGui.SetClipboardText(NatHelper.PublicIp);
                ToastNotifications.ShowText("IP Público Copiado!", $"IP {NatHelper.PublicIp} copiado para a área de transferência!", null, 3.0f);
            }
            ImGui.TextDisabled("-> Para jogar com amigos pela Internet (requer porta 7777 UDP aberta ou UPnP).");

            ImGui.Spacing();
            if (ImGui.Button("Hospedar Sessão (Host)", new Vector2(-1, 32)))
            {
                NetworkManager.StartHost(_inputPort);
            }

            ImGui.Spacing();
            ImGui.Separator();
            ImGui.Spacing();

            // BLOCO 2: CONECTAR A UM AMIGO (CLIENT)
            ImGui.TextColored(new Vector4(0.3f, 0.8f, 1f, 1f), "[2] Conectar a um Servidor / Amigo (Client):");

            // Combo de Seleção Rápida de IP
            var presets = GetIpPresetOptions();
            string[] presetLabels = presets.Select(p => p.Label).ToArray();
            if (_selectedPresetIndex < 0 || _selectedPresetIndex >= presetLabels.Length)
                _selectedPresetIndex = 0;

            if (ImGui.Combo("Selecionar IP Rápido", ref _selectedPresetIndex, presetLabels, presetLabels.Length))
            {
                string chosenIp = presets[_selectedPresetIndex].Ip;
                if (!string.IsNullOrEmpty(chosenIp))
                {
                    _inputIp = chosenIp;
                    NetworkManager.TargetIp = _inputIp;
                }
            }

            ImGui.InputText("IP do Servidor / Amigo", ref _inputIp, 64);
            NetworkManager.TargetIp = _inputIp;

            // Botões de atalho rápido
            if (ImGui.Button("Mesmo PC (127.0.0.1)"))
            {
                _inputIp = "127.0.0.1";
                NetworkManager.TargetIp = _inputIp;
                _selectedPresetIndex = 0;
            }
            ImGui.SameLine();
            if (ImGui.Button("Colar"))
            {
                string clip = ImGui.GetClipboardText();
                if (!string.IsNullOrWhiteSpace(clip))
                {
                    _inputIp = clip.Trim();
                    NetworkManager.TargetIp = _inputIp;
                    ToastNotifications.ShowText("IP Colado!", $"IP {_inputIp} colado da área de transferência!", null, 3.0f);
                }
            }
            if (!string.IsNullOrEmpty(NatHelper.LocalIp) && NatHelper.LocalIp != "127.0.0.1")
            {
                ImGui.SameLine();
                if (ImGui.Button($"IP Local ({NatHelper.LocalIp})"))
                {
                    _inputIp = NatHelper.LocalIp;
                    NetworkManager.TargetIp = _inputIp;
                }
            }

            ImGui.Spacing();
            if (ImGui.Button("Conectar ao Amigo (Client)", new Vector2(-1, 32)))
            {
                NetworkManager.ConnectTo(_inputIp, _inputPort);
            }

            ImGui.EndChild();
        }
        else
        {
            ImGui.BeginChild("connected_status_child", new Vector2(-1, 160), ImGuiChildFlags.Borders);
            ImGui.TextColored(new Vector4(0.2f, 1f, 0.3f, 1f), "[STATUS: CONECTADO]");
            ImGui.Text($"Modo de Conexão: {NetworkManager.Role}");
            ImGui.Text($"Parceiro Remoto: {NetworkManager.RemotePlayerName}");

            Vector4 pingColor = NetworkManager.PingMs < 50 ? new Vector4(0.2f, 1f, 0.3f, 1f) :
                                NetworkManager.PingMs < 120 ? new Vector4(1f, 0.85f, 0.2f, 1f) :
                                new Vector4(1f, 0.3f, 0.3f, 1f);
            ImGui.TextColored(pingColor, $"Latência (Ping): {NetworkManager.PingMs} ms");

            ImGui.Spacing();
            if (ImGui.Button("Desconectar da Sessão", new Vector2(-1, 32)))
            {
                NetworkManager.Stop();
            }
            ImGui.EndChild();
        }

        ImGui.Spacing();
        ImGui.TextColored(new Vector4(0.8f, 0.8f, 0.8f, 1f), "Instruções Rápidas:");
        ImGui.TextWrapped("• No Mesmo Computador: O Host abre em 7777 e o Cliente usa 'Mesmo PC (127.0.0.1)'.");
        ImGui.TextWrapped("• Na Mesma Rede / Wi-Fi (ex: Mac e Windows): O Host copia o 'IP Local' e o Cliente digita/cola esse IP.");
        ImGui.TextWrapped("• Pela Internet: O Host copia o 'IP Público' (ou usem redes virtuais como Tailscale ou ZeroTier).");
    }

    private void DrawCoopMapTab()
    {
        ImGui.TextColored(new Vector4(1f, 0.85f, 0.2f, 1f), "Radar do Castelo em Tempo Real:");
        ImGui.Separator();

        // Legenda de Cores
        ImGui.Text("Legenda:");
        ImGui.SameLine();
        ImGui.TextColored(new Vector4(0.2f, 0.6f, 1f, 1f), "[x] Você");
        ImGui.SameLine();
        ImGui.TextColored(new Vector4(1f, 0.6f, 0.1f, 1f), "[x] Amigo");
        ImGui.SameLine();
        ImGui.TextColored(new Vector4(0.2f, 1f, 0.4f, 1f), "[x] Ambos");
        ImGui.SameLine();
        ImGui.TextColored(new Vector4(1f, 1f, 1f, 1f), "(o) Sua Posição");
        ImGui.SameLine();
        ImGui.TextColored(new Vector4(1f, 0.2f, 0.2f, 1f), "(^) Amigo");

        ImGui.Spacing();

        // Canvas de renderização do mapa
        Vector2 canvasPos = ImGui.GetCursorScreenPos();
        Vector2 canvasSize = new Vector2(512, 280);
        var drawList = ImGui.GetWindowDrawList();

        drawList.AddRectFilled(canvasPos, canvasPos + canvasSize, ImGui.ColorConvertFloat4ToU32(new Vector4(0.05f, 0.05f, 0.08f, 1f)));
        drawList.AddRect(canvasPos, canvasPos + canvasSize, ImGui.ColorConvertFloat4ToU32(new Vector4(0.3f, 0.3f, 0.4f, 1f)));

        float tileSizeX = canvasSize.X / 64f; // 8px
        float tileSizeY = canvasSize.Y / 32f; // 8.75px

        // Desenhar blocos visitados
        for (int y = 0; y < 32; y++)
        {
            for (int x = 0; x < 64; x++)
            {
                int index = y * 64 + x;
                if (index >= WorldStateSync.RemoteMapBitset.Length) break;

                bool localVisited = (WorldStateSync.LocalMapBitset[index] != 0);
                bool remoteVisited = (WorldStateSync.RemoteMapBitset[index] != 0);

                if (!localVisited && !remoteVisited) continue;

                Vector2 pMin = canvasPos + new Vector2(x * tileSizeX, y * tileSizeY);
                Vector2 pMax = pMin + new Vector2(tileSizeX - 1, tileSizeY - 1);

                Vector4 tileColor = (localVisited && remoteVisited) ? new Vector4(0.2f, 1f, 0.4f, 0.85f) :
                                    localVisited ? new Vector4(0.2f, 0.6f, 1f, 0.85f) :
                                    new Vector4(1f, 0.6f, 0.1f, 0.85f);

                drawList.AddRectFilled(pMin, pMax, ImGui.ColorConvertFloat4ToU32(tileColor));
            }
        }

        // Desenhar indicador da posição atual do jogador local
        int localX = Math.Clamp(Stages.RoomX, 0, 63);
        int localY = Math.Clamp(Stages.RoomY, 0, 31);
        Vector2 localPos = canvasPos + new Vector2(localX * tileSizeX + tileSizeX * 0.5f, localY * tileSizeY + tileSizeY * 0.5f);
        drawList.AddCircleFilled(localPos, 5f, ImGui.ColorConvertFloat4ToU32(new Vector4(1f, 1f, 1f, 1f)));
        drawList.AddCircle(localPos, 6f, ImGui.ColorConvertFloat4ToU32(new Vector4(0.2f, 0.8f, 1f, 1f)), 12, 2f);

        // Desenhar indicador da posição atual do jogador remoto
        if (NetworkManager.IsConnected && RemotePuppet.IsActive)
        {
            int remoteX = Math.Clamp((int)RemotePuppet.RoomX, 0, 63);
            int remoteY = Math.Clamp((int)RemotePuppet.RoomY, 0, 31);
            Vector2 remPos = canvasPos + new Vector2(remoteX * tileSizeX + tileSizeX * 0.5f, remoteY * tileSizeY + tileSizeY * 0.5f);
            drawList.AddTriangleFilled(
                remPos + new Vector2(0, -6),
                remPos + new Vector2(-5, 5),
                remPos + new Vector2(5, 5),
                ImGui.ColorConvertFloat4ToU32(new Vector4(1f, 0.2f, 0.2f, 1f))
            );
            drawList.AddText(remPos + new Vector2(7, -6), ImGui.ColorConvertFloat4ToU32(new Vector4(1f, 0.85f, 0.2f, 1f)), NetworkManager.RemotePlayerName);
        }

        ImGui.Dummy(canvasSize);

        ImGui.Spacing();
        if (NetworkManager.IsConnected && RemotePuppet.IsActive)
        {
            if (ImGui.Button("Teleportar até o Amigo (Buddy Warp)", new Vector2(-1, 35)))
            {
                NetworkManager.RequestBuddyWarp();
            }
        }
    }

    private void DrawPlayersTab()
    {
        if (!NetworkManager.IsConnected)
        {
            ImGui.TextColored(new Vector4(1f, 0.5f, 0.2f, 1f), "Você precisa estar conectado a uma sessão multiplayer para ver os jogadores.");
            return;
        }

        ImGui.TextColored(new Vector4(0.3f, 0.8f, 1f, 1f), "Jogadores no Castelo:");
        ImGui.Separator();

        // Local Player
        string localChar = Player.IsMaria ? "Maria Renard" : (Player.IsRichter ? "Richter Belmont" : "Alucard");
        ImGui.Text($"[Você] {NetworkManager.LocalPlayerName} ({localChar}):");
        ImGui.ProgressBar((float)Player.Hp / Math.Max(1, (int)Player.HpMax), new Vector2(240, 18), $"HP: {Player.Hp}/{Player.HpMax}");
        ImGui.TextDisabled($"Área Atual: {Stages.Current} (Sala: {Stages.RoomX}, {Stages.RoomY})");

        ImGui.Separator();

        // Remote Player
        string remoteChar = RemotePuppet.Character switch
        {
            1 => "Richter Belmont",
            2 => "Maria Renard",
            _ => "Alucard"
        };
        ImGui.Text($"[Parceiro] {NetworkManager.RemotePlayerName} ({remoteChar}):");
        ImGui.ProgressBar((float)RemotePuppet.CurHp / Math.Max(1, (int)RemotePuppet.MaxHp), new Vector2(240, 18), $"HP: {RemotePuppet.CurHp}/{RemotePuppet.MaxHp}");
        ImGui.TextDisabled($"Área Atual: {(Stage)RemotePuppet.StageId} (Sala: {RemotePuppet.RoomX}, {RemotePuppet.RoomY})");

        ImGui.Spacing();

        if (ImGui.Button("Teleportar até o Amigo (Buddy Warp)", new Vector2(-1, 35)))
        {
            NetworkManager.RequestBuddyWarp();
        }

        if (PvPManager.IsPvPActive)
        {
            ImGui.Separator();
            ImGui.TextColored(new Vector4(1f, 0.3f, 0.3f, 1f), "DUELO NO COLISEU ATIVO!");
            ImGui.Text($"Placar: {NetworkManager.LocalPlayerName} {PvPManager.LocalScore} x {PvPManager.RemoteScore} {NetworkManager.RemotePlayerName}");
            ImGui.SameLine();
            if (ImGui.SmallButton("Zerar Placar"))
            {
                PvPManager.ResetScores();
            }
        }
    }

    private void DrawChatTab()
    {
        ImGui.TextColored(new Vector4(0.3f, 0.8f, 1f, 1f), "Chat da Sessão Multiplayer:");
        ImGui.Separator();

        // 1. ÁREA DE HISTÓRICO DE MENSAGENS (Scrollable Log Box)
        ImGui.BeginChild("chat_history_child", new Vector2(-1, 310), ImGuiChildFlags.Borders);
        if (NetworkManager.ChatHistory.Count == 0)
        {
            ImGui.Spacing();
            ImGui.TextDisabled("Nenhuma mensagem ainda. Digite abaixo ou use os avisos rápidos para conversar!");
        }
        else
        {
            lock (NetworkManager.ChatHistory)
            {
                foreach (var entry in NetworkManager.ChatHistory)
                {
                    bool isLocal = entry.Sender == NetworkManager.LocalPlayerName;
                    Vector4 nameColor = isLocal ? new Vector4(0.3f, 0.85f, 1f, 1f) : new Vector4(1f, 0.75f, 0.2f, 1f);

                    ImGui.TextDisabled($"[{entry.Time}] ");
                    ImGui.SameLine();
                    ImGui.TextColored(nameColor, $"{entry.Sender}: ");
                    ImGui.SameLine();
                    ImGui.TextWrapped(entry.Message);
                }
            }

            if (_autoScrollChat)
            {
                ImGui.SetScrollHereY(1.0f);
                _autoScrollChat = false;
            }
        }
        ImGui.EndChild();

        ImGui.Spacing();

        // 2. CAMPO DE DIGITAÇÃO E BOTÃO ENVIAR (suporta tecla ENTER!)
        ImGui.SetNextItemWidth(ImGui.GetContentRegionAvail().X - 85);
        bool sendPressed = ImGui.InputTextWithHint("##chat_input", "Digite sua mensagem... (Enter para enviar)", ref _chatInput, 128, ImGuiInputTextFlags.EnterReturnsTrue);
        ImGui.SameLine();
        if ((sendPressed || ImGui.Button("Enviar", new Vector2(75, 0))) && !string.IsNullOrWhiteSpace(_chatInput))
        {
            SendChatMessage(_chatInput);
            _chatInput = "";
            _autoScrollChat = true;
        }

        ImGui.Spacing();
        ImGui.Separator();

        // 3. AVISOS RÁPIDOS
        ImGui.Text("Avisos Rápidos:");
        string[] quicks = ["Preciso de ajuda!", "Chefe encontrado!", "Achei uma passagem secreta!", "Teleporte até mim!", "Vamos duelar no Coliseu!"];
        foreach (var q in quicks)
        {
            if (ImGui.Button(q))
            {
                SendChatMessage(q);
                _autoScrollChat = true;
            }
            ImGui.SameLine();
        }
        ImGui.NewLine();
    }

    private void SendChatMessage(string msg)
    {
        if (!NetworkManager.IsConnected)
        {
            ToastNotifications.ShowText("Chat", "Conecte-se a uma sessão multiplayer para enviar mensagens.", null, 3.0f);
            return;
        }

        byte[] payload = NetworkPacket.CreateChat(NetworkManager.LocalPlayerName, msg);
        NetworkManager.SendPacket(new NetworkPacket(PacketOpCode.ChatMessage, payload));

        string time = DateTime.Now.ToString("HH:mm");
        NetworkManager.AddChatMessage(NetworkManager.LocalPlayerName, msg, time);
        ToastNotifications.ShowText("[Você]", msg, null, 3.0f);
    }
}
