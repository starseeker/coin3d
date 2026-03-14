# =============================================================================
# tests/testlib/CMakeLists.txt
#
# Builds the ObolEx (libObolEx) static library — the shared utility library
# used by both test runners (tests/) and GUI example viewers (examples/).
# It provides:
#   - TestRegistry (test_registry.h / test_registry.cpp)
#   - Scene factories (test_scenes.h / test_scenes.cpp)
#   - Inline unit-test wrappers + REGISTER_TEST static initialisers
#     (unit_test_wrappers.cpp)
#
# Both test executables and example viewers link against this library.
# An alias target "obol_testlib" is provided for backward compatibility.
# =============================================================================

# ---- Backend detection (mirrors tests/rendering/CMakeLists.txt) --------
if(DEFINED OBOL_USE_OSMESA)
    set(TESTLIB_USE_OSMESA ${OBOL_USE_OSMESA})
else()
    set(TESTLIB_USE_OSMESA OFF)
endif()
# Note: OBOL_BUILD_DUAL_GL builds use system GL (GLX+Xvfb) for rendering
# tests, not OSMesa, because the Obol scene traversal uses system GL symbols.
# Only pure OBOL_USE_OSMESA builds use the OSMesa context path here.

# ---- Library target -------------------------------------------------------
add_library(ObolEx STATIC
    test_registry.cpp
    test_scenes.cpp
    unit_test_wrappers.cpp
)

# Backward-compatibility alias so existing code referencing obol_testlib keeps
# working without modification.
add_library(obol_testlib ALIAS ObolEx)

target_include_directories(ObolEx PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}         # test_registry.h, test_scenes.h
    ${CMAKE_SOURCE_DIR}/tests           # test_utils.h
    ${CMAKE_SOURCE_DIR}/tests/utils     # headless_utils.h
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${OBOL_TARGET_INCLUDE_DIRECTORIES}
)

target_link_libraries(ObolEx
    simple_test_utils
    ${PROJECT_NAME}
    ${OBOL_TARGET_LINK_LIBRARIES}
)

# Expose the source-tree fonts directory so scene factories can load
# external font files (e.g. Iosevka) without hard-coding absolute paths.
target_compile_definitions(ObolEx PRIVATE
    OBOL_FONTS_DIR="${CMAKE_SOURCE_DIR}/fonts"
)

# Unity build support for faster testlib compilation
if(OBOL_USE_UNITY_BUILD AND CMAKE_VERSION VERSION_GREATER_EQUAL "3.16")
    set_target_properties(ObolEx PROPERTIES UNITY_BUILD ON)
endif()

# OSMesa backend (pure OBOL_USE_OSMESA builds only)
if(TESTLIB_USE_OSMESA)
    set(OSMESA_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/external/osmesa/include")
    target_compile_definitions(ObolEx PUBLIC OBOL_OSMESA_BUILD)
    if(EXISTS "${OSMESA_INCLUDE_DIR}")
        target_include_directories(ObolEx PUBLIC "${OSMESA_INCLUDE_DIR}")
    endif()
    if(TARGET osmesa_interface)
        target_link_libraries(ObolEx osmesa_interface)
    elseif(TARGET osmesa)
        target_link_libraries(ObolEx osmesa)
    endif()
else()
    # System OpenGL / GLX
    find_package(X11 QUIET)
    find_package(OpenGL QUIET)
    if(X11_FOUND)
        target_include_directories(ObolEx PUBLIC ${X11_INCLUDE_DIR})
        target_link_libraries(ObolEx ${X11_LIBRARIES})
    endif()
    if(OpenGL_OpenGL_FOUND OR OPENGL_FOUND)
        target_link_libraries(ObolEx ${OPENGL_LIBRARIES})
    endif()
endif()

if(USE_PTHREAD)
    target_link_libraries(ObolEx pthread)
endif()
