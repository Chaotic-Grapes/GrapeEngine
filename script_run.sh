#!/bin/bash
# ===============================================
# Unix Run Script (Mac & Ubuntu)
# ===============================================

echo ""
echo "==============================================="
echo "  Running GrapeEngine - Unix"
echo "==============================================="
echo ""

# Check if build directory exists
if [ ! -d "build" ]; then
    echo "ERROR: Build directory not found!"
    echo "Please run ./script_build_and_run.sh first to build the project."
    exit 1
fi

# Check if executable exists
if [ ! -f "build/GrapeEngine" ]; then
    echo "ERROR: Executable not found!"
    echo "Please run ./script_build_and_run.sh first to build the project."
    exit 1
fi

echo "Running GrapeEngine..."
echo ""
cd build
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