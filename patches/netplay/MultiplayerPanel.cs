using System;
using System.Collections.Generic;
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
    private readonly List<string> _chatHistory = [];

    public void Draw()
    {
        ImGui.SetNextWindowSize(new Vector2(560, 560), ImGuiCond.FirstUseEver);
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
            if (ImGui.BeginTabItem("🗺️ Mapa Co-op & Radar"))
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

    private void DrawLobbyTab()
    {
        ImGui.TextColored(new Vector4(1f, 0.85f, 0.2f, 1f), "Castlevania Netplay P2P 60Hz:");
        ImGui.Separator();

        ImGui.InputText("Seu Nome", ref _inputName, 32);
        NetworkManager.LocalPlayerName = _inputName;

        ImGui.Spacing();

        if (!NetworkManager.IsConnected)
        {
            ImGui.BeginChild("host_join_child", new Vector2(-1, 230), ImGuiChildFlags.Borders);
            ImGui.TextColored(new Vector4(0.3f, 0.8f, 1f, 1f), "Opções de Conexão Online:");

            ImGui.InputInt("Porta", ref _inputPort);
            if (ImGui.Button("🛡️ Hospedar Sessão (Host)"))
            {
                NetworkManager.StartHost(_inputPort);
            }

            ImGui.SameLine();
            ImGui.TextDisabled($"(Seu IP Público: {NatHelper.PublicIp})");

            if (ImGui.Button("📋 Copiar Meu IP para Enviar ao Amigo"))
            {
                ImGui.SetClipboardText(NatHelper.PublicIp);
                ToastNotifications.ShowText("📋 IP Copiado!", $"IP {NatHelper.PublicIp} copiado para a área de transferência!", null, 3.0f);
            }

            ImGui.Separator();
            ImGui.InputText("IP do Servidor / Amigo", ref _inputIp, 64);
            if (ImGui.Button("⚔️ Conectar ao Amigo (Client)"))
            {
                NetworkManager.ConnectTo(_inputIp, _inputPort);
            }
            ImGui.EndChild();
        }
        else
        {
            ImGui.BeginChild("connected_status_child", new Vector2(-1, 140), ImGuiChildFlags.Borders);
            ImGui.TextColored(new Vector4(0.2f, 1f, 0.3f, 1f), "✔ STATUS: CONECTADO");
            ImGui.Text($"Modo: {NetworkManager.Role}");
            ImGui.Text($"Parceiro: {NetworkManager.RemotePlayerName}");

            Vector4 pingColor = NetworkManager.PingMs < 50 ? new Vector4(0.2f, 1f, 0.3f, 1f) :
                                NetworkManager.PingMs < 120 ? new Vector4(1f, 0.85f, 0.2f, 1f) :
                                new Vector4(1f, 0.3f, 0.3f, 1f);
            ImGui.TextColored(pingColor, $"Latência (Ping): {NetworkManager.PingMs} ms");

            ImGui.Spacing();
            if (ImGui.Button("Desconectar da Sessão"))
            {
                NetworkManager.Stop();
            }
            ImGui.EndChild();
        }

        ImGui.Spacing();
        ImGui.TextColored(new Vector4(0.8f, 0.8f, 0.8f, 1f), "Como Jogar Pela Internet:");
        ImGui.TextWrapped("1. Conexão Direta: O Host clica em 'Hospedar' e envia seu IP público para o amigo colar no campo IP.");
        ImGui.TextWrapped("2. Redes Virtuais (Sem configurar roteador): Vocês podem usar Radmin VPN, Tailscale ou ZeroTier e colar o IP virtual.");
    }

    private void DrawCoopMapTab()
    {
        ImGui.TextColored(new Vector4(1f, 0.85f, 0.2f, 1f), "Radar do Castelo em Tempo Real:");
        ImGui.Separator();

        // Legenda de Cores
        ImGui.Text("Legenda:");
        ImGui.SameLine();
        ImGui.TextColored(new Vector4(0.2f, 0.6f, 1f, 1f), "■ Você");
        ImGui.SameLine();
        ImGui.TextColored(new Vector4(1f, 0.6f, 0.1f, 1f), "■ Amigo");
        ImGui.SameLine();
        ImGui.TextColored(new Vector4(0.2f, 1f, 0.4f, 1f), "■ Ambos");
        ImGui.SameLine();
        ImGui.TextColored(new Vector4(1f, 1f, 1f, 1f), "● Sua Posição");
        ImGui.SameLine();
        ImGui.TextColored(new Vector4(1f, 0.2f, 0.2f, 1f), "▲ Amigo");

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
                if (index >= 512) break;

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

        // Desenhar indicador da posição atual do jogador local (👑)
        int localX = Math.Clamp(Stages.RoomX, 0, 63);
        int localY = Math.Clamp(Stages.RoomY, 0, 31);
        Vector2 localPos = canvasPos + new Vector2(localX * tileSizeX + tileSizeX * 0.5f, localY * tileSizeY + tileSizeY * 0.5f);
        drawList.AddCircleFilled(localPos, 5f, ImGui.ColorConvertFloat4ToU32(new Vector4(1f, 1f, 1f, 1f)));
        drawList.AddCircle(localPos, 6f, ImGui.ColorConvertFloat4ToU32(new Vector4(0.2f, 0.8f, 1f, 1f)), 12, 2f);

        // Desenhar indicador da posição atual do jogador remoto (🗡️)
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
            if (ImGui.Button("✨ Teleportar até o Amigo (Buddy Warp)", new Vector2(-1, 35)))
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

        ImGui.TextColored(new Vector4(0.3f, 0.8f, 1f, 1f), "👥 Jogadores no Castelo:");
        ImGui.Separator();

        // Local Player
        ImGui.Text($"👑 {NetworkManager.LocalPlayerName} (Você):");
        ImGui.ProgressBar((float)Player.Hp / Math.Max(1, (int)Player.HpMax), new Vector2(240, 18), $"HP: {Player.Hp}/{Player.HpMax}");
        ImGui.TextDisabled($"Área Atual: {Stages.Current} (Sala: {Stages.RoomX}, {Stages.RoomY})");

        ImGui.Separator();

        // Remote Player
        ImGui.Text($"🗡️ {NetworkManager.RemotePlayerName} (Parceiro):");
        ImGui.ProgressBar((float)RemotePuppet.CurHp / Math.Max(1, (int)RemotePuppet.MaxHp), new Vector2(240, 18), $"HP: {RemotePuppet.CurHp}/{RemotePuppet.MaxHp}");
        ImGui.TextDisabled($"Área Atual: {(Stage)RemotePuppet.StageId} (Sala: {RemotePuppet.RoomX}, {RemotePuppet.RoomY})");

        ImGui.Spacing();

        if (ImGui.Button("✨ Teleportar até o Amigo (Buddy Warp)", new Vector2(-1, 35)))
        {
            NetworkManager.RequestBuddyWarp();
        }

        if (PvPManager.IsPvPActive)
        {
            ImGui.Separator();
            ImGui.TextColored(new Vector4(1f, 0.3f, 0.3f, 1f), "⚔️ DUELO NO COLISEU ATIVO!");
            ImGui.Text($"Placar: {NetworkManager.LocalPlayerName} {PvPManager.LocalScore} x {PvPManager.RemoteScore} {NetworkManager.RemotePlayerName}");
        }
    }

    private void DrawChatTab()
    {
        ImGui.TextColored(new Vector4(0.3f, 0.8f, 1f, 1f), "💬 Chat da Sessão:");
        ImGui.Separator();

        // Quick Chat buttons
        ImGui.Text("Avisos Rápidos:");
        string[] quicks = ["Preciso de ajuda!", "Chefe encontrado!", "Achei uma passagem secreta!", "Teleporte até mim!"];
        foreach (var q in quicks)
        {
            if (ImGui.Button(q))
            {
                SendChatMessage(q);
            }
            ImGui.SameLine();
        }
        ImGui.NewLine();

        ImGui.Separator();

        ImGui.InputText("Mensagem", ref _chatInput, 128);
        ImGui.SameLine();
        if (ImGui.Button("Enviar") && !string.IsNullOrWhiteSpace(_chatInput))
        {
            SendChatMessage(_chatInput);
            _chatInput = "";
        }
    }

    private void SendChatMessage(string msg)
    {
        if (!NetworkManager.IsConnected) return;
        byte[] payload = NetworkPacket.CreateChat(NetworkManager.LocalPlayerName, msg);
        NetworkManager.SendPacket(new NetworkPacket(PacketOpCode.ChatMessage, payload));
        ToastNotifications.ShowText($"💬 Você", msg, null, 4.0f);
    }
}
