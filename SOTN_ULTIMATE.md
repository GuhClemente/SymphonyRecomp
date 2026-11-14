# 🏰 SOTN Ultimate no SymphonyRecomp (Documentação Técnica)

Esta documentação detalha a integração do mod **Castlevania: Symphony of the Night Ultimate (Sega Saturn & PSP)** dentro do motor **SymphonyRecomp** (.NET 10).

---

## 📑 Sumário
1. [Visão Geral](#1-visão-geral)
2. [Maria Renard (Sega Saturn)](#2-maria-renard-sega-saturn)
3. [Pipeline de Áudio & SPU (`libsnd` Fix)](#3-pipeline-de-áudio--spu-libsnd-fix)
4. [Relocação de Memória (`PlayerStatus` +0x14 Offset)](#4-relocação-de-memória-playerstatus-0x14-offset)
5. [Modelo de Threads (`GameThread`)](#5-modelo-de-threads-gamethread)
6. [Suporte a Binários Locais no Recompilador](#6-suporte-a-binários-locais-no-recompilador)
7. [Magias Rápidas Adaptativas (`SpellWheel`)](#7-magias-rápidas-adaptativas-spellwheel)
8. [Roadmap: Próximos Personagens & Fases](#8-roadmap-próximos-personagens--fases)
9. [Como Executar e Testar](#9-como-executar-e-testar)
10. [Troca de Personagem em Tempo Real](#10-troca-de-personagem-em-tempo-real)
11. [Maria Renard (PSP) e Richter Saturn Coat](#11-maria-renard-psp-e-richter-saturn-coat)
12. [Relocações: Estáticas e em Tempo de Execução](#12-relocações-estáticas-e-em-tempo-de-execução)
13. [Gráficos e Paletas na Troca](#13-gráficos-e-paletas-na-troca)
14. [HUD (estilo Richter) e Widescreen](#14-hud-estilo-richter-e-widescreen)
15. [Teste Automatizado dos 5 Personagens](#15-teste-automatizado-dos-5-personagens)
16. [Pendências Conhecidas](#16-pendências-conhecidas)
17. [Checklist para Portar Outro Binário do Ultimate](#17-checklist-para-portar-outro-binário-do-ultimate)
18. [HUD Próprio da Saturn Maria](#18-hud-próprio-da-saturn-maria)

---

## 1. Visão Geral
O mod *SOTN Ultimate* une conteúdos do Sega Saturn e do PSP (*Dracula X Chronicles*) na engine de PS1. No SymphonyRecomp, a integração roda diretamente em C# nativo através de recompilação estática MIPS -> C#.

---

## 2. Maria Renard (Sega Saturn)
* **Overlay:** `assets/characters/MARIA.BIN` (278 KB).
* **Base de Memória:** `0x8013C000` (mesmo slot de `RIC.BIN`).
* **Ponto de Entrada:** `MarInit` em `0x80161DA8`.
* **Mecânicas:**
  * Pulo triplo aéreo com restauração de física (`MariaCharacter.cs`).
  * Disparo de pombas mágicas.
  * Invocação das Quatro Bestas Sagradas conectadas ao manipulador nativo `MarSetSpecial` em `0x8016CB10`.

---

## 3. Pipeline de Áudio & SPU (`libsnd` Fix)
* **Arquivo:** `assets/characters/MARIA_SD.BIN` (135 KB).
* **Diagnóstico:** A biblioteca de áudio do PS1 (`libsnd`) colocava o estado do VAB em `2` (*head only*) ao usar `SsVabOpenHeadSticky` sem DMA direto da SPU, bloqueando todos os comandos de nota em `SpuVmVSetUp`.
* **Solução:**
  * Injeção direta dos samples ADPCM na SPU RAM (`0x42CC0` para VAB 1, `0x50E50` para VAB 2).
  * Injeção dos cabeçalhos VAB em RAM `0x800B407C` e `0x800B807C`.
  * Execução da rotina `SsVabFakeBody(vabId)` pelo contexto da CPU, marcando o VAB como carregado e habilitando a reprodução sonora.
  * Ajuste dos modos de canais contínuos do Seiryuu (canais 22/23) e restauração automática de SFX retail via `OnMariaLeft`.

---

## 4. Relocação de Memória (`PlayerStatus` +0x14 Offset)
* O `MARIA.BIN` do mod Ultimate foi compilado contra uma versão que expandiu arrays de inventário em 20 bytes (`+0x14`).
* Para evitar 31 leituras e escritas corrompidas de HP/MP/Corações contra o layout retail do PS1 (`0x80097B90`), foi inserida uma regra de realocação no `config/sotn.json` mapeando de `0x80097BA4` de volta para `0x80097B90`.

---

## 5. Modelo de Threads (`GameThread`)
* A UI do ImGui opera na thread de janela, enquanto a emulação MIPS roda na game thread.
* Para prevenir corridas de dados e SP inválido, foi implementado `GameThread.Post()`, descarregado a cada quadro em `QualityOfLife.Apply(c, m)`.

---

## 6. Suporte a Binários Locais no Recompilador
* Adicionada a propriedade `localFile` em `config/sotn.json` e tratada em `OverlayWriter.cs`.
* Permite que arquivos fora da imagem de disco (.CUE/.BIN) sejam compilados como overlays C# reproduzíveis.

---

## 7. Magias Rápidas Adaptativas (`SpellWheel`)
* **Alucard:** Magias clássicas de sangue e barra de MP.
* **Richter:** Item Crashes (*Hydro Storm*, *Grand Cross*, etc.), artes marciais e barra de Corações.
* **Maria:** Invocações dos Animais Celestiais (*Guardian Knuckle*, *Suzaku*, *Genbu*, *Byakko*, *Seiryuu*, *Quatro Bestas*) com validação de postura e barra de MP.

---

## 8. Roadmap: Próximos Personagens & Fases

| Item | Binário | Status | Próximo Passo |
| :--- | :--- | :--- | :--- |
| **Richter Saturn Coat** | `RIC2.BIN` | Overlay `ric2` jogável, figurino aplicado (seção 11) | Patches de widescreen do `ric` para o `ric2` |
| **Maria PSP** | `MARIA2.BIN` | Overlay `maria2` jogável, HUD de dois ícones (seções 11 e 14) | Validar ícones/voz contra o mod original; quick-cast próprio |
| **Arsenal do Saturn** | N/A | Aguardando | Expandir tabelas de itens do `DRA` retail (IDs 169–175 e 90–92) |
| **Fases do Saturn** | N/A | Aguardando | Portar e recompilar overlays `GDN`, `PRS`, `RGDN`, `RPRS` |
| **Name Entry "MARIA"** | N/A | Pendente | Hook na tela de criação de save para boot direto como Maria |

---

## 9. Como Executar e Testar

Compilação (.NET 10 no macOS):
```bash
~/.dotnet/dotnet build RecompOne.SoTN.csproj -c Release /p:UseSharedCompilation=false
```

Execução:
```bash
./mac_run.sh
```

No jogo, use a barra de menu superior:
* **Mods > SOTN Ultimate (Saturn & PSP)** para transformar ou pular para o castelo.
* **Mods > Magias Rápidas** (ou atalhos `1`-`6`) para disparar as habilidades.

---

## 10. Troca de Personagem em Tempo Real
* **Atalhos:** tecla `C` ou clique dos analógicos (`L3`/`R3`); também pelo combo do painel *SOTN Ultimate*.
* **Ordem do ciclo:** Saturn Maria → PSP Maria → Richter Clássico → Richter Saturn Coat → Alucard.
* **Código:** `MariaCharacter.SwitchToCharacter` (`patches/ultimate/MariaCharacter.cs`). Sempre roda na game thread (`RequestCharacterSwitch` posta em `GameThread`).
* **Bloqueios:** não troca durante loading nem no prólogo.

Todos os personagens que não são o Alucard usam o mesmo slot de overlay, `0x8013C000`:

| Personagem | Overlay | Arquivo | `g_PlayableCharacter` (`0x8003C9A0`) |
| :--- | :--- | :--- | :---: |
| Alucard | `dra` + armas (`w0_043`, `w1_052`) | disco | 0 |
| Richter Clássico | `ric` | `BIN/RIC.BIN`, lido da imagem de disco | 1 |
| Richter Saturn Coat | `ric2` | `assets/characters/RIC2.BIN` | 1 |
| Maria (Saturn) | `maria` | `assets/characters/MARIA.BIN` | 2 |
| Maria (PSP) | `maria2` | `assets/characters/MARIA2.BIN` | 2 |

**Sequência de uma troca** (a mesma ordem que o jogo usa ao carregar um personagem):
1. Restaura prioridades de SFX retail e solta o HUD da PSP Maria.
2. Carrega a folha de gráficos: `BIN/F_GAME2.BIN` (todos menos Alucard) ou `BIN/F_GAME.BIN`.
3. Grava o binário em `0x8013C000`, ativa o overlay no `Dispatcher` e grava `g_PlayableCharacter`.
4. Chama o `Init` do overlay, lido da tabela `PlayerOvl` no início do arquivo (`0x8013C004`).
5. Carrega os bancos de som: os da Maria, ou, ao sair de uma Maria, os originais do disco (`MariaLoader.RestoreRetailSounds`: `VAB/SD_RIH` para o Richter ou `VAB/SD_ALK` para o Alucard no slot 1, `VAB/SD_MAR` no slot 2).
6. `RedrawHud`: libera as primitivas do HUD e chama `DrawHud` + `DrawHudSubweapon`.

**Alucard** não tem overlay próprio: o init dele é o `AluInit` do DRA, chamado no lugar do passo 4 (`MariaCharacter.InitAlucard`). Ele recria a entidade do jogador (banco de sprites, paleta `0x8100`) e preenche as paletas que as partes do sprite usam (capa etc.), que em `F_GAME.BIN` são só placeholders verdes (`0x03E0`). Sem essa chamada ele voltava como uma silhueta verde. `AluInit` também põe o jogador em x = 32, então posição e direção são guardadas antes e devolvidas depois.

> **A ordem importa.** `F_GAME2.BIN` vem **antes** do init do overlay. Carregado depois, ele cobre os sprites que a Maria acabou de pôr na VRAM; sem ele, o HUD estilo Richter usa a folha do Alucard.

---

## 11. Maria Renard (PSP) e Richter Saturn Coat
### PSP Maria (`MARIA2.BIN`)
* Código-fonte de referência: `mods/sotn-ultimate/src/maria_psp/mar2.c`.
* Init: `MarPsxInit` (`0x8015DC3C`). Na primeira carga zera o BSS e chama `MARIA_Load`, que enfileira os gráficos dela e grava as paletas.
* Som: `MARIA2_SD.BIN`, mesmo layout do `MARIA_SD.BIN`. A voz PSP fica nos slots de voz do Richter, então as **prioridades de SFX ficam as retail** (`LoadMariaSounds(..., saturnPriorities: false)`).
* Dois subweapons: `g_Status.subWeapon` e `g_Status.D_80097C40`; Círculo troca. Fora do `MarMain` os dois campos guardam ids do Richter.
* **Dependências do DRA do Ultimate** que o DRA retail não tem (todas resolvidas por relocação, seção 12):

| Símbolo | Endereço no Ultimate | No retail esse endereço é | Para onde foi |
| :--- | :--- | :--- | :--- |
| `D_800A3B70` (bancos de sprite do DRA) | `0x800A3A08` | tabela de carga de gráficos | `0x800A3B70` (o símbolo retail) |
| `g_ModSubwpnIcons` | `0x800A2F10` | dados do DRA | `0x8003C8A8` (`g_api`, slot livre 0x134) |
| `g_ModMar2HudIcons` | `0x800A2DEC` | dados do DRA | `0x8003C8AC` (`g_api`, slot livre 0x138) |

  Sem essas relocações o init dela sobrescrevia a tabela de gráficos do jogo: era a causa dos sprites corrompidos.
* `g_api.CalcDealDamageMaria` / `CalcPlayerDamageMaria` caem em `g_api + 0x12C / 0x130`, que são slots livres no retail.

### Richter Saturn Coat (`RIC2.BIN`)
* Mesmo código do Richter recompilado contra o layout do Ultimate; só precisa da relocação de status.
* A mágica `"RIC2"` é gravada em `g_Status.D_80097C40` (`0x80097964 + 0x2DC`).
* As cores e os braços do figurino ficam no fim da folha de sprites do `RIC2.BIN`: `{mágica "RIC2", ponteiro}` em `0x8013C000 + 0x170AC - 8`, e o ponteiro leva a `{n, {id de paleta, 16 cores}...}` (8 paletas) e `{n, {retângulo de VRAM, pixels}...}` (9 blocos). No Ultimate quem aplica é o `ModRicCostumeApply` do DRA; aqui é `MariaCharacter.ApplyRichterCostume`, chamado logo depois do init do overlay.

---

## 12. Relocações: Estáticas e em Tempo de Execução
Tudo em `"relocations"` no `config/sotn.json`.

| Tipo | Como funciona | Quando usar | Overlays |
| :--- | :--- | :--- | :--- |
| **Estática** (padrão) | Reescreve pares `lui` + offset em tempo de recompilação (`SymbolRelocator`) | Código que usa o endereço absoluto de cada campo | `maria`, `ric2`, símbolos do DRA em `maria2` |
| **Em tempo de execução** (`"runtime": true`) | Cada acesso à memória do overlay passa por `Rm(addr)`, gerado no próprio arquivo | Código que guarda `&g_Status` num registrador e chega nos campos por offset | `maria2` |

* A relocação de status é a mesma nos dois tipos: `0x80097BA4`, tamanho `0xF0`, para `0x80097B90` (de `saveName` até `statsFamiliars`).
* Uma entrada `runtime` **não** é aplicada estaticamente, para não remapear duas vezes.
* Limitação da `runtime`: um ponteiro para um campo movido, passado para o DRA, chega com o endereço do Ultimate.
* Acessos relativos à pilha (`SP`) não passam por `Rm`.
* Depois de mexer em relocações, regenere tudo:
```bash
~/.dotnet/dotnet RecompOne/RecompOne.Recompiler/bin/Release/net10.0/recompone.dll config/sotn.json
```

---

## 13. Gráficos e Paletas na Troca
`MariaCharacter.LoadGameGraphics` refaz o que o loader de CD faz com `F_GAME.BIN` / `F_GAME2.BIN`:
* 32 blocos de 32×128 nas páginas de textura `0x200–0x3FF` (y 256 e 384).
* 256 paletas (ids `0x100–0x1FF`, offset `0x40000` do arquivo), gravadas em `g_Clut` (`0x8006CBCC + id * 32`) **e** na VRAM via `Palette.Write`.

> **Id de paleta não é posição na VRAM.** A posição vem da tabela do jogo `g_ClutIds` (`0x8003C104`); `Palette.Write` já faz essa conversão. A versão antiga gravava nas linhas 4–7 da VRAM, em cima do framebuffer e das texturas do estágio.

* Ler a VRAM de volta (`Palette.Read`) a partir da game thread derrubou o jogo no teste; para snapshots use `g_Clut`.

---

## 14. HUD (estilo Richter) e Widescreen
`DrawRichterHud` aloca 9 primitivas em `g_PlayerHud.primIndex1` (`0x80137970`) e 16 barras de cor em `0x80137974`:

| Prim | O que é | Lado no widescreen |
| :---: | :--- | :---: |
| 0 | Moldura do HUD | esquerda |
| 1 | Moldura da barra do Drácula | direita (fora da tela) |
| 2 | **Barra de vida do jogador** | esquerda |
| 3, 4 | Barra do Drácula | direita (fora da tela) |
| 5, 6 | Dígitos de corações | esquerda |
| 7 | **Ícone do subweapon** | esquerda |
| 8 | Placa "RICHTER / REST" | esquerda |

* **Não esconda primitivas à mão.** A barra do Drácula fica além da borda direita (deslize `0x80137980 = 48`) até a luta do prólogo; o patch de widescreen empurra mais `+margem`.
* No dump de primitivas: `+0x0E` é o CLUT, `+0x26` é a **prioridade** (`0x1EF`/`0x1F0`), `+0x32` é o `drawMode` (`8` = escondido).
* `RedrawHud` só libera as primitivas e chama `DrawHud` / `DrawHudSubweapon`. Não redesenhe em mudança de estágio: o jogo já faz isso, e os índices antigos podem pertencer a outra entidade.
* **Marias:** `MariaCharacter.DrawMariaHud`, chamado no fim de `WidescreenPatch.PostDrawRichterHudSubweapon`. A Saturn Maria ganha o HUD próprio (seção 18); sem a arte dele, ou para a PSP Maria, só a placa (prim 8) é escondida.
* **PSP Maria:** o mesmo hook chama `Mar2HudIcons` do `MARIA2.BIN` (ponteiro em `0x8003C8AC`). A rotina desenha o subweapon da frente em `prim` e o de trás em `prim->next`; como o HUD retail não tem essa primitiva extra, a prim 4 é emprestada só durante a chamada. Depois os dois ícones recebem `-margem`.
* O hook da Maria roda **depois** dos deslocamentos de widescreen, senão os ícones são deslocados duas vezes.

---

## 15. Teste Automatizado dos 5 Personagens
```bash
SOTN_TEST_MARIA=1 SOTN_TEST_DIR=/tmp/sotn-shots ./mac_run.sh
```
* Passa pelo título sozinho, transforma na Saturn Maria, anda até o castelo e roda: ataque, pulo, especial, e a troca por todos os personagens.
* Grava `maria_1_stand.bmp` … `maria_9_maria_return.bmp` em `SOTN_TEST_DIR` (padrão: pasta temporária do sistema).
* O log imprime `[HUD_DUMP]` com as primitivas do HUD após cada troca.
* Termina em `SEQUÊNCIA COMPLETA`; o jogo continua aberto, feche manualmente.
* Faça backup de `carda.sav`, `cardb.sav`, `settings.json` e `interface.ini` antes: o teste aperta botões nos menus.

---

## 16. Pendências Conhecidas
| Problema | Estado |
| :--- | :--- |
| Ícones da PSP Maria | Aparecem no lugar certo, mas não foram comparados com o mod rodando |
| Patches de widescreen/cheats do `ric` | Não se aplicam a `ric2`, `maria` e `maria2` (sem mapa de funções) |
| Áudio | Só confirmado pelo log (bancos abrem); ninguém ouviu no teste |

---

## 17. Checklist para Portar Outro Binário do Ultimate
1. Registrar o overlay em `config/sotn.json` com `localFile` + `linearSweep`.
2. Procurar no binário **referências absolutas ao DRA** (`0x800A0000–0x8013C000`). Cada uma precisa existir no mesmo endereço no retail ou ganhar uma relocação.
3. Conferir os offsets de `g_Status`: HP em `0x7BA0` (retail) ou `0x7BB4` (Ultimate)? Há uma base `0x7964` em registrador? Escolher relocação estática ou `runtime`.
4. Conferir escritas em `g_api` além de `+0x128`.
5. Regenerar tudo com o config completo (um config parcial muda nomes de funções entre overlays).
6. Rodar o teste da seção 15 e olhar os screenshots — o log sozinho não mostra corrupção gráfica.

---

## 18. HUD Próprio da Saturn Maria
O HUD compacto do Saturn: suporte de vinhas, barra de HP (roxa) e de MP nos dois tubos, dígitos de corações na caixa verde e a placa **MARIA**, cujas letras brilham conforme a carga dela.

* **Código:** `patches/ultimate/MariaSaturnHud.cs`. Referência: `mods/sotn-ultimate/src/maria_saturn/5F60C_mod_maria_hud.h` (no Ultimate isso é desenhado pelo DRA).
* **De onde vem a arte:** do `BIN/F_GAME2.BIN` **do disco SOTN Ultimate**, que o `gen_mar_hud.py` do mod preenche numa área que o arquivo retail deixa vazia: tpage `0x18`, u 128–255, v 136–231 (VRAM x 544–575, y 392–487; no arquivo, bloco 3, linhas 8–103).
* **Como é encontrada:** na primeira vez, o jogo procura em `disc/` um `.cue`/`.chd` que não seja o disco em uso e que contenha `BIN/MARIA.BIN`, e copia só esse bloco de 32×96 para a memória. O disco Ultimate **não** é usado para mais nada.
* **Sem o disco Ultimate em `disc/`:** ela fica com o HUD do Richter, sem a placa dele. O log diz qual dos dois casos ocorreu (`[MariaHud] ...`).

**Como é desenhado.** Não aloca primitivas: reescreve, todo quadro, as 9 do HUD estilo Richter, depois do `DrawRichterHudSubweapon` retail e dos deslocamentos de widescreen.

| Prim | Vira | Posição (4:3) | Paleta |
| :---: | :--- | :--- | :---: |
| 0 | Suporte de vinhas | (2, 24) 38×96 | `0x108` |
| 1 | Barra de HP (sobe de y 116; até 90 px) | x 6, largura 4 | `0x103` |
| 2 | Barra de MP (até 57 px), atrás do vidro do tubo | x 21, largura 3 | `0x107` |
| 3–5 | Corações: unidades, dezenas (a partir de 10), centenas (a partir de 100) | x 27, 20, 13; y 29 | `0x103` |
| 6 | Escondida | — | — |
| 7 | Ícone do subweapon: continua sendo o do próprio jogo | — | — |
| 8 | Placa MARIA, ou os quadros da animação de pausa (arte do Richter, tpage `0x1B`) | (36, 21) 64×24 | `0x106` |

* **HP mostrado** segue o HP real em passos de 5 por quadro.
* **Paletas `0x106`–`0x108`** são regravadas em `g_Clut` e na VRAM só quando mudam: brilho da placa acima de 100/200/300 quadros de carga (`g_Player.unk24`, `0x80072F44`), e a barra de MP azul ou, com MP cheio, piscando de vermelho a branco.
* **A arte é reenviada à VRAM a cada 120 quadros**, porque o `F_GAME2.BIN` retail é recarregado em toda troca de personagem e apaga essa área.
* Diferença para o mod: o ícone do subweapon não é descido 2 px para dentro do anel de vinhas.

