# =============================================================================
# tests/threads/CMakeLists.txt
#
# Thread safety stress tests for Phase 1 fixes:
#   1. SbName / namemap mutex
#   2. SoNode::nextUniqueId  (std::atomic<SbUniqueId>)
#   3. SoBase reference count (std::atomic<int32_t>)
#   4. SoType registration mutex
#   5. SoDBP::notificationcounter (std::atomic<int>)
# =============================================================================

# ── Primitive threading API tests (existing) ──────────────────────────────
add_executable(test_threads test_threads.cpp)
target_link_libraries(test_threads
    ${PROJECT_NAME}
    ${OBOL_TARGET_LINK_LIBRARIES}
    simple_test_utils
)
target_include_directories(test_threads PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${OBOL_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_threads pthread)
endif()
add_test(NAME test_threads COMMAND test_threads)

# ── Phase-1 thread safety stress tests ───────────────────────────────────
add_executable(test_thread_safety test_thread_safety.cpp)
target_link_libraries(test_thread_safety
    ${PROJECT_NAME}
    ${OBOL_TARGET_LINK_LIBRARIES}
    simple_test_utils
)
target_include_directories(test_thread_safety PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${OBOL_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_thread_safety pthread)
endif()
add_test(NAME test_thread_safety COMMAND test_thread_safety)
# Run with a generous timeout — stress tests deliberately spin many threads.
set_tests_properties(test_thread_safety PROPERTIES TIMEOUT 120)
