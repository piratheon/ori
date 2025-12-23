#!/bin/bash

# Build script for Ori Assistant

# Exit on error
set -e

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