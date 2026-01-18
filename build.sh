#!/bin/bash

# Build script for Ori Assistant

# Exit on error
set -e

# Detect Debian/Ubuntu and check for required packages
if [ -f /etc/os-release ]; then
    . /etc/os-release
    if [[ "$ID" == "debian" || "$ID" == "ubuntu" ]]; then
        echo "Debian/Ubuntu system detected. Checking for required packages..."
        REQUIRED_PACKAGES=("libjsoncpp-dev" "libcurl4-openssl-dev")
        MISSING_PACKAGES=()

        for pkg in "${REQUIRED_PACKAGES[@]}"; do
            if ! dpkg -s "$pkg" &> /dev/null; then
                MISSING_PACKAGES+=("$pkg")
            fi
        done

        if [ ${#MISSING_PACKAGES[@]} -gt 0 ]; then
            echo "The following required packages are missing: ${MISSING_PACKAGES[*]}"
            read -p "Do you want to install them now? (y/n): " -n 1 -r
            echo
            if [[ $REPLY =~ ^[Yy]$ ]]; then
                sudo apt-get update
                sudo apt-get install -y "${MISSING_PACKAGES[@]}"
            else
                echo "Please install the missing packages manually to proceed:"
                echo "  sudo apt-get install ${MISSING_PACKAGES[*]}"
                exit 1
            fi
        fi
    fi
fi

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
make

echo "Build successful!"
echo "The 'ori' executable is now in the 'build' directory."