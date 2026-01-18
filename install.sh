#!/bin/bash

# ORI Terminal Assistant Installation Script
# Supports Debian/Ubuntu, Fedora, Arch

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}ORI Terminal Assistant Installation Script${NC}"
echo "================================================="

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ] || [ ! -f "src/main.cpp" ]; then
    echo -e "${RED}Error: Please run this script from the project's root directory${NC}"
    exit 1
fi

# Helper: run package manager install
install_deps_apt() {
    echo -e "${YELLOW}Detected apt (Debian/Ubuntu). Installing dependencies...${NC}"
    sudo apt-get update
    sudo apt-get install -y build-essential cmake libjsoncpp-dev libcurl4-openssl-dev
}

install_deps_dnf() {
    echo -e "${YELLOW}Detected dnf (Fedora). Installing dependencies...${NC}"
    sudo dnf install -y @development-tools cmake jsoncpp-devel libcurl-devel
}

install_deps_pacman() {
    echo -e "${YELLOW}Detected pacman (Arch). Installing dependencies...${NC}"
    sudo pacman -Syu --noconfirm base-devel cmake jsoncpp curl
}

# Detect distro/package manager and install deps if possible
if command -v apt-get >/dev/null 2>&1; then
    install_deps_apt
    PM="apt"
elif command -v dnf >/dev/null 2>&1; then
    install_deps_dnf
    PM="dnf"
elif command -v pacman >/dev/null 2>&1; then
    install_deps_pacman
    PM="pacman"
else
    echo -e "${YELLOW}No supported package manager detected (apt, dnf, pacman). Skipping automatic dependency installation.${NC}"
    PM="none"
fi

# Build step (out-of-source build)
echo -e "${BLUE}Configuring and building project...${NC}"
BUILD_DIR="build"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake ..
make -j"$(nproc)"
cd ..

# Packaging for Arch if pacman present
if [ "$PM" = "pacman" ]; then
    echo -e "${YELLOW}pacman detected. Building Arch Linux package...${NC}"
    if command -v makepkg >/dev/null 2>&1; then
        # If PKGBUILD exists, prefer makepkg; otherwise fallback to make package if provided by Makefile
        if [ -f "PKGBUILD" ]; then
            echo -e "${BLUE}Using makepkg to build package from PKGBUILD...${NC}"
            (cd "$BUILD_DIR" && makepkg --printsrcinfo > /dev/null 2>&1) || true
            makepkg -si --noconfirm
            echo -e "${GREEN}Package built/installed via makepkg.${NC}"
        else
            make package || true
            echo -e "${GREEN}Package created. You can install it with 'sudo pacman -U ori-*.pkg.tar.zst'${NC}"
        fi
    else
        make package || true
        echo -e "${GREEN}Package created. You can install it with 'sudo pacman -U ori-*.pkg.tar.zst'${NC}"
    fi
else
    echo -e "${YELLOW}Performing standard installation (make install)...${NC}"
    sudo make install
    echo -e "${GREEN}Installation complete!${NC}"
fi

# Create user configuration directory
echo -e "${YELLOW}Creating user configuration directory...${NC}"
mkdir -p "$HOME/.config/ori"

echo ""
echo -e "${GREEN}================================================="
echo -e "ORI Terminal Assistant is now installed!${NC}"
echo -e "=================================================${NC}"
echo ""
echo -e "${BLUE}Usage:${NC}"
echo "ori --help"
