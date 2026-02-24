# Dragger tests
# Tests for Coin3D SoDragger subclasses.
# Tier 3 coverage improvement: draggers/ (COVERAGE_PLAN.md item 21)

add_executable(test_draggers test_draggers.cpp)
target_link_libraries(test_draggers simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_draggers PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_draggers pthread)
endif()
add_test(NAME test_draggers COMMAND test_draggers)

# Tier 5 / priority 44: additional dragger types
add_executable(test_dragger_sequences test_dragger_sequences.cpp)
target_link_libraries(test_dragger_sequences simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_dragger_sequences PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_dragger_sequences pthread)
endif()
add_test(NAME test_dragger_sequences COMMAND test_dragger_sequences)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG(-Wtautological-compare _DRAGGERS_DISABLE_TAUTCOMP)
if(_DRAGGERS_DISABLE_TAUTCOMP)
    target_compile_options(test_draggers PUBLIC -Wno-tautological-compare)
    target_compile_options(test_dragger_sequences PUBLIC -Wno-tautological-compare)
endif()
