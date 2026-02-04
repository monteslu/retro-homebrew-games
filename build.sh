#!/bin/bash
# Build script for free-retro-games
# Builds all games or specific system/game

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# SGDK Docker image
SGDK_IMAGE="ghcr.io/stephane-d/sgdk:latest"

build_genesis_game() {
    local game_dir="$1"
    local game_name=$(basename "$game_dir")
    
    echo -e "${YELLOW}Building Genesis: ${game_name}${NC}"
    
    if [ ! -d "$game_dir/src" ]; then
        echo -e "${RED}  No src/ directory found, skipping${NC}"
        return 1
    fi
    
    # Build with SGDK Docker
    docker run --rm \
        -v "$game_dir:/src" \
        -w /src \
        "$SGDK_IMAGE" \
        make -f /sgdk/makefile.gen
    
    if [ -f "$game_dir/out/rom.bin" ]; then
        echo -e "${GREEN}  ✓ Built: $game_dir/out/rom.bin${NC}"
        
        # Copy to dist
        mkdir -p "$SCRIPT_DIR/dist/genesis"
        cp "$game_dir/out/rom.bin" "$SCRIPT_DIR/dist/genesis/${game_name}.bin"
        return 0
    else
        echo -e "${RED}  ✗ Build failed${NC}"
        return 1
    fi
}

build_all_genesis() {
    echo -e "${YELLOW}=== Building all Genesis games ===${NC}"
    
    local count=0
    local failed=0
    
    for game in genesis/*/; do
        if [ -d "$game" ]; then
            if build_genesis_game "$game"; then
                ((count++))
            else
                ((failed++))
            fi
        fi
    done
    
    echo ""
    echo -e "${GREEN}Built: $count${NC}, ${RED}Failed: $failed${NC}"
}

build_all() {
    echo -e "${YELLOW}=== Building all games ===${NC}"
    echo ""
    
    # Genesis
    if [ -d "genesis" ]; then
        build_all_genesis
    fi
    
    # NES (TODO)
    # if [ -d "nes" ]; then
    #     build_all_nes
    # fi
    
    echo ""
    echo -e "${GREEN}=== Build complete ===${NC}"
    echo "ROMs are in: dist/"
    ls -la dist/*/ 2>/dev/null || true
}

# Check Docker
if ! command -v docker &> /dev/null; then
    echo -e "${RED}Error: Docker is required but not installed${NC}"
    echo "Install Docker: https://docs.docker.com/get-docker/"
    exit 1
fi

# Parse arguments
case "${1:-all}" in
    genesis)
        if [ -n "$2" ]; then
            # Build specific game
            build_genesis_game "genesis/$2"
        else
            build_all_genesis
        fi
        ;;
    all)
        build_all
        ;;
    help|--help|-h)
        echo "Usage: $0 [system] [game]"
        echo ""
        echo "Examples:"
        echo "  $0              # Build all games"
        echo "  $0 genesis      # Build all Genesis games"
        echo "  $0 genesis tank-battle  # Build specific game"
        echo ""
        echo "Systems: genesis (more coming)"
        ;;
    *)
        echo -e "${RED}Unknown system: $1${NC}"
        echo "Run '$0 help' for usage"
        exit 1
        ;;
esac
