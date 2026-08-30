/**
 * @file test_cad_instance_records.cpp
 * @brief Lossless SoCADAssembly instance-record tests.
 */

#include <Obol/cad/SoCADAssembly.h>
#include <Obol/cad/CadViewState.h>

#include <gtest/gtest.h>

#include <Inventor/SoDB.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/sensors/SoNodeSensor.h>

#include <limits>
#include <memory>
#include <string>
#include <type_traits>

static_assert(std::is_default_constructible<Obol::PartGeometryBuilder>::value,
    "CAD producers need a mutable geometry builder");
static_assert(!std::is_default_constructible<Obol::PartGeometry>::value,
    "renderer-visible geometry must enter through admission");
static_assert(!std::is_copy_constructible<Obol::PartGeometry>::value,
    "admitted geometry snapshots must not regain mutable aliases");

namespace {

static void
nodeChanged(void *data, SoSensor *)
{
    unsigned int *changeCount = static_cast<unsigned int *>(data);
    ++(*changeCount);
}

TEST(CadInstanceRecords, AdmissionCopiesLvalueBuildersIntoImmutableStorage)
{
    Obol::PartGeometryBuilder builder;
    builder.shaded.emplace();
    builder.shaded->positions = {
        SbVec3f(0.0f, 0.0f, 0.0f),
        SbVec3f(1.0f, 0.0f, 0.0f),
        SbVec3f(0.0f, 1.0f, 0.0f)
    };
    builder.shaded->indices = {0, 1, 2};
    builder.shaded->bounds = SbBox3f(
        SbVec3f(0.0f, 0.0f, 0.0f), SbVec3f(1.0f, 1.0f, 0.0f));

    const Obol::CadGeometryAdmission admitted =
        Obol::cadAdmitPartGeometry(builder);
    ASSERT_TRUE(admitted);
    builder.shaded->positions.front() = SbVec3f(9.0f, 9.0f, 9.0f);
    builder.shaded->indices.front() = 2;

    ASSERT_TRUE(admitted.geometry.shared()->shaded.has_value());
    EXPECT_EQ(admitted.geometry.shared()->shaded->positions.front(),
        SbVec3f(0.0f, 0.0f, 0.0f));
    EXPECT_EQ(admitted.geometry.shared()->shaded->indices.front(), 0u);
}

TEST(CadInstanceRecords, ProgressiveClusterRangesRequireTheirActivationData)
{
    Obol::PartGeometryBuilder triangles;
    triangles.shaded.emplace();
    Obol::TriMesh& mesh = *triangles.shaded;
    mesh.positions = {
        SbVec3f(0.0f, 0.0f, 0.0f),
        SbVec3f(1.0f, 0.0f, 0.0f),
        SbVec3f(0.0f, 1.0f, 0.0f),
        SbVec3f(2.0f, 0.0f, 0.0f),
        SbVec3f(3.0f, 0.0f, 0.0f),
        SbVec3f(2.0f, 1.0f, 0.0f)};
    mesh.indices = {0, 1, 2, 3, 4, 5};
    mesh.bounds = SbBox3f(
        SbVec3f(0.0f, 0.0f, 0.0f), SbVec3f(3.0f, 1.0f, 0.0f));
    mesh.progressiveCuts.resize(2);
    mesh.progressiveCuts[0].indexCount = 3;
    mesh.progressiveCuts[0].positionCount = 3;
    mesh.progressiveCuts[1].indexCount = 6;
    mesh.progressiveCuts[1].positionCount = 6;
    mesh.progressiveMinimumCut = 0;
    mesh.progressiveResidentCut = 1;
    mesh.progressiveClusters.resize(1);
    mesh.progressiveClusters[0].bounds = mesh.bounds;
    mesh.progressiveClusters[0].residentCut = 1;
    mesh.progressiveClusters[0].ranges.push_back({3, 3, 0});
    Obol::CadGeometryValidation result =
        Obol::cadValidatePartGeometry(triangles);
    EXPECT_EQ(result.error, Obol::CadGeometryError::InvalidClusterRange);

    mesh.progressiveClusters[0].ranges[0].activationCut = 1;
    EXPECT_TRUE(Obol::cadValidatePartGeometry(triangles));

    Obol::PartGeometryBuilder wires;
    wires.wire.emplace();
    Obol::WireRep& wire = *wires.wire;
    wire.segmentPoints = {
        SbVec3f(0.0f, 0.0f, 0.0f), SbVec3f(1.0f, 0.0f, 0.0f),
        SbVec3f(2.0f, 0.0f, 0.0f), SbVec3f(3.0f, 0.0f, 0.0f)};
    wire.bounds = SbBox3f(
        SbVec3f(0.0f, 0.0f, 0.0f), SbVec3f(3.0f, 0.0f, 0.0f));
    wire.progressiveCuts.resize(2);
    wire.progressiveCuts[0].segmentCount = 1;
    wire.progressiveCuts[1].segmentCount = 2;
    wire.progressiveMinimumCut = 0;
    wire.progressiveResidentCut = 1;
    wire.progressiveClusters.resize(1);
    wire.progressiveClusters[0].bounds = wire.bounds;
    wire.progressiveClusters[0].residentCut = 1;
    wire.progressiveClusters[0].ranges.push_back({1, 1, 0});
    result = Obol::cadValidatePartGeometry(wires);
    EXPECT_EQ(result.error, Obol::CadGeometryError::InvalidClusterRange);

    wire.progressiveClusters[0].ranges[0].activationCut = 1;
    EXPECT_TRUE(Obol::cadValidatePartGeometry(wires));
}

TEST(CadInstanceRecords, ProgressiveSpatialMetadataMustBeConservative)
{
    Obol::PartGeometryBuilder triangles;
    triangles.shaded.emplace();
    Obol::TriMesh& mesh = *triangles.shaded;
    mesh.positions = {
        SbVec3f(0.0f, 0.0f, 0.0f),
        SbVec3f(1.0f, 0.0f, 0.0f),
        SbVec3f(0.0f, 1.0f, 0.0f)};
    mesh.indices = {0, 1, 2};
    mesh.bounds = SbBox3f(
        SbVec3f(0.0f, 0.0f, 0.0f), SbVec3f(1.0f, 1.0f, 0.0f));
    mesh.progressiveCuts.resize(1);
    mesh.progressiveCuts[0].indexCount = 3;
    mesh.progressiveCuts[0].positionCount = 3;
    mesh.progressiveCuts[0].quantization = {8, 0, 0};
    mesh.progressiveMinimumCut = 0;
    mesh.progressiveResidentCut = 0;
    mesh.progressiveQuantizationMinimum = SbVec3f(0.0f, 0.0f, 0.0f);
    mesh.progressiveQuantizationMaximum = SbVec3f(0.5f, 1.0f, 0.0f);
    Obol::CadGeometryValidation result =
        Obol::cadValidatePartGeometry(triangles);
    EXPECT_EQ(result.error, Obol::CadGeometryError::NonConservativeBounds);

    mesh.progressiveQuantizationMaximum[0] = 1.0f;
    mesh.progressiveClusters.resize(1);
    mesh.progressiveClusters[0].bounds = SbBox3f(
        SbVec3f(0.0f, 0.0f, 0.0f), SbVec3f(0.5f, 1.0f, 0.0f));
    mesh.progressiveClusters[0].residentCut = 0;
    mesh.progressiveClusters[0].ranges.push_back({0, 3, 0});
    result = Obol::cadValidatePartGeometry(triangles);
    EXPECT_EQ(result.error, Obol::CadGeometryError::NonConservativeBounds);

    mesh.progressiveClusters[0].bounds = mesh.bounds;
    EXPECT_TRUE(Obol::cadValidatePartGeometry(triangles));

    Obol::PartGeometryBuilder wires;
    wires.wire.emplace();
    Obol::WireRep& wire = *wires.wire;
    wire.segmentPoints = {
        SbVec3f(0.0f, 0.0f, 0.0f), SbVec3f(2.0f, 0.0f, 0.0f)};
    wire.bounds = SbBox3f(
        SbVec3f(0.0f, 0.0f, 0.0f), SbVec3f(2.0f, 0.0f, 0.0f));
    wire.progressiveCuts.resize(1);
    wire.progressiveCuts[0].segmentCount = 1;
    wire.progressiveCuts[0].quantization = {8, 0, 0};
    wire.progressiveMinimumCut = 0;
    wire.progressiveResidentCut = 0;
    wire.progressiveQuantizationMinimum = SbVec3f(0.0f, 0.0f, 0.0f);
    wire.progressiveQuantizationMaximum = SbVec3f(2.0f, 0.0f, 0.0f);
    wire.progressiveClusters.resize(1);
    wire.progressiveClusters[0].bounds = SbBox3f(
        SbVec3f(0.0f, 0.0f, 0.0f), SbVec3f(1.0f, 0.0f, 0.0f));
    wire.progressiveClusters[0].residentCut = 0;
    wire.progressiveClusters[0].ranges.push_back({0, 1, 0});
    result = Obol::cadValidatePartGeometry(wires);
    EXPECT_EQ(result.error, Obol::CadGeometryError::NonConservativeBounds);

    wire.progressiveClusters[0].bounds = wire.bounds;
    EXPECT_TRUE(Obol::cadValidatePartGeometry(wires));
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

static Obol::CadGeometryValidation
admitAndUpsertPart(SoCADAssembly *assembly, Obol::PartId part,
    Obol::PartGeometryBuilder geometry)
{
    const Obol::CadGeometryAdmission admission =
        Obol::cadAdmitPartGeometry(std::move(geometry));
    if (!admission)
        return admission.validation;
    return assembly->upsertParts({{part, admission.geometry}});
}

TEST(CadInstanceRecords, PreserveIdentityGeometryAndBoundsContracts)
{
    SoCADAssembly::initClass();

    SoCADAssembly *assembly = new SoCADAssembly;
    assembly->ref();

    Obol::InstanceRecord first;
    first.part = Obol::CadIdBuilder::partId("shared-part");
    first.parent = Obol::CadIdBuilder::instanceId("parent");
    first.childName = "wheel";
    first.occurrenceIndex = 0;
    first.boolOp = 0;
    first.style.hasColorOverride = true;
    first.style.color = SbColor4f(0.1f, 0.2f, 0.3f, 0.4f);
    first.style.lineWidth = 2.5f;
    first.localToRoot.setTranslate(SbVec3f(1.0f, 2.0f, 3.0f));

    const Obol::CadInstanceUpdateResult firstInsert =
        assembly->upsertInstanceAuto(first);
    ASSERT_TRUE(firstInsert);
    const Obol::InstanceId firstId = firstInsert.instance;
    std::optional<Obol::InstanceRecord> stored =
        assembly->getInstanceRecord(firstId);
    ASSERT_TRUE(stored.has_value());
    EXPECT_TRUE(sameRecord(*stored, first));

    Obol::InstanceRecord duplicate = first;
    duplicate.occurrenceIndex = 1;
    const Obol::CadInstanceUpdateResult duplicateInsert =
        assembly->upsertInstanceAuto(duplicate);
    ASSERT_TRUE(duplicateInsert);
    const Obol::InstanceId duplicateId = duplicateInsert.instance;
    stored = assembly->getInstanceRecord(duplicateId);
    ASSERT_TRUE(stored.has_value());
    EXPECT_NE(duplicateId, firstId);
    EXPECT_TRUE(sameRecord(*stored, duplicate));

    Obol::InstanceRecord subtract = first;
    subtract.boolOp = 1;
    const Obol::CadInstanceUpdateResult subtractInsert =
        assembly->upsertInstanceAuto(subtract);
    ASSERT_TRUE(subtractInsert);
    const Obol::InstanceId subtractId = subtractInsert.instance;
    stored = assembly->getInstanceRecord(subtractId);
    ASSERT_TRUE(stored.has_value());
    EXPECT_NE(subtractId, firstId);
    EXPECT_TRUE(sameRecord(*stored, subtract));

    SbMatrix moved;
    moved.setTranslate(SbVec3f(4.0f, 5.0f, 6.0f));
    ASSERT_TRUE(assembly->updateInstanceTransform(duplicateId, moved));
    stored = assembly->getInstanceRecord(duplicateId);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->parent, duplicate.parent);
    EXPECT_EQ(stored->childName, duplicate.childName);
    EXPECT_EQ(stored->occurrenceIndex, duplicate.occurrenceIndex);
    EXPECT_EQ(stored->boolOp, duplicate.boolOp);
    EXPECT_TRUE(sameMatrix(stored->localToRoot, moved));

    Obol::InstanceStyle restyled = duplicate.style;
    restyled.lineWidth = 7.0f;
    ASSERT_TRUE(assembly->updateInstanceStyle(duplicateId, restyled));
    stored = assembly->getInstanceRecord(duplicateId);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->occurrenceIndex, duplicate.occurrenceIndex);
    EXPECT_EQ(stored->boolOp, duplicate.boolOp);
    EXPECT_EQ(stored->style.lineWidth, restyled.lineWidth);

