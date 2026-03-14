# GitHub Copilot Instructions for Obol

## Development Environment Setup

### Required System Packages

Before building Obol, install the following development packages. On Ubuntu/Debian:

```bash
sudo apt-get update
sudo apt-get install -y \
    cmake build-essential \
    libx11-dev libxext-dev libxi-dev libxrandr-dev \
    libxcursor-dev libxft-dev libxinerama-dev \
    libgl-dev libglu1-mesa-dev mesa-common-dev \
    libfontconfig-dev libfreetype-dev \
    qt6-base-dev qt6-base-private-dev libqt6opengl6-dev \
    libembree-dev \
    vulkan-sdk libvulkan-dev glslang-tools \
    libpng-dev \
    xvfb
```

On Fedora/RHEL/Rocky:

```bash
sudo dnf install -y \
    cmake gcc-c++ \
    libX11-devel libXext-devel libXi-devel libXrandr-devel \
    libXcursor-devel libXft-devel libXinerama-devel \
    mesa-libGL-devel mesa-libGLU-devel \
    fontconfig-devel freetype-devel \
    qt6-qtbase-devel \
    embree-devel \
    vulkan-devel glslang \
    libpng-devel \
    xorg-x11-server-Xvfb
```

#### Optional: FLTK via System Package (Linux only)

On Linux, FLTK-based viewers (`obol_viewer`, `obol_minimalist_viewer`) can use
the system FLTK library when available:

```bash
# Ubuntu/Debian
sudo apt-get install -y libfltk1.3-dev
# Fedora/RHEL
sudo dnf install -y fltk-devel
```

If no system FLTK is found, CMake automatically initialises the `external/fltk`
git submodule at configure time and builds FLTK from source (see Submodules).

### Package Groups

| Group | Packages | Purpose |
|-------|----------|---------|
| X11 | `libx11-dev`, `libxext-dev`, `libxi-dev`, `libxrandr-dev`, `libxcursor-dev`, `libxft-dev`, `libxinerama-dev` | X Window System display and input |
| Mesa/OpenGL | `libgl-dev`, `libglu-dev`, `mesa-common-dev` | OpenGL rendering (system GL backend) |
| FLTK | `libfltk1.3-dev` *(optional — system)* | FLTK GUI toolkit for `obol_viewer` and `obol_minimalist_viewer` |
| Qt6 | `qt6-base-dev`, `libqt6opengl6-dev` | Qt6 GUI toolkit for `obol_qt_viewer` and `qt_obol_example` |
| Embree | `libembree-dev` (Embree 4) | Intel Embree CPU ray-tracing panel in viewers |
| Vulkan SDK | `vulkan-sdk`, `libvulkan-dev`, `glslang-tools` | Vulkan hardware-rasterisation panel in viewers |

### Git Submodules

Two external libraries are managed as git submodules in `external/`:

| Submodule | URL | Purpose |
|-----------|-----|---------|
| `external/fltk` | https://github.com/fltk/fltk (branch-1.3) | FLTK GUI toolkit (fallback when system FLTK absent) |
| `external/osmesa` | https://github.com/starseeker/osmesa | Name-mangled OSMesa for dual-GL / headless builds |

CMake initialises each submodule automatically at configure time when it is
needed and the directory is empty.  To initialise them manually:

```bash
# Initialise all submodules at once
git submodule update --init --recursive

# Or initialise just one
git submodule update --init external/fltk
git submodule update --init external/osmesa
```

### Build

```bash
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DOBOL_BUILD_TESTS=ON
cmake --build build -- -j$(nproc)
```

For headless/CI testing with OSMesa:

```bash
cmake -S . -B build -DOBOL_USE_OSMESA=ON -DOBOL_BUILD_TESTS=ON
```

### Running Tests

```bash
cd build
# With a real display:
ctest --output-on-failure
# Headless (Xvfb):
xvfb-run -a ctest --output-on-failure
```

## Repository Layout

```
obol/
├── src/          Core Obol library (Open Inventor subset)
├── include/      Public headers
├── tests/        Test runners and visual regression tests
│   ├── testlib/  libObolEx: shared scene catalog + test registry
│   ├── utils/    Context managers and test utilities
│   ├── rendering/ Visual regression tests
│   └── tools/    Unit tests for utility subsystems
├── examples/     GUI viewers and code examples
│   ├── fltk/     FLTK-based viewers (obol_viewer, obol_minimalist_viewer)
│   ├── qt/       Qt6-based viewer + widget example
│   └── Mentor/   Open Inventor Mentor book examples
└── external/     Third-party dependencies
    ├── fltk/     FLTK submodule (populated on demand when system FLTK absent)
    ├── osmesa/   OSMesa submodule (name-mangled; for dual-GL/headless)
    ├── nanort/   NanoRT header-only raytracer (optional panel in obol_viewer)
    └── lodepng.{cpp,h}  Bundled PNG codec (used by test image comparison)
```

## Code Conventions

- C++17 required; use standard library features (e.g. `std::optional`, `std::string_view`)
- CMake 3.16+ with modern target-based builds (`target_link_libraries`, `target_include_directories`)
- All shared scene/test utilities go in `libObolEx` (`tests/testlib/`) so both tests and examples can link against them
- GUI viewers belong in `examples/` (not `tests/`)
- Headless/CLI test runners belong in `tests/`
- utf8 support lives in `src/base/utf8/` (implementation) — do not use `external/utf8` (removed)
