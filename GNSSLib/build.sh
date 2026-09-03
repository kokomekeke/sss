#!/bin/bash
# Build script for GNSSLib

set -e

echo "=============================================="
echo "GNSSLib - Build Script"
echo "=============================================="

# Create build directory
mkdir -p build
cd build

# Configure
echo ""
echo "Configuring..."
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=ON \
    -DBUILD_EXAMPLES=ON

# Build
echo ""
echo "Building..."
make -j$(nproc)

# Test
echo ""
echo "Running examples..."
./examples/basic_usage || true

# Install
echo ""
echo "Installing..."
sudo make install

echo ""
echo "=============================================="
echo "Build complete!"
echo "Library installed to /usr/local/lib"
echo "Headers installed to /usr/local/include/gnss"
echo "=============================================="