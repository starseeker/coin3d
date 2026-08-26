cmake_minimum_required(VERSION 3.16)
project(ObolPackageConsumer LANGUAGES CXX)

find_package(Obol 1.0 REQUIRED CONFIG)

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
