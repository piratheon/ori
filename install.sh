#!/bin/bash

# ORI Terminal Assistant Installation Script

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

# Check for pacman
if [ -f "/usr/bin/pacman" ]; then
    echo -e "${YELLOW}pacman detected. Building Arch Linux package...${NC}"
    make package
    echo -e "${GREEN}Package created. You can now install it with 'sudo pacman -U ori-*.pkg.tar.zst'${NC}"
else
    echo -e "${YELLOW}pacman not detected. Performing standard installation...${NC}"
    echo "This script will prompt for your password to install files."
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