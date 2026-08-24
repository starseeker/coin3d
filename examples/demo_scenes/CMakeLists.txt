# Curated, application-facing scenes for the GUI demos.  This target contains
# no test registration or test-only context support.
add_library(obol_demo_scenes STATIC demo_scenes.cpp)
target_link_libraries(obol_demo_scenes
    PUBLIC
        ${PROJECT_NAME}
        ${OBOL_TARGET_LINK_LIBRARIES}
)
target_include_directories(obol_demo_scenes
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/Inventor/annex
        ${PROJECT_BINARY_DIR}/include
        ${OBOL_TARGET_INCLUDE_DIRECTORIES}
)
