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
TESTS_BIN_DIR="$BUILD_DIR/tests/bin"

CONTROL_DIR="${CONTROL_DIR:-$REPO_ROOT/tests/control_images}"
mkdir -p "$CONTROL_DIR"
CONTROL_DIR="$(cd "$CONTROL_DIR" && pwd)"

# Prefer obol_test (unified dispatcher) for generating control images; the
# individual render_* executables are no longer built as standalone binaries.
OBOL_TEST="$TESTS_BIN_DIR/obol_test"

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
    # Prefer standalone executable; fall back to obol_test render_test dispatcher
    if [ ! -f "$exe" ]; then
        if [ -f "$OBOL_TEST" ]; then
            exe="$OBOL_TEST"
            "$exe" render_test "$name" "$base" 2>&1 || true
        else
            echo "    WARNING: neither $BIN_DIR/$exename nor $OBOL_TEST found – skipping"
            return 0
        fi
    else
        "$exe" "$base" 2>&1 || true
    fi

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
    # Convention: <test>_control_<variant>.png  (e.g. render_cameras_control_ortho.png)
    for secondary in "$TMPDIR_OUT/${name}_control_"*.rgb; do
        [ -f "$secondary" ] || continue
        local suffix
        suffix="$(basename "$secondary" .rgb | sed "s|${name}_control||")"
        "$RGB_TO_PNG" "$secondary" "$CONTROL_DIR/${name}_control${suffix}.png"
        echo "    -> ${name}_control${suffix}.png"
    done
}

gen_control render_primitives         render_primitives
gen_control render_materials          render_materials
gen_control render_lighting           render_lighting
gen_control render_transforms         render_transforms
gen_control render_cameras            render_cameras
gen_control render_drawstyle          render_drawstyle
gen_control render_texture            render_texture
gen_control render_text2              render_text2
gen_control render_text3              render_text3
gen_control render_gradient           render_gradient
gen_control render_colored_cube       render_colored_cube
gen_control render_coordinates        render_coordinates
gen_control render_scene              render_scene
gen_control render_shape_hints        render_shape_hints
gen_control render_environment        render_environment
gen_control render_texture3           render_texture3
gen_control render_bump_map           render_bump_map
gen_control render_texture_transform  render_texture_transform
gen_control render_depth_buffer       render_depth_buffer
gen_control render_alpha_test         render_alpha_test
gen_control render_indexed_face_set   render_indexed_face_set
gen_control render_quad_mesh          render_quad_mesh
gen_control render_indexed_line_set   render_indexed_line_set
gen_control render_point_set          render_point_set
gen_control render_lod                render_lod
gen_control render_quad_viewport_lod  render_quad_viewport_lod
gen_control render_viewport_scene     render_viewport_scene
gen_control render_scene_texture      render_scene_texture
gen_control render_array_multiple_copy render_array_multiple_copy

# Shadow rendering: SoShadowGroup GLSL shader compilation crashes on headless
# GLX pixmap contexts (multiple GL contexts in the same process).  OSMesa
# handles this correctly; use an OSMesa build (COIN3D_USE_OSMESA=ON) to
# regenerate this control image.
gen_control render_shadow             render_shadow

# HUD overlay tests
gen_control render_hud_overlay        render_hud_overlay
gen_control render_hud_no3d           render_hud_no3d

# Testlib demo scenes (shared scene factories; match obol_viewer output)
gen_control render_text_demo          render_text_demo
gen_control render_hud_demo           render_hud_demo

# SoProceduralShape visual regression test
gen_control render_procedural_shape   render_procedural_shape

# ARB8 edit-cycle visual progression test (4 images: primary + 3 step images)
gen_control render_arb8_edit_cycle    render_arb8_edit_cycle

echo ""
echo "=== Done – control images in $CONTROL_DIR ==="
