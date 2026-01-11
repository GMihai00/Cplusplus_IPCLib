#!/bin/sh
set -eu

# Build script equivalent to cross-platform/build.txt
# Usage:
#   ./build.sh [build_dir]
# Example:
#   ./build.sh build

BUILD_DIR="${1:-build}"

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

cd "$SCRIPT_DIR"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake -S .. -B . \
  -DCMAKE_CXX_FLAGS="-std=c++17" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=gcc \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_GENERATOR="Unix Makefiles" \
  -DZLIB_BUILD_EXAMPLES=OFF

# Prefer make on Linux/macOS; fall back to mingw32-make.exe if present.
if command -v make >/dev/null 2>&1; then
  make
elif command -v mingw32-make.exe >/dev/null 2>&1; then
  mingw32-make.exe
else
  echo "Error: neither 'make' nor 'mingw32-make.exe' found in PATH." >&2
  exit 1
fi
