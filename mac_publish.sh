#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

# ANSI Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
BOLD='\033[1m'
NC='\033[0m'

say() { printf "${CYAN}${BOLD}[SymphonyRecomp macOS]${NC} %s\n" "$1"; }
ok()  { printf "${GREEN}${BOLD}[✓]${NC} %s\n" "$1"; }
warn(){ printf "${YELLOW}${BOLD}[!]${NC} %s\n" "$1"; }
die() { printf "${RED}${BOLD}[ERROR] %s${NC}\n" "$1" >&2; exit 1; }

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
    die ".NET 10 SDK não encontrado. Execute ./mac_initial_build.sh primeiro para instalar."
fi

if [ ! -d "$SCRIPT_DIR/generated" ]; then
    die "A pasta 'generated/' não existe. Você precisa colocar o jogo em 'disc/' e executar ./mac_initial_build.sh primeiro."
fi

TARGET="${1:-all}"
DIST_DIR="$SCRIPT_DIR/dist"
mkdir -p "$DIST_DIR"

publish_target() {
    local rid="$1"
    local name="$2"
    local out="$DIST_DIR/$name"

    say "Publicando versão $name ($rid)..."
    rm -rf "$out"
    mkdir -p "$out"

    "$DOTNET_CMD" publish "$SCRIPT_DIR/RecompOne.SoTN.csproj" \
        -c Release \
        -r "$rid" \
        --self-contained true \
        -p:PublishSingleFile=false \
        -o "$out"

    # Copiar pastas essenciais de dados se existirem
    for d in assets config disc mods patches; do
        if [ -d "$SCRIPT_DIR/$d" ]; then
            cp -R "$SCRIPT_DIR/$d" "$out/"
        fi
    done

    # Permissão de execução no binário
    if [ -f "$out/sotn" ]; then
        chmod +x "$out/sotn"
    fi

    ok "Publicação de $name concluída em: $out"
}

case "$TARGET" in
    arm64)
        publish_target "osx-arm64" "SymphonyRecomp-macOS-AppleSilicon"
        ;;
    x64|intel)
        publish_target "osx-x64" "SymphonyRecomp-macOS-Intel"
        ;;
    all|universal)
        publish_target "osx-arm64" "SymphonyRecomp-macOS-AppleSilicon"
        publish_target "osx-x64" "SymphonyRecomp-macOS-Intel"

        # Criar pacote Universal 2 combinando executáveis com lipo
        say "Criando binário Universal 2 (Apple Silicon + Intel)..."
        UNI_DIR="$DIST_DIR/SymphonyRecomp-macOS-Universal"
        rm -rf "$UNI_DIR"
        cp -R "$DIST_DIR/SymphonyRecomp-macOS-AppleSilicon" "$UNI_DIR"

        if command -v lipo >/dev/null 2>&1; then
            lipo -create \
                "$DIST_DIR/SymphonyRecomp-macOS-AppleSilicon/sotn" \
                "$DIST_DIR/SymphonyRecomp-macOS-Intel/sotn" \
                -output "$UNI_DIR/sotn"
            chmod +x "$UNI_DIR/sotn"
            ok "Binário Universal criado com sucesso em: $UNI_DIR/sotn"
            say "Arquiteturas do executável Universal:"
            file "$UNI_DIR/sotn"
        fi
        ;;
    *)
        echo "Uso: ./mac_publish.sh [arm64 | x64 | all]"
        exit 1
        ;;
esac

ok "Processo finalizado! Os binários estão disponíveis na pasta 'dist/'."
