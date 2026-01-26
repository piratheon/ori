#!/bin/bash

# Build script for Ori Assistant
# Supports Debian/Ubuntu, Fedora, and Arch checks for required packages

set -e

detect_and_offer_install() {
    # Called with package manager i1d ($1) and array of packages (rest)
    pm="$1"; shift
    pkgs=("$@")
    missing=()

    case "$pm" in
        apt)
            for pkg in "${pkgs[@]}"; do
                if ! dpkg -s "$pkg" &> /dev/null; then
                    missing+=("$pkg")
                fi
            done
            ;;
        dnf)
            for pkg in "${pkgs[@]}"; do
                if ! rpm -q "$pkg" &> /dev/null; then
                    missing+=("$pkg")
                fi
            done
            ;;
        pacman)
            for pkg in "${pkgs[@]}"; do
                if ! pacman -Qi "$pkg" &> /dev/null; then
                    missing+=("$pkg")
                fi
            done
            ;;
    esac

    if [ ${#missing[@]} -gt 0 ]; then
        echo "The following required packages are missing: ${missing[*]}"
        read -p "Do you want to install them now? (y/n): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            case "$pm" in
                apt)
                    sudo apt-get update
                    sudo apt-get install -y "${missing[@]}"
                    ;;
                dnf)
                    sudo dnf install -y "${missing[@]}"
                    ;;
                pacman)
                    sudo pacman -Syu --noconfirm "${missing[@]}"
                    ;;
            esac
        else
            echo "Please install the missing packages manually to proceed."
            exit 1
        fi
    fi
}

echo "Detecting distribution and package manager..."

PM="none"
if command -v apt-get >/dev/null 2>&1; then
    PM="apt"
elif command -v dnf >/dev/null 2>&1; then
    PM="dnf"
elif command -v pacman >/dev/null 2>&1; then
    PM="pacman"
fi

echo "Package manager detected: $PM"

# Define required packages per distro
case "$PM" in
    apt)
        REQUIRED_PACKAGES=(libjsoncpp-dev libcurl4-openssl-dev build-essential cmake)
        detect_and_offer_install apt "${REQUIRED_PACKAGES[@]}"
        ;;
    dnf)
        # On Fedora package names: jsoncpp-devel libcurl-devel @development-tools cmake
        REQUIRED_PACKAGES=(jsoncpp-devel libcurl-devel "@development-tools" cmake)
        detect_and_offer_install dnf "${REQUIRED_PACKAGES[@]}"
        ;;
    pacman)
        # Arch packages: jsoncpp curl base-devel cmake
        REQUIRED_PACKAGES=(jsoncpp curl base-devel cmake)
        detect_and_offer_install pacman "${REQUIRED_PACKAGES[@]}"
        ;;
    *)
        echo "No supported package manager detected (apt, dnf, pacman). Skipping automatic dependency checks."
        ;;
esac

# Create build directory if it doesn't exist
echo "Creating build directory..."
mkdir -p build

# Change to build directory
cd build

# Configure with CMake
echo "Configuring with CMake..."
cmake ..

# Build the project
echo "Building Ori Assistant..."
make -j"$(nproc)"

echo "Build successful!"
echo "The 'ori' executable is now in the 'build' directory."