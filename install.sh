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

# Version resolution function
get_version() {
    if [ -n "${ORI_VERSION:-}" ]; then
        echo "$ORI_VERSION"
    elif [ -n "${VERSION:-}" ]; then
        echo "$VERSION"
    elif [ -f ".version" ] && [ -n "$(tr -d ' \n\r\t' < .version 2>/dev/null)" ]; then
        tr -d ' \n\r\t' < .version
    else
        echo "0.0"
    fi
}

VERSION=$(get_version)
# The resolved version is handed to CMake explicitly via -DORI_VERSION below;
# CMakeLists.txt gives that flag priority over the environment and .version.

echo -e "${BLUE}ORI Terminal Assistant Installation Script (v${VERSION})${NC}"
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
    sudo apt-get install -y build-essential cmake libcurl4-openssl-dev
}

install_deps_dnf() {
    echo -e "${YELLOW}Detected dnf (Fedora). Installing dependencies...${NC}"
    sudo dnf install -y @development-tools cmake libcurl-devel
}

install_deps_pacman() {
    echo -e "${YELLOW}Detected pacman (Arch). Installing dependencies...${NC}"
    sudo pacman -Syu --noconfirm base-devel cmake curl
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
echo -e "${BLUE}Configuring and building project (v${VERSION})...${NC}"
BUILD_DIR="build"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake .. -DORI_VERSION="${VERSION}"
make -j"$(nproc)"
cd ..

# Packaging for Arch if pacman present
if [ "$PM" = "pacman" ]; then
    echo -e "${YELLOW}pacman detected. Building Arch Linux package...${NC}"
    if command -v makepkg >/dev/null 2>&1; then
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
    sudo make install -C "$BUILD_DIR"
    echo -e "${GREEN}Installation complete!${NC}"
fi

# Create user configuration directory
echo -e "${YELLOW}Creating user configuration directory...${NC}"
mkdir -p "$HOME/.config/ori"

echo ""
echo -e "${GREEN}================================================="
echo -e "ORI Terminal Assistant v${VERSION} is now installed!${NC}"
echo -e "=================================================${NC}"
echo ""
echo -e "${BLUE}Usage:${NC}"
echo "ori --help"
