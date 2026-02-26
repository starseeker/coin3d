# =============================================================================
# tests/obol_superbuild/CMakeLists.txt
#
# NOTE: The bridge/superbuild pattern has been eliminated.  The FLTK viewer
# now links directly against the Coin library (built with COIN3D_BUILD_DUAL_GL
# by default) and uses Obol's public API without any bridge libraries or
# dlopen machinery.
#
# To build the viewer, configure the top-level Obol project with:
#
#   cmake <obol_src> -DCOIN_BUILD_VIEWER=ON [other options]
#   cmake --build <build_dir>
#   ./<build_dir>/bin/obol_viewer
#
# The COIN_BUILD_DUAL_GL flag is auto-detected when both system OpenGL and
# the external/osmesa submodule are present (which is the default on Linux
# with libgl-dev installed).  You can override with:
#
#   -DCOIN3D_USE_SYSTEM_ONLY=ON   for system OpenGL only
#   -DCOIN3D_USE_OSMESA=ON        for OSMesa-only (no system GL)
#
# This directory is kept for reference; it no longer drives a build.
# =============================================================================
cmake_minimum_required(VERSION 3.16)
project(ObolViewerInfo LANGUAGES NONE)
message(STATUS "")
message(STATUS "  The bridge/superbuild pattern has been eliminated.")
message(STATUS "  Build the viewer via the top-level project with -DCOIN_BUILD_VIEWER=ON")
message(STATUS "")
