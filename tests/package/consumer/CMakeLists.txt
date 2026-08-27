cmake_minimum_required(VERSION 3.16)
project(ObolPackageConsumer LANGUAGES CXX)

find_package(Obol 2.0 REQUIRED CONFIG COMPONENTS NanoRTContextManager)

if(NOT Obol_NanoRTContextManager_FOUND)
  message(FATAL_ERROR "Installed Obol package did not publish NanoRTContextManager")
endif()
if(NOT EXISTS "${Obol_NanoRTContextManager_INCLUDE_DIR}/SoNanoRTContextManager.h")
  message(FATAL_ERROR "NanoRTContextManager public header is missing")
endif()

add_executable(obol_package_consumer main.cpp)
target_link_libraries(obol_package_consumer PRIVATE Obol::Obol)
target_compile_features(obol_package_consumer PRIVATE cxx_std_17)

enable_testing()
add_test(NAME package_consumer_runs COMMAND obol_package_consumer)
if(WIN32)
  # Imported DLLs remain in the installed prefix rather than being copied
  # beside this test executable.
  set_tests_properties(package_consumer_runs PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:Obol::Obol>\;$ENV{PATH}")
endif()
