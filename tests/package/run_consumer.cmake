if(NOT DEFINED OBOL_BINARY_DIR OR NOT DEFINED OBOL_SOURCE_DIR)
  message(FATAL_ERROR "OBOL_BINARY_DIR and OBOL_SOURCE_DIR are required")
endif()

set(prefix "${OBOL_BINARY_DIR}/package-consumer/prefix")
set(build "${OBOL_BINARY_DIR}/package-consumer/build")
file(REMOVE_RECURSE "${OBOL_BINARY_DIR}/package-consumer")

set(config_args)
set(ctest_config_args)
if(DEFINED OBOL_CONFIG AND NOT OBOL_CONFIG STREQUAL "")
  list(APPEND config_args --config "${OBOL_CONFIG}")
  list(APPEND ctest_config_args -C "${OBOL_CONFIG}")
endif()

set(consumer_configure_args)
if(DEFINED OBOL_GENERATOR AND NOT OBOL_GENERATOR STREQUAL "")
  list(APPEND consumer_configure_args -G "${OBOL_GENERATOR}")
endif()
if(DEFINED OBOL_GENERATOR_PLATFORM AND NOT OBOL_GENERATOR_PLATFORM STREQUAL "")
  list(APPEND consumer_configure_args -A "${OBOL_GENERATOR_PLATFORM}")
endif()
if(DEFINED OBOL_GENERATOR_TOOLSET AND NOT OBOL_GENERATOR_TOOLSET STREQUAL "")
  list(APPEND consumer_configure_args -T "${OBOL_GENERATOR_TOOLSET}")
endif()
if(DEFINED OBOL_TOOLCHAIN_FILE AND NOT OBOL_TOOLCHAIN_FILE STREQUAL "")
  list(APPEND consumer_configure_args
       "-DCMAKE_TOOLCHAIN_FILE=${OBOL_TOOLCHAIN_FILE}")
endif()
if(DEFINED OBOL_BUILD_TYPE AND NOT OBOL_BUILD_TYPE STREQUAL "")
  list(APPEND consumer_configure_args "-DCMAKE_BUILD_TYPE=${OBOL_BUILD_TYPE}")
endif()

foreach(component IN ITEMS runtime development)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${OBOL_BINARY_DIR}"
            --prefix "${prefix}" --component "${component}" ${config_args}
    RESULT_VARIABLE result)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "Installing Obol ${component} component failed")
  endif()
endforeach()

execute_process(
  COMMAND "${CMAKE_COMMAND}"
          -S "${OBOL_SOURCE_DIR}/tests/package/consumer"
          -B "${build}"
          "-DCMAKE_PREFIX_PATH=${prefix}"
          ${consumer_configure_args}
  RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Configuring the installed-package consumer failed")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${build}" ${config_args}
  RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Building the installed-package consumer failed")
endif()

execute_process(
  COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${build}"
          --output-on-failure ${ctest_config_args}
  RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Running the installed-package consumer failed")
endif()
