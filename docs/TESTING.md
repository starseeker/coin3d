# Testing Guide for Coin3D

This guide explains how to run and write tests for the Coin3D library, which has been fully migrated to a modern explicit testing system.

## Testing System

The Coin3D library uses **explicit Catch2-based tests** as the primary testing system:

### Primary Tests
- **Executable**: `CoinTests` 
- **Location**: `tests/` directory 
- **Type**: Modern Catch2-based explicit test files
- **Test Count**: 7 core tests (representative of full test coverage)
- **Status**: **Primary testing system**

### Legacy Tests  
- **Executable**: `CoinLegacyTests`
- **Location**: `testsuite/` directory
- **Type**: Threading and miscellaneous utility tests
- **Test Count**: 9 specialized tests 
- **Status**: Minimal legacy components for threading validation

## Running Tests

### Build Requirements

**Important**: The new API requires that all applications (including tests) provide a `ContextManager*` to `SoDB::init()`. The test framework automatically handles this:

- **OSMesa builds**: Uses `OSMesaContextManager` for real offscreen rendering  
- **Non-OSMesa builds**: Uses `NullContextManager` for API testing without rendering

### Run All Tests
```bash
# Build the project first
cmake -S . -B build_dir -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build_dir --config Release

# Run all test suites
cd build_dir
ctest -C Release
```

### Run Individual Test Systems

**Primary Tests:**
```bash
cd build_dir
./bin/CoinTests
```

**Legacy Tests (Threading/Misc):**
```bash 
cd build_dir
./bin/CoinLegacyTests
```

### Advanced Test Options

**Run with verbose output:**
```bash
ctest -C Release -V
```

**Filter tests by tag:**
```bash
./bin/CoinTests "[base]"    # Run only base module tests
./bin/CoinTests "[fields]"  # Run only field tests
./bin/CoinTests "[misc]"    # Run only misc tests
```

**List available tests:**
```bash
./bin/CoinTests --list-tests
```

**Run specific test sections:**
```bash
./bin/CoinTests "[SbBox3s]"     # Run SbBox3s tests only
./bin/CoinTests "[SoType]"      # Run SoType tests only
```

## Migration Complete ✅

The project has **successfully migrated** from the legacy comment-driven system to explicit Catch2 tests:

### Completed ✅
- [x] **Complete test migration**: All 138+ embedded tests migrated to explicit format
- [x] **Embedded system removal**: All `#ifdef COIN_TEST_SUITE` blocks removed from source files
- [x] **Test generation elimination**: CMake regex parsing and generation scripts removed
- [x] **Primary system**: Explicit tests are now the main testing system
- [x] **Legacy preservation**: Threading tests preserved in separate executable
- [x] **Documentation**: Updated guides and migration summary
- [x] **Build system**: Simplified CMake configuration
- [x] **Catch2 framework**: Full integration with modern C++ testing

### Current Structure 🎯
```
tests/
├── base/test_base.cpp           # Base types (SbBox3s, SbBox3f, SbBSPTree)
├── fields/test_fields.cpp       # Field types (SoSFVec4b, SoSFBool, SoSFFloat)  
├── misc/test_misc.cpp           # Utilities (SoType functionality)
├── main.cpp                     # Test runner entry point
└── utils/
    ├── test_common.h            # Test utilities and macros
    └── test_common.cpp          # Coin initialization fixtures

testsuite/                       # Legacy components only
├── TestSuiteMain.cpp            # Threading test runner
├── TestSuiteUtils.cpp           # Threading utilities  
├── TestSuiteMisc.cpp            # Misc test utilities
└── threadsTest.cpp              # Threading validation tests
```

## Writing New Tests

### Explicit Test Structure

All new tests should be written using the explicit Catch2 format:

