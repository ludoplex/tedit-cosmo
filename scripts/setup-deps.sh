#!/bin/bash
# Setup dependencies for tedit-cosmo

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

echo "Setting up tedit-cosmo dependencies..."
cd "$PROJECT_DIR"

# Clone cimgui
if [ ! -d "deps/cimgui/imgui" ]; then
    echo "Cloning cimgui..."
    rm -rf deps/cimgui
    git clone --recursive https://github.com/ludoplex/cimgui.git deps/cimgui
else
    echo "cimgui already present"
fi

# Check for GLFW
if ! pkg-config --exists glfw3 2>/dev/null; then
    echo ""
    echo "WARNING: GLFW3 not found. Install it with:"
    echo "  Ubuntu/Debian: sudo apt install libglfw3-dev"
    echo "  macOS: brew install glfw"
    echo "  Arch: sudo pacman -S glfw"
fi

# Check for cosmocc
if ! command -v cosmocc &>/dev/null; then
    echo ""
    echo "WARNING: cosmocc not found. Download from:"
    echo "  https://cosmo.zip/pub/cosmocc/"
    echo ""
    echo "Then add to PATH:"
    echo "  export PATH=\$HOME/cosmos/bin:\$PATH"
fi

echo ""
echo "Setup complete!"
echo "Build with: make cli  (CLI-only, portable)"
echo "Build with: make gui  (GUI with cimgui, requires GLFW)"

