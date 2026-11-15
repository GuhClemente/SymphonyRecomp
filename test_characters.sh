#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

MODE="${1:-quick}"
SHOT_DIR="${2:-/tmp/sotn-shots}"

mkdir -p "$SHOT_DIR"

case "$MODE" in
    quick)
        echo "=========================================================="
        echo "🚀 Rodando Teste Rápido dos 5 Personagens (Hot-Swap)..."
        echo "   Capturas em: $SHOT_DIR"
        echo "=========================================================="
        SOTN_TEST_MARIA=1 SOTN_TEST_QUICK=1 SOTN_TEST_DIR="$SHOT_DIR" "$SCRIPT_DIR/mac_run_no_build.sh"
        ;;

    full)
        echo "=========================================================="
        echo "🎮 Rodando Teste Completo (103 Passos, Golpes e Especiais)..."
        echo "   Capturas em: $SHOT_DIR"
        echo "=========================================================="
        SOTN_TEST_MARIA=1 SOTN_TEST_DIR="$SHOT_DIR" "$SCRIPT_DIR/mac_run_no_build.sh"
        ;;

    teleport)
        LOC_FILTER="${3:-}"
        echo "=========================================================="
        echo "🌌 Rodando Teste de Teleporte (10 destinos, todos os personagens)..."
        echo "   Capturas em: $SHOT_DIR"
        if [ -n "$LOC_FILTER" ]; then
            echo "   Filtro de Local: $LOC_FILTER"
        fi
        echo "=========================================================="
        SOTN_TEST_MARIA=1 SOTN_TEST_TELEPORT=1 SOTN_TEST_LOCATION="$LOC_FILTER" SOTN_TEST_DIR="$SHOT_DIR" "$SCRIPT_DIR/mac_run_no_build.sh"
        ;;

    teleport-all)
        echo "Teleporte pelos 28 destinos do Fast Travel (sem troca de personagem). Capturas em: $SHOT_DIR"
        SOTN_TEST_MARIA=1 SOTN_TEST_TELEPORT=1 SOTN_TEST_ALL=1 SOTN_TEST_PROBE=1 SOTN_TEST_DIR="$SHOT_DIR" "$SCRIPT_DIR/mac_run_no_build.sh"
        ;;

    probe)
        # ./test_characters.sh probe destinos.txt [pasta]   (codigo,Stage,roomX,roomY,offsetX,offsetY por linha)
        LOC_FILE="${2:?arquivo de destinos}"
        SHOT_DIR="${3:-/tmp/sotn-shots}"
        mkdir -p "$SHOT_DIR"
        echo "Sondagem dos destinos de $LOC_FILE. Capturas em: $SHOT_DIR"
        SOTN_TEST_MARIA=1 SOTN_TEST_TELEPORT=1 SOTN_TEST_PROBE=1 SOTN_TEST_LOCFILE="$LOC_FILE" SOTN_TEST_DIR="$SHOT_DIR" "$SCRIPT_DIR/mac_run_no_build.sh"
        ;;

    view)
        echo "🖼️  Abrindo diretório de capturas: $SHOT_DIR"
        open "$SHOT_DIR"
        exit 0
        ;;

    *)
        echo "Uso: ./test_characters.sh [quick | full | teleport | teleport-all | probe | view] [pasta_de_capturas]"
        echo "Exemplos:"
        echo "  ./test_characters.sh quick       # Rápido: troca todos os 5 personagens (~1 min)"
        echo "  ./test_characters.sh teleport    # 10 Lugares Homologados: teleporte + todos os personagens"
        echo "  ./test_characters.sh full        # Completo: 103 ações, magias e especiais"
        echo "  ./test_characters.sh view        # Abre a pasta de fotos tiradas"
        exit 1
        ;;
esac

echo ""
echo "✅ Teste finalizado com sucesso! Capturas salvas em: $SHOT_DIR"
