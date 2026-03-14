# =============================================================================
# examples/CMakeLists.txt
#
# Builds all Obol example programs and GUI viewers.
#
# Directory structure:
#   examples/
#   ├── CMakeLists.txt          this file
#   ├── fltk/                   FLTK-based GUI viewers
#   │   ├── CMakeLists.txt
#   │   ├── obol_viewer.cpp         full-featured multi-panel scene viewer
#   │   └── obol_minimalist_viewer.cpp  basic-context scene stress-tester
#   ├── qt/                     Qt6-based viewer and widget example
#   │   ├── CMakeLists.txt
#   │   ├── obol_qt_viewer.cpp      Qt6 scene catalogue browser
#   │   ├── qt_obol_example.cpp     standalone QtObolWidget demo
#   │   └── qt_obol_widget.h        self-contained Qt/Obol widget
#   ├── Mentor/                 Open Inventor Mentor book examples
#   ├── arb8_dragger_example.cpp
#   ├── truncated_cone_example.cpp
#   ├── modern_api_example.h
#   └── osmesa_example.h
#
# Both FLTK and Qt viewers link against ObolEx (libObolEx) which provides
# the shared scene catalogue and test registry (built in tests/testlib/).
# =============================================================================

# -----------------------------------------------------------------------
# FLTK viewers (obol_viewer, obol_minimalist_viewer)
# -----------------------------------------------------------------------
if(OBOL_BUILD_VIEWER OR OBOL_BUILD_MINIMALIST_VIEWER)
    if(FLTK_FOUND)
        add_subdirectory(fltk)
    else()
        message(STATUS "examples/fltk: skipped (FLTK not found)")
    endif()
endif()

# -----------------------------------------------------------------------
# Qt viewers and examples (obol_qt_viewer, qt_obol_example)
# -----------------------------------------------------------------------
if(OBOL_BUILD_QT_VIEWER OR OBOL_BUILD_QT_EXAMPLE)
    if(Qt6_FOUND)
        add_subdirectory(qt)
    else()
        message(STATUS "examples/qt: skipped (Qt6 not found)")
    endif()
endif()

# -----------------------------------------------------------------------
# Mentor book examples (Open Inventor Mentor chapters)
# -----------------------------------------------------------------------
add_subdirectory(Mentor)

# -----------------------------------------------------------------------
# Standalone headless rendering examples
# These examples render scenes using the headless backend (headless_utils.h)
# and write SGI RGB output images.  They demonstrate various Obol features
# without requiring a display server.
# -----------------------------------------------------------------------

# arb8_dragger_example: SoProceduralShape with interactive SoDragger handles
add_executable(arb8_dragger_example arb8_dragger_example.cpp)
target_include_directories(arb8_dragger_example PRIVATE
    ${PROJECT_SOURCE_DIR}/tests/utils   # headless_utils.h
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${OBOL_TARGET_INCLUDE_DIRECTORIES}
)
target_link_libraries(arb8_dragger_example PRIVATE
    ${PROJECT_NAME}
    ${OBOL_TARGET_LINK_LIBRARIES}
)
if(OBOL_USE_OSMESA)
    target_compile_definitions(arb8_dragger_example PRIVATE OBOL_OSMESA_BUILD)
    target_include_directories(arb8_dragger_example PRIVATE
        "${PROJECT_SOURCE_DIR}/external/osmesa/include")
elseif(OBOL_BUILD_DUAL_GL)
    target_compile_definitions(arb8_dragger_example PRIVATE OBOL_BUILD_DUAL_GL)
endif()
if(UNIX AND NOT APPLE)
    if(X11_FOUND)
        target_link_libraries(arb8_dragger_example PRIVATE ${X11_X11_LIB})
    endif()
    if(TARGET OpenGL::GL)
        target_link_libraries(arb8_dragger_example PRIVATE OpenGL::GL)
    elseif(OPENGL_gl_LIBRARY)
        target_link_libraries(arb8_dragger_example PRIVATE ${OPENGL_gl_LIBRARY})
    endif()
endif()

# truncated_cone_example: SoProceduralShape with a custom frustum geometry type
add_executable(truncated_cone_example truncated_cone_example.cpp)
target_include_directories(truncated_cone_example PRIVATE
    ${PROJECT_SOURCE_DIR}/tests/utils   # headless_utils.h
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${OBOL_TARGET_INCLUDE_DIRECTORIES}
)
target_link_libraries(truncated_cone_example PRIVATE
    ${PROJECT_NAME}
    ${OBOL_TARGET_LINK_LIBRARIES}
)
if(OBOL_USE_OSMESA)
    target_compile_definitions(truncated_cone_example PRIVATE OBOL_OSMESA_BUILD)
    target_include_directories(truncated_cone_example PRIVATE
        "${PROJECT_SOURCE_DIR}/external/osmesa/include")
elseif(OBOL_BUILD_DUAL_GL)
    target_compile_definitions(truncated_cone_example PRIVATE OBOL_BUILD_DUAL_GL)
endif()
if(UNIX AND NOT APPLE)
    if(X11_FOUND)
        target_link_libraries(truncated_cone_example PRIVATE ${X11_X11_LIB})
    endif()
    if(TARGET OpenGL::GL)
        target_link_libraries(truncated_cone_example PRIVATE OpenGL::GL)
    elseif(OPENGL_gl_LIBRARY)
        target_link_libraries(truncated_cone_example PRIVATE ${OPENGL_gl_LIBRARY})
    endif()
endif()
