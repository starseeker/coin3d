cmake_minimum_required(VERSION 3.16)

project(MentorHeadlessExamples)

# Detect how parent Obol was built (OSMesa or system OpenGL)
if(DEFINED OBOL_USE_OSMESA)
    set(EXAMPLES_USE_OSMESA ${OBOL_USE_OSMESA})
else()
    set(EXAMPLES_USE_OSMESA OFF CACHE BOOL "Build examples for OSMesa (ON) or system OpenGL (OFF)")
endif()

if(EXAMPLES_USE_OSMESA)
    if(TARGET Obol)
        set(OSMESA_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../external/osmesa/include")
        # When building as a subdirectory of the main Obol project, the osmesa target
        # is available via the parent project's FindOSMesa.cmake
        if(TARGET osmesa_interface)
            set(OSMESA_LINK_TARGET osmesa_interface)
        elseif(TARGET osmesa)
            set(OSMESA_LINK_TARGET osmesa)
        endif()
    else()
        find_path(OSMESA_INCLUDE_DIR OSMesa/osmesa.h)
        find_library(OSMESA_LIBRARY NAMES osmesa OSMesa)
        if(OSMESA_LIBRARY)
            set(OSMESA_LINK_TARGET ${OSMESA_LIBRARY})
        endif()
    endif()
    message(STATUS "Mentor examples: Building with OSMesa backend")
else()
    message(STATUS "Mentor examples: Building with system OpenGL backend")
    find_package(X11 QUIET)
    find_package(OpenGL QUIET)
endif()

# Image comparison sensitivity control
# Lower values = stricter comparison, Higher values = more tolerant
# Hash threshold: 0-64 (default 5), RMSE threshold: 0-255 (default 5.0)
set(IMAGE_COMPARISON_HASH_THRESHOLD "5" CACHE STRING "Perceptual hash comparison threshold (0-64)")
set(IMAGE_COMPARISON_RMSE_THRESHOLD "5.0" CACHE STRING "RMSE comparison threshold (0-255)")
set(IMAGE_COMPARISON_TEST_TIMEOUT "30" CACHE STRING "Timeout in seconds for each test (default 30)")

# Find Obol package
# Priority:
# 1. When built as part of the main Obol project the Obol target is available directly.
# 2. Try to locate an installed or build-tree package via find_package(Obol).
# 3. Fall back to referencing the build directory directly (standalone build).
if(TARGET Obol)
    # Building inside the main project: Obol target already defined.
    # Set a local alias so the macro below uses the correct target name.
    set(_OBOL_LINK_TARGET Obol)
    set(Obol_FOUND TRUE)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR})
    if(OBOL_TARGET_INCLUDE_DIRECTORIES)
        include_directories(${OBOL_TARGET_INCLUDE_DIRECTORIES})
    endif()
    find_package(X11 QUIET)
    find_package(OpenGL QUIET)
    macro(add_headless_example name)
        add_executable(${name} ${name}.cpp)
        target_include_directories(${name} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}/include/Inventor/annex
            ${PROJECT_BINARY_DIR}/include
            ${OBOL_TARGET_INCLUDE_DIRECTORIES}
        )
        target_link_libraries(${name} ${_OBOL_LINK_TARGET} ${OBOL_TARGET_LINK_LIBRARIES})
        if(EXAMPLES_USE_OSMESA)
            target_compile_definitions(${name} PRIVATE OBOL_OSMESA_BUILD)
            if(OSMESA_INCLUDE_DIR)
                target_include_directories(${name} PRIVATE ${OSMESA_INCLUDE_DIR})
            endif()
            if(TARGET osmesa_interface)
                target_link_libraries(${name} osmesa_interface)
            elseif(TARGET osmesa)
                target_link_libraries(${name} osmesa)
            endif()
        elseif(X11_FOUND)
            target_link_libraries(${name} ${X11_X11_LIB} ${OPENGL_LIBRARIES})
            target_include_directories(${name} PRIVATE ${X11_INCLUDE_DIR})
        endif()
        set_target_properties(${name} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
        )
    endmacro()
