# =============================================================================
# examples/qt/CMakeLists.txt
#
# Builds the Qt6-based GUI examples for Obol:
#   obol_qt_viewer   — full-featured Qt scene viewer (scene catalogue browser)
#   qt_obol_example  — standalone Qt/Obol widget demonstration
#
# Qt6 detection and viewer options all live here.
#
# obol_qt_viewer uses examples/demo_scenes/, so it is independent of the
# test registry and can be built with OBOL_BUILD_TESTS=OFF.
#
# Requires Qt6 Widgets + Gui (+ OpenGL/OpenGLWidgets for qt_obol_example).
# =============================================================================

# -----------------------------------------------------------------------
# Qt6 detection and viewer option
# -----------------------------------------------------------------------
find_package(Qt6 QUIET COMPONENTS Widgets Gui)
if(Qt6_FOUND)
    message(STATUS "Qt6 found (${Qt6_VERSION}): obol_qt_viewer can be built")
    set(_obol_qt_viewer_default ON)
else()
    message(STATUS "Qt6 not found – obol_qt_viewer disabled")
    set(_obol_qt_viewer_default OFF)
endif()
cmake_dependent_option(OBOL_BUILD_QT_VIEWER
    "Build the Qt feature-demo viewer; requires Qt6"
    "${_obol_qt_viewer_default}"
    "Qt6_FOUND"
    OFF)
unset(_obol_qt_viewer_default)

# -----------------------------------------------------------------------
# obol_qt_viewer: full-featured Qt6 scene viewer
# -----------------------------------------------------------------------
if(OBOL_BUILD_QT_VIEWER)

    add_executable(obol_qt_viewer obol_qt_viewer.cpp)

    set_target_properties(obol_qt_viewer PROPERTIES
        AUTOMOC ON
        AUTOUIC ON
    )

    target_include_directories(obol_qt_viewer PRIVATE
        ${PROJECT_SOURCE_DIR}/examples/demo_support
        ${PROJECT_SOURCE_DIR}/examples/demo_scenes
        ${CMAKE_CURRENT_SOURCE_DIR}           # qt_obol_widget.h (for qt_obol_example)
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/Inventor/annex
        ${PROJECT_BINARY_DIR}/include
        ${OBOL_TARGET_INCLUDE_DIRECTORIES}
    )

    target_link_libraries(obol_qt_viewer
        PRIVATE
            obol_demo_scenes
            ${PROJECT_NAME}
            ${OBOL_TARGET_LINK_LIBRARIES}
            Qt6::Widgets
            Qt6::Gui
    )

    if(UNIX AND NOT APPLE)
        if(X11_FOUND)
            target_link_libraries(obol_qt_viewer PRIVATE ${X11_X11_LIB})
        endif()
        if(TARGET OpenGL::GL)
            target_link_libraries(obol_qt_viewer PRIVATE OpenGL::GL)
        elseif(OPENGL_gl_LIBRARY)
            target_link_libraries(obol_qt_viewer PRIVATE ${OPENGL_gl_LIBRARY})
        endif()
    endif()

    install(TARGETS obol_qt_viewer RUNTIME DESTINATION bin)
    message(STATUS "obol_qt_viewer: enabled (Qt6 ${Qt6_VERSION})")
endif() # OBOL_BUILD_QT_VIEWER

# -----------------------------------------------------------------------
# qt_obol_example: standalone Qt/Obol widget demonstration
#
# Demonstrates QtObolWidget — the self-contained Qt/Obol integration
# widget (qt_obol_widget.h).  Built when Qt6 Widgets + OpenGL is found.
# Enable explicitly with -DOBOL_BUILD_QT_EXAMPLE=ON.
# -----------------------------------------------------------------------
option(OBOL_BUILD_QT_EXAMPLE "Build the Qt/Obol widget example (requires Qt6)" OFF)

if(OBOL_BUILD_QT_EXAMPLE)
    find_package(Qt6 QUIET COMPONENTS Core Widgets OpenGL OpenGLWidgets)
    if(Qt6Widgets_FOUND AND Qt6OpenGL_FOUND AND Qt6OpenGLWidgets_FOUND)
        add_executable(qt_obol_example
            qt_obol_example.cpp
            qt_obol_widget.h   # listed so AUTOMOC finds Q_OBJECT
        )
        set_target_properties(qt_obol_example PROPERTIES AUTOMOC ON)
        target_include_directories(qt_obol_example PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}   # qt_obol_widget.h
            ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}/include/Inventor/annex
            ${PROJECT_BINARY_DIR}/include
            ${OBOL_TARGET_INCLUDE_DIRECTORIES}
        )
        target_link_libraries(qt_obol_example PRIVATE
            ${PROJECT_NAME}
            ${OBOL_TARGET_LINK_LIBRARIES}
            Qt6::Core
            Qt6::Widgets
            Qt6::OpenGL
            Qt6::OpenGLWidgets
        )
        message(STATUS "qt_obol_example: enabled (Qt6 ${Qt6Widgets_VERSION})")
    else()
        message(STATUS "qt_obol_example: disabled (Qt6 Widgets/OpenGL/OpenGLWidgets not found)")
    endif()
endif()
