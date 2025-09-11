# Testing Migration Guide

This document explains how to migrate from the comment-driven test generation system to explicit Catch2-based tests.

## Overview

The Coin3D test suite is being migrated from:
- **Old System**: Tests embedded in source files within `#ifdef COIN_TEST_SUITE` blocks
- **New System**: Explicit test files using Catch2 framework in the `tests/` directory

## Current Status

Both systems are running in parallel during the transition:
- **Original tests**: 184 tests in CoinTests executable (generated from comments)
- **Explicit tests**: 7 test cases in CoinExplicitTests executable (Catch2-based)

## Migration Process

### 1. Identify Tests to Migrate

Find source files with embedded tests:
```bash
find src/ -name "*.cpp" -exec grep -l "COIN_TEST_SUITE" {} \;
```

### 2. Create Explicit Test Files

For each module, create a corresponding test file in `tests/`:
- `src/base/` → `tests/base/test_base.cpp`
- `src/fields/` → `tests/fields/test_fields.cpp`  
- `src/misc/` → `tests/misc/test_misc.cpp`

### 3. Port Test Cases

#### Old Format (in source files):
```cpp
#ifdef COIN_TEST_SUITE
BOOST_AUTO_TEST_CASE(testName) {
    // test code
    BOOST_CHECK_MESSAGE(condition, "message");
}
#endif // COIN_TEST_SUITE
```

#### New Format (in explicit test files):
```cpp
#include "utils/test_common.h"
#include <Inventor/SomeClass.h>

TEST_CASE("SomeClass functionality", "[module][SomeClass]") {
    CoinTestUtils::CoinTestFixture fixture;
    
    SECTION("testName") {
        // test code
        CHECK(condition);
    }
}
```

### 4. Test Organization

- **Test files**: Organized by module (base, fields, misc, etc.)
- **Test tags**: Use `[module][class]` for filtering
- **Fixtures**: Use `CoinTestFixture` for Coin initialization
- **Sections**: Group related tests within test cases

### 5. Building and Running Tests

```bash
# Build all tests
cmake --build build_dir --config Release

# Run original tests
./build_dir/bin/CoinTests

# Run new explicit tests
./build_dir/bin/CoinExplicitTests

# Run all tests via CTest
ctest -C Release
```

## Example Migration

### Before (src/misc/SoType.cpp):
```cpp
#ifdef COIN_TEST_SUITE
#include <Inventor/SoType.h>
#include <Inventor/SbName.h>
#include <Inventor/nodes/SoNode.h>

static void * createInstance(void) { return (void *)0x1234; }

BOOST_AUTO_TEST_CASE(testRemoveType)
{
  BOOST_CHECK_MESSAGE(SoType::fromName(SbName("MyClass")) == SoType::badType(),
                      "Type didn't init to badType");
  SoType newtype = SoType::createType(SoNode::getClassTypeId(), SbName("MyClass"), createInstance, 0);
  BOOST_CHECK_MESSAGE(SoType::fromName(SbName("MyClass")) != SoType::badType(),
                      "Type didn't init correctly");
  bool success = SoType::removeType(SbName("MyClass"));
  BOOST_CHECK_MESSAGE(success, "removeType() failed");
  BOOST_CHECK_MESSAGE(SoType::fromName(SbName("MyClass")) == SoType::badType(),
                      "Type didn't deregister correctly");
}
#endif // COIN_TEST_SUITE
```

### After (tests/misc/test_misc.cpp):
```cpp
#include "utils/test_common.h"
#include <Inventor/SoType.h>
#include <Inventor/SbName.h>
#include <Inventor/nodes/SoNode.h>

using namespace CoinTestUtils;

static void * createInstance(void) { return (void *)0x1234; }

TEST_CASE("SoType tests", "[misc][SoType]") {
    CoinTestFixture fixture;

    SECTION("testRemoveType") {
        CHECK(SoType::fromName(SbName("MyClass")) == SoType::badType());
        
        SoType newtype = SoType::createType(SoNode::getClassTypeId(), SbName("MyClass"), createInstance, 0);
        CHECK(SoType::fromName(SbName("MyClass")) != SoType::badType());
        
        bool success = SoType::removeType(SbName("MyClass"));
        CHECK(success);
        CHECK(SoType::fromName(SbName("MyClass")) == SoType::badType());
    }
}
```

## Benefits of New System

1. **Clarity**: Tests are in separate files, easier to find and modify
2. **Organization**: Logical grouping by module/functionality
3. **Maintainability**: No code generation, direct compilation
4. **Modern Framework**: Catch2 provides better output and features
5. **Independence**: Tests can be run individually or by category
6. **CI Integration**: Better test reporting and failure analysis

## Next Steps

1. Port remaining tests from all modules
2. Add comprehensive test coverage for untested functionality
3. Remove comment-driven test generation system
4. Update CI/CD to use new test structure
5. Create test templates and guidelines for new tests

## Migration Checklist

- [x] Setup Catch2 framework
- [x] Create test directory structure 
- [x] Port base module tests (SbBox3s, SbBox3f, SbBSPTree)
- [x] Port field tests (SoSFVec4b, SoSFBool, SoSFFloat)
- [x] Port misc tests (SoType)
- [ ] Port remaining base module tests
- [ ] Port all field tests
- [ ] Port node tests
- [ ] Port engine tests
- [ ] Port action tests
- [ ] Remove test generation system
- [ ] Update documentation