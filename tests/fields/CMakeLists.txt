# Field tests
# Tests for Coin3D field types: SoSF* and SoMF* fields.
# Baselined against coin_vanilla COIN_TEST_SUITE blocks.

add_executable(test_sf_fields test_sf_fields.cpp)
target_link_libraries(test_sf_fields simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_sf_fields PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_sf_fields pthread)
endif()
add_test(NAME test_sf_fields COMMAND test_sf_fields)

add_executable(test_mf_fields test_mf_fields.cpp)
target_link_libraries(test_mf_fields simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_mf_fields PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_mf_fields pthread)
endif()
add_test(NAME test_mf_fields COMMAND test_mf_fields)

add_executable(test_field_connections test_field_connections.cpp)
target_link_libraries(test_field_connections simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_field_connections PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_field_connections pthread)
endif()
add_test(NAME test_field_connections COMMAND test_field_connections)

add_executable(test_field_extras test_field_extras.cpp)
target_link_libraries(test_field_extras simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_field_extras PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_field_extras pthread)
endif()
add_test(NAME test_field_extras COMMAND test_field_extras)

add_executable(test_field_containers test_field_containers.cpp)
target_link_libraries(test_field_containers simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_field_containers PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_field_containers pthread)
endif()
add_test(NAME test_field_containers COMMAND test_field_containers)

add_executable(test_field_advanced test_field_advanced.cpp)
target_link_libraries(test_field_advanced simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_field_advanced PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_field_advanced pthread)
endif()
add_test(NAME test_field_advanced COMMAND test_field_advanced)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG(-Wtautological-compare _FIELDS_DISABLE_TAUTCOMP)
if(_FIELDS_DISABLE_TAUTCOMP)
    target_compile_options(test_sf_fields PUBLIC -Wno-tautological-compare)
    target_compile_options(test_mf_fields PUBLIC -Wno-tautological-compare)
    target_compile_options(test_field_connections PUBLIC -Wno-tautological-compare)
    target_compile_options(test_field_extras PUBLIC -Wno-tautological-compare)
    target_compile_options(test_field_containers PUBLIC -Wno-tautological-compare)
    target_compile_options(test_field_advanced PUBLIC -Wno-tautological-compare)
endif()
