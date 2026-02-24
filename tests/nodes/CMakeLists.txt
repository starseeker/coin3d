# Node tests
# Tests for Coin3D SoNode subclasses, baselined against coin_vanilla.

add_executable(test_nodes_suite test_nodes_suite.cpp)
target_link_libraries(test_nodes_suite simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_nodes_suite PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_nodes_suite pthread)
endif()
add_test(NAME test_nodes_suite COMMAND test_nodes_suite)

add_executable(test_nodekit_traversal test_nodekit_traversal.cpp)
target_link_libraries(test_nodekit_traversal simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_nodekit_traversal PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_nodekit_traversal pthread)
endif()
add_test(NAME test_nodekit_traversal COMMAND test_nodekit_traversal)

add_executable(test_nodes_extended test_nodes_extended.cpp)
target_link_libraries(test_nodes_extended simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_nodes_extended PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_nodes_extended pthread)
endif()
add_test(NAME test_nodes_extended COMMAND test_nodes_extended)

add_executable(test_nodekit_deep test_nodekit_deep.cpp)
target_link_libraries(test_nodekit_deep simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_nodekit_deep PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_nodekit_deep pthread)
endif()
add_test(NAME test_nodekit_deep COMMAND test_nodekit_deep)

# SoSelection + SoExtSelection (currently at 0% coverage)
add_executable(test_selection_nodes test_selection_nodes.cpp)
target_link_libraries(test_selection_nodes simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_selection_nodes PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_selection_nodes pthread)
endif()
add_test(NAME test_selection_nodes COMMAND test_selection_nodes)

add_executable(test_nodes_misc test_nodes_misc.cpp)
target_link_libraries(test_nodes_misc simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_nodes_misc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_nodes_misc pthread)
endif()
add_test(NAME test_nodes_misc COMMAND test_nodes_misc)

add_executable(test_nodes_shape_ext test_nodes_shape_ext.cpp)
target_link_libraries(test_nodes_shape_ext simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_nodes_shape_ext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_nodes_shape_ext pthread)
endif()
add_test(NAME test_nodes_shape_ext COMMAND test_nodes_shape_ext)

add_executable(test_nodes_anim_misc test_nodes_anim_misc.cpp)
target_link_libraries(test_nodes_anim_misc simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_nodes_anim_misc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_nodes_anim_misc pthread)
endif()
add_test(NAME test_nodes_anim_misc COMMAND test_nodes_anim_misc)

add_executable(test_nodes_geometry test_nodes_geometry.cpp)
target_link_libraries(test_nodes_geometry simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_nodes_geometry PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_nodes_geometry pthread)
endif()
add_test(NAME test_nodes_geometry COMMAND test_nodes_geometry)

add_executable(test_nodes_path test_nodes_path.cpp)
target_link_libraries(test_nodes_path simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_nodes_path PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_nodes_path pthread)
endif()
add_test(NAME test_nodes_path COMMAND test_nodes_path)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG(-Wtautological-compare _NODES_DISABLE_TAUTCOMP)
if(_NODES_DISABLE_TAUTCOMP)
    target_compile_options(test_nodes_suite PUBLIC -Wno-tautological-compare)
    target_compile_options(test_nodekit_traversal PUBLIC -Wno-tautological-compare)
endif()
