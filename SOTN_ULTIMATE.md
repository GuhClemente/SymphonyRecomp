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
19. [Tempo Parado da Maria (Byakko e Seiryuu)](#19-tempo-parado-da-maria-byakko-e-seiryuu)
20. [O Que o Teste Completo Encontrou](#20-o-que-o-teste-completo-encontrou)
21. [Script Unificado de Testes (`test_characters.sh`)](#21-script-unificado-de-testes-test_characterssh)
22. [Teleporte: Como Funciona e os Destinos Homologados](#22-teleporte-como-funciona-e-os-destinos-homologados)
23. [Widescreen & Estabilidade do HUD](#23-widescreen--estabilidade-do-hud)
24. [Matriz Comparativa dos 5 Personagens](#24-matriz-comparativa-dos-5-personagens)

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
0. Zera a flag de tempo parado, **destrói as entidades 1–63** (subweapons, invocações e efeitos do personagem que sai: as funções delas moram no overlay que vai ser trocado) e guarda posição e direção do jogador.
1. Restaura prioridades de SFX retail e solta o HUD da PSP Maria.
2. Carrega a folha de gráficos: `BIN/F_GAME2.BIN` (todos menos Alucard) ou `BIN/F_GAME.BIN`.
3. Grava o binário em `0x8013C000`, ativa o overlay no `Dispatcher` e grava `g_PlayableCharacter`.
4. Chama o `Init` do overlay, lido da tabela `PlayerOvl` no início do arquivo (`0x8013C004`).
5. Carrega os bancos de som: os da Maria, ou, ao sair de uma Maria, os originais do disco (`MariaLoader.RestoreRetailSounds`: `VAB/SD_RIH` para o Richter ou `VAB/SD_ALK` para o Alucard no slot 1, `VAB/SD_MAR` no slot 2).
6. **Descarrega do `Dispatcher` os overlays dos outros personagens** e devolve posição e direção.
7. `RedrawHud`: libera as primitivas do HUD e chama `DrawHud` + `DrawHudSubweapon`.

> **Por que descarregar.** `ric`, `ric2`, `maria` e `maria2` ocupam a mesma faixa de endereços e nenhum cobre outro por inteiro, então o `Dispatcher` mantinha as funções do antigo onde o novo não tem nenhuma. Um ponteiro velho executava, em silêncio, código de um personagem que nem está mais na memória. Com só o overlay ativo mapeado, isso vira um erro visível (`unmapped call`).

> **Por que guardar a posição.** Todo init (dos overlays e o `AluInit`) coloca o jogador num ponto próprio. Na entrada do castelo esse ponto fica dentro do chão, e o personagem atravessava o piso e caía para fora do mapa.

**Alucard** não tem overlay próprio: o init dele é o `AluInit` do DRA, chamado no lugar do passo 4 (`MariaCharacter.InitAlucard`). Ele recria a entidade do jogador (banco de sprites, paleta `0x8100`) e preenche as paletas que as partes do sprite usam (capa etc.), que em `F_GAME.BIN` são só placeholders verdes (`0x03E0`). Sem essa chamada ele voltava como uma silhueta verde.

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
* **14 funções declaradas à mão** em `config/sotn.json` (`"functions"` do overlay `maria2`): as entradas da tabela de funções de entidade dela (`0x8015CF28`) que a varredura do recompilador não reconhece. O código da PSP foi compilado por um GCC que põe cargas de registrador **antes** do ajuste de pilha, então a função não começa com o prólogo usual. Sem elas, usar um subweapon dava `unmapped call: 0x8015F2F8`. `maria` e `ric2` não têm nenhuma nessa situação.

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
Roteiro em `patches/ultimate/CharacterTest.cs`. Passa pelo título sozinho, transforma na Saturn Maria, entra no castelo e roda, **para cada personagem**:

| Bloco | Passos |
| :--- | :--- |
| Básico (todos) | parado, ataque, pulo, agachado, andando, subweapon (↑ + Quadrado), HP baixo (barra do HUD), pausa (Start; menos o Alucard) |
| Saturn Maria | pulo triplo, os 6 especiais (Soco Guardião, Suzaku, Genbu, Byakko, Seiryuu, Quatro Bestas), carga do ataque (brilho da placa em 3 estágios) |
| PSP Maria | os dois ícones, troca com Círculo, subweapons gato / cardeal / tartaruga / dragão, Triângulo |
| Richter e Richter de casaco | os 5 Item Crashes, Blade Dash, Super Jump |
| Alucard | as 5 magias (Summon Spirit, Tetra Spirit, Dark Metamorphosis, Hellfire, Soul Steal) |

* Antes de cada especial e de cada troca o roteiro **espera o personagem voltar a ficar parado** (até 1500 quadros), em vez de usar tempos fixos; se não voltar, o log traz `[TEST] AVISO`.
* Capturas em `SOTN_TEST_DIR` como `NNN_personagem_passo.bmp` (padrão: pasta temporária do sistema). Cada especial tem uma segunda captura `_b`, 70 quadros depois.
* O log tem uma linha `[TEST]` por captura: personagem, `step` do jogador, HP / MP / corações, subweapon, carga, posição, e se o SPU tocou amostras do banco de voz (`vab1`), do banco de especiais (`vab2`) ou de outros bancos desde a captura anterior.
* O jogo **fecha sozinho** no fim (`[TEST] FIM`).
* `SOTN_TEST_QUICK=1` roda só as trocas (11 capturas, ~1 minuto): serve para ver se cada personagem fica em pé no lugar.
* Faça backup de `carda.sav`, `cardb.sav`, `settings.json` e `interface.ini` antes: o teste aperta botões nos menus.
* **Resultado da última rodada, com as folhas de capturas e a tabela passo a passo:** [`docs/teste-personagens/README.md`](docs/teste-personagens/README.md).
* O que o log **não** prova: que o som é o certo. Ele mostra que amostras do banco tocaram; se a voz é a do personagem, só ouvindo.

---

## 16. Pendências Conhecidas
| Problema | Estado |
| :--- | :--- |
| Ícones da PSP Maria | Aparecem no lugar certo, mas não foram comparados com o mod rodando |
| Patches de widescreen/cheats do `ric` | Não se aplicam a `ric2`, `maria` e `maria2` (sem mapa de funções) |
| Blocos translúcidos grandes em alguns especiais da Saturn Maria | Vistos nas capturas; não se sabe se é o efeito ou textura faltando (ver o relatório do teste) |
| Áudio | O teste mostra o SPU tocando amostras dos bancos de voz e de especiais nos momentos certos; ninguém ouviu se a voz é a certa |
| Golpes diretos do quick-cast do Richter (Blade Dash, Super Jump) | São atalhos do recomp (escrevem velocidade), não os golpes do jogo |

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

---

## 19. Tempo Parado da Maria (Byakko e Seiryuu)
Enquanto invoca Byakko ou Seiryuu, a Saturn Maria grava `g_unkGraphicsStruct.unk28 = 4` (`0x80097420`, `MarTimeStop` no mod): o jogo para — inimigos, projéteis, rolagem — e só o código dela continua.

* No Ultimate, quem mantém o código dela rodando nesse estado é o passo "parado" do motor do DRA.
* O DRA retail tem o mesmo passo, mas só roda o overlay do jogador nele **durante o prólogo** (estágio `0x1F`). Fora dele, nada roda: ela nunca chegava a desligar a flag, ficava presa no passo `0x33` e o jogo inteiro congelava — nenhum personagem respondia mais, nem depois de trocar.
* Correção: `MariaCharacter.RunMariaDuringTimeStop`, chamado todo quadro quando a flag vale 4 e a Saturn Maria está ativa, faz as duas chamadas do ramo do prólogo: o `Main` do overlay (`0x8013C000`) e o `UpdatePlayerEntities` (`0x8013C008`).
* A flag é zerada em toda troca de personagem.
* Ao sair do tempo parado, o próprio DRA retail dá 48 quadros de invencibilidade e retoma os scripts de som.

---

## 20. O Que o Teste Completo Encontrou
Problemas que só apareceram rodando todas as ações de todos os personagens, e o que foi feito:

| Achado | Causa | Correção |
| :--- | :--- | :--- |
| Seiryuu congelava o jogo para sempre | Tempo parado sem ninguém rodando o código dela (seção 19) | `RunMariaDuringTimeStop` |
| Crash ao voltar para o Alucard (`unmapped call: 0x00000000`) | Entidades do personagem anterior continuavam vivas e chamavam código de um overlay já trocado | Destruir as entidades 1–63 na troca |
| Personagem atravessava o chão depois da troca | O init regrava a posição do jogador | Guardar e devolver posição e direção |
| PSP Maria caía ao usar subweapon (`unmapped call: 0x8015F2F8`) | 14 funções de entidade não reconhecidas pelo recompilador | Declaradas em `config/sotn.json` |
| Endereços velhos executavam código de outro personagem sem erro | Overlays antigos continuavam mapeados no `Dispatcher` | Descarregar os overlays dos outros personagens |
| Byakko e Quatro Bestas não eram lançados no teste | O roteiro disparava o especial com o anterior ainda em execução | O roteiro espera o personagem ficar parado |

Confirmado pelas capturas, e que antes não tinha sido exercitado: a barra de HP do HUD da Maria descendo, a placa dobrando e virando "PAUSE PUSH START" na pausa, o brilho da placa nos três estágios de carga, e o ícone do subweapon dentro do anel de vinhas.

---

## 21. Script Unificado de Testes (`test_characters.sh`)
O script `test_characters.sh` automatiza toda a validação dos personagens através de linha de comando:

* `./test_characters.sh quick`: Teste rápido (~1 minuto). Alterna entre os 5 personagens no mesmo local e garante estabilidade do hot-swap.
* `./test_characters.sh teleport [pasta] [num_local]`: teleporta pelos 10 destinos principais do primeiro castelo e testa os 5 personagens em cada um (100 capturas). Com `num_local`, só aquele destino. Ver também `teleport-all` e `probe` na seção 22.
* `./test_characters.sh full`: Teste exaustivo de 103 passos (todos os golpes, invocações, subweapons e Item Crashes).
* `./test_characters.sh view`: Abre a pasta com as capturas de tela geradas.

---

## 22. Teleporte: Como Funciona e os Destinos Homologados

`Player.TeleportTo(stage, roomX, roomY, offsetX, offsetY)` leva o jogador para a célula `(roomX, roomY)` do mapa do castelo (0..63) e o coloca no pixel `(offsetX, offsetY)` **dentro dessa célula** (256×256). A tabela de destinos fica num lugar só, `FastTravelPanel.Destinations`; o teste de personagens lê a mesma tabela.

### O que estava errado (e por que caía em lugar preto ou em tiles soltos)

1. **A célula pedida não era a célula de chegada.** No passo 3 da transição de sala (`RunMainEngine`, `0x800F3D60`), o motor não usa a sala passada a `SetNextRoomToLoad` para posicionar o jogador: ele calcula a célula como `(g_PlayerX >> 8) + g_Tilemap.left` (posição **antiga** do jogador na sala **antiga**) e fica só com o byte baixo de `D_801375C0 + scroll antigo` como pixel. O `offsetX/offsetY` antigo era escrito direto em `D_801375C0/C4`, então o destino dependia de onde o jogador estava antes: sala certa por acaso, sala vizinha, ou fora de qualquer sala (tela preta, queda infinita, tiles soltos).
   * Correção em `wrapers/Stage.cs` (`PlaceTick`): no quadro em que a transição chega ao passo 3, a origem da sala antiga (`0x800730B0/B4`) é ajustada para que aquela soma dê a célula pedida, e `D_801375C0/C4` recebe `offset - scroll`. Vale para qualquer sala, não só 1×1.
2. **Células que não existem.** Várias entradas da tabela antiga apontavam para células fora da tabela de salas do estágio (Cavernas `(34,42)`, Catacumbas `(18,52)`, Mina `(22,46)`, Warp `(32,24)` e quase todo o castelo invertido). O estágio carregava e nenhuma sala era carregada: a tela de tiles soltos. `Stages.Load` agora recusa a célula (`Stages.HasRoom`) e avisa no console.
3. **Personagem invisível depois de trocar de estágio.** O carregamento por `Stages.Load` zera a entidade do jogador e libera todos os primitivos, e o motor não roda de novo o `Init` do personagem. Os personagens de overlay perdiam os bancos de sprite (andavam invisíveis) e o Alucard seguia primitivos já liberados (`func_801093C4`). Agora `Stages` avisa a chegada (`Stages.StageArrived`) e `MariaCharacter.OnStageArrived` roda o `Init` do personagem ativo de novo, no lugar; enquanto o estágio carrega, `EntityAlucard` é pulado (gancho `PlayerReady`).
4. **Corredores de CD.** As salas 1×1 vizinhas de uma saída de estágio (tileset `0xFF` na tabela de salas) são os corredores de carregamento: o jogo assume o controle e leva o jogador para o outro estágio. Nenhum destino usa essas salas. A Sala do Relógio da Marble Gallery `(32,26)` também ficou de fora: a primeira entrada dispara a cena da Maria e trava o teleporte.
5. **Thread.** O painel chamava código do jogo a partir da thread da interface; agora passa por `GameThread.Post`.
6. **Trocar de personagem dentro de uma sala de save estragava a sala.** A sala de save (flag `0x20` em `0x8003C708`) e o corredor de CD (`0x40`) desenham seus tiles a partir de gráficos que o motor carrega por cima de parte da área do `F_GAME` ao entrar. A troca recarrega `F_GAME`/`F_GAME2` e a sala virava fileiras de um tijolo só. `MariaCharacter.ReloadRoomGraphics` repete as cargas do próprio motor (`LoadGfxAsync(3)` / `LoadGfxAsync(4)`) depois da troca.

### Destinos homologados

Medidos no jogo (`SOTN_TEST_PROBE=1`): o jogador é solto, o log registra onde pousa, e a coordenada final é testada de novo. Em todos, o primeiro quadro com controle já tem `step = Standing` e `vy = 0`. Chão da sala de save: `Y = 164` (castelo normal) e `Y = 168` (invertido, a sala é a mesma de cabeça para baixo). `X = 72` fica à esquerda do ponto de save.

| # | Região | `Stage` | Sala (X,Y) | Offset (X,Y) | Local | Piso |
| :-: | :--- | :--- | :-: | :-: | :--- | :-: |
| 1 | Entrada do Castelo | `CastleEntrance` (NP3) | (17,37) | (72,164) | Sala de save | vy=0 |
| 2 | Laboratório de Alquimia | `AlchemyLaboratory` (NZ0) | (7,30) | (72,164) | Sala de save | vy=0 |
| 3 | Marble Gallery | `MarbleGallery` (NO0) | (29,27) | (72,164) | Sala de save | vy=0 |
| 4 | Muralha Exterior | `OuterWall` (NO1) | (55,24) | (72,164) | Sala de save | vy=0 |
| 5 | Biblioteca Longa | `LongLibrary` (LIB) | (51,19) | (72,164) | Sala de save | vy=0 |
| 6 | Capela Real | `RoyalChapel` (DAI) | (29,13) | (72,164) | Sala de save | vy=0 |
| 7 | Aposentos de Olrox | `OlroxsQuarters` (NO2) | (31,17) | (72,164) | Sala de save | vy=0 |
| 8 | Coliseu | `Colosseum` (ARE) | (14,23) | (72,164) | Sala de save | vy=0 |
| 9 | Torre do Relógio | `ClockTower` (NZ1) | (49,14) | (128,132) | Passarela das engrenagens (o estágio não tem sala de save fora dos corredores de CD) | vy=0 |
| 10 | Keep do Castelo | `CastleKeep` (TOP) | (39,9) | (72,164) | Sala de save | vy=0 |
| 11 | Cavernas Subterrâneas | `UndergroundCaverns` (NO4) | (40,37) | (72,164) | Sala de save | vy=0 |
| 12 | Catacumbas | `Catacombs` (CAT) | (30,50) | (72,164) | Sala de save | vy=0 |
| 13 | Mina Abandonada | `AbandonedMine` (CHI) | (28,41) | (72,164) | Sala de save | vy=0 |
| 14 | Warp | `Warp` (WRP) | (15,38) | (72,180) | Sala de teleporte ao lado da Entrada, à esquerda do portal | vy=0 |

Castelo invertido (mesmas salas espelhadas, `63 - x`, `63 - y`):

| Região | `Stage` | Sala (X,Y) | Offset (X,Y) |
| :--- | :--- | :-: | :-: |
| Entrada Invertida | `ReverseEntrance` | (46,26) | (72,168) |
| Laboratório de Necromancia | `NecromancyLaboratory` | (56,33) | (72,168) |
| Galeria de Mármore Negro | `BlackMarbleGallery` | (34,36) | (72,168) |
| Muralha Exterior Invertida | `ReverseOuterWall` | (8,39) | (72,168) |
| Biblioteca Proibida | `ForbiddenLibrary` | (12,44) | (72,168) |
| Anti-Capela | `AntiChapel` | (34,50) | (72,168) |
| Death Wing's Lair | `DeathWingsLair` | (32,46) | (72,168) |
| Coliseu Invertido | `ReverseColosseum` | (49,40) | (72,168) |
| Torre do Relógio Invertida | `ReverseClockTower` | (14,49) | (128,180) |
| Keep Invertido | `ReverseCastleKeep` | (24,54) | (72,168) |
| Cavernas Invertidas | `ReverseCaverns` | (23,26) | (72,168) |
| Catacumbas Flutuantes | `FloatingCatacombs` | (33,13) | (72,168) |
| Caverna | `Cave` | (35,22) | (72,168) |
| Teleporte Invertido | `ReverseWarp` | (48,25) | (72,180) |

### Resultado da validação (2026-11-15)

* 10 destinos × 5 personagens (parado e ataque): 100 capturas, todos com `vy=0` na chegada, nenhuma chamada pulada pelo dispatcher. Folhas: [a](docs/teste-teleporte/10_locais_5_personagens_a.jpg), [b](docs/teste-teleporte/10_locais_5_personagens_b.jpg).
* 28 destinos (dois castelos): todos com `step = Standing` e `vy = 0` no primeiro quadro. Folha: [28 destinos](docs/teste-teleporte/28_destinos.jpg).
* Tudo isso em modo estrito (sem tolerância, código gerado limpo), junto com o teste completo de 103 passos.
* Ficou em aberto: o forro da capa do Alucard volta com outra cor depois de uma troca (vermelho no original); um item caído no chão no momento de uma troca de personagem vira um quadrado de cor única (continua podendo ser pego); a Sala do Relógio não serve de destino.

### Como testar e como medir um destino novo

* `./test_characters.sh teleport [pasta] [n]`: os 10 primeiros destinos com os 5 personagens (parado e ataque), ou só o destino `n`.
* `./test_characters.sh teleport-all [pasta]`: os 28 destinos, sem trocar de personagem.
* `./test_characters.sh probe arquivo.txt [pasta]`: destinos de um arquivo de texto (`codigo,Stage,roomX,roomY,offsetX,offsetY` por linha), sem recompilar. Solte o jogador acima do chão e leia a linha `[TELEPORT] apoiado ...` do log: é a coordenada a usar.
* `python3 inspect_stage.py NO4`: tabela de salas de um estágio lida do disco, com os corredores de CD marcados.

No log, `chegada` é o primeiro quadro com controle e `apoiado` é depois de parar; num destino bom as duas linhas têm a mesma posição e `vy=0`.

### A "tolerância" foi removida: o que ela escondia

Uma sessão anterior tinha deixado três coisas para o teleporte "não crashar": `Dispatcher.Tolerant = true`, um buffer vazio no `PSMemory` para endereços inválidos e, **editado à mão dentro de `generated/dra.cs`** (pasta fora do git), um `try { ... } catch { }` vazio em volta de `DrawRichterHud`, `DrawRichterHudSubweapon` e `EntityAlucard`. As três foram desfeitas (o código gerado foi regenerado). O `catch` vazio era o pior: a exceção saía no meio da função do jogo, o epílogo não rodava, a pilha ficava `0x38` bytes fora do lugar e os registradores salvos de `MainGame` voltavam zerados; o jogo seguia com estado corrompido.

Os erros de verdade, agora corrigidos na origem:

* **HUD depois de um carregamento de estágio.** Os primitivos do HUD são liberados e o HUD não é refeito; `DrawHud`, `DrawHudSubweapon`, `DrawRichterHud` e `DrawRichterHudSubweapon` seguiam ponteiros de primitivos reutilizados. Gancho `pre` nas quatro (`MariaCharacter.HudReady`, em `config/sotn.json`): nada é desenhado enquanto o carregamento está em curso, e no primeiro quadro depois dele o HUD é reconstruído (`RedrawHud`). `Stages.StageLoadCount` diz que houve um carregamento mesmo que o HUD não tenha sido chamado durante ele.
* **`EntityAlucard` durante o carregamento**: item 3 acima.

Regra para o futuro: nada de editar `generated/` à mão (some no próximo `recompone`), e nada de `catch` vazio em volta de código do jogo.

### Alucard depois de uma troca de personagem

O Alucard não tem overlay próprio, mas o espaço do overlay do jogador (`0x8013C000`) guarda a folha de sprites dele, `BIN/ARC_F.BIN`, que o jogo carrega ali quando começa como Alucard. Depois de carregar Maria ou Richter nesse espaço, a volta para o Alucard desenhava os bytes do binário anterior: uma figura oliva de capa roxa, ou nada. `SwitchToCharacter` agora recarrega `BIN/ARC_F.BIN` antes do `AluInit`. Capturas: [original e volta](docs/teste-teleporte/alucard_original_e_volta.jpg). Referência para comparar: `SOTN_TEST_MARIA=1 SOTN_TEST_START=alucard` começa o jogo como o Alucard intocado, captura, troca para a Maria e volta.

---

## 23. Widescreen & Estabilidade do HUD
* **Prevenção de Duplo Deslocamento:** No arquivo `patches/widescreen/HudWidescreenPatch.cs`, o método `PostDrawHud` verificava se o HUD do Richter/Maria estava ativo, mas zerava `_richterHudMargin = 0`. Isso fazia com que `PostDrawRichterHudSubweapon` aplicasse a margem widescreen duas vezes, jogando o HUD para $X = -110$ (fora da tela). O hook foi corrigido com `if (RichterHudActive(m)) return;`.
* **Segurança de Limites de Tela:** Em `patches/widescreen/RichterEdgeWidescreenPatch.cs`, a chamada estática direta a `SoTN_ric.func_8016DCB8` foi desacoplada com `Dispatcher.Call(c, m, 0x8016DCB8u);`, impedindo crashes imediatos ao tocar as bordas da tela quando jogando com Maria ou Alucard.

---

## 24. Matriz Comparativa dos Personagens & Estilos
| Personagem / Estilo | Overlay | Binário | Base | HUD | Habilidade Especial |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Saturn Maria** | `maria` | `MARIA.BIN` | `0x8013C000` | Vinhas Saturn + HP/MP | Pulo Triplo, 6 Bestas Sagradas, Carga de Ataque |
| **PSP Maria** | `maria2` | `MARIA2.BIN` | `0x8013C000` | Subweapon Duplo | Subweapons duplos rotativos, 4 animais celestiais |
| **Richter Clássico** | `ric` | `RIC.BIN` | `0x8013C000` | Richter Retail | Colete sem mangas PS1, Chicote, 5 Item Crashes, Dash |
| **Richter Saturn Coat** | `ric2` | `RIC2.BIN` | `0x8013C000` | Richter Retail | Casaco longo azul com mangas e acabamento dourado do Saturn |
| **Richter Couro Marrom**| `ric2` | `RIC2.BIN` (Paleta Simon) | `0x8013C000` | Richter Retail | Sobretudo rústico de couro marrom estilo Simon/Trevor |
| **Alucard** | `dra` | `DRA.BIN` | Core | Alucard Clássico | Espada/Escudo, 6 Magias, Relíquias, Transformações |

---

## 25. Bypass da Morte (Death Skip Oficial)
* **Arquivo:** `patches/translation/scripts/ScriptFunctions_no3.cs` e `patches/ultimate/UltimateOptions.cs`.
* **Mecânica:**
  * No código original de PS1 da Entrada do Castelo (`no3`), o encontro com a Morte verifica `g_PlayableCharacter` (`0x8003BE21`). Para Richter (`!= 0`), a Morte sempre era destruída nativamente (`DestroyEntity_no3`).
  * Para o Alucard, a cena padrão retirava todo o equipamento lendário inicial.
  * O novo toggle **`BypassDeath`** (`[X] Ignorar Encontro com a Morte`) estende essa destruição condicional imediata para o Alucard também: a entidade da Morte é apagada antes de rodar o script de cutscene.
  * Com isso, o Alucard mantém integralmente o arsenal supremo (*Alucard Sword, Alucard Shield, Dragon Helm, Alucard Mail, Twilight Cloak, Ring of Ares*) e o corredor fica desobstruído.

---

## 26. Roda de Magias Adaptativa por Personagem (`SpellWheel`)
* **Arquivo:** `patches/qol/SpellWheel.cs` e `patches/qol/SpellWheelPanel.cs`.
* **Comportamento Dinâmico:**
  * **Alucard:** Exibe a barra de MP e permite disparo rápido com teclas `1` a `6` das magias: *Summon Spirit [1]*, *Tetra Spirit [2]*, *Dark Metamorphosis [3]*, *Hellfire [4]*, *Soul Steal [5]* e *Sword Brothers [6]*.
  * **Richter (Clássico, Saturn Coat, Couro Marrom):** Exibe a barra de Corações e dispara os Item Crashes e Artes Marciais pelas teclas `1` a `6`: *Hydro Storm [1]*, *Grand Cross [2]*, *Thousand Blades [3]*, *Axe Rain [4]*, *Burning Bible [5]* e *Blade Dash [6]*.
  * **Maria (Saturn e PSP):** Exibe a barra de MP e aciona as Invocações e Animais Celestiais pelas teclas `1` a `6`: *Guardian Knuckle [1]*, *Suzaku [2]*, *Genbu [3]*, *Byakko [4]*, *Seiryuu [5]* e *Quatro Bestas [6]*.

---

## 27. Correção de Glifos e Limpeza da Interface ImGui
* **Diagnóstico:** O atlas de fontes padrão do Dear ImGui suporta apenas a faixa de caracteres ASCII e Latin-1. Emojis Unicode (> `0x2500`) em títulos e botões eram substituídos automaticamente pelo glifo de fallback `'?'`.
* **Resolução:** Todos os emojis foram removidos de todos os painéis e ferramentas ([UltimatePanel.cs](file:///c:/github/SymphonyRecomp/patches/ultimate/UltimatePanel.cs), [SpellWheelPanel.cs](file:///c:/github/SymphonyRecomp/patches/qol/SpellWheelPanel.cs), etc.), resultando em uma interface gráfica limpa, consistente e sem glifos corrompidos.


