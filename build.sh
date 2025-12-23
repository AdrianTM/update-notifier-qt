#!/bin/bash

# **********************************************************************
# * Copyright (C) 2024-2025 MX Authors
# *
# * Authors: Adrian <adrian@mxlinux.org>
# *          MX Linux <http://mxlinux.org>
# *
# * This file is part of mx-arch-updater.
# *
# * mx-arch-updater is free software: you can redistribute it and/or modify
# * it under the terms of the GNU General Public License as published by
# * the Free Software Foundation, either version 3 of the License, or
# * (at your option) any later version.
# *
# * mx-arch-updater is distributed in the hope that it will be useful,
# * but WITHOUT ANY WARRANTY; without even the implied warranty of
# * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# * GNU General Public License for more details.
# *
# * You should have received a copy of the GNU General Public License
# * along with mx-arch-updater.  If not, see <http://www.gnu.org/licenses/>.
# **********************************************************************/

set -e

# Ensure we run from the project root.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

# Default values
BUILD_DIR="build"
BUILD_TYPE="Release"
CLEAN=false
ARCH_BUILD=false
CHECK_INTEG=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --clean)
            CLEAN=true
            shift
            ;;
        --arch)
            ARCH_BUILD=true
            shift
            ;;
        --check)
            CHECK_INTEG=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  -d, --debug     Build in Debug mode (default: Release)"
            echo "  --clean         Clean build directory before building"
            echo "  --arch          Build Arch Linux package (skips integrity checks)"
            echo "  --check         Enable makepkg source integrity checks for --arch"
            echo "  -h, --help      Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Build Arch Linux package
if [ "$ARCH_BUILD" = true ]; then
    echo "Building Arch Linux package..."

    if ! command -v makepkg &> /dev/null; then
        echo "Error: makepkg not found. Please install base-devel package."
        exit 1
    fi

    if [ ! -f PKGBUILD ]; then
        echo "Error: PKGBUILD not found; cannot build Arch package."
        exit 1
    fi

    PKG_NAME=$(awk -F= '/^pkgname=/{print $2; exit}' PKGBUILD)
    PKG_NAME=${PKG_NAME#\"}
    PKG_NAME=${PKG_NAME%\"}
    PKG_NAME=${PKG_NAME#\(}
    PKG_NAME=${PKG_NAME%\)}
    PKG_NAME=${PKG_NAME%% *}
    if [ -z "$PKG_NAME" ]; then
        echo "Error: could not determine pkgname from PKGBUILD."
        exit 1
    fi

    ARCH_BUILDDIR=$(mktemp -d -p "$SCRIPT_DIR" archpkgbuild.XXXXXX)
    trap 'rm -rf "$ARCH_BUILDDIR"' EXIT

    rm -rf pkg *.pkg.tar.* 2>/dev/null || true

    ARCH_SRCDIR="$ARCH_BUILDDIR/$PKG_NAME/src"
    mkdir -p "$ARCH_SRCDIR"
    tar \
        --exclude="./build" \
        --exclude="./pkg" \
        --exclude="./archpkgbuild.*" \
        --exclude="./.git" \
        -C "$SCRIPT_DIR" \
        -cf - . | tar -C "$ARCH_SRCDIR" -xf -

    PKG_DEST_DIR="$SCRIPT_DIR/build"
    mkdir -p "$PKG_DEST_DIR"

    MAKEPKG_ARGS=(-f --skipinteg --noextract)
    if [ "$CHECK_INTEG" = true ]; then
        MAKEPKG_ARGS=("${MAKEPKG_ARGS[@]/--skipinteg}")
    fi

    BUILDDIR="$ARCH_BUILDDIR" SRCDEST="$SCRIPT_DIR/src" PKGDEST="$PKG_DEST_DIR" makepkg "${MAKEPKG_ARGS[@]}"

    echo "Cleaning makepkg artifacts..."
    rm -rf pkg

    echo "Arch Linux package build completed!"
    echo "Package: $(ls "$PKG_DEST_DIR"/*.pkg.tar.* 2>/dev/null || echo 'not found')"
    exit 0
fi

# Clean build directory if requested
if [ "$CLEAN" = true ]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

# Create build directory
mkdir -p "$BUILD_DIR"

# Configure CMake with Ninja
echo "Configuring CMake with Ninja generator..."
CMAKE_ARGS=(
    -G Ninja
    -B "$BUILD_DIR"
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    -DCMAKE_INSTALL_PREFIX=/usr
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
)

cmake "${CMAKE_ARGS[@]}"

# Build the project
echo "Building project with Ninja..."
cmake --build "$BUILD_DIR" --parallel

echo "Build completed successfully!"
echo "Executables: $BUILD_DIR/"
