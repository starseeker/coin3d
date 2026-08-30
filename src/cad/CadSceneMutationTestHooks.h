#ifndef OBOL_CAD_SCENE_MUTATION_TEST_HOOKS_H
#define OBOL_CAD_SCENE_MUTATION_TEST_HOOKS_H

/* Private fault-injection surface for the integration test binary. */

#include <Inventor/basic.h>

namespace Obol {
namespace internal {

/** Throw std::bad_alloc after the numbered sparse-mutation commit stage. */
OBOL_DLL_API void cadSetSceneMutationFailurePointForTesting(
    unsigned int point) noexcept;

} // namespace internal
} // namespace Obol

#endif // OBOL_CAD_SCENE_MUTATION_TEST_HOOKS_H
