# Obol tests are owned by the modern GTest framework.  Keep this small
# forwarding file because a few downstream projects still add tests/ rather
# than tests/framework/ directly; there is no second test graph to drift.
add_subdirectory(framework)
