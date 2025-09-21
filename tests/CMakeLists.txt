# Primary Test Suite for Coin3D - Explicit Catch2 Tests
# This is now the main test system replacing the embedded comment-driven tests

# Find Catch2
find_package(Catch2 3 QUIET)
if(NOT Catch2_FOUND)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../external/Catch2 ${CMAKE_CURRENT_BINARY_DIR}/Catch2)
endif()

# Common test utilities with comprehensive testing framework
add_library(coin_test_utils STATIC
    utils/test_common.cpp
    utils/scene_graph_test_utils.cpp
)

# Add OSMesa test utilities if available
if(COIN3D_USE_OSMESA)
    target_sources(coin_test_utils PRIVATE 
        utils/osmesa_test_context.cpp
    )
    target_compile_definitions(coin_test_utils PUBLIC COIN3D_OSMESA_BUILD)
endif()

target_link_libraries(coin_test_utils Coin Catch2::Catch2)
target_include_directories(coin_test_utils PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)

# Primary explicit test executable with comprehensive testing
add_executable(CoinTests
    main.cpp
    # Core test modules (migrated from embedded tests)
    base/test_base.cpp
    fields/test_fields.cpp
    misc/test_misc.cpp
    # Phase 3 modernization tests
    tools/test_pimpl_basic.cpp
    tools/test_pimpl_modernization.cpp
    tools/test_modern_utils.cpp
    # Context management tests
    context/test_context_callbacks.cpp
    # FBO-based rendering tests
    rendering/test_fbo_rendering.cpp
    # Comprehensive testing modules - NEW FRAMEWORK
    nodes/test_nodes_comprehensive.cpp
    # TODO: Add more comprehensive test modules:
    # actions/test_actions_comprehensive.cpp
    # engines/test_engines_comprehensive.cpp  
    # sensors/test_sensors_comprehensive.cpp
    # TODO: Add the complete migrated test files once compilation issues are resolved:
    # base/test_base_complete.cpp
    # fields/test_fields_complete.cpp
    # misc/test_misc_complete.cpp
    # nodes/test_nodes_complete.cpp
    # actions/test_actions_complete.cpp
    # geo/test_geo_complete.cpp
    # shaders/test_shaders_complete.cpp
    # shadows/test_shadows_complete.cpp
    # caches/test_caches_complete.cpp
    # details/test_details_complete.cpp
    # elements/test_elements_complete.cpp
    # engines/test_engines_complete.cpp
    # events/test_events_complete.cpp
    # lists/test_lists_complete.cpp
    # manips/test_manips_complete.cpp
    # sensors/test_sensors_complete.cpp
)

target_link_libraries(CoinTests
    Coin
    coin_test_utils
    Catch2::Catch2WithMain
    ${COIN_TARGET_LINK_LIBRARIES}
)

target_include_directories(CoinTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)

if (USE_PTHREAD)
    target_link_libraries(CoinTests pthread)
endif()

# Primary test registration
include(CTest)
add_test(NAME CoinTests COMMAND CoinTests)

# Enable compiler warnings suppression for tautological comparisons on macOS
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG(-Wtautological-compare DISABLE_WARNING_TAUTOLOGCOMPARE)
if(DISABLE_WARNING_TAUTOLOGCOMPARE)
    target_compile_options(CoinTests PUBLIC -Wno-tautological-compare)
endif()

# Include FBO demo
include(fbo_demo.cmake)
