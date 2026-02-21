#!/bin/bash
# Copyright (c) Kongsberg Oil & Gas Technologies AS
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# Redistributions of source code must retain the above copyright notice,
# this list of conditions and the following disclaimer.
#
# Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.
#
# Neither the name of the copyright holder nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Generate control images for visual regression tests.
#
# Each rendering test produces an SGI RGB file via SoOffscreenRenderer.
# This script converts each RGB output to a lossless PNG for compact
# repository storage.
#
# OUTPUT NAMING CONVENTION (in control_images/):
#   <test_name>_control.png
#
# REQUIREMENTS:
#   - Xvfb available (or DISPLAY already set to a working X11/GLX display)
#   - Coin library on LD_LIBRARY_PATH (or installed)
#   - rgb_to_png and image_comparator built in BUILD_DIR/bin/
#
# ENVIRONMENT:
#   BUILD_DIR   - path to CMake build directory (default: <script_dir>/build)
#   CONTROL_DIR - output directory (default: <script_dir>/control_images)
#   DISPLAY     - X11 display (auto-starts Xvfb on :99 if unset)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$(cd "${BUILD_DIR:-$SCRIPT_DIR/build}" && pwd)"
BIN_DIR="$BUILD_DIR/bin"
CONTROL_DIR="${CONTROL_DIR:-$SCRIPT_DIR/control_images}"
mkdir -p "$CONTROL_DIR"
CONTROL_DIR="$(cd "$CONTROL_DIR" && pwd)"

if [ ! -d "$BUILD_DIR" ]; then
    echo "Error: Build directory not found: $BUILD_DIR"
    exit 1
fi

RGB_TO_PNG="$BIN_DIR/rgb_to_png"
if [ ! -f "$RGB_TO_PNG" ]; then
    echo "Error: rgb_to_png not found at $RGB_TO_PNG"
    echo "Please build with libpng available."
    exit 1
fi

# Coin offscreen rendering config
export COIN_GLX_PIXMAP_DIRECT_RENDERING=1

# Start Xvfb if DISPLAY is not set
XVFB_PID=""
cleanup_xvfb() {
    if [ -n "$XVFB_PID" ]; then
        kill "$XVFB_PID" 2>/dev/null || true
    fi
}
trap cleanup_xvfb EXIT

if [ -z "$DISPLAY" ]; then
    echo "No DISPLAY set, starting Xvfb..."
    Xvfb :99 -screen 0 1024x768x24 +extension GLX &
    XVFB_PID=$!
    sleep 1
    export DISPLAY=:99
fi

TMPDIR_OUT="$(mktemp -d)"
cleanup_tmp() {
    rm -rf "$TMPDIR_OUT"
    cleanup_xvfb
}
trap cleanup_tmp EXIT

# Helper: run a test executable and convert its RGB output to a control PNG
# Usage: generate_control <test_executable> <test_name>
generate_control() {
    local exe="$1"
    local name="$2"
    local rgb_file="$TMPDIR_OUT/${name}_control.rgb"
    local png_file="$CONTROL_DIR/${name}_control.png"

    echo "Generating control image for: $name"

    if [ ! -f "$exe" ]; then
        echo "  WARNING: executable not found: $exe (skipping)"
        return 0
    fi

    "$exe" "$TMPDIR_OUT/${name}_control" 2>&1 || true

    if [ -f "${rgb_file}" ]; then
        "$RGB_TO_PNG" "$rgb_file" "$png_file"
        echo "  -> $png_file"
    elif [ -f "$TMPDIR_OUT/${name}_control" ]; then
        "$RGB_TO_PNG" "$TMPDIR_OUT/${name}_control" "$png_file"
        echo "  -> $png_file"
    else
        echo "  WARNING: no output generated for $name"
    fi
}

echo "=== Generating Coin test control images ==="
echo "Build dir: $BUILD_DIR"
echo "Output dir: $CONTROL_DIR"
echo ""

# Add calls to generate_control for each visual test here, for example:
#   generate_control "$BIN_DIR/test_basic_sphere" "test_basic_sphere"
#
# Tests are added here as they are written. See tests/rendering/ for
# the rendering test sources.

echo ""
echo "=== Done ==="
