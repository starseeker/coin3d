if(NOT DEFINED PROJECT_SOURCE_DIR)
    message(FATAL_ERROR "PROJECT_SOURCE_DIR is required")
endif()

file(GLOB_RECURSE headers
    "${PROJECT_SOURCE_DIR}/include/Obol/*.h")

set(failures "")
foreach(header IN LISTS headers)
    file(TO_CMAKE_PATH "${header}" header_path)

    if(header_path MATCHES "/include/Obol/compat/")
        continue()
    endif()
    if(header_path MATCHES "/include/Obol/cad/SoCAD[^/]*\\.h$")
        continue()
    endif()

    file(READ "${header}" content)
    if(content MATCHES "#[ \t]*include[ \t]*[<\"]Inventor/")
        string(APPEND failures
            "v2 public header includes Inventor directly: ${header_path}\n")
    endif()
endforeach()

set(umbrella "${PROJECT_SOURCE_DIR}/include/Obol/Obol.h")
if(EXISTS "${umbrella}")
    file(READ "${umbrella}" umbrella_content)
    if(umbrella_content MATCHES "Obol/compat/")
        string(APPEND failures
            "v2 umbrella includes compatibility headers: ${umbrella}\n")
    endif()
    if(umbrella_content MATCHES "Obol/cad/SoCAD")
        string(APPEND failures
            "v2 umbrella includes CAD Inventor forwarding headers: ${umbrella}\n")
    endif()
endif()

if(NOT failures STREQUAL "")
    message(FATAL_ERROR "${failures}")
endif()