    Obol::InstanceRecord intersection = first;
    intersection.childName = "overlap";
    intersection.boolOp = 2;
    const Obol::InstanceId intersectionId =
        Obol::CadIdBuilder::instanceId("external-intersection-id");
    Obol::InstanceUpdate update;
    update.instance = intersectionId;
    update.record = intersection;
    ASSERT_TRUE(assembly->upsertInstances(
        std::vector<Obol::InstanceUpdate>(1, update)));
    stored = assembly->getInstanceRecord(intersectionId);
    ASSERT_TRUE(stored.has_value());
    EXPECT_TRUE(sameRecord(*stored, intersection));

    Obol::PartGeometryBuilder shaded;
    Obol::TriMesh triangle;
    triangle.positions = {SbVec3f(0.0f, 0.0f, 0.0f),
                          SbVec3f(1.0f, 0.0f, 0.0f),
                          SbVec3f(0.0f, 1.0f, 0.0f)};
    triangle.indices = {0, 1, 2};
    triangle.bounds.setBounds(SbVec3f(0.0f, 0.0f, 0.0f),
                              SbVec3f(1.0f, 1.0f, 0.0f));
    shaded.shaded = std::move(triangle);
    ASSERT_TRUE(admitAndUpsertPart(assembly, first.part, shaded));
    EXPECT_FALSE(assembly->hasProgressivePartLod());

