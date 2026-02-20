#!/bin/bash
# Generate control images for all headless Mentor examples.
#
# Each example renders to SGI RGB via SoOffscreenRenderer. The script then
# converts each RGB output to a lossless PNG for compact repository storage.
#
# OUTPUT NAMING CONVENTION (in control_images/):
#   Single-view examples:  <example_name>_control.png
#   Multi-view examples:   <example_name><view_suffix>_control.png
#     e.g. 03.1.Molecule_front_control.png
#          03.1.Molecule_side_control.png
#
# For regression tests, the "primary" control image is always:
#   <example_name>_control.png
# which covers single-output examples; for multi-output examples a copy of
# the first alphabetically-sorted view is stored as the primary.
#
# REQUIREMENTS:
#   - Xvfb available (or DISPLAY already set to a working X11/GLX display)
#   - Coin library on LD_LIBRARY_PATH (or installed)
#   - rgb_to_png built in BUILD_DIR/bin/
#
# ENVIRONMENT:
#   BUILD_DIR   - path to CMake build directory (default: <script_dir>/build)
#   CONTROL_DIR - path to output directory (default: <script_dir>/control_images)
#   DISPLAY     - X11 display (auto-starts Xvfb on :99 if unset)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$(cd "${BUILD_DIR:-$SCRIPT_DIR/build}" && pwd)"
BIN_DIR="$BUILD_DIR/bin"
CONTROL_DIR="${CONTROL_DIR:-$SCRIPT_DIR/control_images}"
CONTROL_DIR="$(mkdir -p "$CONTROL_DIR" && cd "$CONTROL_DIR" && pwd)"

if [ ! -d "$BUILD_DIR" ]; then
    echo "Error: Build directory not found: $BUILD_DIR"
    exit 1
fi

RGB_TO_PNG="$BIN_DIR/rgb_to_png"
if [ ! -f "$RGB_TO_PNG" ]; then
    echo "Error: rgb_to_png not found at $RGB_TO_PNG"
    echo "Please build with libpng available (cmake .. && make rgb_to_png)"
    exit 1
fi

# Data directory for examples that load .iv files
export COIN_DATA_DIR="$SCRIPT_DIR/data"
export IVEXAMPLES_DATA_DIR="$SCRIPT_DIR/data"

# Coin offscreen rendering config for Xvfb compatibility:
#   COIN_GLX_PIXMAP_DIRECT_RENDERING=1  - use direct-rendered GLX pixmaps
#   COIN_GLXGLUE_NO_PBUFFERS=1          - skip pbuffer path (max-pixels=0 on Xvfb)
export COIN_GLX_PIXMAP_DIRECT_RENDERING=1
export COIN_GLXGLUE_NO_PBUFFERS=1

# Start Xvfb if DISPLAY is not set
XVFB_PID=""
cleanup_xvfb() {
    if [ -n "$XVFB_PID" ]; then
        kill "$XVFB_PID" 2>/dev/null || true
    fi
}
trap cleanup_xvfb EXIT INT TERM

if [ -z "$DISPLAY" ]; then
    echo "DISPLAY not set - starting Xvfb on :99"
    Xvfb :99 -screen 0 1024x768x24 +extension GLX +render -noreset &
    XVFB_PID=$!
    sleep 1
    export DISPLAY=:99
fi

echo "Generating control images in: $CONTROL_DIR"
echo "Using binaries from: $BIN_DIR"
echo "Data directory: $COIN_DATA_DIR"
echo "Display: $DISPLAY"
echo ""

# Examples list
EXAMPLES=(
    "02.1.HelloCone"
    "02.2.EngineSpin"
    "02.3.Trackball"
    "02.4.Examiner"
    "03.1.Molecule"
    "03.2.Robot"
    "03.3.Naming"
    "04.1.Cameras"
    "04.2.Lights"
    "05.1.FaceSet"
    "05.2.IndexedFaceSet"
    "05.3.TriangleStripSet"
    "05.4.QuadMesh"
    "05.5.Binding"
    "05.6.TransformOrdering"
    "06.1.Text"
    "06.2.Simple3DText"
    "06.3.Complex3DText"
    "07.1.BasicTexture"
    "07.2.TextureCoordinates"
    "07.3.TextureFunction"
    # "08.1.BSCurve"  # NURBS not available in this fork
    # "08.2.UniCurve" # NURBS not available in this fork
    # "08.3.BezSurf"  # NURBS not available in this fork
    # "08.4.TrimSurf" # NURBS not available in this fork
    "09.1.Print"
    "09.2.Texture"
    "09.3.Search"
    "09.4.PickAction"
    "09.5.GenSph"
    "10.1.addEventCB"
    "10.2.setEventCB"
    "10.5.SelectionCB"
    "10.6.PickFilterTopLevel"
    "10.7.PickFilterManip"
    "10.8.PickFilterNodeKit"
    "11.1.ReadFile"
    "11.2.ReadString"
    "12.1.FieldSensor"
    "12.2.NodeSensor"
    "12.3.AlarmSensor"
    "12.4.TimerSensor"
    "13.1.GlobalFlds"
    "13.2.ElapsedTime"
    "13.3.TimeCounter"
    "13.4.Gate"
    "13.5.Boolean"
    "13.6.Calculator"
    "13.7.Rotor"
    "13.8.Blinker"
    "14.1.FrolickingWords"
    "14.3.Balance"
    "15.1.ConeRadius"
    "15.2.SliderBox"
    "15.3.AttachManip"
    "15.4.Customize"
    "17.2.GLCallback"
)

