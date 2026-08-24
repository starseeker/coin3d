#ifndef OBOL_TEST_CONTEXT_H
#define OBOL_TEST_CONTEXT_H

#include <Inventor/SoDB.h>

namespace ObolTestSupport {

/**
 * Initialise the database for a normal, non-rendering test process.
 *
 * The manager intentionally cannot create a GL context.  Rendering tests use
 * an explicit per-renderer manager instead, which keeps backend selection out
 * of global test state.
 */
void initializeObol();

/** Return the non-rendering manager installed by initializeObol(). */
SoDB::ContextManager & nullContextManager();

} // namespace ObolTestSupport

#endif // OBOL_TEST_CONTEXT_H
