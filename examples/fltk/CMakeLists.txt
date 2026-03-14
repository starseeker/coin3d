# =============================================================================
# examples/fltk/CMakeLists.txt
#
# Builds the FLTK-based GUI viewers for Obol:
#   obol_viewer            — full-featured multi-panel scene viewer
#   obol_minimalist_viewer — basic FLTK context scene stress-tester
#
# Both viewers use the ObolEx (libObolEx) scene catalogue from tests/testlib/
# with --whole-archive so that all REGISTER_TEST static initialisers run.
#
# Requires:
#   FLTK_FOUND    — system FLTK (libfltk1.3-dev or equivalent)
#   OBOL_BUILD_TESTS — ObolEx library is built as part of the test suite
# =============================================================================

# -----------------------------------------------------------------------
# obol_viewer: full-featured FLTK scene viewer
# -----------------------------------------------------------------------
if(OBOL_BUILD_VIEWER)

    add_executable(obol_viewer obol_viewer.cpp)

    target_include_directories(obol_viewer PRIVATE
        ${PROJECT_SOURCE_DIR}/tests           # testlib/test_registry.h, utils/...
        ${PROJECT_SOURCE_DIR}/tests/utils     # fltk_context_manager.h (no prefix)
        ${PROJECT_SOURCE_DIR}/tests/testlib   # test_registry.h (direct)
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/Inventor/annex
        ${PROJECT_BINARY_DIR}/include
        ${OBOL_TARGET_INCLUDE_DIRECTORIES}
        ${FLTK_INCLUDE_DIRS}
    )

    # Compile definitions matching the Obol library build
    if(OBOL_BUILD_DUAL_GL)
        target_compile_definitions(obol_viewer PRIVATE OBOL_BUILD_DUAL_GL)
    elseif(OBOL_USE_OSMESA)
        target_compile_definitions(obol_viewer PRIVATE OBOL_OSMESA_BUILD)
        target_include_directories(obol_viewer PRIVATE
            "${PROJECT_SOURCE_DIR}/external/osmesa/include")
    endif()

    # Select the system-GL context manager for the viewer.
    if(OBOL_VIEWER_USE_FLTK_GL AND (OBOL_BUILD_DUAL_GL OR NOT OBOL_USE_OSMESA))
        target_compile_definitions(obol_viewer PRIVATE OBOL_VIEWER_FLTK_GL)
        if(OBOL_BUILD_DUAL_GL)
            message(STATUS "obol_viewer: using FLTK GL context manager for system-GL panel (dual-GL build)")
        else()
            message(STATUS "obol_viewer: using FLTK GL context manager (portable, hidden 1x1 window)")
        endif()
    else()
        if(NOT OBOL_USE_OSMESA)
            message(STATUS "obol_viewer: using GLX context manager (headless_utils.h)")
        endif()
    endif()

    # Optional CPU raytracing panels: Embree and/or NanoRT.
    find_package(embree 4 QUIET
        HINTS /usr/lib/x86_64-linux-gnu/cmake/embree-4.3.0
              /usr/lib/cmake/embree4
              /usr/local/lib/cmake/embree4)
    if(embree_FOUND OR TARGET embree4)
        if(TARGET embree4)
            set(_obol_embree_link embree4)
        else()
            set(_obol_embree_link embree)
        endif()
        option(OBOL_VIEWER_USE_EMBREE
            "Use Embree as the CPU raytracing panel in obol_viewer"
            ON)
    else()
        option(OBOL_VIEWER_USE_EMBREE
            "Use Embree as the CPU raytracing panel in obol_viewer"
            OFF)
    endif()

    if(OBOL_VIEWER_USE_EMBREE AND (embree_FOUND OR TARGET embree4))
        target_compile_definitions(obol_viewer PRIVATE OBOL_VIEWER_EMBREE)
        target_include_directories(obol_viewer PRIVATE
            "${PROJECT_SOURCE_DIR}/tests/utils"   # embree_context_manager.h
        )
        target_link_libraries(obol_viewer PRIVATE ${_obol_embree_link})
        message(STATUS "obol_viewer: Embree panel enabled")
    endif()
    if(EXISTS "${PROJECT_SOURCE_DIR}/external/nanort/nanort.h")
        target_compile_definitions(obol_viewer PRIVATE OBOL_VIEWER_NANORT)
        target_include_directories(obol_viewer PRIVATE
            "${PROJECT_SOURCE_DIR}/external/nanort"
            "${PROJECT_SOURCE_DIR}/tests/utils"   # nanort_context_manager.h
        )
        message(STATUS "obol_viewer: NanoRT panel enabled (external/nanort/nanort.h found)")
    endif()

    # Optional Vulkan rasterisation panel.
    find_package(Vulkan QUIET)
    if(Vulkan_FOUND)
        option(OBOL_VIEWER_USE_VULKAN
            "Enable Vulkan hardware-rasterisation panel in obol_viewer"
            ON)
    else()
        option(OBOL_VIEWER_USE_VULKAN
            "Enable Vulkan hardware-rasterisation panel in obol_viewer"
            OFF)
    endif()
    if(OBOL_VIEWER_USE_VULKAN AND Vulkan_FOUND)
        target_compile_definitions(obol_viewer PRIVATE OBOL_VIEWER_VULKAN)
        target_include_directories(obol_viewer PRIVATE
            "${PROJECT_SOURCE_DIR}/tests/utils"   # vulkan_context_manager.h
        )
        target_link_libraries(obol_viewer PRIVATE Vulkan::Vulkan)
        message(STATUS "obol_viewer: Vulkan panel enabled")
    elseif(OBOL_VIEWER_USE_VULKAN AND NOT Vulkan_FOUND)
        message(STATUS "obol_viewer: Vulkan panel requested but Vulkan not found")
    else()
        message(STATUS "obol_viewer: Vulkan panel disabled")
    endif()

    # Link against ObolEx (libObolEx) with --whole-archive so REGISTER_TEST
    # static initialisers are not discarded, plus the main Obol library.
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_link_libraries(obol_viewer
            PRIVATE
                -Wl,--whole-archive ObolEx -Wl,--no-whole-archive
                ${PROJECT_NAME}
                ${OBOL_TARGET_LINK_LIBRARIES}
                ${FLTK_LIBRARIES}
        )
    else()
        target_link_libraries(obol_viewer
            PRIVATE
                ObolEx
                ${PROJECT_NAME}
                ${OBOL_TARGET_LINK_LIBRARIES}
                ${FLTK_LIBRARIES}
        )
    endif()
    if(UNIX AND NOT APPLE)
        if(X11_FOUND)
            target_link_libraries(obol_viewer PRIVATE ${X11_X11_LIB})
        endif()
        if(TARGET OpenGL::GL)
            target_link_libraries(obol_viewer PRIVATE OpenGL::GL)
        elseif(OPENGL_gl_LIBRARY)
            target_link_libraries(obol_viewer PRIVATE ${OPENGL_gl_LIBRARY})
        endif()
    endif()

    find_package(Threads REQUIRED)
    target_link_libraries(obol_viewer PRIVATE Threads::Threads)
    message(STATUS "obol_viewer: C++17 std::thread parallel CPU raytracing enabled")

    install(TARGETS obol_viewer RUNTIME DESTINATION bin)
