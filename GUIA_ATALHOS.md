# 🎮 Guia Completo de Atalhos & Funcionalidades - SymphonyRecomp

Este documento reúne todas as teclas de atalho, controles e novos painéis implementados no **SymphonyRecomp**.

---

## 🕹️ 1. Atalhos de Gameplay (Controle & Teclado)

| Ação / Funcionalidade | Teclado | Controle (Xbox / DualShock) | Descrição |
| :--- | :---: | :---: | :--- |
| **Troca Rápida de Armas** *(Quick Weapon Swap)* | **`Q`** | **`R3`** *(Clique Analógico Direito)* | Alterna ciclicamente entre os 3 conjuntos de armas (mão direita e esquerda) instantaneamente. |
| **Corrida do Alucard** *(Godspeed Boots / Sprint)* | **`→ →`** ou **`← ←`** | **`→ →`** / **`← ←`** ou segurar **`L1`** / **`R1`** | Dois toques na direção ou segurar botão de ombro para correr em alta velocidade. |
| **Invocar Espírito** *(Summon Spirit)* | **`1`** | Menu de Magias | Conjura a magia instantaneamente consumindo 5 MP. |
| **Tetra Espírito** *(Tetra Spirit)* | **`2`** | Menu de Magias | Conjura a magia instantaneamente consumindo 20 MP. |
| **Metamorfose Sombria** *(Dark Metamorphosis)* | **`3`** | Menu de Magias | Conjura a magia instantaneamente consumindo 10 MP. |
| **Fogo do Inferno** *(Hellfire)* | **`4`** | Menu de Magias | Conjura a magia instantaneamente consumindo 15 MP. |
| **Roubo de Almas** *(Soul Steal)* | **`5`** | Menu de Magias | Conjura a magia instantaneamente consumindo 50 MP. |
| **Irmãos da Espada** *(Sword Brothers)* | **`6`** | Menu de Magias | Conjura a magia instantaneamente consumindo 30 MP (requer familiar Espada). |
| **Barra de Menus ImGui** | **`F11`** ou Mover Mouse ao Topo | Menu Superior | Abre/oculta a barra superior de painéis e opções. |

---

## 📋 2. Novos Painéis & Menus (Acessíveis na barra `Diversos`)

### 🏆 1. Conquistas & Troféus (`Diversos -> Conquistas & Troféus`)
- Acompanhe o progresso de 14+ conquistas de História, Combate, Exploração e Mestria.
- Notificações visuais Toast na tela no momento do desbloqueio com pontuação.
- Registro da data e hora exatas em que cada troféu foi conquistado.

### 👑 2. Reviver Chefes & Arenas (`Diversos -> Reviver Chefes & Arenas`)
- Lista completa dos 20 chefes do Castelo Normal e Castelo Invertido com indicador (*Vivo* vs *Derrotado*).
- **Reviver Chefe**: Reativa a batalha do chefe selecionado para lutar novamente no mesmo savegame.
- **Teleportar para a Arena**: Pula direto para a sala do chefe sem precisar andar pelo castelo.

### 📖 3. Bestiário & Tabela de Drops (`Diversos -> Bestiário & Drops`)
- **Radar em Tempo Real (*Live Scanner*)**: Inspeciona as entidades ativas na sala, exibindo a barra de vida exata (HP restante), dano e ID de qualquer monstro na tela.
- **Compêndio Pesquisável**: Busque por nome do monstro ou item para ver fraquezas elementais (Holy, Dark, Fire, Ice, etc.) e taxas exatas de drop de itens raros (ex: *Crissaegrim 1.5%*, *Heaven Sword 1.2%*).

### 🪄 4. Magias Rápidas (`Diversos -> Magias Rápidas`)
- Paleta visual com barra de status de MP em tempo real (`curMp / maxMp`).
- Botões de 1 clique para disparar qualquer feitiço do Alucard com validação de custo de MP.

### 🗺️ 5. Viagem Rápida Global (`Diversos -> Viagem Rápida`)
- Teleporte instantâneo para mais de 25 zonas do Castelo Normal e Castelo Invertido.
- Filtro inteligente *"Apenas Áreas Já Visitadas"* para manter a exploração orgânica.

### 🌐 6. Multiplayer Co-op & Duelos PvP (`Diversos -> Multiplayer Co-op & PvP`)
- **Lobby & Conexão P2P 60Hz**: Hospedar ou conectar via IP/Porta em rede local ou internet.
- **Exploração Livre Desacoplada**: Cada jogador joga na sua própria tela sem limites de câmera ou tela dividida.
- **Sincronização em Tempo Real**: Mini-mapa compartilhado, eventos de chefes, portas e alavancas.
- **Buddy Warp**: Botão para se teleportar instantaneamente até a sala do seu parceiro.
- **Chat da Sessão**: Mensagens de texto e avisos rápidos (*"Preciso de ajuda!"*, *"Chefe aqui!"*).
- **Duelos no Coliseu**: Batalhas PvP equilibradas no Coliseu com placar de vitórias.
- **Manual Completo**: Consulte o [MULTIPLAYER.md](file:///Users/guhf/Documents/Workspace/SymphonyRecomp/MULTIPLAYER.md) para o guia detalhado com passo a passo e arquitetura.

### ☠️ 7. Dificuldade & Modo Hardcore (`Diversos -> Dificuldade & Hardcore`)
- **Modo Hard**: Multiplicador de dano recebido (1.5x a 5.0x) e restrições anti-spam de poções.
- **Modo HARDCORE (Permadeath)**: Se você morrer, o savegame é permanentemente deletado!

### ⚙️ 8. Opções de Qualidade de Vida (`Diversos -> Opções de Qualidade de Vida`)
- Alternância para ativar/desativar:
  - *Corrida & Dash do Alucard (Botas de Velocidade)*
  - *Controles Aprimorados de Richter (Item Crash no Triângulo)*
  - *Troca Rápida de Armas*
  - *Transições Rápidas (Pular Salas de CD)*
  - *Música Secreta da Fada em Japonês*
  - *Modo Daltônico e Correções Anti-Freeze*

---

## ⚡ 3. Inicialização Rápida no PC

Para iniciar o jogo sem recompilar tudo do zero:
```powershell
.\windows_run_no_build.bat
```
*(ou `.\windows_run.bat` para compilar e iniciar).*
