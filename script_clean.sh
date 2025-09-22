#!/bin/bash
# ===============================================
# Unix Clean Script (Mac & Ubuntu)
# ===============================================

echo ""
echo "==============================================="
echo "  Cleaning Build Directory - GrapeEngine"
echo "==============================================="
echo ""

# Clean build directory
if [ -d "build" ]; then
    echo "Removing build directory..."
    rm -rf build
    echo "Build directory removed successfully!"
else
    echo "No build directory found - nothing to clean."
fi

echo ""
echo "Clean completed!"