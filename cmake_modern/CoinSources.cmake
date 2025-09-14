# Modern source file organization for Coin3D
# This file replaces the 34 individual CMakeLists.txt files in src subdirectories

# Collect all source files from subdirectories
file(GLOB_RECURSE COIN_SOURCES
    # Core modules
    "src/actions/*.cpp"
    "src/base/*.cpp" 
    "src/bundles/*.cpp"
    "src/caches/*.cpp"
    "src/details/*.cpp"
    "src/elements/*.cpp"
    "src/engines/*.cpp"
    "src/errors/*.cpp"
    "src/events/*.cpp"
    "src/fields/*.cpp"
    "src/fonts/*.cpp"
    "src/glue/*.cpp"
    "src/hardcopy/*.cpp"
    "src/io/*.cpp"
    "src/lists/*.cpp"
    "src/misc/*.cpp"
    "src/nodes/*.cpp"
    "src/projectors/*.cpp"
    "src/rendering/*.cpp"
    "src/sensors/*.cpp"
    "src/shapenodes/*.cpp"
    "src/threads/*.cpp"
    "src/tools/*.cpp"
    
    # Geometry and shading
    "src/collision/*.cpp"
    "src/geo/*.cpp"
    "src/shaders/*.cpp"
    "src/shadows/*.cpp"
    
    # Main entry point
    "src/C/*.cpp"
)

# Optional modules based on configuration
if(HAVE_DRAGGERS)
    file(GLOB_RECURSE DRAGGER_SOURCES "src/draggers/*.cpp")
    list(APPEND COIN_SOURCES ${DRAGGER_SOURCES})
endif()

if(HAVE_MANIPULATORS)
    file(GLOB_RECURSE MANIP_SOURCES "src/manips/*.cpp") 
    list(APPEND COIN_SOURCES ${MANIP_SOURCES})
endif()

# Extensions (can be conditional based on features)
file(GLOB_RECURSE EXTENSION_SOURCES "src/extensions/*.cpp")
list(APPEND COIN_SOURCES ${EXTENSION_SOURCES})

# Profiler (optional, usually for debug builds)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    file(GLOB_RECURSE PROFILER_SOURCES "src/profiler/*.cpp")
    list(APPEND COIN_SOURCES ${PROFILER_SOURCES})
endif()

# Filter out any test files that might be mixed in
list(FILTER COIN_SOURCES EXCLUDE REGEX ".*test.*\\.cpp$")
list(FILTER COIN_SOURCES EXCLUDE REGEX ".*Test.*\\.cpp$")

# Temporarily exclude problematic files to demonstrate CMake system
list(FILTER COIN_SOURCES EXCLUDE REGEX ".*SoGlobalSimplifyAction\\.cpp$")
list(FILTER COIN_SOURCES EXCLUDE REGEX ".*SoShapeSimplifyAction\\.cpp$")

# Add all sources to the main target
target_sources(Coin PRIVATE ${COIN_SOURCES})

# Print source count for verification
list(LENGTH COIN_SOURCES COIN_SOURCE_COUNT)
message(STATUS "Building Coin with ${COIN_SOURCE_COUNT} source files")