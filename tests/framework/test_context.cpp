#include "test_context.h"

#include <Inventor/SoInteraction.h>
#include <Inventor/nodekits/SoNodeKit.h>

namespace ObolTestSupport {
namespace {

class NullContextManager final : public SoDB::ContextManager {
public:
    void * createOffscreenContext(unsigned int, unsigned int) override { return nullptr; }
    SbBool makeContextCurrent(void *) override { return FALSE; }
    void restorePreviousContext(void *) override {}
    void destroyContext(void *) override {}
};

NullContextManager & manager()
{
    static NullContextManager instance;
    return instance;
}

} // namespace

void initializeObol()
{
    if (!SoDB::isInitialized()) {
        SoDB::init(&manager());
        SoNodeKit::init();
        SoInteraction::init();
    }
}

SoDB::ContextManager & nullContextManager()
{
    return manager();
}

} // namespace ObolTestSupport
