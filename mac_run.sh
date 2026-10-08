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
    warn ".NET 10 SDK não foi detectado no PATH."
    say "Iniciando assistente de build inicial..."
    ./mac_initial_build.sh nocredits
    exit 0
fi

if [ ! -d "$SCRIPT_DIR/generated" ]; then
    warn "A pasta 'generated/' ainda não foi criada. Executando o build inicial..."
    ./mac_initial_build.sh nocredits
fi

say "Iniciando Castlevania: Symphony of the Night..."
exec "$DOTNET_CMD" run --project "$SCRIPT_DIR/RecompOne.SoTN.csproj" -c Release -- "$@"
