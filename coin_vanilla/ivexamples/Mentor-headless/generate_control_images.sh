#!/bin/bash
# Generate control images for all headless examples
#
# This script runs all the headless examples and generates control images
# that will be used for regression testing.

set -e

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-$SCRIPT_DIR/build}"
BIN_DIR="$BUILD_DIR/bin"
CONTROL_DIR="${CONTROL_DIR:-$SCRIPT_DIR/control_images}"

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo "Error: Build directory not found: $BUILD_DIR"
    echo "Please build the examples first or set BUILD_DIR environment variable"
    exit 1
fi

# Create control images directory
mkdir -p "$CONTROL_DIR"

# Set data directory path for examples that need it
export IVEXAMPLES_DATA_DIR="$SCRIPT_DIR/data"

echo "Generating control images in: $CONTROL_DIR"
echo "Using binaries from: $BIN_DIR"
echo "Data directory: $IVEXAMPLES_DATA_DIR"
echo ""

# Set up cleanup on exit
cleanup() {
    rm -f "$temp_error_file" 2>/dev/null
}
trap cleanup EXIT INT TERM

# Create temporary file for error capture
temp_error_file=$(mktemp)

# List of all examples to run
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
    "08.1.BSCurve"
    "08.2.UniCurve"
    "08.3.BezSurf"
    "08.4.TrimSurf"
    "09.1.Print"
    "09.2.Texture"
    "09.3.Search"
    "09.4.PickAction"
    "09.5.GenSph"
    "10.1.addEventCB"
    "10.5.SelectionCB"
    "10.6.PickFilterTopLevel"
    "10.7.PickFilterManip"
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

# Counter for progress
total=${#EXAMPLES[@]}
count=0
failed=0

# Run each example
for example in "${EXAMPLES[@]}"; do
    count=$((count + 1))
    printf "[%2d/%2d] Generating %s..." "$count" "$total" "$example"
    
    exe="$BIN_DIR/$example"
    output="$CONTROL_DIR/${example}_control.rgb"
    
    if [ ! -f "$exe" ]; then
        echo " SKIP (not built)"
        continue
    fi
    
    # Run the example and capture error output
    if "$exe" "$output" >"$temp_error_file" 2>&1; then
        if [ -f "$output" ]; then
            echo " OK"
        else
            echo " FAILED (no output file)"
            if [ -s "$temp_error_file" ]; then
                echo "   Error: $(head -1 "$temp_error_file")"
            fi
            failed=$((failed + 1))
        fi
    else
        echo " FAILED (execution error)"
        if [ -s "$temp_error_file" ]; then
            echo "   Error: $(head -1 "$temp_error_file")"
        fi
        failed=$((failed + 1))
    fi
done

echo ""
echo "Control image generation complete"
echo "Total: $total, Succeeded: $((total - failed)), Failed: $failed"

if [ $failed -eq 0 ]; then
    echo "All control images generated successfully!"
    exit 0
else
    echo "Some control images failed to generate"
    exit 1
fi
