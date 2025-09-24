#!/bin/bash
# ===============================================
# Unix Build and Run Script (Mac & Ubuntu)
# ===============================================

set -e  # Exit on any error

echo ""
echo "==============================================="
echo "  GrapeEngine - Unix Build and Run"
echo "==============================================="
echo ""

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Check if CMake is available
if ! command_exists cmake; then
    echo "ERROR: CMake is not installed"
    echo "Please install CMake:"
    echo "  Ubuntu/Debian: sudo apt-get install cmake"
    echo "  macOS: brew install cmake"
    echo "  Or download from: https://cmake.org/download/"
    exit 1
fi

# Check if make is available
if ! command_exists make; then
    echo "ERROR: Make is not installed"
    echo "Please install build-essential:"
    echo "  Ubuntu/Debian: sudo apt-get install build-essential"
    echo "  macOS: xcode-select --install"
    exit 1
fi

# Check if g++ is available
if ! command_exists g++; then
    echo "ERROR: g++ compiler is not installed"
    echo "Please install g++:"
    echo "  Ubuntu/Debian: sudo apt-get install g++"
    echo "  macOS: xcode-select --install"
    exit 1
fi

echo "Build tools check passed!"
echo ""

# Clean previous build
echo "[1/4] Cleaning previous build..."
if [ -d "build" ]; then
    rm -rf build
    echo "   Previous build directory removed"
else
    echo "   No previous build found"
fi

# Create build directory
echo "[2/4] Creating build directory..."
mkdir build
cd build

# Configure with CMake
echo "[3/4] Configuring project with CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Debug
if [ $? -ne 0 ]; then
    echo "ERROR: CMake configuration failed"
    cd ..
    exit 1
fi

# Build the project
echo "[4/4] Building project..."
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
if [ $? -ne 0 ]; then
    echo "ERROR: Build failed"
    cd ..
    exit 1
fi

echo ""
echo "==============================================="
echo "  Build completed successfully!"
echo "==============================================="
echo ""

# Run the application
echo "Running GrapeEngine..."
echo ""
./GrapeEngine
if [ $? -ne 0 ]; then
    echo ""
    echo "Application exited with error code: $?"
else
    echo ""
    echo "Application completed successfully!"
fi

echo ""
echo "Script completed!"