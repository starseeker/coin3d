# I/O tests
# Tests for Coin3D SoDB, SoInput/SoOutput, SoBase, SoType read/write.
# Baselined against coin_vanilla COIN_TEST_SUITE blocks.

add_executable(test_sodb test_sodb.cpp)
target_link_libraries(test_sodb simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_sodb PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_sodb pthread)
endif()
add_test(NAME test_sodb COMMAND test_sodb)

add_executable(test_io_edge_cases test_io_edge_cases.cpp)
target_link_libraries(test_io_edge_cases simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_io_edge_cases PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_io_edge_cases pthread)
endif()
add_test(NAME test_io_edge_cases COMMAND test_io_edge_cases)

add_executable(test_io_input_binary test_io_input_binary.cpp)
target_link_libraries(test_io_input_binary simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_io_input_binary PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_io_input_binary pthread)
endif()
add_test(NAME test_io_input_binary COMMAND test_io_input_binary)

# File-system I/O: openFile/closeFile, getCurFileName, isFileURL, SoWriteAction
add_executable(test_io_file_ops test_io_file_ops.cpp)
target_link_libraries(test_io_file_ops simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_io_file_ops PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_io_file_ops pthread)
endif()
add_test(NAME test_io_file_ops COMMAND test_io_file_ops)

add_executable(test_io_write_action test_io_write_action.cpp)
target_link_libraries(test_io_write_action simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_io_write_action PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_io_write_action pthread)
endif()
add_test(NAME test_io_write_action COMMAND test_io_write_action)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG(-Wtautological-compare _IO_DISABLE_TAUTCOMP)
if(_IO_DISABLE_TAUTCOMP)
    target_compile_options(test_sodb PUBLIC -Wno-tautological-compare)
endif()
