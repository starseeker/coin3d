add_executable(obol_inline_texture inline_texture.cpp)
target_link_libraries(obol_inline_texture PRIVATE ${PROJECT_NAME})
target_include_directories(obol_inline_texture PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_BINARY_DIR}/include)
set_target_properties(obol_inline_texture PROPERTIES
    OUTPUT_NAME inline_texture
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    UNITY_BUILD OFF)

install(TARGETS obol_inline_texture
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT tools)
