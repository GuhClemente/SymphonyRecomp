#!/usr/bin/env bash
set -euo pipefail

# Directory of this script
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

DISC_DIR="$SCRIPT_DIR/disc"
CONFIG_DIR="$SCRIPT_DIR/config"
RECOMPONE_DIR="$SCRIPT_DIR/RecompOne"
RECOMPILER_PROJ="$RECOMPONE_DIR/RecompOne.Recompiler"
SOTN_JSON="$CONFIG_DIR/sotn.json"

# ANSI Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
BOLD='\033[1m'
NC='\033[0m' # No Color

say() { printf "${CYAN}${BOLD}[SymphonyRecomp macOS]${NC} %s\n" "$1"; }
ok()  { printf "${GREEN}${BOLD}[✓]${NC} %s\n" "$1"; }
warn(){ printf "${YELLOW}${BOLD}[!]${NC} %s\n" "$1"; }
die() { printf "${RED}${BOLD}[ERROR] %s${NC}\n" "$1" >&2; exit 1; }

# ============================================= Credits =============================================
if [ "${1:-}" != "nocredits" ]; then
    cat << "EOF"
================================================================================
                    🦇 SymphonyRecomp - macOS Port (Apple Silicon / Intel) 🦇
================================================================================
Este projeto é baseado no trabalho incrível da equipe BlackLabelHQ e comunidade:
  - flaffymg: Criador da ferramenta "RecompOne", mods, patches e arquitetura
  - Derp Princess: Centenas de correções, mods, lore e refinamento
  - wowjinxy: Versão inicial do SymphonyRecomp
  - Mottzilla0 & eldri7ch: Modos QoL e Randomizer integrado
  - Decomp Team: Contribuidores do projeto de decompilação do SOTN
================================================================================
EOF
fi

# ================================= Did You Mean To Run This File? ==================================
if [ -d "$SCRIPT_DIR/generated" ] && [ "${1:-}" != "force" ] && [ "${1:-}" != "nocredits" ]; then
    warn "A pasta 'generated/' já existe. O projeto provavelmente já foi inicializado."
    read -p "Deseja reexecutar a recompilação completa? (s/N): " -r answer
    case "$answer" in
        [sS][iI][mM]|[sS]|[yY][eE][sS]|[yY])
            say "Continuando recompilação..."
            ;;
        *)
            say "Operação cancelada."
            exit 0
            ;;
    esac
fi

# ======================================== Update Submodules ========================================
if [ -f "$SCRIPT_DIR/.gitmodules" ]; then
    say "Verificando submódulos Git..."
    git submodule update --init --recursive 2>/dev/null || warn "Aviso: Submódulos não atualizados, continuando se os arquivos já existirem."
fi

# ======================================== Check for .NET 10+ ========================================
say "Verificando instalação do .NET 10 SDK..."

has_dotnet10_sdk() {
    local cmd="$1"
    if [ -x "$cmd" ]; then
        if "$cmd" --list-sdks 2>/dev/null | grep -qE '^10\.'; then
            return 0
        fi
    fi
    return 1
}

DOTNET_CMD=""

if command -v dotnet >/dev/null 2>&1 && has_dotnet10_sdk "$(command -v dotnet)"; then
    DOTNET_CMD="$(command -v dotnet)"
elif has_dotnet10_sdk "$HOME/.dotnet/dotnet"; then
    DOTNET_CMD="$HOME/.dotnet/dotnet"
    export DOTNET_ROOT="$HOME/.dotnet"
    export PATH="$HOME/.dotnet:$PATH"
fi

if [ -z "$DOTNET_CMD" ]; then
    warn ".NET 10 SDK não encontrado no PATH nem em ~/.dotnet."
    say "Deseja que o script baixe e instale o .NET 10 SDK automaticamente em ~/.dotnet? (Recomendado)"
    read -p "Instalar .NET 10 SDK agora? (S/n): " -r install_ans
    case "$install_ans" in
        [nN][ãÃaA][oO]|[nN])
            die "O .NET 10 SDK é obrigatório para compilar e executar o SymphonyRecomp. Baixe em https://dotnet.microsoft.com/download/dotnet/10.0"
            ;;
        *)
            say "Baixando o instalador oficial da Microsoft..."
            INSTALL_SCRIPT="$(mktemp)"
            curl -fsSL https://dot.net/v1/dotnet-install.sh -o "$INSTALL_SCRIPT" || die "Falha no download do instalador."
            say "Instalando .NET 10 SDK em $HOME/.dotnet..."
            bash "$INSTALL_SCRIPT" --channel 10.0 --install-dir "$HOME/.dotnet" || die "Falha ao instalar o .NET 10 SDK."
            rm -f "$INSTALL_SCRIPT"
            export DOTNET_ROOT="$HOME/.dotnet"
            export PATH="$HOME/.dotnet:$PATH"
            DOTNET_CMD="$HOME/.dotnet/dotnet"
            ;;
    esac
