#!/bin/bash
set -e

# Get directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

# Detect Operating System / WSL environment
IS_LINUX=false
if [[ "$OSTYPE" == "linux-gnu"* ]] || [[ "$(uname)" == "Linux" ]]; then
  IS_LINUX=true
fi

if [ "$IS_LINUX" = true ]; then
  echo "=== WSL / Linux Environment Detected ==="
  BUILD_DIR="build_wsl"
else
  echo "=== Windows Environment Detected ==="
  BUILD_DIR="build"
fi

echo "=== Creating build directory: $BUILD_DIR ==="
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "=== Configuring CMake in Release Mode ==="
CMAKE_ARGS=""

if [ "$IS_LINUX" = true ]; then
  # On Linux/WSL, standard package manager directories are used
  # LLVM/Clang/Flang 22 dev packages install directly into system directories or /usr/lib/llvm-22/
  if [ -d "/usr/lib/llvm-22" ]; then
    echo "Using LLVM/Clang 22 directory at: /usr/lib/llvm-22"
    CMAKE_ARGS="-DLLVM_DIR=/usr/lib/llvm-22/lib/cmake/llvm -DClang_DIR=/usr/lib/llvm-22/lib/cmake/clang"
  fi
else
  # Determine LLVM path for Windows (argument > environment variables > default fallback)
  LLVM_PATH_INPUT="${1:-${LLVM_DIR:-$LLVM_PATH}}"
  LLVM_PATH=""

  if [ -n "$LLVM_PATH_INPUT" ] && [ -d "$LLVM_PATH_INPUT" ]; then
    LLVM_PATH="$LLVM_PATH_INPUT"
  else
    # Fallback to the known Windows development path if it exists
    DEFAULT_PATH="E:/EL/clang+llvm-22.1.1-x86_64-pc-windows-msvc"
    if [ -d "$DEFAULT_PATH" ]; then
      LLVM_PATH="$DEFAULT_PATH"
    fi
  fi

  if [ -n "$LLVM_PATH" ]; then
    echo "Using LLVM/Clang directory at: $LLVM_PATH"
    # Replace backslashes with forward slashes for CMake robustness
    LLVM_PATH_CONV=$(echo "$LLVM_PATH" | tr '\\' '/')
    CMAKE_ARGS="-DLLVM_DIR=$LLVM_PATH_CONV/lib/cmake/llvm -DClang_DIR=$LLVM_PATH_CONV/lib/cmake/clang"
  else
    echo "No custom LLVM/Clang path provided or found. Using CMake standard search paths."
  fi
fi

cmake $CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release ..

echo "=== Building Target in Release Mode ==="
cmake --build . --config Release

echo "=== Build Complete! ==="
