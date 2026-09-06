# 🦇 Castlevania: Symphony of the Night — SymphonyRecomp (State of the Art Edition)

[![Platform: Windows x64](https://img.shields.io/badge/Platform-Windows%20x64-blue.svg)](https://dotnet.microsoft.com/en-us/download/dotnet/10.0)
[![Website: Online Demo](https://img.shields.io/badge/Website-sotn.mister4all.com-red.svg)](https://sotn.mister4all.com)
[![Engine: .NET 10 C#](https://img.shields.io/badge/Engine-.NET%2010%20C%23-purple.svg)](https://dotnet.microsoft.com/)
[![Multiplayer: P2P 60Hz](https://img.shields.io/badge/Multiplayer-UDP%2060Hz%20Co--op%20%26%20PvP-green.svg)](#-1-multiplayer-co-op--duelos-pvp-60hz-p2p)
[![Mode: Hardcore Permadeath](https://img.shields.io/badge/Challenge-Hardcore%20Permadeath-crimson.svg)](#-2-modo-hard--modo-hardcore-permadeath)
[![Framerate: 60 FPS Native](https://img.shields.io/badge/Framerate-60%20FPS%20Native-gold.svg)](#)
[![Shaders: CRT & Trinitron](https://img.shields.io/badge/Visuals-Scanlines%20%26%20Trinitron%20PVM-orange.svg)](#-7-shaders-retrô-de-alta-fidelidade)

Bem-vindo à versão definitiva e mais avançada do **Castlevania: Symphony of the Night** no PC! Este projeto é uma **recompilação estática (Static Recompilation)** do código binário original de PlayStation 1 para **C# (.NET 10) nativo**, proporcionando desempenho puro a 60 FPS, sem emulação, suporte a Widescreen e um ecossistema moderno completo.

---

## 📜 Origem, História & Créditos do Projeto

Este projeto foi construído com profundo respeito e reconhecimento aos pioneiros que tornaram a recompilação e engenharia reversa do jogo possíveis:

- **[BlackLabelHQ](https://github.com/BlackLabelHQ) & [flaffymg](https://github.com/flaffymg)**: Criador do motor de recompilação estática **RecompOne** e arquiteto da base original do **SymphonyRecomp**.
- **[SOTN Decomp Team](https://github.com/xeeynamo/sotn-decomp)**: Comunidade responsável pelo esforço hercúleo de descompilação de símbolos, structs e lógica do jogo original.
- **Comunidade & Criadores (@VideoGameEsoterica, @jedkelly7862)**: Pelas análises técnicas e discussões detalhadas que inspiraram os novos recursos de Qualidade de Vida, Hardcore, Shaders e Multiplayer.

---

## 🏰 O Que o Projeto Tinha (Recursos Base)

- **Recompilação Estática 100% C#**: Tradução direta dos binários MIPS de PS1 para instruções nativas no PC.
- **Suporte a Widescreen 16:9 / 21:9**: Extensão de campo de visão com sistema de *Room Fills* para evitar bordas pretas.
- **Áudio Nativo OpenAL**: Reprodução perfeita das faixas de CD-DA e trilhas XA com baixa latência.
- **Interface Gráfica ImGui**: Barra de ferramentas superior (`F11`) para configurações e ajustes em tempo real.
- **Opções de Qualidade de Vida Iniciais**: Correções de anti-freeze, modo daltônico e música secreta da fada em japonês.

---

## 🔥 O Que o Projeto Tem Agora (Entregas State of the Art)

### 🌐 1. Multiplayer Co-op & Duelos PvP (60Hz P2P)
- **Exploração Livre Desacoplada**: Dois jogadores exploram o castelo com liberdade total. Cada um na sua tela a 60 FPS lisos sem divisão de tela ou atraso de controle!
- **Mini-Mapa Compartilhado com Cores Distintas**:
  - 🟦 **Azul**: Salas exploradas exclusivamente por você.
  - 🟧 **Laranja**: Salas exploradas pelo parceiro remoto.
  - 🟩 **Verde**: Salas exploradas por ambos os jogadores.
  - 👑 **Marcador Branco/Ciano**: Sua posição exata em tempo real.
  - 🗡️ **Marcador Vermelho/Dourado com Nome**: Posição exata do seu amigo no castelo.
- **Renderização do Parceiro na Mesma Sala**: O personagem do seu amigo (Alucard ou Richter) aparece ao seu lado atacando e se movendo com interpolação linear suave (*Lerp 0.35*).
- **Buddy Warp (Teleporte de Ajuda)**: Botão no menu para se teleportar instantaneamente até a sala do seu parceiro.
- **Duelos no Coliseu (PvP Arena)**: Ao entrarem na arena do Coliseu, ativa-se o modo de combate x1 equilibrado com contagem e placar de vitórias.
- **Conexão Pela Internet Fora da Rede Local (`NatHelper.cs`)**:
  - Detecção automática do seu **IP Público**.
  - Botão *"📋 Copiar Meu IP para Enviar ao Amigo"* em 1 clique.
  - Suporte total a conexões diretas, roteadores com UPnP e redes virtuais gratuitas (**Radmin VPN**, **Tailscale**, **ZeroTier**).

---

### ☠️ 2. Modo Hard & Modo HARDCORE (Permadeath)
- **Modo Hard**: Multiplicador de dano recebido configurável (1.5x a 5.0x) e restrições anti-spam de poções em batalhas de chefes.
- **Modo HARDCORE (*Permadeath Real*)**: Se o Alucard morrer durante a campanha, um alerta surge na tela e o savegame é permanentemente deletado, forçando o recomeço do zero!

---

### 🏆 3. Conquistas & Troféus Nativos no PC
- **14+ Conquistas Desafiadoras**: História, Exploração, Combate e Segredos (*"What is a Man?"*, *"Sword of the Century"*, *"Grand Cartographer"*, etc.).
- **Toast Notifications**: Alertas visuais animados na tela no instante do desbloqueio com pontuação.
- **Painel de Troféus**: Barra de porcentagem geral (`X / 14 - X% Concluído`) e data/hora de cada conquista.

---

### 👑 4. Reviver Chefes & Teleporte de Arenas (Boss Respawn)
- **Todos os 20 Chefes Catalogados**: Status em tempo real (*Vivo* vs *Derrotado*).
- **Reviver Chefe com 1 Clique**: Reative batalhas de chefes individualmente ou em grupo no mesmo savegame.
- **Teleporte para Arenas**: Salte direto para a sala do chefe sem precisar andar pelo castelo.

---

### 🏃 5. Corrida do Alucard (Godspeed Boots / Forward Dash)
- Ativação de corrida com **dois toques para a frente (`→ →` / `← ←`)** ou segurando **`L1` / `R1`**.
- Aceleração imediata e fluida inspirada nas botas exclusivas do Sega Saturn.

---

### 📖 6. Bestiário Interativo & Live Radar Scanner
- **Live Radar Scanner**: Inspeciona entidades ativas na sala, exibindo a barra de HP exata, atributos de ataque, defesa e elemento de qualquer monstro na tela.
- **Compêndio de Drops Raros**: Fraquezas elementais e taxas exatas de drop (*Crissaegrim 1.5%*, *Heaven Sword 1.2%*).

---

### 🪄 7. Magias Rápidas (Spell Quick Cast)
- Atalhos numéricos no teclado de **`1` a `6`** para conjuração imediata de magias (*Summon Spirit*, *Tetra Spirit*, *Dark Metamorphosis*, *Hellfire*, *Soul Steal*, *Sword Brothers*).
- Paleta gráfica no ImGui com validação em tempo real de custo de MP.

---

### 📺 8. Shaders Retrô de Alta Fidelidade (CRT Pro)
- **`Scanlines Clean (Flat CRT)`**: Linhas de varredura nítidas sem curvatura ou escurecimento de pretos.
- **`Sony Trinitron PVM & Glow`**: Máscara vertical de fósforos RGB com leve efeito de bloom/glow em projéteis e magias.
- **`CRT-Geom`**: Curvatura clássica ajustável.

---

### 🗡️ 9. Controles Aprimorados para o Richter Belmont
- **Item Crash no botão Triângulo (`△` / `V`)** para disparar o golpe supremo da sub-arma imediatamente.

---

### 🗺️ 10. Viagem Rápida Global & Troca Rápida de Armas
- **Fast Travel**: Teletransporte entre mais de 25 zonas dos dois castelos com filtro inteligente de salas visitadas.
- **Quick Weapon Swap**: 3 conjuntos completos de armas/escudos alternáveis com **`R3`** ou tecla **`Q`**.

---

### 🌐 11. Website Moderno Interativo & Guia de Manutenção
- Website moderno em `website/index.html` com arte cinemática gótica, vitrine de recursos, simulador de radar co-op e cheatsheet pesquisável.
- Guia mandatório em `website/INSTRUCTIONS_FOR_AI.md` para qualquer desenvolvedor ou IA manter o site atualizado.

---

## 🔮 O Que o Projeto Terá no Futuro (Master Roadmap)

1. **🏯 Fase Sega Saturn via Ghidra**:
   - Extração, descompilação e porte dos binários da **Maria Renard** do Saturn (`BIN/MAR.BIN`).
   - Extração das áreas exclusivas do Saturn: **Underground Garden** (`ST/NO0.BIN`) e **Cursed Prison** (`ST/NO1.BIN`).
   - Músicas extras e equipamentos exclusivos do Saturn.
2. **🎵 Trilha Sonora Orquestrada em Alta Resolução (FLAC/OGG)**:
   - Sistema de carregamento de faixas de áudio externas em `packs/soundtrack_hd/`.
3. **📱 Ports Multiplataforma**:
   - Suporte nativo para Linux / Steam Deck via backend Vulkan/OpenGL.

---

## 🕹️ Tabela de Atalhos & Controles

| Funcionalidade | Teclado | Controle (Xbox / DualShock) | Descrição |
| :--- | :---: | :---: | :--- |
| **Troca Rápida de Armas** | <kbd>Q</kbd> | <kbd>R3</kbd> *(Clique Analógico)* | Alterna entre os 3 perfis de armas e escudos. |
| **Corrida do Alucard** | <kbd>→</kbd><kbd>→</kbd> / <kbd>←</kbd><kbd>←</kbd> | <kbd>→</kbd><kbd>→</kbd> ou Segurar <kbd>L1</kbd>/<kbd>R1</kbd> | Corrida em alta velocidade (Godspeed Boots). |
| **Invocar Espírito (5 MP)** | <kbd>1</kbd> | Menu de Magias | Conjura *Summon Spirit* instantaneamente. |
| **Tetra Espírito (20 MP)** | <kbd>2</kbd> | Menu de Magias | Conjura *Tetra Spirit* instantaneamente. |
| **Metamorfose Sombria (10 MP)**| <kbd>3</kbd> | Menu de Magias | Conjura *Dark Metamorphosis* instantaneamente. |
| **Fogo do Inferno (15 MP)** | <kbd>4</kbd> | Menu de Magias | Conjura *Hellfire* instantaneamente. |
| **Roubo de Almas (50 MP)** | <kbd>5</kbd> | Menu de Magias | Conjura *Soul Steal* instantaneamente. |
| **Irmãos da Espada (30 MP)** | <kbd>6</kbd> | Menu de Magias | Conjura *Sword Brothers* (requer Familiar). |
| **Item Crash do Richter** | <kbd>V</kbd> | <kbd>△</kbd> *(Triângulo / Y)* | Dispara o golpe supremo da sub-arma imediatamente. |
| **Barra de Menus ImGui** | <kbd>F11</kbd> / Mouse no Topo | Menu Superior | Abre/fecha a barra com todos os painéis. |

---

## 🛠️ Como Compilar e Jogar no PC

### Pré-requisitos:
- [.NET 10 SDK](https://dotnet.microsoft.com/en-us/download/dotnet/10.0)
- Placa de vídeo com suporte a OpenGL 2.1+
- Imagem do disco original de Castlevania SOTN (PS1 USA - `SLUS-00067`) em formato `.bin`/`.cue`.

### Passo a Passo:

1. **Clonar o Repositório**:
   ```bash
   git clone -b dev https://github.com/GuhClemente/SymphonyRecomp.git
   cd SymphonyRecomp
   ```

2. **Colocar a ROM na pasta `disc/`**:
   - `disc/Castlevania - Symphony of the Night (Track 1).bin`
   - `disc/Castlevania - Symphony of the Night (Track 2).bin`
   - `disc/Castlevania - Symphony of the Night (USA).cue`

3. **Compilar e Iniciar**:
   Execute o script inicial uma única vez para extrair as texturas e compilar os binários:
   ```powershell
   .\windows_initial_build.bat
   ```

4. **Para Jogar Posteriormente sem Recompilar do Zero**:
   ```powershell
   .\windows_run_no_build.bat
   ```

---

## 🌐 Como Rodar o Website Localmente
O website do projeto é 100% estático (HTML5, Vanilla CSS e JS):
```powershell
python -m http.server 3000 --directory website
```
Em seguida, abra seu navegador em: **`http://localhost:3000`**

---

*Castlevania: Symphony of the Night é marca registrada da Konami. Este projeto destina-se estritamente à preservação histórica, interoperabilidade e pesquisa de engenharia reversa para proprietários de cópias legítimas do jogo.*