    shaded.shaded->progressiveMinimumCut = 0;
    shaded.shaded->progressiveResidentCut = 15;
    shaded.shaded->progressiveCuts.resize(16);
    for (Obol::ProgressiveTriangleCut& cut : shaded.shaded->progressiveCuts) {
        cut.indexCount = 3;
        cut.positionCount = 3;
    }
    ASSERT_TRUE(admitAndUpsertPart(assembly, first.part, shaded));
    EXPECT_TRUE(assembly->hasProgressivePartLod());

    Obol::CadViewState viewState;
    viewState.progressiveCutCeiling = 5;
    viewState.progressiveCutNextFraction = 0.5f;
    size_t promotedParts = 0;
    for (size_t i = 0; i < 128; ++i) {
        const Obol::PartId part = Obol::CadIdBuilder::partId(
            std::string("fractional-part-") + std::to_string(i));
        const uint8_t firstCut =
            Obol::cadEffectiveProgressiveCut(viewState, part, 10);
        const uint8_t repeatedCut =
            Obol::cadEffectiveProgressiveCut(viewState, part, 10);
        if (firstCut == 6)
            ++promotedParts;
        if (firstCut != repeatedCut || (firstCut != 5 && firstCut != 6)) {
            promotedParts = 0;
            break;
        }
    }
    EXPECT_GT(promotedParts, 32u);
    EXPECT_LT(promotedParts, 96u);
    viewState.progressiveCutNextFraction = 0.0f;
    EXPECT_EQ(Obol::cadEffectiveProgressiveCut(
        viewState, first.part, 10), 5);
    viewState.progressiveCutNextFraction = 1.0f;
    EXPECT_EQ(Obol::cadEffectiveProgressiveCut(
        viewState, first.part, 10), 6);
    EXPECT_EQ(Obol::cadMaximumEffectiveProgressiveCut(
        viewState, 10), 6);

