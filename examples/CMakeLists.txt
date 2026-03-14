# =============================================================================
# examples/CMakeLists.txt
#
# Builds all Obol example programs and GUI viewers.
#
# Directory structure:
#   examples/
#   ├── CMakeLists.txt          this file
#   ├── fltk/                   FLTK-based GUI viewers
#   │   ├── CMakeLists.txt      FLTK detection + viewer options live here
#   │   ├── obol_viewer.cpp         full-featured multi-panel scene viewer
#   │   └── obol_minimalist_viewer.cpp  basic-context scene stress-tester
#   ├── qt/                     Qt6-based viewer and widget example
#   │   ├── CMakeLists.txt      Qt6 detection + viewer options live here
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
# Detection, submodule management and viewer options live in fltk/.
# -----------------------------------------------------------------------
add_subdirectory(fltk)

# -----------------------------------------------------------------------
# Qt viewers and examples (obol_qt_viewer, qt_obol_example)
# Qt6 detection and viewer options live in qt/.
# -----------------------------------------------------------------------
add_subdirectory(qt)

# -----------------------------------------------------------------------
# Mentor book examples (Open Inventor Mentor chapters)
# Uses headless_utils.h → requires a GL backend
# -----------------------------------------------------------------------
if(OBOL_USE_SWRAST OR OBOL_USE_SYSTEM_GL)
    add_subdirectory(Mentor)
else()
    message(STATUS "examples/Mentor: skipped (no OpenGL backend configured)")
endif()

# -----------------------------------------------------------------------
# Standalone headless rendering examples
# These examples render scenes using the headless backend (headless_utils.h)
# and write SGI RGB output images.  They demonstrate various Obol features
# without requiring a display server.
# Skipped in no-OpenGL mode (both OBOL_USE_SWRAST and OBOL_USE_SYSTEM_GL off).
# -----------------------------------------------------------------------
if(OBOL_USE_SWRAST OR OBOL_USE_SYSTEM_GL)

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
if(OBOL_USE_SWRAST AND NOT OBOL_USE_SYSTEM_GL)
    target_compile_definitions(arb8_dragger_example PRIVATE OBOL_SWRAST_BUILD)
    target_include_directories(arb8_dragger_example PRIVATE
        "${PROJECT_SOURCE_DIR}/external/osmesa/include")
elseif(OBOL_USE_SWRAST AND OBOL_USE_SYSTEM_GL)
    target_compile_definitions(arb8_dragger_example PRIVATE OBOL_SWRAST_BUILD OBOL_DUAL_GL_BUILD)
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
if(OBOL_USE_SWRAST AND NOT OBOL_USE_SYSTEM_GL)
    target_compile_definitions(truncated_cone_example PRIVATE OBOL_SWRAST_BUILD)
    target_include_directories(truncated_cone_example PRIVATE
        "${PROJECT_SOURCE_DIR}/external/osmesa/include")
elseif(OBOL_USE_SWRAST AND OBOL_USE_SYSTEM_GL)
    target_compile_definitions(truncated_cone_example PRIVATE OBOL_SWRAST_BUILD OBOL_DUAL_GL_BUILD)
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

endif() # OBOL_USE_SWRAST OR OBOL_USE_SYSTEM_GL
