#ifndef OBOL_CAD_SCENE_MUTATION_TEST_HOOKS_H
#define OBOL_CAD_SCENE_MUTATION_TEST_HOOKS_H

/* Private fault-injection surface, compiled only in test-enabled builds. */

#if defined(OBOL_CAD_ENABLE_SCENE_MUTATION_TEST_HOOKS)

#include <Inventor/basic.h>

namespace Obol {
namespace internal {

/** Throw std::bad_alloc after the numbered sparse-mutation commit stage. */
OBOL_DLL_API void cadSetSceneMutationFailurePointForTesting(
    unsigned int point) noexcept;

} // namespace internal
} // namespace Obol

#endif

#endif // OBOL_CAD_SCENE_MUTATION_TEST_HOOKS_H
