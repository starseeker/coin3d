#!/bin/bash

# Migration Script: Remove Comment-Driven Test Generation System
# This script completes the migration from embedded tests to explicit Catch2 tests

set -e

echo "🔄 Starting final migration to explicit tests..."

# Step 1: Verify all tests have been ported
echo "📊 Checking test coverage..."

# Count original embedded tests
EMBEDDED_TESTS=$(find src/ -name "*.cpp" -exec grep -l "COIN_TEST_SUITE" {} \; | wc -l)
echo "Found $EMBEDDED_TESTS source files with embedded tests"

# Count explicit tests  
EXPLICIT_TESTS=$(find tests/ -name "test_*.cpp" | wc -l)
echo "Found $EXPLICIT_TESTS explicit test files"

if [ "$EMBEDDED_TESTS" -gt "$EXPLICIT_TESTS" ]; then
    echo "⚠️  WARNING: Not all tests have been ported yet!"
    echo "   Embedded test files: $EMBEDDED_TESTS"
    echo "   Explicit test files: $EXPLICIT_TESTS" 
    echo "   Please port remaining tests before removing generation system"
    exit 1
fi

echo "✅ Test coverage check passed"

# Step 2: Remove test generation from CMake
echo "🗑️  Removing test generation system..."

# Backup original testsuite CMakeLists.txt
cp testsuite/CMakeLists.txt testsuite/CMakeLists.txt.backup

# Create new simplified testsuite CMakeLists.txt (keep only threading and misc tests)
cat > testsuite/CMakeLists.txt << 'EOF'
# Simplified test suite - legacy tests that don't fit the module structure
# Most tests have been migrated to explicit Catch2 tests in tests/ directory

add_executable(CoinLegacyTests 
    TestSuiteMain.cpp 
    TestSuiteUtils.cpp 
    TestSuiteMisc.cpp 
    threadsTest.cpp
)

set_target_properties(CoinLegacyTests PROPERTIES DEBUG_POSTFIX "${CMAKE_DEBUG_POSTFIX}")
target_link_libraries(CoinLegacyTests Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(CoinLegacyTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)

if (USE_PTHREAD)
    target_link_libraries(CoinLegacyTests pthread)
endif()

add_test(NAME LegacyTests COMMAND CoinLegacyTests)

# Compiler warning suppression for tautological comparisons on macOS
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG(-Wtautological-compare DISABLE_WARNING_TAUTOLOGCOMPARE)
if(DISABLE_WARNING_TAUTOLOGCOMPARE)
    target_compile_options(CoinLegacyTests PUBLIC -Wno-tautological-compare)
endif()
EOF

# Step 3: Remove generation scripts and templates
echo "🗑️  Removing generation scripts..."
rm -f testsuite/makeextract.sh
rm -f testsuite/makemakefile.sh  
rm -f testsuite/TestSuiteTemplate.cmake.in

# Step 4: Remove embedded test blocks from source files
echo "🗑️  Removing embedded test blocks from source files..."

# This is the most dangerous part - we'll create a script but not run it automatically
cat > remove_embedded_tests.sh << 'EOF'
#!/bin/bash
# Script to remove embedded test blocks from source files
# Run this after verifying all tests have been ported

find src/ -name "*.cpp" -type f | while read file; do
    if grep -q "COIN_TEST_SUITE" "$file"; then
        echo "Removing embedded tests from $file"
        # Create backup
        cp "$file" "$file.backup"
        # Remove test blocks
        sed -i '/#ifdef COIN_TEST_SUITE/,/#endif.*COIN_TEST_SUITE/d' "$file"
    fi
done
EOF

chmod +x remove_embedded_tests.sh

# Step 5: Update main CMakeLists.txt
echo "📝 Updating main CMakeLists.txt..."
sed -i 's/add_subdirectory(testsuite)/add_subdirectory(testsuite)  # Legacy tests only/' CMakeLists.txt

echo "✅ Migration preparation complete!"
echo ""
echo "📋 Manual steps required:"
echo "1. Verify all tests pass: ctest -C Release"  
echo "2. Run remove_embedded_tests.sh to remove embedded test blocks"
echo "3. Remove CoinTestFramework.h and related files"
echo "4. Update documentation to remove references to embedded tests"
echo "5. Update CI scripts to use new test structure"
echo ""
echo "🔧 Files ready for manual review:"
echo "- testsuite/CMakeLists.txt (simplified)"
echo "- remove_embedded_tests.sh (script to remove embedded tests)"
echo ""
echo "⚠️  Run tests before and after each step to ensure no regressions!"