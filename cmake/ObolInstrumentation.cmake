# Coverage and sanitizer instrumentation.  This module is included before
# source subdirectories so every library and test target receives the same
# compile/link instrumentation.

if(OBOL_COVERAGE)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" OR
     CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    find_program(LCOV_EXECUTABLE lcov DOC "lcov coverage tool")
    find_program(GENHTML_EXECUTABLE genhtml DOC "genhtml report generator")
    if(LCOV_EXECUTABLE AND GENHTML_EXECUTABLE)
      message(STATUS "Code coverage instrumentation enabled")
      message(STATUS "  lcov:    ${LCOV_EXECUTABLE}")
      message(STATUS "  genhtml: ${GENHTML_EXECUTABLE}")
      add_compile_options(--coverage)
      add_link_options(--coverage)
    else()
      message(WARNING
        "OBOL_COVERAGE=ON but lcov/genhtml not found; coverage disabled")
      set(OBOL_COVERAGE OFF CACHE BOOL "" FORCE)
    endif()
  else()
    message(WARNING
      "OBOL_COVERAGE=ON but compiler is not GCC/Clang; coverage disabled")
    set(OBOL_COVERAGE OFF CACHE BOOL "" FORCE)
  endif()
endif()

if(OBOL_ASAN_UBSAN)
  if(OBOL_TSAN OR OBOL_COVERAGE)
    message(FATAL_ERROR
      "OBOL_ASAN_UBSAN cannot be combined with OBOL_TSAN or OBOL_COVERAGE")
  elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" OR
         CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(STATUS "AddressSanitizer and UndefinedBehaviorSanitizer enabled")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
  else()
    message(WARNING
      "OBOL_ASAN_UBSAN=ON but compiler is not GCC/Clang; sanitizers disabled")
    set(OBOL_ASAN_UBSAN OFF CACHE BOOL "" FORCE)
  endif()
endif()

if(OBOL_TSAN)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" OR
     CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    if(OBOL_COVERAGE)
      message(FATAL_ERROR
        "OBOL_TSAN cannot be combined with OBOL_COVERAGE")
    endif()
    message(STATUS "ThreadSanitizer instrumentation enabled")
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
  else()
    message(WARNING
      "OBOL_TSAN=ON but compiler is not GCC/Clang; ThreadSanitizer disabled")
    set(OBOL_TSAN OFF CACHE BOOL "" FORCE)
  endif()
endif()
