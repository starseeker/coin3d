# Base type tests
# Tests for Coin3D base classes: SbVec*, SbBox*, SbMatrix, SbRotation,
# SbString, SbByteBuffer, SbBSPTree, SbPlane, SbViewVolume, etc.
# Baselined against coin_vanilla COIN_TEST_SUITE blocks.

add_executable(test_sb_types test_sb_types.cpp)
target_link_libraries(test_sb_types simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_sb_types PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_sb_types pthread)
endif()
add_test(NAME test_sb_types COMMAND test_sb_types)

add_executable(test_sbdpmatrix_full test_sbdpmatrix_full.cpp)
target_link_libraries(test_sbdpmatrix_full simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_sbdpmatrix_full PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_sbdpmatrix_full pthread)
endif()
add_test(NAME test_sbdpmatrix_full COMMAND test_sbdpmatrix_full)

add_executable(test_base_extras test_base_extras.cpp)
target_link_libraries(test_base_extras simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_base_extras PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_base_extras pthread)
endif()
add_test(NAME test_base_extras COMMAND test_base_extras)

add_executable(test_sbtime_sbdict test_sbtime_sbdict.cpp)
target_link_libraries(test_sbtime_sbdict simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_sbtime_sbdict PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_sbtime_sbdict pthread)
endif()
add_test(NAME test_sbtime_sbdict COMMAND test_sbtime_sbdict)

add_executable(test_sb_plist test_sb_plist.cpp)
target_link_libraries(test_sb_plist simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_sb_plist PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_sb_plist pthread)
endif()
add_test(NAME test_sb_plist COMMAND test_sb_plist)

add_executable(test_sb_viewvolume test_sb_viewvolume.cpp)
target_link_libraries(test_sb_viewvolume simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_sb_viewvolume PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_sb_viewvolume pthread)
endif()
add_test(NAME test_sb_viewvolume COMMAND test_sb_viewvolume)

add_executable(test_sb_matrix_box test_sb_matrix_box.cpp)
target_link_libraries(test_sb_matrix_box simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_sb_matrix_box PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_sb_matrix_box pthread)
endif()
add_test(NAME test_sb_matrix_box COMMAND test_sb_matrix_box)

add_executable(test_sb_rotation_plane test_sb_rotation_plane.cpp)
target_link_libraries(test_sb_rotation_plane simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_sb_rotation_plane PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_sb_rotation_plane pthread)
endif()
add_test(NAME test_sb_rotation_plane COMMAND test_sb_rotation_plane)

add_executable(test_base_advanced test_base_advanced.cpp)
target_link_libraries(test_base_advanced simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_base_advanced PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_base_advanced pthread)
endif()
add_test(NAME test_base_advanced COMMAND test_base_advanced)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG(-Wtautological-compare _BASE_DISABLE_TAUTCOMP)
if(_BASE_DISABLE_TAUTCOMP)
    target_compile_options(test_sb_types PUBLIC -Wno-tautological-compare)
endif()
