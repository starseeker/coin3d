# Obol 3D Graphics Library - Copilot Instructions

## Repository Overview

**Obol** is a C++ library implementing a subset of the the Open Inventor 2.1 API (with some variations) for 3D graphics and visualization. This is an OpenGL-based, scene graph retained mode rendering library originally designed by SGI, now maintained as open source.  It is a fork of Coin - goals here are modernization, minimization of dependencies (by eliminating features, in some cases, to focus on core capabilities).

## Build Requirements and Dependencies

### Essential Dependencies
- **CMake 3.0+** (tested with 3.31.6) - primary build system
- **libpng+** development headers for compiling

### Documentation Dependencies
- **Doxygen** - for API documentation generation (required by default)

### Platform-Specific Requirements
- **Linux**: Install the following packages before building. The full set is required to build the bundled FLTK viewer (`obol_viewer`) as well as the core library with GLX support:
  ```bash
  sudo apt-get install -y \
    gdb cmake \
    libpng-dev \
    libx11-dev libxext-dev libxi-dev libxrandr-dev \
    libxcursor-dev libxft-dev libxinerama-dev \
    libgl-dev libglu1-mesa-dev \
    libfontconfig-dev libfreetype-dev
  ```
  - `libx11-dev`, `libxext-dev`, `libxi-dev` – core X11 headers (required for GLX and FLTK)
  - `libxrandr-dev`, `libxcursor-dev`, `libxft-dev`, `libxinerama-dev` – X11 extension headers required by the bundled FLTK
  - `libgl-dev`, `libglu1-mesa-dev` – OpenGL and GLU headers (provides `GL/gl.h`, `GL/glx.h`, `GL/glu.h`)
  - `libfontconfig-dev`, `libfreetype-dev` – font configuration headers (required by FLTK)
  - **Note**: Without these packages the bundled FLTK in `external/fltk` cannot configure or compile, causing `obol_viewer` to be silently disabled and the FLTK-dependent build targets to fail.
- **Windows**: Visual Studio 2022
- **macOS**: XQuartz for X11 builds

## Critical Build Instructions

### CMake Build (Primary - Always Use This)

**ALWAYS clone with submodules**:
```bash
git clone --recurse-submodules https://github.com/starseeker/obol
```

**Standard Release Build**:
```bash
cmake -S . -B build_dir -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=install_dir
cmake --build build_dir --target install --config Release -- -j4
```

**Critical CMake Notes**:
- **Out-of-source builds are ENFORCED** - will fail if attempted in source directory
- Build time: ~5-10 minutes on modern hardware
- Always use `--recurse-submodules` when cloning - required for proper submodule setup

**Critical Testing Notes**:
- When testing with Xvfb, headless CI runners on Github sometimes have difficulties with context setup and access.  See various notes and test program workarounds for how to get GLX to produce images in the CI environment.  Refine notes here if new problems are encountered so future sessions will more readily be able to generate images.
