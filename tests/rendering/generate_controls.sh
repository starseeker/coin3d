#!/bin/bash
# Generate PNG control images for tests/rendering/ visual regression tests.
#
# Control images are generated once from vanilla (upstream) Coin + GLX and
# committed to tests/control_images/.  The CTest suite then compiles the same
# sources against Obol and compares the pixel output.
#
# REQUIREMENTS:
#   - Xvfb installed (or DISPLAY already set to a working X11/GLX display)
#   - Coin library on LD_LIBRARY_PATH (or installed)
#   - rgb_to_png and image_comparator built in BUILD_DIR/bin/
#
# ENVIRONMENT:
#   BUILD_DIR   - path to CMake build directory
#                 (default: <repo_root>/build)
#   CONTROL_DIR - output directory for PNG files
#                 (default: <repo_root>/tests/control_images)
#   DISPLAY     - X11 display (Xvfb started on :99 if unset and not OSMesa)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build}"
BUILD_DIR="$(cd "$BUILD_DIR" && pwd)"
BIN_DIR="$BUILD_DIR/tests/rendering/bin"

CONTROL_DIR="${CONTROL_DIR:-$REPO_ROOT/tests/control_images}"
mkdir -p "$CONTROL_DIR"
CONTROL_DIR="$(cd "$CONTROL_DIR" && pwd)"

RGB_TO_PNG="$BUILD_DIR/tests/bin/rgb_to_png"
if [ ! -f "$RGB_TO_PNG" ]; then
    # Fallback: check rendering subdir bin
    RGB_TO_PNG_ALT="$BIN_DIR/rgb_to_png"
    if [ -f "$RGB_TO_PNG_ALT" ]; then
        RGB_TO_PNG="$RGB_TO_PNG_ALT"
    else
        echo "Error: rgb_to_png not found (looked in $BUILD_DIR/tests/bin/ and $BIN_DIR)"
        echo "Build the project with libpng support first."
        exit 1
    fi
fi

# Coin offscreen rendering environment variables for Xvfb compatibility
export COIN_GLX_PIXMAP_DIRECT_RENDERING=1
export COIN_GLXGLUE_NO_PBUFFERS=1
export COIN_FULL_INDIRECT_RENDERING=1

# Start Xvfb if DISPLAY is unset
XVFB_PID=""
cleanup() {
    [ -n "$XVFB_PID" ] && kill "$XVFB_PID" 2>/dev/null || true
    [ -n "$TMPDIR_OUT" ] && rm -rf "$TMPDIR_OUT"
}
trap cleanup EXIT INT TERM

if [ -z "$DISPLAY" ]; then
    echo "DISPLAY not set – starting Xvfb on :99"
    Xvfb :99 -screen 0 1024x768x24 +extension GLX +render -noreset &
    XVFB_PID=$!
    sleep 1
    export DISPLAY=:99
fi

TMPDIR_OUT="$(mktemp -d)"

echo "=== Generating rendering test control images ==="
echo "Binaries : $BIN_DIR"
echo "Output   : $CONTROL_DIR"
echo "Display  : $DISPLAY"
echo ""

# Helper: run executable → convert primary .rgb → control PNG
# Usage: gen_control <exe_name> <base_name>
gen_control() {
    local exename="$1"
    local name="$2"
    local exe="$BIN_DIR/$exename"
    local base="$TMPDIR_OUT/${name}_control"

    echo "  $name"
    if [ ! -f "$exe" ]; then
        echo "    WARNING: $exe not found – skipping"
        return 0
    fi

    "$exe" "$base" 2>&1 || true

    # Convert primary output
    local rgb="$base.rgb"
    if [ ! -f "$rgb" ]; then
        rgb="$base"   # direct-write fallback
    fi
    if [ -f "$rgb" ]; then
        "$RGB_TO_PNG" "$rgb" "$CONTROL_DIR/${name}_control.png"
        echo "    -> ${name}_control.png"
    else
        echo "    WARNING: no primary RGB output for $name"
    fi

    # Convert any secondary outputs  (e.g. _ortho, _lines, _points)
    for secondary in "$TMPDIR_OUT/${name}_control_"*.rgb; do
        [ -f "$secondary" ] || continue
        local suffix
        suffix="$(basename "$secondary" .rgb | sed "s|${name}_control||")"
        "$RGB_TO_PNG" "$secondary" "$CONTROL_DIR/${name}${suffix}_control.png"
        echo "    -> ${name}${suffix}_control.png"
    done
}

gen_control render_primitives  render_primitives
gen_control render_materials   render_materials
gen_control render_lighting    render_lighting
gen_control render_transforms  render_transforms
gen_control render_cameras     render_cameras
gen_control render_drawstyle   render_drawstyle
gen_control render_texture     render_texture

echo ""
echo "=== Done – control images in $CONTROL_DIR ==="
