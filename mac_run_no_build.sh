#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

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
    echo "[ERROR] .NET 10 SDK não encontrado." >&2
    exit 1
fi

exec "$DOTNET_CMD" run --project "$SCRIPT_DIR/RecompOne.SoTN.csproj" -c Release --no-build -- "$@"
