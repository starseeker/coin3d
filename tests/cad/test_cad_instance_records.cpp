/**
 * @file test_cad_instance_records.cpp
 * @brief Lossless SoCADAssembly instance-record tests.
 */

#include "../test_utils.h"

#include <Obol/cad/SoCADAssembly.h>

#include <Inventor/SoDB.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/sensors/SoNodeSensor.h>

using namespace SimpleTest;

static void
nodeChanged(void *data, SoSensor *)
{
    unsigned int *changeCount = static_cast<unsigned int *>(data);
    ++(*changeCount);
}

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
    shaded.shaded->progressiveMinimumCut = 0;
    shaded.shaded->progressiveResidentCut = 15;
    shaded.shaded->progressiveCuts.resize(16);
    for (Obol::ProgressiveTriangleCut& cut : shaded.shaded->progressiveCuts) {
        cut.indexCount = 3;
        cut.positionCount = 3;
    }
    assembly->upsertPart(first.part, shaded);
    runner.endTest(assembly->hasProgressivePartLod(),
        "only an explicit resident prefix may enable progressive drawing");

    runner.startTest("identical shared geometry publication is a strict no-op");
    unsigned int changeCount = 0;
    SoNodeSensor changeSensor(nodeChanged, &changeCount);
    changeSensor.setPriority(0);
    changeSensor.attach(assembly);
    const std::shared_ptr<const Obol::PartGeometry> immutableGeometry =
        std::make_shared<const Obol::PartGeometry>(shaded);
    Obol::SharedPartUpdate sharedUpdate;
    sharedUpdate.part = first.part;
    sharedUpdate.geometry = immutableGeometry;
    assembly->upsertSharedParts({sharedUpdate});
    const unsigned int firstPublicationChanges = changeCount;
    assembly->upsertSharedParts({sharedUpdate});
    runner.endTest(firstPublicationChanges > 0 &&
        changeCount == firstPublicationChanges,
        "republishing one immutable generation must not notify or rescan");
    changeSensor.detach();

    runner.startTest("replacing shaded geometry clears progressive capability");
    assembly->upsertPart(first.part, Obol::PartGeometry());
    runner.endTest(!assembly->hasProgressivePartLod(),
        "part replacement must discard the previous progressive prefix");

    runner.startTest("empty parts do not invent bounds at the origin");
    SoCADAssembly *emptyBoundsAssembly = new SoCADAssembly;
    emptyBoundsAssembly->ref();
    Obol::InstanceRecord emptyRecord;
    emptyRecord.part = Obol::CadIdBuilder::hash128("empty-bounds-part");
    emptyBoundsAssembly->upsertPart(emptyRecord.part, Obol::PartGeometry());
    emptyBoundsAssembly->upsertInstanceAuto(emptyRecord);
    SoGetBoundingBoxAction emptyBoundsAction(SbViewportRegion(64, 64));
    emptyBoundsAction.apply(emptyBoundsAssembly);
    runner.endTest(emptyBoundsAction.getBoundingBox().isEmpty(),
        "a part with no channels or explicit extent must remain empty");
    emptyBoundsAssembly->unref();

    runner.startTest("explicit conservative bounds survive instance transforms");
    SoCADAssembly *conservativeBoundsAssembly = new SoCADAssembly;
    conservativeBoundsAssembly->ref();
    Obol::PartGeometry boundedPlaceholder;
    SbBox3f conservativeBounds;
    conservativeBounds.setBounds(SbVec3f(-2.0f, -3.0f, -4.0f),
                                 SbVec3f( 5.0f,  7.0f, 11.0f));
    boundedPlaceholder.conservativeBounds = conservativeBounds;
    Obol::InstanceRecord boundedRecord;
    boundedRecord.part =
        Obol::CadIdBuilder::hash128("conservative-bounds-part");
    boundedRecord.localToRoot.setTranslate(SbVec3f(10.0f, 20.0f, 30.0f));
    conservativeBoundsAssembly->upsertPart(
        boundedRecord.part, boundedPlaceholder);
    conservativeBoundsAssembly->upsertInstanceAuto(boundedRecord);
    SoGetBoundingBoxAction conservativeBoundsAction(SbViewportRegion(64, 64));
    conservativeBoundsAction.apply(conservativeBoundsAssembly);
    const SbBox3f transformedBounds =
        conservativeBoundsAction.getBoundingBox();
    runner.endTest(!transformedBounds.isEmpty() &&
        transformedBounds.getMin() == SbVec3f(8.0f, 17.0f, 26.0f) &&
        transformedBounds.getMax() == SbVec3f(15.0f, 27.0f, 41.0f),
        "a producer extent must define bounds without renderable channels");
    conservativeBoundsAssembly->unref();

    runner.startTest("progressive wire cuts retain independent bounded ranges");
    Obol::WireRep progressiveWire;
    for (int i = 0; i < 12; ++i)
        progressiveWire.segmentPoints.push_back(SbVec3f(
            static_cast<float>(i), 0.0f, 0.0f));
    progressiveWire.progressiveMinimumCut = 2;
    progressiveWire.progressiveResidentCut = 5;
    progressiveWire.progressiveCuts.resize(6);
    progressiveWire.progressiveCuts[2].segmentFirst = 1;
    progressiveWire.progressiveCuts[2].segmentCount = 2;
    progressiveWire.progressiveCuts[5].segmentFirst = 4;
    progressiveWire.progressiveCuts[5].segmentCount = 9;
    runner.endTest(
        progressiveWire.segmentFirstAtCut(0) == 1 &&
        progressiveWire.segmentCountAtCut(0) == 2 &&
        progressiveWire.segmentFirstAtCut(63) == 4 &&
        progressiveWire.segmentCountAtCut(63) == 2,
        "cut selection must clamp and bound its own segment range");

    assembly->unref();
    return runner.getSummary();
}
