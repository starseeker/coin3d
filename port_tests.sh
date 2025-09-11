#!/bin/bash

# Helper script to port embedded tests to explicit Catch2 tests
# Usage: ./port_tests.sh <source_file> <target_module>

if [ $# -ne 2 ]; then
    echo "Usage: $0 <source_file> <target_module>"
    echo "Example: $0 src/base/SbBox3s.cpp base"
    exit 1
fi

SOURCE_FILE="$1"
MODULE="$2"
TARGET_DIR="tests/$MODULE"

# Extract class name from source file
CLASS_NAME=$(basename "$SOURCE_FILE" .cpp)

echo "🔄 Porting tests from $SOURCE_FILE to $MODULE module..."

# Check if source file has tests
if ! grep -q "COIN_TEST_SUITE" "$SOURCE_FILE"; then
    echo "❌ No embedded tests found in $SOURCE_FILE"
    exit 1
fi

# Create target directory if it doesn't exist
mkdir -p "$TARGET_DIR"

# Extract test code
echo "📝 Extracting test code..."
TEST_CODE=$(sed -n '/#ifdef COIN_TEST_SUITE/,/#endif.*COIN_TEST_SUITE/p' "$SOURCE_FILE" | \
            grep -v '#ifdef COIN_TEST_SUITE' | \
            grep -v '#endif.*COIN_TEST_SUITE')

# Extract include statements from the test block
INCLUDES=$(echo "$TEST_CODE" | grep '^#include' | head -5)

# Get the main class header (usually first include in the file)
MAIN_HEADER=$(head -20 "$SOURCE_FILE" | grep '#include.*Inventor' | head -1)

# Remove includes from test code
CLEAN_TEST_CODE=$(echo "$TEST_CODE" | grep -v '^#include')

# Generate explicit test file
TARGET_FILE="$TARGET_DIR/test_${CLASS_NAME,,}.cpp"  # lowercase filename

cat > "$TARGET_FILE" << EOF
/**************************************************************************\\
* Copyright (c) Kongsberg Oil & Gas Technologies AS
* All rights reserved.
*
* [LICENSE TEXT TRUNCATED FOR BREVITY]
\\**************************************************************************/

#include "utils/test_common.h"
$MAIN_HEADER
$(echo "$INCLUDES" | grep -v "$MAIN_HEADER")

using namespace CoinTestUtils;

// Tests for $CLASS_NAME class (ported from $SOURCE_FILE)
TEST_CASE("$CLASS_NAME tests", "[$MODULE][$CLASS_NAME]") {
    CoinTestFixture fixture;

EOF

# Convert BOOST_AUTO_TEST_CASE to SECTION
echo "$CLEAN_TEST_CODE" | sed 's/BOOST_AUTO_TEST_CASE(\([^)]*\))/    SECTION("\1") {/' | \
                          sed 's/BOOST_CHECK_MESSAGE(/        CHECK_THAT(/' | \
                          sed 's/BOOST_CHECK(/        CHECK(/' | \
                          sed 's/BOOST_REQUIRE(/        REQUIRE(/' | \
                          sed 's/^}/    }/' >> "$TARGET_FILE"

echo "}" >> "$TARGET_FILE"

echo "✅ Generated $TARGET_FILE"

# Add to CMakeLists.txt if not already present
CMAKE_FILE="$TARGET_DIR/../CMakeLists.txt"
if [ -f "$CMAKE_FILE" ]; then
    if ! grep -q "test_${CLASS_NAME,,}.cpp" "$CMAKE_FILE"; then
        echo "📝 Adding to CMakeLists.txt..."
        # Add the new test file to the CMakeLists.txt
        sed -i "/# $MODULE module tests/a\\    $MODULE/test_${CLASS_NAME,,}.cpp" "$CMAKE_FILE"
        echo "✅ Added to $CMAKE_FILE"
    else
        echo "ℹ️  Already in $CMAKE_FILE"
    fi
else
    echo "⚠️  CMakeLists.txt not found at $CMAKE_FILE"
fi

echo ""
echo "📋 Next steps:"
echo "1. Review and fix the generated test file: $TARGET_FILE"
echo "2. Update the CMakeLists.txt if needed"
echo "3. Build and test: cmake --build build_dir && ./build_dir/bin/CoinExplicitTests"
echo "4. Remove the embedded test from: $SOURCE_FILE"