fi

ok "Usando .NET: $("$DOTNET_CMD" --version)"

# ========================================= Build RecompOne =========================================
say "Compilando RecompOne (Recompiler & Runtime)..."
"$DOTNET_CMD" build "$RECOMPONE_DIR/RecompOne.sln" -c Release || die "Falha ao compilar RecompOne."
ok "RecompOne compilado com sucesso!"

# ======================================== Check Disc Files =========================================
say "Verificando arquivos da imagem de disco em '$DISC_DIR'..."

MISSING=0
CUE_FILE="$DISC_DIR/Castlevania - Symphony of the Night (USA).cue"

if [ ! -f "$CUE_FILE" ]; then
    warn "Arquivo ausente: $CUE_FILE"
    MISSING=1
fi

BIN_FOUND=0
if compgen -G "$DISC_DIR/*.bin" > /dev/null; then
    BIN_FOUND=1
fi

if [ "$BIN_FOUND" -eq 0 ]; then
    warn "Nenhum arquivo .bin encontrado em $DISC_DIR"
    MISSING=1
fi

if [ "$MISSING" -eq 1 ]; then
    printf "\n"
    printf "================================================================================\n"
    printf "${RED}${BOLD}ERRO: Arquivos de disco do Castlevania SOTN não encontrados!${NC}\n"
    printf "================================================================================\n"
    printf "Para gerar o código do jogo, você precisa colocar sua cópia original do jogo PS1\n"
    printf "(em formato BIN/CUE, versão americana NTSC-U) dentro da pasta:\n"
    printf "  ${CYAN}%s${NC}\n\n" "$DISC_DIR"
    printf "Arquivos necessários:\n"
    printf "  - ${YELLOW}Castlevania - Symphony of the Night (USA).cue${NC}\n"
    printf "  - ${YELLOW}Castlevania - Symphony of the Night (Track 1).bin${NC} (e Track 2 se aplicável)\n"
    printf "    ou a imagem .bin referenciada dentro do arquivo .cue.\n\n"
    printf "Depois de copiar os arquivos para a pasta 'disc', execute este script novamente:\n"
    printf "  ${GREEN}./mac_initial_build.sh${NC}\n"
    printf "================================================================================\n\n"
    exit 1
fi

ok "Arquivos de disco encontrados!"

# ========================================== Check Config ===========================================
say "Verificando arquivo de configuração..."
if [ ! -f "$SOTN_JSON" ]; then
    die "Arquivo de configuração não encontrado: $SOTN_JSON"
fi
ok "Arquivo sotn.json encontrado!"

# ======================================== Generate C# Code =========================================
say "Executando o RecompOne para extrair e recompilar o código do jogo..."
"$DOTNET_CMD" run --project "$RECOMPILER_PROJ" "$SOTN_JSON" || die "Falha na geração do código C# pelo RecompOne."
ok "Código C# gerado em '$SCRIPT_DIR/generated'!"

# ========================================= Build SymphonyRecomp ====================================
say "Compilando SymphonyRecomp nativo para macOS..."
"$DOTNET_CMD" build "$SCRIPT_DIR/RecompOne.SoTN.csproj" -c Release || die "Falha ao compilar SymphonyRecomp."
ok "SymphonyRecomp compilado com sucesso!"

cat << "EOF"

================================================================================
🎉 Parabéns! O SymphonyRecomp foi compilado com sucesso para macOS! 🎉
================================================================================
Para jogar agora ou em sessões futuras, use:
  ./mac_run.sh

Para fechar ou configurar atalhos, consulte GUIA_ATALHOS.md.
Aproveite Castlevania: Symphony of the Night no seu Mac!
================================================================================

EOF
