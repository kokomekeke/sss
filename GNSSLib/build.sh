#!/bin/bash
# ============================================================================
# Sisi/GNSSLib/build.sh
# Linux/macOS build script for GNSSLib
# ============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

# ---- Configuration ----------------------------------------------------------

BUILD_TYPE="${BUILD_TYPE:-Release}"
BUILD_DIR="${BUILD_DIR:-build}"
INSTALL_PREFIX="${INSTALL_PREFIX:-/usr/local}"
JOBS="${JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}"

DO_INSTALL="${DO_INSTALL:-1}"
DO_TEST="${DO_TEST:-1}"
USE_NATIVE_OPT="${USE_NATIVE_OPT:-ON}"

# ---- Helpers ----------------------------------------------------------------

info()  { printf "\033[1;34m[INFO]\033[0m  %s\n" "$*"; }
warn()  { printf "\033[1;33m[WARN]\033[0m  %s\n" "$*"; }
error() { printf "\033[1;31m[ERROR]\033[0m %s\n" "$*" >&2; }

# ---- Sanity checks ----------------------------------------------------------

if [ ! -f "../SignalSim/src/Coordinate.cpp" ]; then
    error "SignalSim sources not found at ../SignalSim/src"
    error "Expected layout:"
    error "  Sisi/"
    error "  ├── GNSSLib/       (this directory)"
    error "  ├── RinexActualizer/"
    error "  └── SignalSim/"
    exit 1
fi

# ---- Configure --------------------------------------------------------------

info "Cleaning previous build (if any)..."
rm -rf "${BUILD_DIR}"

info "Configuring CMake (build type: ${BUILD_TYPE})..."
cmake -S . -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
    -DGNSSLIB_BUILD_SHARED=ON \
    -DGNSSLIB_BUILD_STATIC=ON \
    -DGNSSLIB_BUILD_EXAMPLES=ON \
    -DGNSSLIB_BUILD_TOOLS=ON \
    -DUSE_NATIVE_OPT="${USE_NATIVE_OPT}"

# ---- Build ------------------------------------------------------------------

info "Building with ${JOBS} parallel jobs..."
cmake --build "${BUILD_DIR}" --parallel "${JOBS}"

# ---- Test -------------------------------------------------------------------

if [ "${DO_TEST}" = "1" ] && [ -x "${BUILD_DIR}/bin/basic_usage" ]; then
    info "Running example..."
    if [ -f "${BUILD_DIR}/bin/configs/example.json" ]; then
        (cd "${BUILD_DIR}/bin" && ./basic_usage) || warn "Example exited non-zero (this is fine if it needs a RINEX file)"
    else
        (cd "${BUILD_DIR}/bin" && ./basic_usage) || warn "Example exited non-zero"
    fi
fi

# ---- Install ----------------------------------------------------------------

if [ "${DO_INSTALL}" = "1" ]; then
    info "Installing to ${INSTALL_PREFIX}..."
    if [ -w "${INSTALL_PREFIX}" ]; then
        cmake --install "${BUILD_DIR}"
    else
        sudo cmake --install "${BUILD_DIR}"
    fi
fi

# ---- Summary ----------------------------------------------------------------

echo ""
echo "=============================================="
echo " GNSSLib build complete"
echo "=============================================="
echo " Build type       : ${BUILD_TYPE}"
echo " Build directory  : ${SCRIPT_DIR}/${BUILD_DIR}"
echo " Libraries        : ${BUILD_DIR}/lib"
echo " Executables      : ${BUILD_DIR}/bin"
echo " Install prefix   : ${INSTALL_PREFIX}"
echo "=============================================="
echo ""
echo "To use the library in your own CMake project:"
echo ""
echo "    find_package(GNSSLib REQUIRED)"
echo "    target_link_libraries(myapp PRIVATE GNSSLib::GNSSLib)"
echo ""
echo "Or with pkg-config:"
echo ""
echo "    pkg-config --cflags --libs gnsslib"
echo ""

##!/bin/bash
## Build script for GNSSLib
#
#set -e
#
#echo "=============================================="
#echo "GNSSLib - Build Script"
#echo "=============================================="
#
## Create build directory
#mkdir -p build
#cd build
#
## Configure
#echo ""
#echo "Configuring..."
#cmake .. \
#    -DCMAKE_BUILD_TYPE=Release \
#    -DBUILD_SHARED_LIBS=ON \
#    -DBUILD_EXAMPLES=ON
#
## Build
#echo ""
#echo "Building..."
#make -j$(nproc)
#
## Test
#echo ""
#echo "Running examples..."
#./examples/basic_usage || true
#
## Install
#echo ""
#echo "Installing..."
#sudo make install
#
#echo ""
#echo "=============================================="
#echo "Build complete!"
#echo "Library installed to /usr/local/lib"
#echo "Headers installed to /usr/local/include/gnss"
#echo "=============================================="