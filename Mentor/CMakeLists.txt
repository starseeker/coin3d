cmake_minimum_required(VERSION 3.16)

project(MentorHeadlessExamples)

# Image comparison sensitivity control
# Lower values = stricter comparison, Higher values = more tolerant
# Hash threshold: 0-64 (default 5), RMSE threshold: 0-255 (default 5.0)
set(IMAGE_COMPARISON_HASH_THRESHOLD "5" CACHE STRING "Perceptual hash comparison threshold (0-64)")
set(IMAGE_COMPARISON_RMSE_THRESHOLD "5.0" CACHE STRING "RMSE comparison threshold (0-255)")
set(IMAGE_COMPARISON_TEST_TIMEOUT "30" CACHE STRING "Timeout in seconds for each test (default 30)")

# When built as subdirectory, Coin target is already available
# When built standalone, try to find the installed Coin library
if(NOT TARGET Coin)
    # Try to find Coin package installed on system
    find_package(Coin REQUIRED)
    set(COIN_TARGET Coin::Coin)
    # When using installed Coin, we may need to find OSMesa separately
    # For now, assume it's part of the Coin installation
else()
    # Use the Coin target from parent build (includes OSMesa internally)
    set(COIN_TARGET Coin)
    # Add OSMesa include directory from external submodule
    set(OSMESA_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../external/osmesa/include")
endif()

# Macro to add headless examples
# Examples link against Coin which includes OSMesa for offscreen rendering
macro(add_headless_example name)
    add_executable(${name} ${name}.cpp)
    target_include_directories(${name} PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${OSMESA_INCLUDE_DIR}
    )
    # Link against Coin and osmesa_interface (for OSMesa functions used in headless_utils.h)
    if(TARGET osmesa_interface)
        target_link_libraries(${name} ${COIN_TARGET} osmesa_interface)
    else()
        target_link_libraries(${name} ${COIN_TARGET})
    endif()
    set_target_properties(${name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
    )
endmacro()

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
add_headless_example(08.1.BSCurve)
add_headless_example(08.2.UniCurve)
add_headless_example(08.3.BezSurf)
add_headless_example(08.4.TrimSurf)
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

# Build image comparator utility
add_executable(image_comparator image_comparator.cpp)
set_target_properties(image_comparator PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)

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
macro(add_image_test example_name)
    set(CONTROL_IMAGE "${CONTROL_IMAGES_DIR}/${example_name}_control.rgb")
    set(TEST_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/test_output/${example_name}_test.rgb")
    
    # Check if control image exists
    if(EXISTS ${CONTROL_IMAGE})
        add_test(
            NAME ${example_name}_test
            COMMAND ${CMAKE_COMMAND}
                -DEXECUTABLE=$<TARGET_FILE:${example_name}>
                -DTEST_IMAGE=${TEST_IMAGE}
                -DCONTROL_IMAGE=${CONTROL_IMAGE}
                -DCOMPARATOR=$<TARGET_FILE:image_comparator>
                -DHASH_THRESHOLD=${IMAGE_COMPARISON_HASH_THRESHOLD}
                -DRMSE_THRESHOLD=${IMAGE_COMPARISON_RMSE_THRESHOLD}
                -DTEST_TIMEOUT=${IMAGE_COMPARISON_TEST_TIMEOUT}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/run_image_test.cmake
        )
        set_tests_properties(${example_name}_test PROPERTIES
            TIMEOUT ${IMAGE_COMPARISON_TEST_TIMEOUT}
        )
    endif()
endmacro()

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
add_image_test(08.1.BSCurve)
add_image_test(08.2.UniCurve)
add_image_test(08.3.BezSurf)
add_image_test(08.4.TrimSurf)
add_image_test(09.1.Print)
add_image_test(09.2.Texture)
add_image_test(09.3.Search)
add_image_test(09.4.PickAction)
add_image_test(09.5.GenSph)
add_image_test(10.1.addEventCB)
add_image_test(10.5.SelectionCB)
add_image_test(10.6.PickFilterTopLevel)
add_image_test(10.7.PickFilterManip)
add_image_test(11.1.ReadFile)
add_image_test(11.2.ReadString)
add_image_test(12.1.FieldSensor)
add_image_test(12.2.NodeSensor)
add_image_test(12.3.AlarmSensor)
add_image_test(12.4.TimerSensor)
add_image_test(13.1.GlobalFlds)
add_image_test(13.2.ElapsedTime)
add_image_test(13.3.TimeCounter)
add_image_test(13.4.Gate)
add_image_test(13.5.Boolean)
add_image_test(13.6.Calculator)
add_image_test(13.7.Rotor)
add_image_test(13.8.Blinker)
add_image_test(14.1.FrolickingWords)
add_image_test(14.3.Balance)
add_image_test(15.1.ConeRadius)
add_image_test(15.2.SliderBox)
add_image_test(15.3.AttachManip)
add_image_test(15.4.Customize)
add_image_test(17.2.GLCallback)

# Custom target to generate control images
# Depends on all examples to ensure they are built first
add_custom_target(generate_controls
    COMMAND ${CMAKE_COMMAND} -E env BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR} 
            ${CMAKE_CURRENT_SOURCE_DIR}/generate_control_images.sh
    DEPENDS 
        image_comparator
        02.1.HelloCone 02.2.EngineSpin 02.3.Trackball 02.4.Examiner
        03.1.Molecule 03.2.Robot 03.3.Naming
        04.1.Cameras 04.2.Lights
        05.1.FaceSet 05.2.IndexedFaceSet 05.3.TriangleStripSet 05.4.QuadMesh 05.5.Binding 05.6.TransformOrdering
        06.1.Text 06.2.Simple3DText 06.3.Complex3DText
        07.1.BasicTexture 07.2.TextureCoordinates 07.3.TextureFunction
        08.1.BSCurve 08.2.UniCurve 08.3.BezSurf 08.4.TrimSurf
        09.1.Print 09.2.Texture 09.3.Search 09.4.PickAction 09.5.GenSph
        10.1.addEventCB 10.5.SelectionCB 10.6.PickFilterTopLevel 10.7.PickFilterManip
        11.1.ReadFile 11.2.ReadString
        12.1.FieldSensor 12.2.NodeSensor 12.3.AlarmSensor 12.4.TimerSensor
        13.1.GlobalFlds 13.2.ElapsedTime 13.3.TimeCounter 13.4.Gate 13.5.Boolean 13.6.Calculator 13.7.Rotor 13.8.Blinker
        14.1.FrolickingWords 14.3.Balance
        15.1.ConeRadius 15.2.SliderBox 15.3.AttachManip 15.4.Customize
        17.2.GLCallback
    COMMENT "Generating control images for all examples"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
