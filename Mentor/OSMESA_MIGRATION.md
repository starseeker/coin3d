# OSMesa Migration Summary

This document describes the changes made to adapt the Mentor examples to work with the reworked Coin3D library that uses OSMesa for headless rendering.

## Overview

The Mentor directory contains headless versions of the Inventor Mentor examples. These examples were originally written for vanilla Coin3D but needed updates to work with the reworked version that uses OSMesa (Off-Screen Mesa) instead of system OpenGL.

## Major Changes

### 1. Build System Updates (CMakeLists.txt)

**Changes:**
- Removed dependency on system OpenGL package
- Updated to use Coin target directly from parent build
- Added OSMesa include directory from external/osmesa submodule
- Link against `osmesa_interface` target for OSMesa functions
- Updated CMake minimum version to 3.16

**Key changes:**
```cmake
# Use Coin target from parent build
if(NOT TARGET Coin)
    find_package(Coin REQUIRED)
    set(COIN_TARGET Coin::Coin)
else()
    set(COIN_TARGET Coin)
    set(OSMESA_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../external/osmesa/include")
endif()

# Link against both Coin and osmesa_interface
target_link_libraries(${name} ${COIN_TARGET} osmesa_interface)
```

### 2. Initialization Updates (headless_utils.h)

**Changes:**
- Added OSMesa headers (`<OSMesa/osmesa.h>`, `<OSMesa/gl.h>`)
- Implemented `CoinOSMesaContext` structure for OSMesa context management
- Implemented `CoinOSMesaContextManager` class implementing `SoDB::ContextManager` interface
- Updated `initCoinHeadless()` to pass context manager to `SoDB::init()`

**Key implementation:**
```cpp
class CoinOSMesaContextManager : public SoDB::ContextManager {
public:
    virtual void* createOffscreenContext(unsigned int width, unsigned int height) override;
    virtual SbBool makeContextCurrent(void* context) override;
    virtual void restorePreviousContext(void* context) override;
    virtual void destroyContext(void* context) override;
};

inline void initCoinHeadless() {
    static CoinOSMesaContextManager osmesa_manager;
    SoDB::init(&osmesa_manager);
}
```

### 3. API Changes in Examples

#### 3.1 NURBS Examples Disabled (Chapter 8)

**Issue:** NURBS nodes (`SoNurbsCurve`, `SoNurbsSurface`) were removed from the OSMesa build.

**Solution:** Disabled building of chapter 8 examples:
- 08.1.BSCurve
- 08.2.UniCurve
- 08.3.BezSurf
- 08.4.TrimSurf

**Status:** 54 out of 58 examples now build (4 NURBS examples skipped)

#### 3.2 SoShapeKit API Changes (10.8.PickFilterNodeKit.cpp)

**Issue:** `SoShapeKit::getChild()` method no longer exists.

**Solution:** Use `getPart()` method instead:
```cpp
// Old code:
path->append(((SoShapeKit*)sel->getChild(2))->getChild(0));

// New code:
SoShapeKit *shapeKit = (SoShapeKit*)sel->getChild(2);
path->append(shapeKit->getPart("shape", FALSE));
```

#### 3.3 SoMaterial Destructor Protection (10.8, 16.2, 16.3)

**Issue:** `SoMaterial` destructor is now protected - cannot create on stack.

**Solution:** Use ref/unref pattern with heap allocation:
```cpp
// Old code:
SoMaterial redMtl;
redMtl.diffuseColor.setValue(1.0f, 0.0f, 0.0f);
ed->setMaterial(redMtl);

// New code:
SoMaterial *redMtl = new SoMaterial;
redMtl->ref();
redMtl->diffuseColor.setValue(1.0f, 0.0f, 0.0f);
ed->setMaterial(*redMtl);
redMtl->unref();
```

**Affected files:**
- 10.8.PickFilterNodeKit.cpp (3 instances)
- 16.2.Callback.cpp (3 instances)
- 16.3.AttachEditor.cpp (1 instance)

#### 3.4 GL Headers (17.2.GLCallback.cpp)

**Issue:** System GL headers not available in OSMesa build.

**Solution:** Use OSMesa headers:
```cpp
// Old code:
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

// New code:
#include <OSMesa/gl.h>
```

#### 3.5 setTitle() Method (14.2.Editors.cpp)

**Issue:** Mock toolkit classes don't have `setTitle()` method.

**Solution:** Comment out calls (not needed for headless rendering):
```cpp
// myRenderArea->setTitle("NodeKit Editors Demo");  // No title in headless mode
// mtlEditor->setTitle("Material of Desk");  // No title in headless mode
```

## Building

### From Top Level (Recommended)

```bash
cd /path/to/coin3d
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCOIN_BUILD_TESTS=ON
cmake --build build -j4
```

This will build Coin with OSMesa support and all Mentor examples.

### Running Examples

```bash
cd build/Mentor
bin/02.1.HelloCone
bin/17.2.GLCallback
# etc.
```

Output files are generated in the `output/` subdirectory.

## Summary of Changes

### Files Modified
1. **Mentor/CMakeLists.txt** - Build system updates for OSMesa
2. **Mentor/headless_utils.h** - OSMesa context management
3. **Mentor/10.8.PickFilterNodeKit.cpp** - SoShapeKit API, SoMaterial fixes
4. **Mentor/14.2.Editors.cpp** - Remove setTitle() calls
5. **Mentor/16.2.Callback.cpp** - SoMaterial ref/unref pattern
6. **Mentor/16.3.AttachEditor.cpp** - SoMaterial ref/unref pattern
7. **Mentor/17.2.GLCallback.cpp** - OSMesa GL headers

### Build Statistics
- **Total examples in Mentor:** 58
- **Examples disabled (NURBS):** 4 (08.1-08.4)
- **Examples building:** 54
- **Build status:** All 54 examples compile and run successfully

### Key Differences from Vanilla Coin
1. **Context Management:** Must provide ContextManager to SoDB::init()
2. **NURBS Support:** Not available in OSMesa build
3. **GL Headers:** Must use OSMesa headers instead of system OpenGL
4. **SoMaterial:** Protected destructor requires ref/unref pattern
5. **SoShapeKit:** Use getPart() instead of getChild()

## Testing

All 54 examples have been tested and verified to:
1. Compile without errors or warnings (except minor unused variable warnings)
2. Execute successfully
3. Generate output RGB files
4. Complete without crashes

Example outputs are ~1.4MB each (800x600 RGBA images in SGI RGB format).

## Future Work

If NURBS support is added back to the OSMesa build, the following examples can be re-enabled:
- 08.1.BSCurve
- 08.2.UniCurve
- 08.3.BezSurf
- 08.4.TrimSurf

These examples currently exist in the source tree but are commented out in CMakeLists.txt.

## References

- Original vanilla Coin examples: `coin_vanilla/ivexamples/Mentor/`
- OSMesa example template: `examples/osmesa_example.h`
- OSMesa submodule: `external/osmesa/`
