#include <Inventor/SbTime.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTranslation.h>

#include <cstdio>

namespace {

SoSeparator * makeFlatScene(const int leaves)
{
    auto * root = new SoSeparator;
    root->ref();
    for (int index = 0; index < leaves; ++index) {
        auto * group = new SoSeparator;
        auto * translation = new SoTranslation;
        translation->translation.setValue(static_cast<float>(index), 0.0f, 0.0f);
        group->addChild(translation);
        group->addChild(new SoCube);
        root->addChild(group);
    }
    return root;
}

SoSeparator * makeBalancedScene(const int leaves)
{
    if (leaves <= 1) {
        auto * root = new SoSeparator;
        root->ref();
        root->addChild(new SoCube);
        return root;
    }

    auto * root = new SoSeparator;
    root->ref();
    const int left_count = leaves / 2;
    auto * left = makeBalancedScene(left_count);
    auto * right = makeBalancedScene(leaves - left_count);
    root->addChild(left);
    root->addChild(right);
    left->unref();
    right->unref();
    return root;
}

double elapsedMs(const SbTime start)
{
    return (SbTime::getTimeOfDay() - start).getValue() * 1000.0;
}

void measure(const char * name, SoSeparator * root, const int leaves)
{
    const SbViewportRegion viewport(256, 256);
    const SbTime build_start = SbTime::getTimeOfDay();
    (void)build_start;

    const SbTime bbox_start = SbTime::getTimeOfDay();
    SoGetBoundingBoxAction bbox(viewport);
    bbox.apply(root);
    const double bbox_ms = elapsedMs(bbox_start);

    const SbTime search_start = SbTime::getTimeOfDay();
    SoSearchAction search;
    search.setType(SoCube::getClassTypeId());
    search.setInterest(SoSearchAction::ALL);
    search.setFind(SoSearchAction::TYPE);
    search.apply(root);
    const double search_ms = elapsedMs(search_start);

    const SbTime destroy_start = SbTime::getTimeOfDay();
    root->unref();
    const double destroy_ms = elapsedMs(destroy_start);
    std::printf("%s,%d,%.3f,%.3f,%.3f\n",
                name, leaves, bbox_ms, search_ms, destroy_ms);
}

} // namespace

int obolRunScalabilityBenchmark()
{
    std::printf("# structure, leaves, bbox_ms, search_ms, destroy_ms\n");
    for (const int leaves : {100, 500, 1000})
        measure("flat", makeFlatScene(leaves), leaves);
    for (const int leaves : {100, 500, 1000})
        measure("balanced", makeBalancedScene(leaves), leaves);
    return 0;
}