total=${#EXAMPLES[@]}
count=0
succeeded=0
failed=0
skipped=0

# run_example EXAMPLE EXE
# Runs the example in a temp dir, collects .rgb outputs, converts to PNG.
run_example() {
    local example="$1"
    local exe="$2"
    local tmpdir
    tmpdir=$(mktemp -d)
    local errfile="$tmpdir/.stderr"

    # Create output/ sub-directory for examples with hardcoded "output/" paths
    mkdir -p "$tmpdir/output"

    # Run from inside the temp dir so that relative output paths stay local.
    # Pass "ctrl" as base name; examples append their own suffixes.
    local exit_code=0
    (cd "$tmpdir" && "$exe" "ctrl") >"$errfile" 2>&1 || exit_code=$?

    if [ "$exit_code" -eq 139 ]; then
        echo " SKIP (segfault)"
        rm -rf "$tmpdir"
        return 2
    fi

    # Collect all .rgb files generated anywhere under the temp dir
    local rgb_files=()
    while IFS= read -r -d '' f; do
        rgb_files+=("$f")
    done < <(find "$tmpdir" -name "*.rgb" -print0 2>/dev/null | sort -z)

    # Also check for a raw file named exactly "ctrl" (HelloCone writes argv[1] directly)
    if [ -f "$tmpdir/ctrl" ]; then
        local magic
        magic=$(xxd -l 2 -p "$tmpdir/ctrl" 2>/dev/null || echo "")
        if [ "$magic" = "01da" ]; then
            # Prepend to list so it becomes the primary (first) image
            rgb_files=("$tmpdir/ctrl" "${rgb_files[@]}")
        fi
    fi

    if [ ${#rgb_files[@]} -eq 0 ]; then
        if [ "$exit_code" -ne 0 ]; then
            echo " FAILED (execution error, exit=$exit_code)"
        else
            echo " SKIP (no image output)"
        fi
        cat "$errfile" 2>/dev/null | head -2 | sed 's/^/   /'
        rm -rf "$tmpdir"
        [ "$exit_code" -ne 0 ] && return 1 || return 2
    fi

    local first_png=""
    local num_converted=0
    for rgb_file in "${rgb_files[@]}"; do
        # Derive view suffix from file path relative to tmpdir/ctrl base:
        #   ctrl            -> ""         (direct output, single view)
        #   ctrl.rgb        -> ""         (base+.rgb examples like 09.1.Print)
        #   ctrl_front.rgb  -> "_front"   (multi-view examples)
        #   output/17.2.GLCallback_00_default.rgb -> "_17.2.GLCallback_00_default"
        local rel_path="${rgb_file#$tmpdir/}"
        local view_suffix=""
        if [ "$rel_path" = "ctrl" ]; then
            view_suffix=""
        elif [ "$rel_path" = "ctrl.rgb" ]; then
            view_suffix=""
        elif [[ "$rel_path" == ctrl_* ]]; then
            # e.g. ctrl_front.rgb -> _front
            view_suffix="_${rel_path#ctrl_}"
            view_suffix="${view_suffix%.rgb}"
        elif [[ "$rel_path" == ctrl*.rgb ]]; then
            # e.g. ctrl_view.rgb -> _view (belt-and-suspenders)
            view_suffix="_${rel_path#ctrl}"
            view_suffix="${view_suffix%.rgb}"
        elif [[ "$rel_path" == output/* ]]; then
            # Hardcoded output/ path - strip any leading example-name prefix from filename
            # e.g. "17.2.GLCallback_00_default" -> "_00_default" (strips "17.2.GLCallback" prefix)
            local fname
            fname=$(basename "$rgb_file" .rgb)
            local stripped="${fname#${example}_}"
            if [ "$stripped" != "$fname" ]; then
                view_suffix="_${stripped}"
            else
                view_suffix="_${fname}"
            fi
        else
            # Unexpected - use path-derived suffix
            view_suffix="_$(echo "$rel_path" | tr '/' '_' | sed 's/\.rgb$//')"
        fi

        local png_out="$CONTROL_DIR/${example}${view_suffix}_control.png"
        if "$RGB_TO_PNG" "$rgb_file" "$png_out" 2>>"$errfile"; then
            num_converted=$((num_converted + 1))
            [ -z "$first_png" ] && first_png="$png_out"
        fi
    done

    rm -rf "$tmpdir"

    if [ "$num_converted" -eq 0 ]; then
        echo " FAILED (png conversion error)"
        return 1
    fi

    # Ensure a canonical _control.png exists (for multi-view examples).
    # Always update it to keep it in sync with the first alphabetical view.
    local primary_png="$CONTROL_DIR/${example}_control.png"
    if [ -n "$first_png" ] && [ "$first_png" != "$primary_png" ]; then
        cp "$first_png" "$primary_png"
    fi

    if [ "$num_converted" -eq 1 ]; then
        echo " OK"
    else
        echo " OK ($num_converted views)"
    fi
    return 0
}

for example in "${EXAMPLES[@]}"; do
    count=$((count + 1))
    printf "[%2d/%2d] Generating %s..." "$count" "$total" "$example"

    exe="$BIN_DIR/$example"
    if [ ! -f "$exe" ]; then
        echo " SKIP (not built)"
        skipped=$((skipped + 1))
        continue
    fi

    result=0
    run_example "$example" "$exe" || result=$?
    case "$result" in
        0) succeeded=$((succeeded + 1)) ;;
        2) skipped=$((skipped + 1)) ;;
        *) failed=$((failed + 1)) ;;
    esac
done

echo ""
echo "Control image generation complete"
echo "Total: $total, Succeeded: $succeeded, Failed: $failed, Skipped: $skipped"

if [ $failed -eq 0 ]; then
    echo "All available control images generated successfully!"
    exit 0
else
    echo "Some control images failed to generate"
    exit 1
fi
