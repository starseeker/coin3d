/*
 * Opt-in scene-graph scalability benchmark.
 *
 * This is deliberately a standalone benchmark executable rather than a
 * CTest unit. It prints CSV timing data and returns success after producing
 * the measurements; timing is for characterization, not a pass/fail API
 * contract.
 */

#include <Inventor/SoDB.h>

int obolRunScalabilityBenchmark();

namespace {

class NullContextManager final : public SoDB::ContextManager {
public:
    void * createOffscreenContext(unsigned int, unsigned int) override { return nullptr; }
    SbBool makeContextCurrent(void *) override { return FALSE; }
    void restorePreviousContext(void *) override {}
    void destroyContext(void *) override {}
};

} // namespace

int main()
{
    NullContextManager manager;
    SoDB::init(&manager);
    return obolRunScalabilityBenchmark();
}