    unsigned int changeCount = 0;
    SoNodeSensor changeSensor(nodeChanged, &changeCount);
    changeSensor.setPriority(0);
    changeSensor.attach(assembly);
    Obol::PartUpdate sharedUpdate;
    sharedUpdate.part = first.part;
    const Obol::CadGeometryAdmission sharedAdmission =
        Obol::cadAdmitPartGeometry(shaded);
    ASSERT_TRUE(sharedAdmission);
    sharedUpdate.geometry = sharedAdmission.geometry;
    ASSERT_TRUE(assembly->upsertParts({sharedUpdate}));
    const unsigned int firstPublicationChanges = changeCount;
    ASSERT_TRUE(assembly->upsertParts({sharedUpdate}));
    EXPECT_GT(firstPublicationChanges, 0u);
    EXPECT_EQ(changeCount, firstPublicationChanges);
    changeSensor.detach();

    ASSERT_TRUE(admitAndUpsertPart(
        assembly, first.part, Obol::PartGeometryBuilder()));
    EXPECT_FALSE(assembly->hasProgressivePartLod());

    SoCADAssembly *emptyBoundsAssembly = new SoCADAssembly;
    emptyBoundsAssembly->ref();
    Obol::InstanceRecord emptyRecord;
    emptyRecord.part = Obol::CadIdBuilder::partId("empty-bounds-part");
    ASSERT_TRUE(admitAndUpsertPart(emptyBoundsAssembly,
        emptyRecord.part, Obol::PartGeometryBuilder()));
    ASSERT_TRUE(emptyBoundsAssembly->upsertInstanceAuto(emptyRecord));
    SoGetBoundingBoxAction emptyBoundsAction(SbViewportRegion(64, 64));
    emptyBoundsAction.apply(emptyBoundsAssembly);
    EXPECT_TRUE(emptyBoundsAction.getBoundingBox().isEmpty());
    emptyBoundsAssembly->unref();

