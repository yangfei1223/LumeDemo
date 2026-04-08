#!/bin/bash

# LumeDemo macOS Build Script
# This script configures and builds LumeDemo for macOS

set -e  # Exit on error

echo "=========================================="
echo "LumeDemo macOS Build Script"
echo "=========================================="

# Check if running on macOS
if [[ "$OSTYPE" != "darwin"* ]]; then
    echo "Error: This script is for macOS only"
    exit 1
fi

# Check for required tools
echo "Checking prerequisites..."

if ! command -v cmake &> /dev/null; then
    echo "Error: CMake not found. Install with: brew install cmake"
    exit 1
fi

if ! command -v git &> /dev/null; then
    echo "Error: Git not found"
    exit 1
fi

# Check for Vulkan SDK
if [[ -z "$VULKAN_SDK" ]]; then
    echo "Warning: VULKAN_SDK environment variable not set"
    echo "Please install Vulkan SDK for macOS from:"
    echo "https://vulkan.lunarg.com/sdk/home"
    echo ""
    echo "Or set VULKAN_SDK manually:"
    echo "export VULKAN_SDK=/path/to/vulkan/sdk"
    exit 1
fi

echo "VULKAN_SDK: $VULKAN_SDK"

# Create build directory
BUILD_DIR="build_macos"
if [ -d "$BUILD_DIR" ]; then
    echo "Cleaning existing build directory..."
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo ""
echo "Configuring with CMake..."
echo "=========================================="

# Configure with CMake
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_STANDARD=17 \
    -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
    -DVulkan_INCLUDE_DIR="$VULKAN_SDK/include" \
    -DVulkan_LIBRARY="$VULKAN_SDK/lib/libvulkan.dylib" \
    2>&1 | tee cmake_configure.log

if [ $? -ne 0 ]; then
    echo "Error: CMake configuration failed"
    echo "Check cmake_configure.log for details"
    exit 1
fi

echo ""
echo "Building..."
echo "=========================================="

# Build
cmake --build . --config Release -j$(sysctl -n hw.ncpu) 2>&1 | tee build.log

if [ $? -ne 0 ]; then
    echo "Error: Build failed"
    echo "Check build.log for details"
    exit 1
fi

echo ""
echo "=========================================="
echo "Build completed successfully!"
echo "=========================================="
echo ""
echo "Executable location:"
echo "  ./Release/LumeDemo"
echo ""
echo "To run:"
echo "  cd $BUILD_DIR/Release"
echo "  ./LumeDemo"