endif() # OBOL_BUILD_VIEWER

# -----------------------------------------------------------------------
# obol_minimalist_viewer: basic FLTK context scene stress-tester
# -----------------------------------------------------------------------
option(OBOL_BUILD_MINIMALIST_VIEWER
    "Build obol_minimalist_viewer (BasicFLTKContextManager scene stress-tester)"
    ${OBOL_BUILD_VIEWER})

if(OBOL_BUILD_MINIMALIST_VIEWER AND FLTK_FOUND AND OBOL_BUILD_TESTS)
    add_executable(obol_minimalist_viewer obol_minimalist_viewer.cpp)

    target_include_directories(obol_minimalist_viewer PRIVATE
        ${PROJECT_SOURCE_DIR}/tests           # testlib/test_registry.h, utils/...
        ${PROJECT_SOURCE_DIR}/tests/utils     # fltk_context_manager.h
        ${PROJECT_SOURCE_DIR}/tests/testlib
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/Inventor/annex
        ${PROJECT_BINARY_DIR}/include
        ${OBOL_TARGET_INCLUDE_DIRECTORIES}
        ${FLTK_INCLUDE_DIRS}
    )

    # BasicFLTKContextManager always uses the FLTK GL path.
    target_compile_definitions(obol_minimalist_viewer PRIVATE OBOL_VIEWER_FLTK_GL)

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_link_libraries(obol_minimalist_viewer
            PRIVATE
                -Wl,--whole-archive ObolEx -Wl,--no-whole-archive
                ${PROJECT_NAME}
                ${OBOL_TARGET_LINK_LIBRARIES}
                ${FLTK_LIBRARIES}
        )
    else()
        target_link_libraries(obol_minimalist_viewer
            PRIVATE
                ObolEx
                ${PROJECT_NAME}
                ${OBOL_TARGET_LINK_LIBRARIES}
                ${FLTK_LIBRARIES}
        )
    endif()

    if(UNIX AND NOT APPLE)
        if(X11_FOUND)
            target_link_libraries(obol_minimalist_viewer PRIVATE ${X11_X11_LIB})
        endif()
        if(TARGET OpenGL::GL)
            target_link_libraries(obol_minimalist_viewer PRIVATE OpenGL::GL)
        elseif(OPENGL_gl_LIBRARY)
            target_link_libraries(obol_minimalist_viewer PRIVATE ${OPENGL_gl_LIBRARY})
        endif()
    endif()

    find_package(Threads REQUIRED)
    target_link_libraries(obol_minimalist_viewer PRIVATE Threads::Threads)

    install(TARGETS obol_minimalist_viewer RUNTIME DESTINATION bin)
    message(STATUS "obol_minimalist_viewer: enabled (BasicFLTKContextManager)")
endif() # OBOL_BUILD_MINIMALIST_VIEWER