    SoCADAssembly *conservativeBoundsAssembly = new SoCADAssembly;
    conservativeBoundsAssembly->ref();
    Obol::PartGeometryBuilder boundedPlaceholder;
    SbBox3f conservativeBounds;
    conservativeBounds.setBounds(SbVec3f(-2.0f, -3.0f, -4.0f),
                                 SbVec3f( 5.0f,  7.0f, 11.0f));
    boundedPlaceholder.conservativeBounds = conservativeBounds;
    Obol::InstanceRecord boundedRecord;
    boundedRecord.part =
        Obol::CadIdBuilder::partId("conservative-bounds-part");
    boundedRecord.localToRoot.setTranslate(SbVec3f(10.0f, 20.0f, 30.0f));
    ASSERT_TRUE(admitAndUpsertPart(conservativeBoundsAssembly,
        boundedRecord.part, boundedPlaceholder));
    ASSERT_TRUE(conservativeBoundsAssembly->upsertInstanceAuto(boundedRecord));
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

TEST(CadInstanceRecords, RejectsMalformedGeometryAtomically)
{
    SoCADAssembly::initClass();

    SoCADAssembly *assembly = new SoCADAssembly;
    assembly->ref();

    Obol::PartGeometryBuilder invalidGeometry;
    Obol::TriMesh invalidMesh;
    invalidMesh.positions = {
        SbVec3f(0.0f, 0.0f, 0.0f),
        SbVec3f(1.0f, 0.0f, 0.0f),
        SbVec3f(0.0f, 1.0f, 0.0f)
    };
    invalidMesh.bounds.setBounds(SbVec3f(0.0f, 0.0f, 0.0f),
        SbVec3f(1.0f, 1.0f, 0.0f));
    invalidMesh.indices = {0, 1, 7};
    invalidGeometry.shaded = invalidMesh;

    const Obol::PartId invalidPart =
        Obol::CadIdBuilder::partId("invalid-index-part");
    Obol::CadGeometryValidation result =
        admitAndUpsertPart(assembly, invalidPart, invalidGeometry);
    EXPECT_EQ(result.error, Obol::CadGeometryError::InvalidVertexIndex);
    EXPECT_EQ(result.elementIndex, 2u);
    EXPECT_EQ(assembly->partCount(), 0u);

    Obol::PartGeometryBuilder validGeometry = invalidGeometry;
    validGeometry.shaded->indices[2] = 2;
    const Obol::PartId validPart =
        Obol::CadIdBuilder::partId("valid-part");

    Obol::PartGeometryBuilder invalidProgression = validGeometry;
    invalidProgression.shaded->progressiveMinimumCut = 0;
    invalidProgression.shaded->progressiveResidentCut = 1;
    invalidProgression.shaded->progressiveCuts.resize(2);
    invalidProgression.shaded->progressiveCuts[0].indexCount = 3;
    invalidProgression.shaded->progressiveCuts[0].positionCount = 3;
    invalidProgression.shaded->progressiveCuts[1].indexCount = 0;
    invalidProgression.shaded->progressiveCuts[1].positionCount = 0;
    result = admitAndUpsertPart(assembly, validPart, invalidProgression);
    EXPECT_EQ(result.error, Obol::CadGeometryError::InvalidProgressiveOrder);
    EXPECT_EQ(assembly->partCount(), 0u);

    Obol::PartGeometryBuilder progressiveWireGeometry;
    Obol::WireRep certifiedWire;
    certifiedWire.bounds = SbBox3f(SbVec3f(0.0f, 0.0f, 0.0f),
        SbVec3f(2.0f, 1.0f, 0.0f));
    certifiedWire.segmentPoints = {
        SbVec3f(0.0f, 0.0f, 0.0f), SbVec3f(1.0f, 0.0f, 0.0f),
        SbVec3f(1.0f, 0.0f, 0.0f), SbVec3f(2.0f, 1.0f, 0.0f)
    };
    certifiedWire.progressiveMinimumCut = 0;
    certifiedWire.progressiveResidentCut = 1;
    certifiedWire.progressiveCuts.resize(2);
    certifiedWire.progressiveCuts[0].segmentCount = 1;
    certifiedWire.progressiveCuts[0].maximumNormalizedError = 0.25f;
    certifiedWire.progressiveCuts[1].segmentCount = 2;
    certifiedWire.progressiveCuts[1].maximumNormalizedError = 0.0f;
    progressiveWireGeometry.wire = certifiedWire;
    EXPECT_TRUE(Obol::cadValidatePartGeometry(progressiveWireGeometry));

    progressiveWireGeometry.wire->progressiveCuts[1]
        .maximumNormalizedError = 0.5f;
    result = Obol::cadValidatePartGeometry(progressiveWireGeometry);
    EXPECT_EQ(result.error, Obol::CadGeometryError::InvalidProgressiveOrder);

    progressiveWireGeometry.wire->progressiveCuts[0]
        .maximumNormalizedError = 0.25f;
    progressiveWireGeometry.wire->progressiveCuts[1]
        .maximumNormalizedError = -1.0f;
    result = Obol::cadValidatePartGeometry(progressiveWireGeometry);
    EXPECT_EQ(result.error, Obol::CadGeometryError::InvalidProgressiveOrder);

    result = admitAndUpsertPart(assembly, validPart, validGeometry);
    EXPECT_TRUE(result.valid());
    EXPECT_EQ(assembly->partCount(), 1u);

    /* A whole-scene structural overview stays visible in shaded mode but is
     * intentionally not collapsed to a point.  Those are independent
     * presentation properties. */
    Obol::PartGeometryBuilder structuralOverview;
    structuralOverview.structuralProxy = true;
    structuralOverview.conservativeBounds = SbBox3f(
        SbVec3f(-1.0f, -1.0f, -1.0f), SbVec3f(1.0f, 1.0f, 1.0f));
    EXPECT_TRUE(Obol::cadValidatePartGeometry(structuralOverview));

    Obol::PartGeometryBuilder invalidSubpixelProxy;
    invalidSubpixelProxy.subpixelProxyEligible = true;
    result = Obol::cadValidatePartGeometry(invalidSubpixelProxy);
    EXPECT_EQ(result.error, Obol::CadGeometryError::InvalidSubpixelProxy);

    Obol::PartGeometryBuilder wireSubpixelProxy;
    wireSubpixelProxy.subpixelProxyEligible = true;
    Obol::WireRep proxyWire;
    proxyWire.bounds = SbBox3f(SbVec3f(-2.0f, -1.0f, 0.0f),
        SbVec3f(2.0f, 1.0f, 0.0f));
    proxyWire.polylines.push_back({
        {SbVec3f(-2.0f, 0.0f, 0.0f), SbVec3f(2.0f, 0.0f, 0.0f)}, 0});
    wireSubpixelProxy.wire = std::move(proxyWire);
    EXPECT_TRUE(Obol::cadValidatePartGeometry(wireSubpixelProxy));

    Obol::PartUpdate nullUpdate;
    nullUpdate.part = invalidPart;
    result = assembly->upsertParts({nullUpdate});
    EXPECT_EQ(result.error, Obol::CadGeometryError::NullGeometry);
    EXPECT_EQ(assembly->partCount(), 1u);

    assembly->unref();
}

TEST(CadInstanceRecords, RejectsMalformedSceneMutationsAtomically)
{
    SoCADAssembly::initClass();

    SoCADAssembly *assembly = new SoCADAssembly;
    assembly->ref();

    const Obol::PartId part = Obol::CadIdBuilder::partId("scene-part");
    Obol::InstanceRecord valid;
    valid.part = part;
    valid.parent = Obol::CadIdBuilder::rootInstance();
    valid.childName = "valid";

    Obol::InstanceRecord invalid = valid;
    invalid.childName = "invalid";
    invalid.localToRoot[1][2] =
        (std::numeric_limits<float>::quiet_NaN)();

    Obol::InstanceUpdate validUpdate;
    validUpdate.instance = Obol::CadIdBuilder::instanceId("valid-instance");
    validUpdate.record = valid;
    Obol::InstanceUpdate invalidUpdate;
    invalidUpdate.instance =
        Obol::CadIdBuilder::instanceId("invalid-instance");
    invalidUpdate.record = invalid;

    Obol::CadSceneValidation validation =
        assembly->upsertInstances({validUpdate, invalidUpdate});
    EXPECT_EQ(validation.error, Obol::CadSceneError::NonFiniteTransform);
    EXPECT_EQ(validation.updateIndex, 1u);
    EXPECT_EQ(assembly->instanceCount(), 0u);

    validation = assembly->upsertInstance(validUpdate.instance, valid);
    ASSERT_TRUE(validation);
    const std::optional<Obol::InstanceRecord> before =
        assembly->getInstanceRecord(validUpdate.instance);
    ASSERT_TRUE(before.has_value());

    validation = assembly->updateInstanceTransform(
        validUpdate.instance, invalid.localToRoot);
    EXPECT_EQ(validation.error, Obol::CadSceneError::NonFiniteTransform);
    const std::optional<Obol::InstanceRecord> afterTransform =
        assembly->getInstanceRecord(validUpdate.instance);
    ASSERT_TRUE(afterTransform.has_value());
    EXPECT_EQ(afterTransform->localToRoot, before->localToRoot);

    Obol::InstanceStyle invalidStyle = valid.style;
    invalidStyle.lineWidth = 0.0f;
    validation = assembly->updateInstanceStyle(
        validUpdate.instance, invalidStyle);
    EXPECT_EQ(validation.error, Obol::CadSceneError::InvalidStyle);
    const std::optional<Obol::InstanceRecord> afterStyle =
        assembly->getInstanceRecord(validUpdate.instance);
    ASSERT_TRUE(afterStyle.has_value());
    EXPECT_EQ(afterStyle->style.lineWidth, before->style.lineWidth);

    Obol::InstanceStyleUpdate validStyleUpdate;
    validStyleUpdate.instance = validUpdate.instance;
    validStyleUpdate.style = valid.style;
    Obol::InstanceStyleUpdate missingStyleUpdate = validStyleUpdate;
    missingStyleUpdate.instance =
        Obol::CadIdBuilder::instanceId("missing-instance");
    validation = assembly->updateInstanceStyles(
        {validStyleUpdate, missingStyleUpdate});
    EXPECT_EQ(validation.error, Obol::CadSceneError::MissingInstance);
    EXPECT_EQ(validation.updateIndex, 1u);

    Obol::InstanceLodUpdate missingCut;
    missingCut.instance = missingStyleUpdate.instance;
    validation = assembly->updateInstanceCuts({missingCut});
    EXPECT_EQ(validation.error, Obol::CadSceneError::MissingInstance);
    EXPECT_EQ(validation.updateIndex, 0u);

    invalid.part = Obol::PartId();
    const Obol::CadInstanceUpdateResult invalidAuto =
        assembly->upsertInstanceAuto(invalid);
    EXPECT_EQ(invalidAuto.validation.error,
        Obol::CadSceneError::NonFiniteTransform);
    EXPECT_FALSE(invalidAuto.instance.isValid());
    EXPECT_EQ(assembly->instanceCount(), 1u);

    assembly->unref();
}

TEST(CadInstanceRecords, BatchScopesNestAndPurePreflightDoesNotMutate)
{
    SoCADAssembly::initClass();

    SoCADAssembly *assembly = new SoCADAssembly;
    assembly->ref();
    unsigned int changeCount = 0;
    SoNodeSensor changeSensor(nodeChanged, &changeCount);
    changeSensor.setPriority(0);
    changeSensor.attach(assembly);

    Obol::PartGeometryBuilder geometry;
    geometry.conservativeBounds = SbBox3f(
        SbVec3f(-1.0f, -1.0f, -1.0f), SbVec3f(1.0f, 1.0f, 1.0f));
    const Obol::PartId part =
        Obol::CadIdBuilder::partId("batch-scope-part");
    {
        auto outer = assembly->batchUpdate();
        auto inner = assembly->batchUpdate();
        ASSERT_TRUE(admitAndUpsertPart(assembly, part, geometry));
        inner.finish();
        EXPECT_EQ(changeCount, 0u);
        outer.finish();
        EXPECT_GT(changeCount, 0u);
    }
    const unsigned int committedChanges = changeCount;

    Obol::PartGeometryBuilder malformed = geometry;
    Obol::TriMesh mesh;
    mesh.positions = {SbVec3f(0.0f, 0.0f, 0.0f)};
    mesh.indices = {0, 0, 4};
    mesh.bounds.setBounds(mesh.positions.front(), mesh.positions.front());
    malformed.shaded = std::move(mesh);
    const Obol::CadGeometryValidation preflight =
        Obol::cadValidatePartGeometry(malformed);
    EXPECT_EQ(preflight.error, Obol::CadGeometryError::InvalidVertexIndex);
    EXPECT_EQ(assembly->partCount(), 1u);
    EXPECT_EQ(changeCount, committedChanges);

    changeSensor.detach();
    assembly->unref();
}

TEST(CadInstanceRecords, CompleteReplacementRejectsBeforeClearingLiveScene)
{
    SoCADAssembly::initClass();

    SoCADAssembly *assembly = new SoCADAssembly;
    assembly->ref();

    unsigned int changeCount = 0;
    SoNodeSensor changeSensor(nodeChanged, &changeCount);
    changeSensor.setPriority(0);
    changeSensor.attach(assembly);

    Obol::PartGeometryBuilder geometry;
    geometry.conservativeBounds = SbBox3f(
        SbVec3f(-1.0f, -1.0f, -1.0f), SbVec3f(1.0f, 1.0f, 1.0f));
    const auto admitted = Obol::cadAdmitPartGeometry(geometry);
    ASSERT_TRUE(admitted);

    const Obol::PartId firstPart =
        Obol::CadIdBuilder::partId("replacement-first-part");
    const Obol::InstanceId firstInstance =
        Obol::CadIdBuilder::instanceId("replacement-first-instance");
    Obol::InstanceRecord firstRecord;
    firstRecord.part = firstPart;
    const Obol::CadSceneReplacementResult first = assembly->replaceScene(
        {{firstPart, admitted.geometry, false}},
        {{firstInstance, firstRecord}});
    ASSERT_TRUE(first);
    EXPECT_GT(changeCount, 0u);
    EXPECT_EQ(assembly->partCount(), 1u);
    EXPECT_EQ(assembly->instanceCount(), 1u);
    const unsigned int committedChanges = changeCount;

    Obol::InstanceRecord malformedRecord = firstRecord;
    malformedRecord.part = Obol::PartId();
    const Obol::CadSceneReplacementResult rejected = assembly->replaceScene(
        {{Obol::CadIdBuilder::partId("replacement-second-part"),
          admitted.geometry, false}},
        {{Obol::CadIdBuilder::instanceId("replacement-second-instance"),
          malformedRecord}});
    EXPECT_EQ(rejected.error,
        Obol::CadSceneReplacementError::Instances);
    EXPECT_EQ(rejected.instances.error, Obol::CadSceneError::InvalidPartId);
    EXPECT_EQ(assembly->partCount(), 1u);
    EXPECT_EQ(assembly->instanceCount(), 1u);
    EXPECT_TRUE(assembly->getInstanceRecord(firstInstance).has_value());
    EXPECT_EQ(changeCount, committedChanges);
    EXPECT_STREQ(Obol::cadSceneReplacementErrorName(
        Obol::CadSceneReplacementError::ResourceUnavailable),
        "resource-unavailable");

    changeSensor.detach();
    assembly->unref();
}

TEST(CadInstanceRecords, SparseMutationRejectsBeforeChangingLiveScene)
{
    SoCADAssembly::initClass();

    SoCADAssembly *assembly = new SoCADAssembly;
    assembly->ref();

    Obol::PartGeometryBuilder geometry;
    geometry.conservativeBounds = SbBox3f(
        SbVec3f(-1.0f, -1.0f, -1.0f), SbVec3f(1.0f, 1.0f, 1.0f));
    const auto admitted = Obol::cadAdmitPartGeometry(std::move(geometry));
    ASSERT_TRUE(admitted);

    const Obol::PartId part =
        Obol::CadIdBuilder::partId("sparse-rejected-part");
    const Obol::InstanceId instance =
        Obol::CadIdBuilder::instanceId("sparse-rejected-instance");
    Obol::InstanceRecord record;
    record.part = part;

    Obol::CadSceneMutation mutation;
    mutation.parts.push_back({part, admitted.geometry, false});
    mutation.instances.push_back({instance, record});
    mutation.styles.push_back({
        Obol::CadIdBuilder::instanceId("missing-style-target"),
        Obol::InstanceStyle()});

    const Obol::CadSceneMutationResult rejected =
        assembly->applySceneMutation(mutation);
    EXPECT_EQ(rejected.domain, Obol::CadSceneMutationDomain::Styles);
    EXPECT_EQ(rejected.scene.error, Obol::CadSceneError::MissingInstance);
    EXPECT_EQ(assembly->partCount(), 0u);
    EXPECT_EQ(assembly->instanceCount(), 0u);

    assembly->unref();
}

TEST(CadInstanceRecords, SparseMutationCommitsOneValidatedDelta)
{
    SoCADAssembly::initClass();

    SoCADAssembly *assembly = new SoCADAssembly;
    assembly->ref();

    Obol::PartGeometryBuilder geometry;
    geometry.conservativeBounds = SbBox3f(
        SbVec3f(-1.0f, -1.0f, -1.0f), SbVec3f(1.0f, 1.0f, 1.0f));
    const auto admitted = Obol::cadAdmitPartGeometry(std::move(geometry));
    ASSERT_TRUE(admitted);

    const Obol::PartId part =
        Obol::CadIdBuilder::partId("sparse-committed-part");
    const Obol::InstanceId instance =
        Obol::CadIdBuilder::instanceId("sparse-committed-instance");
    Obol::InstanceRecord record;
    record.part = part;
    Obol::InstanceStyle style;
    style.hasColorOverride = true;
    style.color = SbColor4f(0.2f, 0.3f, 0.4f, 1.0f);

    Obol::CadSceneMutation mutation;
    mutation.parts.push_back({part, admitted.geometry, false});
    mutation.instances.push_back({instance, record});
    mutation.styles.push_back({instance, style});
    mutation.cuts.push_back({instance, 3u});

    const Obol::CadSceneMutationResult committed =
        assembly->applySceneMutation(mutation);
    ASSERT_TRUE(committed);
    ASSERT_EQ(assembly->partCount(), 1u);
    ASSERT_EQ(assembly->instanceCount(), 1u);
    const std::optional<Obol::InstanceRecord> stored =
        assembly->getInstanceRecord(instance);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->lodCut, 3u);
    EXPECT_TRUE(stored->style.hasColorOverride);
    EXPECT_EQ(stored->style.color, style.color);

    Obol::CadSceneMutation conflict;
    conflict.instances.push_back({instance, record});
    conflict.removedInstances.push_back(instance);
    const Obol::CadSceneMutationResult rejected =
        assembly->applySceneMutation(conflict);
    EXPECT_EQ(rejected.domain,
        Obol::CadSceneMutationDomain::RemovedInstances);
    EXPECT_TRUE(assembly->getInstanceRecord(instance).has_value());

    assembly->unref();
}

} // namespace
