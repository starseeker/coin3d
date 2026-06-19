if(NOT DEFINED PROJECT_SOURCE_DIR)
    message(FATAL_ERROR "PROJECT_SOURCE_DIR is required")
endif()

if(POLICY CMP0057)
    cmake_policy(SET CMP0057 NEW)
endif()

set(allowed_direct_renderer_examples
    "examples/Mentor/05.1.FaceSet.cpp"
    "examples/Mentor/05.2.IndexedFaceSet.cpp"
    "examples/Mentor/05.4.QuadMesh.cpp"
    "examples/Mentor/06.2.Simple3DText.cpp"
)

file(GLOB mentor_examples
    "${PROJECT_SOURCE_DIR}/examples/Mentor/*.cpp"
)
file(GLOB standalone_examples
    "${PROJECT_SOURCE_DIR}/examples/*.cpp"
    "${PROJECT_SOURCE_DIR}/examples/*.h"
)

set(failures)
foreach(example_path IN LISTS mentor_examples standalone_examples)
    file(RELATIVE_PATH relative_path "${PROJECT_SOURCE_DIR}" "${example_path}")
    if(relative_path IN_LIST allowed_direct_renderer_examples)
        continue()
    endif()

    file(READ "${example_path}" contents)
    if(contents MATCHES "obol::OffscreenRenderer")
        list(APPEND failures
             "${relative_path}: uses direct obol::OffscreenRenderer; use obol::Renderer with obol::FrameRequest")
    endif()
endforeach()

if(failures)
    string(REPLACE ";" "\n  " failure_text "${failures}")
    message(FATAL_ERROR
        "Migrated v2 examples must use request-oriented rendering.\n"
        "Allowed direct renderer holdbacks: ${allowed_direct_renderer_examples}\n"
        "Unexpected direct renderer use:\n  ${failure_text}")
endif()
