/**
 * @file test_cad_instance_records.cpp
 * @brief Lossless SoCADAssembly instance-record tests.
 */

#include "../test_utils.h"

#include <Obol/cad/SoCADAssembly.h>

#include <Inventor/SoDB.h>

using namespace SimpleTest;

static bool
sameMatrix(const SbMatrix &a, const SbMatrix &b)
{
    return a == b;
}

static bool
sameRecord(const Obol::InstanceRecord &a, const Obol::InstanceRecord &b)
{
    return a.part == b.part && sameMatrix(a.localToRoot, b.localToRoot) &&
        a.parent == b.parent && a.childName == b.childName &&
        a.occurrenceIndex == b.occurrenceIndex && a.boolOp == b.boolOp &&
        a.style.hasColorOverride == b.style.hasColorOverride &&
        a.style.color == b.style.color && a.style.lineWidth == b.style.lineWidth;
}

int
main()
{
    TestRunner runner;
    SoDB::init(nullptr);
    SoCADAssembly::initClass();

    SoCADAssembly *assembly = new SoCADAssembly;
    assembly->ref();

    Obol::InstanceRecord first;
    first.part = Obol::CadIdBuilder::hash128("shared-part");
    first.parent = Obol::CadIdBuilder::hash128("parent");
    first.childName = "wheel";
    first.occurrenceIndex = 0;
    first.boolOp = 0;
    first.style.hasColorOverride = true;
    first.style.color = SbColor4f(0.1f, 0.2f, 0.3f, 0.4f);
    first.style.lineWidth = 2.5f;
    first.localToRoot.setTranslate(SbVec3f(1.0f, 2.0f, 3.0f));

    runner.startTest("automatic instance insertion preserves the full record");
    const Obol::InstanceId firstId = assembly->upsertInstanceAuto(first);
    std::optional<Obol::InstanceRecord> stored =
        assembly->getInstanceRecord(firstId);
    runner.endTest(stored.has_value() && sameRecord(*stored, first),
        "getInstanceRecord must preserve identity, transform, and style");

    runner.startTest("duplicate occurrence receives a distinct stable ID");
    Obol::InstanceRecord duplicate = first;
    duplicate.occurrenceIndex = 1;
    const Obol::InstanceId duplicateId =
        assembly->upsertInstanceAuto(duplicate);
    stored = assembly->getInstanceRecord(duplicateId);
    runner.endTest(duplicateId != firstId && stored.has_value() &&
        sameRecord(*stored, duplicate),
        "occurrence identity must survive assembly insertion");

    runner.startTest("Boolean operation receives a distinct stable ID");
    Obol::InstanceRecord subtract = first;
    subtract.boolOp = 1;
    const Obol::InstanceId subtractId = assembly->upsertInstanceAuto(subtract);
    stored = assembly->getInstanceRecord(subtractId);
    runner.endTest(subtractId != firstId && stored.has_value() &&
        sameRecord(*stored, subtract),
        "Boolean identity must survive assembly insertion");

    runner.startTest("transform fast path preserves occurrence identity");
    SbMatrix moved;
    moved.setTranslate(SbVec3f(4.0f, 5.0f, 6.0f));
    assembly->updateInstanceTransform(duplicateId, moved);
    stored = assembly->getInstanceRecord(duplicateId);
    runner.endTest(stored.has_value() &&
        stored->parent == duplicate.parent &&
        stored->childName == duplicate.childName &&
        stored->occurrenceIndex == duplicate.occurrenceIndex &&
        stored->boolOp == duplicate.boolOp &&
        sameMatrix(stored->localToRoot, moved),
        "transform updates must not demote identity metadata");

    runner.startTest("style fast path preserves occurrence identity");
    Obol::InstanceStyle restyled = duplicate.style;
    restyled.lineWidth = 7.0f;
    assembly->updateInstanceStyle(duplicateId, restyled);
    stored = assembly->getInstanceRecord(duplicateId);
    runner.endTest(stored.has_value() &&
        stored->occurrenceIndex == duplicate.occurrenceIndex &&
        stored->boolOp == duplicate.boolOp &&
        stored->style.lineWidth == restyled.lineWidth,
        "style updates must not demote identity metadata");

    runner.startTest("explicit bulk insertion preserves full records");
    Obol::InstanceRecord intersection = first;
    intersection.childName = "overlap";
    intersection.boolOp = 2;
    const Obol::InstanceId intersectionId =
        Obol::CadIdBuilder::hash128("external-intersection-id");
    Obol::InstanceUpdate update;
    update.instance = intersectionId;
    update.record = intersection;
    assembly->upsertInstances(std::vector<Obol::InstanceUpdate>(1, update));
    stored = assembly->getInstanceRecord(intersectionId);
    runner.endTest(stored.has_value() && sameRecord(*stored, intersection),
        "bulk explicit insertion must be lossless");

    runner.startTest("ordinary shaded parts do not invent LoD");
    Obol::PartGeometry shaded;
    Obol::TriMesh triangle;
    triangle.positions = {SbVec3f(0.0f, 0.0f, 0.0f),
                          SbVec3f(1.0f, 0.0f, 0.0f),
                          SbVec3f(0.0f, 1.0f, 0.0f)};
    triangle.indices = {0, 1, 2};
    triangle.bounds.setBounds(SbVec3f(0.0f, 0.0f, 0.0f),
                              SbVec3f(1.0f, 1.0f, 0.0f));
    shaded.shaded = std::move(triangle);
    assembly->upsertPart(first.part, shaded);
    runner.endTest(!assembly->hasProgressivePartLod(),
        "the CAD node must not build a camera-driven hierarchy");

    runner.startTest("producer-authored progressive metadata is retained");
    shaded.shaded->progressiveMinimumLevel = 0;
    shaded.shaded->progressiveResidentLevel = 15;
    shaded.shaded->progressiveIndexCount.fill(3);
    shaded.shaded->progressivePositionCount.fill(3);
    assembly->upsertPart(first.part, shaded);
    runner.endTest(assembly->hasProgressivePartLod(),
        "only an explicit resident prefix may enable progressive drawing");

    runner.startTest("replacing shaded geometry clears progressive capability");
    assembly->upsertPart(first.part, Obol::PartGeometry());
    runner.endTest(!assembly->hasProgressivePartLod(),
        "part replacement must discard the previous progressive prefix");

    assembly->unref();
    return runner.getSummary();
}
