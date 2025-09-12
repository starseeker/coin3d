# Add modern C++17 utility functions
add_library(tools OBJECT
    SbModernUtils.cpp
)

target_include_directories(tools PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)