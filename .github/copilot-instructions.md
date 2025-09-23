# Coin 3D Graphics Library - Copilot Instructions

## Repository Overview

**Coin** is a mature, large-scale C++ library implementing the Open Inventor 2.1 API for 3D graphics and visualization. This is an OpenGL-based, scene graph retained mode rendering library originally designed by SGI, now maintained as open source.

- **Size**: ~70MB repository, 959 C/C++ source files, 160 header files
- **Language**: C++11 standard (minimum required)
- **License**: BSD (c) Kongsberg Oil & Gas Technologies AS
- **Version**: 4.0.5 (major.minor.micro versioning scheme)
- **Build Systems**: CMake (primary), Autotools (legacy/lower priority)

## Build Requirements and Dependencies

### Essential Dependencies
- **CMake 3.0+** (tested with 3.31.6) - primary build system

### Documentation Dependencies
- **Doxygen** - for API documentation generation (required by default)

### Platform-Specific Requirements
- **Linux**: `sudo apt-get install gdb`
- **Windows**: Visual Studio 2022
- **macOS**: XQuartz for X11 builds

## Critical Build Instructions

### CMake Build (Primary - Always Use This)

**ALWAYS clone with submodules**:
```bash
git clone --recurse-submodules https://github.com/coin3d/coin
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
- Default documentation generation can be disabled with `-DCOIN_BUILD_DOCUMENTATION=OFF`

### Testing
```bash
cd build_dir
ctest -C Release -VV
```
- **327 test cases** - all should pass
- Test execution time: ~1-2 seconds
- Uses single executable `CoinTests` in `build_dir/bin/`

### Common Build Issues & Workarounds

1. **Missing submodules**: Repository MUST be cloned with `--recurse-submodules`
2. **Format warnings in xml/element.cpp**: Expected warnings about `%lld` format specifiers (non-fatal)
3. **Template files (.in)**: 164 template files generate source during build - don't edit generated files
4. **Build artifacts**: Clean with `rm -rf build_dir install_dir` (no top-level .gitignore exists)

## Autotools Build (Legacy - Lower Priority)

The autotools build system still works but receives lower maintenance priority:
```bash
./configure --prefix=install_dir --enable-debug
make install
```

**Special autotools options**:
- `--enable-hacking`: Creates modular shared libraries (one per src/ subdirectory) for faster development iteration
- `--enable-compact`: For systems with "Arg list too long" errors (AIX)
- `--enable-debug`, `--enable-symbols`: Debug builds

## Project Architecture and Layout

### Source Code Organization
```
src/                    # Main source code (42 subdirectories)
├── actions/           # Action classes
├── base/              # Base utility classes  
├── elements/          # Scene graph elements
├── engines/           # Field engines
├── fields/            # Field types
├── nodes/             # Scene graph nodes
├── sensors/           # Event sensors
└── ... (35+ more subdirs)

include/               # Public header files
examples/              # Example applications (glfw, sdl3, misc, bindings)
docs/                  # Documentation and API guides
testsuite/            # Test framework and test cases
data/                 # Resource files (shaders, draggers, etc.)
```

### Key Configuration Files
- **CMakeLists.txt** (root): Main build configuration (37KB)
- **configure.ac**: Autotools configuration  
- **src/CMakeLists.txt**: Per-directory build files (2182 lines total)
- **.github/workflows/**: CI/CD for Ubuntu, Windows, macOS
  - `continuous-integration-workflow.yml`: Main CI pipeline
  - `documentation-workflow.yml`: Doc generation
  - `codeql.yml`: Security analysis

### Build Targets Available
- `all` (default): Build entire library
- `install`: Install headers and libraries
- `test`: Run test suite via CTest
- `clean`: Clean build artifacts
- Individual component targets (actions, base, elements, etc.)

## Development and Validation Workflow

### Before Making Changes
1. **Always run existing build and tests first**:
   ```bash
   cmake -S . -B build_dir -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
   cmake --build build_dir --config Release -- -j4
   cd build_dir && ctest -C Release -VV
   ```

### After Making Changes
1. **Incremental build** (builds only changed components):
   ```bash
   cmake --build build_dir --config Release
   ```
2. **Run tests**: `cd build_dir && ctest -C Release -VV`
3. **Check for new warnings** - the build should produce minimal warnings

### Known Issues in Codebase
- **1851 HACK/TODO/FIXME comments** exist in source code - these indicate known issues
- Most are non-critical legacy items, but review relevant ones when working in those areas
- Critical issues are documented in `docs/BUGS.txt`

## Continuous Integration

### GitHub Actions Workflow
- **Triggers**: Pull requests to master branch, manual dispatch
- **Platforms**: Ubuntu (latest), Windows (latest), macOS-14
- **Build time**: ~5-10 minutes per platform
- **Artifact upload**: Build results saved as artifacts
- **Dependencies**: Auto-installed per platform via package managers

### Validation Steps
1. CMake configuration and build
2. Full test suite execution (`ctest -C Release -VV`)
3. Cross-platform compatibility verification
4. Build artifact generation and upload

## Documentation Generation

- **Doxygen configuration**: `docs/coin.doxygen.cmake.in`
- **Disable docs**: Add `-DCOIN_BUILD_DOCUMENTATION=OFF` to cmake command
- **Doc dependencies**: Requires Doxygen installation
- **Output**: HTML documentation generated in build directory

## Important Notes for Coding Agents

### Trust These Instructions
**Follow this guide precisely** - only search for additional information if these instructions are incomplete or proven incorrect. This guide represents validated build and development procedures.

### File Management
- **Never edit generated files** (template .in files generate .cpp/.h files)
- **Use proper .gitignore patterns**: Exclude `build_dir/`, `install_dir/`, `*_build/`, `*_install/`
- **Generated files**: Many files in src/ are generated during build - check if file exists in git before editing

### Build Timing
- **Configuration**: ~5 seconds
- **Full build**: 5-10 minutes  
- **Incremental builds**: 10-60 seconds
- **Test execution**: 1-2 seconds
- **Documentation**: 2-5 minutes (if enabled)

### Platform Considerations
- **Linux**: Primary development platform, most reliable
- **Windows**: Requires Visual Studio, uses different boost setup
- **macOS**: XQuartz needed for X11 builds, framework builds available
- **Cross-platform**: All platforms supported but build dependencies vary

This library is mature and stable. Focus changes on specific components rather than broad architectural modifications. The extensive test suite (327 tests) should always pass after changes.
