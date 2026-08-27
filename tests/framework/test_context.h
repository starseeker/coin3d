#ifndef OBOL_TEST_CONTEXT_H
#define OBOL_TEST_CONTEXT_H

#include <Inventor/SoDB.h>

namespace ObolTestSupport {

/**
 * Initialise the database for a normal, non-rendering test process.
 *
 * The manager intentionally cannot create a GL context. Rendering test
 * processes instead install their CTest-selected backend once at startup;
 * fixture-backed renderers additionally receive that backend explicitly.
 */
void initializeObol();

/** Return the non-rendering manager installed by initializeObol(). */
SoDB::ContextManager & nullContextManager();

} // namespace ObolTestSupport

#endif // OBOL_TEST_CONTEXT_H
