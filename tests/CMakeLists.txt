# Simple Test Suite for Coin3D - No External Frameworks
# Each test executable corresponds to a major API grouping and returns 0 for success

# Common test utilities
add_library(simple_test_utils STATIC 
    test_utils.cpp
    utils/scene_graph_utils.cpp
)
target_link_libraries(simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(simple_test_utils PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)

# Test executables for major API groups
set(SIMPLE_TESTS
    test_actions
    test_nodes  
    test_fields
    test_events
    test_base
    test_engines
    test_sensors
    test_scene_rendering
    test_gradient_debug
    test_simple_debug
    test_coordinate_debug
    test_sphere_positioning
    test_comprehensive
    test_legacy_integration
)

# Create executable for each test group
foreach(test_name ${SIMPLE_TESTS})
    add_executable(${test_name} ${test_name}.cpp)
    
    # Link to Coin3D library and test utilities
    target_link_libraries(${test_name} simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
    
    # Set include directories
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/Inventor/annex
        ${PROJECT_BINARY_DIR}/include
        ${COIN_TARGET_INCLUDE_DIRECTORIES}
    )
    
    # Add threading support if needed
    if(USE_PTHREAD)
        target_link_libraries(${test_name} pthread)
    endif()
    
    # Register with CTest
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

# Enable compiler warnings suppression for tautological comparisons on macOS
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG(-Wtautological-compare DISABLE_WARNING_TAUTOLOGCOMPARE)
if(DISABLE_WARNING_TAUTOLOGCOMPARE)
    foreach(test_name ${SIMPLE_TESTS})
        target_compile_options(${test_name} PUBLIC -Wno-tautological-compare)
    endforeach()
endif()