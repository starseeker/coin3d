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

# test_threads.cpp has been migrated to obol_unit_tests.  The remaining
# thread-safety source below is an intentionally separate stress/sanitizer
# lane until its mutable runtime parameters are expressed as GTest settings.

# test_thread_safety.cpp has been migrated to the opt-in obol_stress_tests
# target in tests/framework/CMakeLists.txt.