else()
    set(Obol_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../build")
    find_package(Obol QUIET)

    if(Obol_FOUND)
        # Use found Obol package (Obol::Obol imported target)
        include_directories(${Obol_INCLUDE_DIRS})
        include_directories(${CMAKE_CURRENT_SOURCE_DIR})
        find_package(X11 QUIET)
        find_package(OpenGL QUIET)
        macro(add_headless_example name)
            add_executable(${name} ${name}.cpp)
            target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
            target_link_libraries(${name} Obol::Obol)
            if(EXAMPLES_USE_OSMESA)
                target_compile_definitions(${name} PRIVATE OBOL_OSMESA_BUILD)
                if(OSMESA_INCLUDE_DIR)
                    target_include_directories(${name} PRIVATE ${OSMESA_INCLUDE_DIR})
                endif()
                if(OSMESA_LINK_TARGET)
                    target_link_libraries(${name} ${OSMESA_LINK_TARGET})
                endif()
            elseif(X11_FOUND)
                target_link_libraries(${name} ${X11_X11_LIB} ${OPENGL_LIBRARIES})
                target_include_directories(${name} PRIVATE ${X11_INCLUDE_DIR})
            endif()
            set_target_properties(${name} PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
            )
        endmacro()
    else()
        # Standalone build: reference the library from the build directory directly
        set(OBOL_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
        set(OBOL_BUILD_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../build/include")
        set(OBOL_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../build/lib")
        include_directories(${OBOL_BUILD_INCLUDE_DIR})
        include_directories(${OBOL_INCLUDE_DIR})
        include_directories(${CMAKE_CURRENT_SOURCE_DIR})
        find_package(OpenGL REQUIRED)
        find_package(X11 QUIET)
        link_directories(${OBOL_LIB_DIR})
        macro(add_headless_example name)
            add_executable(${name} ${name}.cpp)
            target_include_directories(${name} PRIVATE
                ${OBOL_BUILD_INCLUDE_DIR}
                ${OBOL_INCLUDE_DIR}
                ${CMAKE_CURRENT_SOURCE_DIR})
            target_link_libraries(${name} Obol ${OPENGL_LIBRARIES})
            if(EXAMPLES_USE_OSMESA)
                target_compile_definitions(${name} PRIVATE OBOL_OSMESA_BUILD)
                if(OSMESA_INCLUDE_DIR)
                    target_include_directories(${name} PRIVATE ${OSMESA_INCLUDE_DIR})
                endif()
                if(OSMESA_LINK_TARGET)
                    target_link_libraries(${name} ${OSMESA_LINK_TARGET})
                endif()
            elseif(X11_FOUND)
                target_link_libraries(${name} ${X11_X11_LIB} ${OPENGL_LIBRARIES})
                target_include_directories(${name} PRIVATE ${X11_INCLUDE_DIR})
            endif()
            set_target_properties(${name} PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
            )
        endmacro()
    endif()
endif()

# Add headless examples
add_headless_example(02.1.HelloCone)
add_headless_example(02.2.EngineSpin)
add_headless_example(02.3.Trackball)
add_headless_example(02.4.Examiner)
add_headless_example(03.1.Molecule)
add_headless_example(03.2.Robot)
add_headless_example(03.3.Naming)
add_headless_example(04.1.Cameras)
add_headless_example(04.2.Lights)
add_headless_example(05.1.FaceSet)
add_headless_example(05.2.IndexedFaceSet)
add_headless_example(05.3.TriangleStripSet)
add_headless_example(05.4.QuadMesh)
add_headless_example(05.5.Binding)
add_headless_example(05.6.TransformOrdering)
add_headless_example(06.1.Text)
add_headless_example(06.2.Simple3DText)
add_headless_example(06.3.Complex3DText)
add_headless_example(07.1.BasicTexture)
add_headless_example(07.2.TextureCoordinates)
add_headless_example(07.3.TextureFunction)
# Chapter 8: NURBS examples - SKIPPED (SoNurbs* nodes not available in this fork)
# add_headless_example(08.1.BSCurve)
# add_headless_example(08.2.UniCurve)
# add_headless_example(08.3.BezSurf)
# add_headless_example(08.4.TrimSurf)
add_headless_example(09.1.Print)
add_headless_example(09.2.Texture)
add_headless_example(09.3.Search)
add_headless_example(09.4.PickAction)
add_headless_example(09.5.GenSph)
add_headless_example(10.1.addEventCB)
add_headless_example(10.2.setEventCB)
add_headless_example(10.5.SelectionCB)
add_headless_example(10.6.PickFilterTopLevel)
add_headless_example(10.7.PickFilterManip)
add_headless_example(10.8.PickFilterNodeKit)
add_headless_example(11.1.ReadFile)
add_headless_example(11.2.ReadString)
add_headless_example(12.1.FieldSensor)
add_headless_example(12.2.NodeSensor)
add_headless_example(12.3.AlarmSensor)
add_headless_example(12.4.TimerSensor)
add_headless_example(13.1.GlobalFlds)
add_headless_example(13.2.ElapsedTime)
add_headless_example(13.3.TimeCounter)
add_headless_example(13.4.Gate)
add_headless_example(13.5.Boolean)
add_headless_example(13.6.Calculator)
add_headless_example(13.7.Rotor)
add_headless_example(13.8.Blinker)

# Chapter 14: NodeKits
add_headless_example(14.1.FrolickingWords)
add_headless_example(14.2.Editors)
add_headless_example(14.3.Balance)

# Chapter 15: Manipulators
add_headless_example(15.1.ConeRadius)
add_headless_example(15.2.SliderBox)
add_headless_example(15.3.AttachManip)
add_headless_example(15.4.Customize)

# Chapter 16: Component Integration (NEW - previously skipped)
add_headless_example(16.2.Callback)
add_headless_example(16.3.AttachEditor)

# Chapter 17: OpenGL Integration
add_headless_example(17.2.GLCallback)

# Build image comparator utility using bundled lodepng
# When building as part of the main project, reuse the image_comparator target
# that is already defined in the tests/ directory to avoid duplicate targets.
if(NOT TARGET image_comparator)
    add_executable(image_comparator image_comparator.cpp)
    if(TARGET lodepng)
        target_link_libraries(image_comparator lodepng)
    else()
        # Standalone build: compile lodepng alongside image_comparator
        target_sources(image_comparator PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../../external/lodepng.cpp"
        )
        target_include_directories(image_comparator PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../../external"
        )
    endif()
    set_target_properties(image_comparator PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
    )
endif()

# Build rgb_to_png converter using bundled lodepng
if(NOT TARGET rgb_to_png)
    add_executable(rgb_to_png rgb_to_png.cpp)
    if(TARGET lodepng)
        target_link_libraries(rgb_to_png lodepng)
    else()
        # Standalone build: compile lodepng alongside rgb_to_png
        target_sources(rgb_to_png PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../../external/lodepng.cpp"
        )
        target_include_directories(rgb_to_png PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../../external"
        )
    endif()
    set_target_properties(rgb_to_png PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
    )
endif()

# Create output directory for rendered images
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/output")

# Add a custom target to run all examples and generate images
add_custom_target(run_examples
    COMMAND ${CMAKE_COMMAND} -E echo "Running headless examples..."
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/output"
    DEPENDS 02.1.HelloCone 02.2.EngineSpin 03.1.Molecule 03.2.Robot
    COMMENT "Building all headless examples"
)

# Individual run targets
add_custom_target(run_HelloCone
    COMMAND cd ${CMAKE_CURRENT_BINARY_DIR}/output && ${CMAKE_CURRENT_BINARY_DIR}/bin/02.1.HelloCone
    DEPENDS 02.1.HelloCone
    COMMENT "Running HelloCone example"
)

add_custom_target(run_EngineSpin
    COMMAND cd ${CMAKE_CURRENT_BINARY_DIR}/output && ${CMAKE_CURRENT_BINARY_DIR}/bin/02.2.EngineSpin
    DEPENDS 02.2.EngineSpin
    COMMENT "Running EngineSpin example"
)

add_custom_target(run_Molecule
    COMMAND cd ${CMAKE_CURRENT_BINARY_DIR}/output && ${CMAKE_CURRENT_BINARY_DIR}/bin/03.1.Molecule
    DEPENDS 03.1.Molecule
    COMMENT "Running Molecule example"
)

add_custom_target(run_Robot
    COMMAND cd ${CMAKE_CURRENT_BINARY_DIR}/output && ${CMAKE_CURRENT_BINARY_DIR}/bin/03.2.Robot
    DEPENDS 03.2.Robot
    COMMENT "Running Robot example"
)

# ============================================================================
# CTest integration for image comparison testing
# ============================================================================

# Enable testing
enable_testing()

# Set control images directory
set(CONTROL_IMAGES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/control_images")

# Macro to add image comparison test
# Control images are stored as PNG (lossless compression for repository storage).
# At test time, the example produces a .rgb (SGI RGB) runtime image which is
# compared against the PNG control image decoded to the same pixel data.
#
# TEST_BASE (no .rgb extension) is passed as argv[1] to the example so that:
#   - append-.rgb examples (09.1.Print style) write TEST_BASE.rgb
#   - direct-write examples (02.1.HelloCone style) write TEST_BASE
#   - multi-view examples write TEST_BASE_<view>.rgb
# The comparator finds the primary generated image automatically.
#
# Usage:
#   add_image_test(example_name)                               # use global defaults
#   add_image_test(example_name RMSE_THRESHOLD 25.0)           # override RMSE only
#   add_image_test(example_name HASH_THRESHOLD 10 RMSE_THRESHOLD 25.0)
macro(add_image_test example_name)
    set(CONTROL_IMAGE "${CONTROL_IMAGES_DIR}/${example_name}_control.png")
    set(TEST_BASE "${CMAKE_CURRENT_BINARY_DIR}/test_output/${example_name}_test")

    # Parse optional keyword overrides: HASH_THRESHOLD <N>, RMSE_THRESHOLD <N>
    set(_hash_thresh ${IMAGE_COMPARISON_HASH_THRESHOLD})
    set(_rmse_thresh ${IMAGE_COMPARISON_RMSE_THRESHOLD})
    set(_i 0)
    foreach(_arg ${ARGN})
        if(_arg STREQUAL "HASH_THRESHOLD")
            set(_next_is_hash TRUE)
        elseif(_arg STREQUAL "RMSE_THRESHOLD")
            set(_next_is_rmse TRUE)
        elseif(DEFINED _next_is_hash AND _next_is_hash)
            set(_hash_thresh ${_arg})
            unset(_next_is_hash)
        elseif(DEFINED _next_is_rmse AND _next_is_rmse)
            set(_rmse_thresh ${_arg})
            unset(_next_is_rmse)
        endif()
    endforeach()

    # Check if control image exists
    if(EXISTS ${CONTROL_IMAGE})
        add_test(
            NAME ${example_name}_test
            COMMAND ${CMAKE_COMMAND}
                -DEXECUTABLE=$<TARGET_FILE:${example_name}>
                -DTEST_BASE=${TEST_BASE}
                -DCONTROL_IMAGE=${CONTROL_IMAGE}
                -DCOMPARATOR=$<TARGET_FILE:image_comparator>
                -DHASH_THRESHOLD=${_hash_thresh}
                -DRMSE_THRESHOLD=${_rmse_thresh}
                -DTEST_TIMEOUT=${IMAGE_COMPARISON_TEST_TIMEOUT}
                -DOBOL_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/data
                -P ${CMAKE_CURRENT_SOURCE_DIR}/run_image_test.cmake
        )
        set_tests_properties(${example_name}_test PROPERTIES
            TIMEOUT ${IMAGE_COMPARISON_TEST_TIMEOUT}
        )
    endif()
endmacro()

# Override the default RMSE threshold for Mentor tests to 25.0.
# Mentor examples were converted to headless rendering with control images
# generated on a specific build.  Minor cross-build rendering differences
# (typically RMSE 5–22) require a more generous threshold than the 5.0
# used for the core rendering regression suite.
set(IMAGE_COMPARISON_RMSE_THRESHOLD "25.0")

# Add tests for all examples
add_image_test(02.1.HelloCone)
add_image_test(02.2.EngineSpin)
add_image_test(02.3.Trackball)
add_image_test(02.4.Examiner)
add_image_test(03.1.Molecule)
add_image_test(03.2.Robot)
add_image_test(03.3.Naming)
add_image_test(04.1.Cameras)
add_image_test(04.2.Lights)
add_image_test(05.1.FaceSet)
add_image_test(05.2.IndexedFaceSet)
add_image_test(05.3.TriangleStripSet)
add_image_test(05.4.QuadMesh)
add_image_test(05.5.Binding)
add_image_test(05.6.TransformOrdering)
add_image_test(06.1.Text)
add_image_test(06.2.Simple3DText)
add_image_test(06.3.Complex3DText)
add_image_test(07.1.BasicTexture)
add_image_test(07.2.TextureCoordinates)
add_image_test(07.3.TextureFunction)
# Chapter 8: NURBS examples - SKIPPED (SoNurbs* nodes not available in this fork)
# add_image_test(08.1.BSCurve)
# add_image_test(08.2.UniCurve)
# add_image_test(08.3.BezSurf)
# add_image_test(08.4.TrimSurf)
add_image_test(09.1.Print)
add_image_test(09.2.Texture)
add_image_test(09.3.Search)
add_image_test(09.4.PickAction)
add_image_test(09.5.GenSph)
add_image_test(10.1.addEventCB)
add_image_test(10.2.setEventCB)
add_image_test(10.5.SelectionCB)
add_image_test(10.6.PickFilterTopLevel)
add_image_test(10.7.PickFilterManip)
add_image_test(10.8.PickFilterNodeKit)
add_image_test(11.1.ReadFile)
add_image_test(11.2.ReadString)
add_image_test(12.1.FieldSensor)
add_image_test(12.2.NodeSensor)
add_image_test(12.3.AlarmSensor)
add_image_test(12.4.TimerSensor)
# 13.1.GlobalFlds renders 3D text via SoText3, whose glyph rasterization varies
# slightly across independent GL contexts. The RMSE threshold is raised to
# accommodate this rendering variance while still detecting major regressions.
add_image_test(13.1.GlobalFlds RMSE_THRESHOLD 30.0)
add_image_test(13.2.ElapsedTime)
add_image_test(13.3.TimeCounter)
add_image_test(13.4.Gate)
add_image_test(13.5.Boolean)
add_image_test(13.6.Calculator)
add_image_test(13.7.Rotor RMSE_THRESHOLD 40.0)
add_image_test(13.8.Blinker)
add_image_test(14.1.FrolickingWords)
add_image_test(14.2.Editors)
add_image_test(14.3.Balance)
add_image_test(15.1.ConeRadius)
add_image_test(15.2.SliderBox)
add_image_test(15.3.AttachManip)
add_image_test(15.4.Customize)
add_image_test(16.2.Callback)
add_image_test(16.3.AttachEditor)
add_image_test(17.2.GLCallback)

# Custom target to generate control images
# Depends on all examples and rgb_to_png to ensure they are built first.
# Produces PNG control images in control_images/ for repository storage.
add_custom_target(generate_mentor_controls
    COMMAND ${CMAKE_COMMAND} -E env BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR} 
            ${CMAKE_CURRENT_SOURCE_DIR}/generate_control_images.sh
    DEPENDS 
        rgb_to_png image_comparator
        02.1.HelloCone 02.2.EngineSpin 02.3.Trackball 02.4.Examiner
        03.1.Molecule 03.2.Robot 03.3.Naming
        04.1.Cameras 04.2.Lights
        05.1.FaceSet 05.2.IndexedFaceSet 05.3.TriangleStripSet 05.4.QuadMesh 05.5.Binding 05.6.TransformOrdering
        06.1.Text 06.2.Simple3DText 06.3.Complex3DText
        07.1.BasicTexture 07.2.TextureCoordinates 07.3.TextureFunction
        09.1.Print 09.2.Texture 09.3.Search 09.4.PickAction 09.5.GenSph
        10.1.addEventCB 10.2.setEventCB 10.5.SelectionCB 10.6.PickFilterTopLevel 10.7.PickFilterManip 10.8.PickFilterNodeKit
        11.1.ReadFile 11.2.ReadString
        12.1.FieldSensor 12.2.NodeSensor 12.3.AlarmSensor 12.4.TimerSensor
        13.1.GlobalFlds 13.2.ElapsedTime 13.3.TimeCounter 13.4.Gate 13.5.Boolean 13.6.Calculator 13.7.Rotor 13.8.Blinker
        14.1.FrolickingWords 14.3.Balance
        15.1.ConeRadius 15.2.SliderBox 15.3.AttachManip 15.4.Customize
        17.2.GLCallback
    COMMENT "Generating PNG control images for all examples"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
