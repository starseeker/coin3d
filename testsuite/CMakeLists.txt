# Legacy Test Suite - Minimal Components Only
# Most tests have been migrated to explicit Catch2 tests in tests/ directory
# This retains only threading tests and misc utilities that don't fit the new structure

# Legacy tests that don't fit the module structure
add_executable(CoinLegacyTests 
    TestSuiteMain.cpp 
    TestSuiteUtils.cpp 
    TestSuiteMisc.cpp 
    threadsTest.cpp
)

set_target_properties(CoinLegacyTests PROPERTIES DEBUG_POSTFIX "${CMAKE_DEBUG_POSTFIX}")
target_link_libraries(CoinLegacyTests Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(CoinLegacyTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)

if (USE_PTHREAD)
    target_link_libraries(CoinLegacyTests pthread)
endif()

add_test(NAME LegacyTests COMMAND CoinLegacyTests)

# Compiler warning suppression for tautological comparisons on macOS
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG(-Wtautological-compare DISABLE_WARNING_TAUTOLOGCOMPARE)
if(DISABLE_WARNING_TAUTOLOGCOMPARE)
    target_compile_options(CoinLegacyTests PUBLIC -Wno-tautological-compare)
endif()
