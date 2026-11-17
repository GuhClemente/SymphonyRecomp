# SymphonyRecomp v2.0 - Ultimate State of the Art Edition (2026-11-17)

Esta versão unifica a recompilação estática de Castlevania: Symphony of the Night (.NET 10) com o ecossistema completo de recursos State of the Art: Multiplayer Co-op & PvP 60Hz, Modo Hardcore, Conquistas, Shaders CRT, e a integração massiva do mod **SOTN Ultimate (Sega Saturn & PSP)**.

### Destaques Recentes & SOTN Ultimate
- **Richter Belmont com 3 Trajes Selecionáveis:**
  - *Richter Clássico:* Colete azul sem mangas original do PS1.
  - *Richter Saturn Coat:* Sobretudo longo azul com mangas compridas e acabamento dourado do Sega Saturn.
  - *Richter Couro Marrom:* Sobretudo rústico de couro marrom inspirado no estilo clássico de Simon e Trevor Belmont, com injeção de paleta dedicada no VRAM preservando acabamentos dourados, botas e tom de pele.
- **Roda de Magias e Habilidades Adaptativa (Spell Wheel):**
  - Adaptação dinâmica automática baseada no personagem ativo em jogo.
  - **Alucard:** Barra de MP em tempo real e atalhos rápidos `1` a `6` para *Summon Spirit [1]*, *Tetra Spirit [2]*, *Dark Metamorphosis [3]*, *Hellfire [4]*, *Soul Steal [5]* e *Sword Brothers [6]*.
  - **Richter (todos os 3 trajes):** Barra de Corações em tempo real e atalhos rápidos `1` a `6` para *Hydro Storm [1]*, *Grand Cross [2]*, *Thousand Blades [3]*, *Axe Rain [4]*, *Burning Bible [5]* e *Blade Dash [6]*.
  - **Maria (Saturn e PSP):** Barra de MP em tempo real e atalhos rápidos `1` a `6` para *Guardian Knuckle [1]*, *Suzaku [2]*, *Genbu [3]*, *Byakko [4]*, *Seiryuu [5]* e *Quatro Bestas [6]*.
- **Bypass da Morte (Death Skip Oficial):**
  - Novo toggle `[X] Ignorar Encontro com a Morte (Bypass Death / Manter Equipamentos)` no painel SOTN Ultimate.
  - Destruição condicional da entidade da Morte na Entrada do Castelo (`no3`).
  - Alucard mantém intacto todo o seu arsenal supremo inicial (*Alucard Sword, Alucard Shield, Dragon Helm, Alucard Mail, Twilight Cloak, Ring of Ares*) e caminha livremente até o Laboratório Alquímico sem interrupções.
- **Interface ImGui 100% Limpa (Fix dos Glifos `?`):**
  - Substituição de emojis Unicode que extrapolavam o atlas ASCII/Latin-1 do Dear ImGui por textos limpos e descritivos, eliminando completamente os pontos de interrogação indesejados.
- **Maria Renard (Sega Saturn & PSP):**
  - Saturn Maria com física de pulo triplo, pombas mágicas, carga de ataque, Quatro Bestas Sagradas e HUD próprio de vinhas.
  - PSP Maria com subweapons duplos rotativos e HUD de dois ícones.
  - Pipeline de áudio SPU corrigido (`libsnd` fix) com reprodução autêntica de SFX e vozes em todos os canais.
- **Arsenal e Relíquias do Saturn:**
  - Botas da Velocidade (*God Speed Shoes - Relíquia 30*).
  - Terceira Mão / Holder (*Item rápido no botão L2*).
  - Efeito Dinâmico de Cor no Manto do Arco-Íris.
- **Seleção de Dublagens:** PSP Dracula X Chronicles, PS1 Original e Japonês com legendas.

---

# pre-release 1.1.2 (this is a pre-release for 1.0.0)
this is a pre-release, it has a lot of new stuff and wasnt tested as much as a full release, so expect some bugs, please report anything you find!

heads up: the settings system was rewritten, so your settings from previous versions are reset to the defaults

# Changes
- added language packs (put them in the translations folder)
- added a game text language option, separate from the interface language
- added room extension (settings > display), fills the area outside the room on widescreen with custom fills, a color fill, or tun off
- added room fills for most rooms of the castle, some still need revision tho
- added support for slang/libretro shader presets through ShaderMod
- added the Asset Pack manager and an in-game Asset Pack Editor to create and edit packs
- texture replacement now works properly on alucard, enemies, items, hud and tilesets with a custom provider (games sends this data weirdly by default)
- added true 24bit color option
- added option to control the display of the dithering
- added autohide for top bar
- added autohide mouse after 5 seconds
- added a crash popup that shows where the crash log was saved
- added skip prologue to the QoL options
- disabling letterbox now zooms the image to cover the bars instead of removing the bars
- fixed water in the underground caverns, reverse caverns, castle entrance and scylla fight in widescreen
- fixed lava glow in the catacombs in widescreen
- fixed trees and foliage in the castle entrance intro in widescreen
- fixed the castle destruction effect in the prologue in widescreen
- fixed static backgrounds ending at the 4:3 border in some rooms
- fixed characters disappearing early near the screen edges, they now fade
- richter's hud now follows the widescreen margin correctly
- fixed the game over screen melting effect being corrupted without letterbox, the game over image being stretched and the title screen corrupting after it
- fixed richter not being able to slide near the screen edges in widescreen
- fixed richter's whip, subweapons and effects disappearing near the screen edges in widescreen
- fixed richter's cross, rebound stone and agunea disappearing before leaving the screen
- fixed a one frame jump on horizontal room transitions
- improved stability overhaul
- improved other widescreen effects

Just to reinterate, this is a pre-release version, some stuff is not yet fully tested or finished

# Dependencies

You're required to install [dotnet 10 runtime](https://dotnet.microsoft.com/pt-br/download/dotnet/10.0) to run the recomp, otherwise the game will not open, ideally also make sure to install [OpenAL](https://www.openal.org/downloads/), as of the current version no further dependencies are required

you can find instructions downloading dotnet 10 for linux systems that dont have it on their packages [here](https://wiki.archlinux.org/title/.NET), or you can run "run.sh" wich will install dotent if not available and execute sotn for you
