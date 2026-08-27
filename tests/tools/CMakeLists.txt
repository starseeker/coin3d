# Standalone image utilities used by Mentor image tests and by developers
# converting or comparing reference images.  They are available independently
# of the example programs so the tooling graph remains usable in CI and
# package-oriented builds.

function(obol_add_image_tool target source)
  add_executable(${target} "${PROJECT_SOURCE_DIR}/tests/utils/${source}")
  if(TARGET lodepng)
    target_link_libraries(${target} PRIVATE lodepng)
  else()
    target_sources(${target} PRIVATE
        "${PROJECT_SOURCE_DIR}/external/lodepng.cpp")
    target_include_directories(${target} PRIVATE
        "${PROJECT_SOURCE_DIR}/external")
  endif()
  target_include_directories(${target} PRIVATE
      "${PROJECT_SOURCE_DIR}/external")
  set_target_properties(${target} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endfunction()

obol_add_image_tool(image_comparator image_comparator.cpp)
obol_add_image_tool(rgb_to_png rgb_to_png.cpp)
