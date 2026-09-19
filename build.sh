#!/bin/bash

# Build script for Ori Assistant
# Supports Debian/Ubuntu, Fedora, and Arch checks for required packages

set -e

# Version resolution function:
# 1. Environment variable ORI_VERSION or VERSION
# 2. Text file .version in project root
# 3. Fallback to 0.0
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

# Handle arguments
CLEAN_BUILD=false
for arg in "$@"; do
    case "$arg" in
        --clean)
            CLEAN_BUILD=true
            ;;
        --help|-h)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --clean    Remove existing build directory before building"
            echo "  --help, -h Show this help message"
            exit 0
            ;;
    esac
done

if [ "$CLEAN_BUILD" = true ]; then
    echo "Cleaning build directory..."
    rm -rf build
fi

detect_and_offer_install() {
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
        install_pkgs=false
        if [ ! -t 0 ] || [ "${CI:-false}" = "true" ] || [ "${DEBIAN_FRONTEND:-}" = "noninteractive" ]; then
            echo "Non-interactive environment detected. Installing missing packages automatically..."
            install_pkgs=true
        else
            read -p "Do you want to install them now? (y/n): " -n 1 -r
            echo
            if [[ $REPLY =~ ^[Yy]$ ]]; then
                install_pkgs=true
            fi
        fi

        if [ "$install_pkgs" = true ]; then
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
        REQUIRED_PACKAGES=(libcurl4-openssl-dev build-essential cmake)
        detect_and_offer_install apt "${REQUIRED_PACKAGES[@]}"
        ;;
    dnf)
        REQUIRED_PACKAGES=(libcurl-devel "@development-tools" cmake)
        detect_and_offer_install dnf "${REQUIRED_PACKAGES[@]}"
        ;;
    pacman)
        REQUIRED_PACKAGES=(curl base-devel cmake)
        detect_and_offer_install pacman "${REQUIRED_PACKAGES[@]}"
        ;;
    *)
        echo "No supported package manager detected (apt, dnf, pacman). Skipping automatic dependency checks."
        ;;
esac

echo "Building Ori Assistant v${VERSION}..."

mkdir -p build
cd build

echo "Configuring with CMake..."
cmake .. -DORI_VERSION="${VERSION}"

echo "Building Ori Assistant..."
make -j"$(nproc)"

echo "Build successful!"
echo "The 'ori' executable (v${VERSION}) is now in the 'build' directory."
