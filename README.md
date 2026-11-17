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
- **Manual Completo**: Consulte o [MULTIPLAYER.md](file:///Users/guhf/Documents/Workspace/SymphonyRecomp/MULTIPLAYER.md) para o guia detalhado passo a passo, arquitetura técnica e solução de problemas.

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

### 🪄 7. Magias Rápidas & Roda de Habilidades Adaptativa
- **Adaptação Dinâmica ao Personagem Ativo**:
  - **Alucard**: Magias com consumo de MP (*Summon Spirit [1]*, *Tetra Spirit [2]*, *Dark Metamorphosis [3]*, *Hellfire [4]*, *Soul Steal [5]*, *Sword Brothers [6]*).
  - **Richter (Clássico, Saturn Coat, Couro Simon)**: Barra de Corações e disparo imediato de Item Crashes e Artes Marciais (*Hydro Storm [1]*, *Grand Cross [2]*, *Thousand Blades [3]*, *Axe Rain [4]*, *Burning Bible [5]*, *Blade Dash [6]*).
  - **Maria (Saturn e PSP)**: Barra de MP e Invocações dos Animais Celestiais (*Guardian Knuckle [1]*, *Suzaku [2]*, *Genbu [3]*, *Byakko [4]*, *Seiryuu [5]*, *Quatro Bestas [6]*).
- Atalhos numéricos universais no teclado de **`1` a `6`** e paleta gráfica no ImGui com barras dinâmicas.

---

### 📺 8. Shaders Retrô de Alta Fidelidade (CRT Pro)
- **`Scanlines Clean (Flat CRT)`**: Linhas de varredura nítidas sem curvatura ou escurecimento de pretos.
- **`Sony Trinitron PVM & Glow`**: Máscara vertical de fósforos RGB com leve efeito de bloom/glow em projéteis e magias.
- **`CRT-Geom`**: Curvatura clássica ajustável.

---

### 🗡️ 9. Controles Aprimorados para o Richter Belmont
- **Item Crash no botão Triângulo (`△` / `V`)** para disparar o golpe supremo da sub-arma imediatamente.
- **Atalhos 1 a 6** para disparar qualquer Item Crash ou Blade Dash sem precisar de motion inputs complexos.

---

### 🗺️ 10. Viagem Rápida Global & Troca Rápida de Armas
- **Fast Travel**: Teletransporte entre mais de 25 zonas dos dois castelos com filtro inteligente de salas visitadas.
- **Quick Weapon Swap**: 3 conjuntos completos de armas/escudos alternáveis com **`R3`** ou tecla **`Q`**.

---

### 🏰 11. SOTN Ultimate Integrado (Sega Saturn & PSP)
- **Maria Renard 100% Jogável**:
  - **Saturn Maria**: Pulo triplo aéreo, pombas mágicas, carga de ataque e as Quatro Bestas Sagradas (*Suzaku*, *Genbu*, *Byakko*, *Seiryuu*).
  - **PSP Maria**: Subweapons duplos rotativos, HUD autêntico de dois ícones e animais celestiais.
- **3 Trajes Selecionáveis de Richter Belmont**:
  - **Richter Clássico**: Figurino original de PS1 com colete azul sem mangas.
  - **Richter Saturn Coat**: Sobretudo azul longo com mangas compridas e detalhes dourados do Sega Saturn.
  - **Richter Couro Marrom**: Sobretudo de couro rústico inspirado nos trajes clássicos de Simon e Trevor Belmont.
- **Arsenal e Relíquias do Saturn**:
  - Botas da Velocidade (*God Speed Shoes - Relíquia 30*).
  - Terceira Mão / Holder (*Item rápido no botão L2*).
  - Efeito Dinâmico de Cor no Manto do Arco-Íris.
- **Dublagens e Áudio Múltiplo**:
  - Dublagem PSP (*Dracula X Chronicles* com roteiro revisado).
  - Dublagem Clássica de PS1 (*"What is a man? A miserable little pile of secrets!"*).
  - Dublagem Japonesa Original com legendas.

---

### ⚡ 12. Bypass da Morte (Death Skip Oficial)
- **Morte Opcional na Entrada do Castelo**:
  - Ative a opção **`Ignorar Encontro com a Morte`** no painel SOTN Ultimate.
  - A entidade da Morte é apagada imediatamente ao entrar no corredor: Alucard não assiste à cena e **mantém intacto todo o seu arsenal supremo inicial** (*Alucard Sword, Alucard Shield, Dragon Helm, Alucard Mail, Twilight Cloak, Ring of Ares*).
  - O corredor fica livre e desobstruído até o Laboratório Alquímico.

---

### 🌐 13. Website Moderno Interativo & Guia de Manutenção
- Website moderno em `website/index.html` com arte cinemática gótica, vitrine de recursos, simulador de radar co-op e cheatsheet pesquisável.
- Guia mandatório em `website/INSTRUCTIONS_FOR_AI.md` para qualquer desenvolvedor ou IA manter o site atualizado.

---

## 🔮 O Que o Projeto Terá no Futuro (Master Roadmap)

1. **🏯 Fases Exclusivas do Sega Saturn**:
   - Portagem e recompilação completa dos overlays de cenários: **Underground Garden** e **Cursed Prison**.
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
| **Magias de Alucard (1 a 6)** | <kbd>1</kbd> a <kbd>6</kbd> | Menu de Magias Rápidas | Dispara Summon Spirit, Tetra, Metamorphosis, Hellfire, Soul Steal, Sword Brothers. |
| **Habilidades de Richter (1 a 6)** | <kbd>1</kbd> a <kbd>6</kbd> | Menu de Magias Rápidas | Dispara Hydro Storm, Grand Cross, Thousand Blades, Axe Rain, Bible e Blade Dash. |
| **Habilidades de Maria (1 a 6)** | <kbd>1</kbd> a <kbd>6</kbd> | Menu de Magias Rápidas | Invoca Guardian Knuckle, Suzaku, Genbu, Byakko, Seiryuu e Quatro Bestas. |
| **Item Crash do Richter** | <kbd>V</kbd> | <kbd>△</kbd> *(Triângulo / Y)* | Dispara o golpe supremo da sub-arma imediatamente. |
| **Seleção de Personagens & Trajes**| Menu <kbd>F11</kbd> | Menu SOTN Ultimate | Alterna entre Alucard, Maria Saturn, Maria PSP e os 3 trajes de Richter. |
| **Bypass da Morte (Death Skip)** | Menu <kbd>F11</kbd> | Menu SOTN Ultimate | Pula a cena da Morte e preserva todo o equipamento inicial de Alucard. |
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

3. **No Windows**:
   - Build inicial (extrai dados e compila):
     ```powershell
     .\windows_initial_build.bat
     ```
   - Jogar / Iniciar:
     ```powershell
     .\windows_run.bat
     ```
   - Jogar sem reconstruir:
     ```powershell
     .\windows_run_no_build.bat
     ```

4. **No macOS (Apple Silicon M1/M2/M3/M4 & Intel)**:
   - Build inicial (instala .NET 10 SDK se necessário, extrai dados e compila nativo ARM64):
     ```bash
     chmod +x mac_initial_build.sh mac_run.sh
     ./mac_initial_build.sh
     ```
   - Jogar / Iniciar:
     ```bash
     ./mac_run.sh
     ```
   - Jogar sem reconstruir:
     ```bash
     ./mac_run_no_build.sh
     ```
   - Exportar binários prontos para distribuição (Apple Silicon, Intel ou Universal):
     ```bash
     ./mac_publish.sh all   # Gera pacotes em dist/ para Apple Silicon, Intel e Universal 2
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
