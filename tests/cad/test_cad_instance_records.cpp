/**
 * @file test_cad_instance_records.cpp
 * @brief Lossless SoCADAssembly instance-record tests.
 */

#include <Obol/cad/SoCADAssembly.h>

#include <gtest/gtest.h>

#include <Inventor/SoDB.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/sensors/SoNodeSensor.h>

#include <string>

namespace {

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

TEST(CadInstanceRecords, PreserveIdentityGeometryAndBoundsContracts)
{
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

    const Obol::InstanceId firstId = assembly->upsertInstanceAuto(first);
    std::optional<Obol::InstanceRecord> stored =
        assembly->getInstanceRecord(firstId);
    ASSERT_TRUE(stored.has_value());
    EXPECT_TRUE(sameRecord(*stored, first));

    Obol::InstanceRecord duplicate = first;
    duplicate.occurrenceIndex = 1;
    const Obol::InstanceId duplicateId =
        assembly->upsertInstanceAuto(duplicate);
    stored = assembly->getInstanceRecord(duplicateId);
    ASSERT_TRUE(stored.has_value());
    EXPECT_NE(duplicateId, firstId);
    EXPECT_TRUE(sameRecord(*stored, duplicate));

    Obol::InstanceRecord subtract = first;
    subtract.boolOp = 1;
    const Obol::InstanceId subtractId = assembly->upsertInstanceAuto(subtract);
    stored = assembly->getInstanceRecord(subtractId);
    ASSERT_TRUE(stored.has_value());
    EXPECT_NE(subtractId, firstId);
    EXPECT_TRUE(sameRecord(*stored, subtract));

    SbMatrix moved;
    moved.setTranslate(SbVec3f(4.0f, 5.0f, 6.0f));
    assembly->updateInstanceTransform(duplicateId, moved);
    stored = assembly->getInstanceRecord(duplicateId);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->parent, duplicate.parent);
    EXPECT_EQ(stored->childName, duplicate.childName);
    EXPECT_EQ(stored->occurrenceIndex, duplicate.occurrenceIndex);
    EXPECT_EQ(stored->boolOp, duplicate.boolOp);
    EXPECT_TRUE(sameMatrix(stored->localToRoot, moved));

    Obol::InstanceStyle restyled = duplicate.style;
    restyled.lineWidth = 7.0f;
    assembly->updateInstanceStyle(duplicateId, restyled);
    stored = assembly->getInstanceRecord(duplicateId);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->occurrenceIndex, duplicate.occurrenceIndex);
    EXPECT_EQ(stored->boolOp, duplicate.boolOp);
    EXPECT_EQ(stored->style.lineWidth, restyled.lineWidth);

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
    ASSERT_TRUE(stored.has_value());
    EXPECT_TRUE(sameRecord(*stored, intersection));

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
    EXPECT_FALSE(assembly->hasProgressivePartLod());

    shaded.shaded->progressiveMinimumCut = 0;
    shaded.shaded->progressiveResidentCut = 15;
    shaded.shaded->progressiveCuts.resize(16);
    for (Obol::ProgressiveTriangleCut& cut : shaded.shaded->progressiveCuts) {
        cut.indexCount = 3;
        cut.positionCount = 3;
    }
    assembly->upsertPart(first.part, shaded);
    EXPECT_TRUE(assembly->hasProgressivePartLod());

    assembly->progressiveCutCeiling = 5;
    assembly->progressiveCutNextFraction = 0.5f;
    size_t promotedParts = 0;
    for (size_t i = 0; i < 128; ++i) {
        const Obol::PartId part = Obol::CadIdBuilder::hash128(
            std::string("fractional-part-") + std::to_string(i));
        const uint8_t firstCut =
            assembly->effectiveProgressiveCut(part, 10);
        const uint8_t repeatedCut =
            assembly->effectiveProgressiveCut(part, 10);
        if (firstCut == 6)
            ++promotedParts;
        if (firstCut != repeatedCut || (firstCut != 5 && firstCut != 6)) {
            promotedParts = 0;
            break;
        }
    }
    EXPECT_GT(promotedParts, 32u);
    EXPECT_LT(promotedParts, 96u);
    assembly->progressiveCutNextFraction = 0.0f;
    EXPECT_EQ(assembly->effectiveProgressiveCut(first.part, 10), 5);
    assembly->progressiveCutNextFraction = 1.0f;
    EXPECT_EQ(assembly->effectiveProgressiveCut(first.part, 10), 6);
    EXPECT_EQ(assembly->maximumEffectiveProgressiveCut(10), 6);
    assembly->progressiveCutCeiling = -1;
    assembly->progressiveCutNextFraction = 0.0f;

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
    EXPECT_GT(firstPublicationChanges, 0u);
    EXPECT_EQ(changeCount, firstPublicationChanges);
    changeSensor.detach();

    assembly->upsertPart(first.part, Obol::PartGeometry());
    EXPECT_FALSE(assembly->hasProgressivePartLod());

    SoCADAssembly *emptyBoundsAssembly = new SoCADAssembly;
    emptyBoundsAssembly->ref();
    Obol::InstanceRecord emptyRecord;
    emptyRecord.part = Obol::CadIdBuilder::hash128("empty-bounds-part");
    emptyBoundsAssembly->upsertPart(emptyRecord.part, Obol::PartGeometry());
    emptyBoundsAssembly->upsertInstanceAuto(emptyRecord);
    SoGetBoundingBoxAction emptyBoundsAction(SbViewportRegion(64, 64));
    emptyBoundsAction.apply(emptyBoundsAssembly);
    EXPECT_TRUE(emptyBoundsAction.getBoundingBox().isEmpty());
    emptyBoundsAssembly->unref();

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
    EXPECT_FALSE(transformedBounds.isEmpty());
    EXPECT_EQ(transformedBounds.getMin(), SbVec3f(8.0f, 17.0f, 26.0f));
    EXPECT_EQ(transformedBounds.getMax(), SbVec3f(15.0f, 27.0f, 41.0f));
    conservativeBoundsAssembly->unref();

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
    EXPECT_EQ(progressiveWire.segmentFirstAtCut(0), 1u);
    EXPECT_EQ(progressiveWire.segmentCountAtCut(0), 2u);
    EXPECT_EQ(progressiveWire.segmentFirstAtCut(63), 4u);
    EXPECT_EQ(progressiveWire.segmentCountAtCut(63), 2u);

    assembly->unref();
}

} // namespace