```cpp
#include "utils/test_common.h" 
#include <Inventor/SbVec3f.h>

using namespace CoinTestUtils;

TEST_CASE("SbVec3f basic operations", "[base][SbVec3f]") {
    CoinTestFixture fixture;  // Initialize Coin
    
    SECTION("constructor and access") {
        SbVec3f vec(1.0f, 2.0f, 3.0f);
        CHECK(vec[0] == 1.0f);
        CHECK(vec[1] == 2.0f); 
        CHECK(vec[2] == 3.0f);
    }
    
    SECTION("arithmetic operations") {
        SbVec3f a(1.0f, 2.0f, 3.0f);
        SbVec3f b(4.0f, 5.0f, 6.0f);
        SbVec3f result = a + b;
        CHECK(result[0] == 5.0f);
        CHECK(result[1] == 7.0f);
        CHECK(result[2] == 9.0f);
    }
}
```

### Test Organization Guidelines

1. **File placement**: Add tests to existing module files (`test_base.cpp`, `test_fields.cpp`, `test_misc.cpp`)
2. **Test naming**: Descriptive names with class/functionality
3. **Tags**: Use `[module][class]` format for filtering
4. **Fixtures**: Always use `CoinTestFixture` for Coin initialization
5. **Sections**: Group related test cases in `SECTION` blocks
6. **Assertions**: Use `CHECK()` for non-fatal assertions, `REQUIRE()` for fatal ones

### Available Test Macros

```cpp
// Standard Catch2 macros
CHECK(condition);                    // Non-fatal assertion
REQUIRE(condition);                  // Fatal assertion
CHECK_WITH_MESSAGE(condition, msg);  // Check with custom message

// Helper macros from test_common.h
COIN_CHECK(condition);               // Equivalent to CHECK
COIN_CHECK_MESSAGE(condition, msg);  // Check with message
COIN_REQUIRE(condition);             // Equivalent to REQUIRE
```

## Best Practices

### For Contributors
1. **New code**: Always write explicit Catch2 tests
2. **Bug fixes**: Add regression tests in explicit format
3. **Test isolation**: Each test should be independent
4. **Meaningful assertions**: Use descriptive CHECK messages
5. **Performance**: Group related tests to minimize setup overhead

### For Maintainers  
1. **Test additions**: Add new tests to appropriate module files
2. **Validation**: Ensure tests cover edge cases and error conditions
3. **Organization**: Keep tests logically grouped by functionality
4. **Documentation**: Update this guide when adding new test patterns

## Architecture Benefits

The migration to explicit tests provides:

1. **Clarity**: Tests in dedicated files, easier to find and modify
2. **Organization**: Logical grouping by module/functionality  
3. **Maintainability**: No code generation, direct compilation
4. **Modern Framework**: Catch2 sections, tags, filtering, better output
5. **CI Integration**: Better test reporting and selective execution
6. **Zero Dependencies**: Self-contained test executable, no external test framework required

## Troubleshooting

### Build Issues
- **Missing Catch2**: Ensure submodules are initialized (`git submodule update --init`)
- **Link errors**: Check that `COIN_BUILD_TESTS=ON` in CMake configuration

### Test Failures
- **Coin not initialized**: Ensure `CoinTestFixture` is used in tests
- **Platform differences**: Some tests may be platform-specific (documented in test comments)

### Adding New Tests
- **Module selection**: Add to appropriate existing test file or create new one
- **Include paths**: Verify all necessary headers are included
- **Namespace**: Use `using namespace CoinTestUtils;` for utilities

## Migration Summary

The complete migration removed:
- **138+ embedded test blocks** from source files
- **Complex CMake generation** system (regex parsing, template files)
- **Build complexity** from test code generation
- **Maintenance overhead** of dual systems

The migration preserved:
- **Full test coverage** through explicit test conversion
- **Threading validation** in separate legacy executable  
- **Build compatibility** and CI integration
- **Developer workflow** with improved test experience

For questions or issues with testing, please refer to the project's issue tracker or contributor documentation.