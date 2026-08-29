#include <Obol/cad/CadIds.h>
#include <Obol/cad/SoCADAssembly.h>

#include "CadPicking.h"

#include <gtest/gtest.h>

#include <Inventor/SbBox3f.h>
#include <Inventor/SbLine.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbVec3f.h>

#include <memory>
#include <unordered_map>
#include <vector>

namespace {

using Obol::CadIdBuilder;
using Obol::InstanceId;
using Obol::PartId;
using Obol::PartGeometry;
using Obol::PartGeometryBuilder;
using Obol::PointRep;
using Obol::ProgressiveTriangleCluster;
using Obol::ProgressiveTriangleCut;
using Obol::TriMesh;
using Obol::WireRep;
using Obol::picking::CadInstanceBVH;
using Obol::picking::CadPartEdgeBVH;
using Obol::picking::CadPartTriBVH;
using Obol::picking::CadPickQuery;
using Obol::picking::CadPickResult;

std::shared_ptr<const PartGeometry>
admitGeometry(PartGeometryBuilder geometry)
{
    Obol::CadGeometryAdmission admission =
        Obol::cadAdmitPartGeometry(std::move(geometry));
    if (!admission) {
        ADD_FAILURE() << "test geometry admission failed: "
                      << Obol::cadGeometryErrorName(admission.validation.error);
        return {};
    }
    return admission.geometry.shared();
}

WireRep makeCubeWireframe()
{
    WireRep wire;
    const SbVec3f points[] = {
        {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
        {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1},
    };
    const unsigned int edges[][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
        {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7},
    };
    for (unsigned int edge = 0; edge < 12; ++edge) {
        wire.segmentPoints.push_back(points[edges[edge][0]]);
        wire.segmentPoints.push_back(points[edges[edge][1]]);
        wire.segmentIds.push_back(edge);
    }
    wire.bounds.setBounds(SbVec3f(0, 0, 0), SbVec3f(1, 1, 1));
    return wire;
}

TriMesh makePyramid()
{
    TriMesh mesh;
    mesh.positions = {
        {0, 0, 0}, {1, 0, 0}, {0.5f, 1, 0}, {0.5f, 0.5f, 1},
    };
    mesh.indices = {0, 1, 2, 0, 1, 3, 1, 2, 3, 2, 0, 3};
    mesh.bounds.setBounds(SbVec3f(0, 0, 0), SbVec3f(1, 1, 1));
    return mesh;
}

CadInstanceBVH::Entry makeEntry(const PartId part, const InstanceId instance,
                                const SbBox3f & bounds, const SbMatrix & transform)
{
    CadInstanceBVH::Entry entry;
    entry.partId = part;
    entry.instanceId = instance;
    entry.worldBounds = bounds;
    entry.localToWorld = transform;
    return entry;
}

TEST(CadPicking, InstanceBvhQueriesOnlyIntersectedInstances)
{
    CadInstanceBVH bvh;
    EXPECT_TRUE(bvh.query(SbLine(SbVec3f(0, 0, -1), SbVec3f(0, 0, 1))).empty());

    SbMatrix identity;
    identity.makeIdentity();
    SbBox3f first_bounds;
    first_bounds.setBounds(SbVec3f(0, 0, 0), SbVec3f(1, 1, 1));
    SbBox3f second_bounds;
    second_bounds.setBounds(SbVec3f(5, 5, 5), SbVec3f(6, 6, 6));
    const InstanceId first = CadIdBuilder::instanceId("first-instance");
    bvh.build({
        makeEntry(CadIdBuilder::partId("first-part"), first,
                  first_bounds, identity),
        makeEntry(CadIdBuilder::partId("second-part"),
                  CadIdBuilder::instanceId("second-instance"),
                  second_bounds, identity),
    });

    const auto hit = bvh.query(
        SbLine(SbVec3f(0.5f, 0.5f, -1.0f), SbVec3f(0.5f, 0.5f, 2.0f)));
    ASSERT_EQ(hit.size(), 1u);
    EXPECT_EQ(hit.front()->instanceId, first);
    EXPECT_TRUE(bvh.query(
        SbLine(SbVec3f(10, 10, 0), SbVec3f(11, 10, 0))).empty());
}

TEST(CadPicking, EdgeBvhReturnsOnlySegmentsInsideTolerance)
{
    CadPartEdgeBVH bvh;
    EXPECT_FALSE(bvh.queryClosest(
        SbLine(SbVec3f(0, 0, -1), SbVec3f(0, 0, 1)), 0.5f).has_value());

    CadPartEdgeBVH::SegEntry near_segment;
    near_segment.p0 = SbVec3f(0, 0, 0);
    near_segment.p1 = SbVec3f(1, 0, 0);
    near_segment.polylineIdx = 7;
    bvh.build({near_segment});
    const auto near_hit = bvh.queryClosest(
        SbLine(SbVec3f(0.5f, 0, 2), SbVec3f(0.5f, 0, -1)), 0.5f);
    ASSERT_TRUE(near_hit.has_value());
    EXPECT_EQ(near_hit->seg.polylineIdx, 7u);

    near_segment.p0.setValue(0, 10, 0);
    near_segment.p1.setValue(1, 10, 0);
    bvh.build({near_segment});
    EXPECT_FALSE(bvh.queryClosest(
        SbLine(SbVec3f(0.5f, 0, 2), SbVec3f(0.5f, 0, -1)), 0.1f).has_value());
}

TEST(CadPicking, EdgeAndBoundsPicksRetainStableInstanceIdentity)
{
    const PartId cube_part = CadIdBuilder::partId("cube-part");
    const PartId pyramid_part = CadIdBuilder::partId("pyramid-part");
    const InstanceId cube = CadIdBuilder::childInstance(
        CadIdBuilder::rootInstance(), "cube", 0, 0);
    const InstanceId pyramid = CadIdBuilder::childInstance(
        CadIdBuilder::rootInstance(), "pyramid", 0, 0);
    PartGeometryBuilder cube_geometry;
    cube_geometry.wire = makeCubeWireframe();
    PartGeometryBuilder pyramid_geometry;
    pyramid_geometry.shaded = makePyramid();
    std::unordered_map<PartId, std::shared_ptr<const PartGeometry>> parts;
    parts.emplace(cube_part, admitGeometry(std::move(cube_geometry)));
    parts.emplace(pyramid_part, admitGeometry(std::move(pyramid_geometry)));

    SbMatrix identity;
    identity.makeIdentity();
    SbMatrix translate;
    translate.setTranslate(SbVec3f(10, 0, 0));
    SbBox3f cube_bounds;
    cube_bounds.setBounds(SbVec3f(0, 0, 0), SbVec3f(1, 1, 1));
    SbBox3f pyramid_bounds;
    pyramid_bounds.setBounds(SbVec3f(10, 0, 0), SbVec3f(11, 1, 1));
    CadInstanceBVH instances;
    instances.build({
        makeEntry(cube_part, cube, cube_bounds, identity),
        makeEntry(pyramid_part, pyramid, pyramid_bounds, translate),
    });

    std::unordered_map<PartId, CadPartEdgeBVH> edges;
    const CadPickResult edge = CadPickQuery::pickEdge(
        SbLine(SbVec3f(0.5f, -0.5f, 0), SbVec3f(0.5f, 2, 0)),
        instances, parts, edges, 0.2f);
    ASSERT_TRUE(edge.valid);
    EXPECT_EQ(edge.instanceId, cube);
    EXPECT_EQ(edge.primType, CadPickResult::EDGE);

    const CadPickResult bounds = CadPickQuery::pickBounds(
        SbLine(SbVec3f(10.5f, 0.5f, -2), SbVec3f(10.5f, 0.5f, 3)), instances);
    ASSERT_TRUE(bounds.valid);
    EXPECT_EQ(bounds.instanceId, pyramid);
    EXPECT_FALSE(CadPickQuery::pickEdge(
        SbLine(SbVec3f(10.5f, 0.5f, -2), SbVec3f(10.5f, 0.5f, 3)),
        instances, parts, edges, 0.01f).valid);
}

TEST(CadPicking, TriangleBvhHandlesHitMissAndBehindRay)
{
    CadPartTriBVH bvh;
    EXPECT_FALSE(bvh.queryClosest(
        SbLine(SbVec3f(0, 0, 2), SbVec3f(0, 0, -1))).has_value());

    const std::vector<SbVec3f> positions = {
        {-1, -1, 0}, {1, -1, 0}, {0, 1, 0},
    };
    bvh.build(positions, {0, 1, 2});
    const auto hit = bvh.queryClosest(
        SbLine(SbVec3f(0, 0, 2), SbVec3f(0, 0, -1)));
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->triIndex, 0u);
    EXPECT_FALSE(bvh.queryClosest(
        SbLine(SbVec3f(10, 10, 2), SbVec3f(10, 10, -1))).has_value());
    EXPECT_FALSE(bvh.queryClosest(
        SbLine(SbVec3f(0, 0, 2), SbVec3f(0, 0, 3))).has_value());
}

TEST(CadPicking, TrianglePicksRespectProgressiveCutAndGeometryAvailability)
{
    const PartId part = CadIdBuilder::partId("adaptive-part");
    const InstanceId instance = CadIdBuilder::childInstance(
        CadIdBuilder::rootInstance(), "adaptive", 0, 0);
    TriMesh mesh;
    mesh.positions = {
        {-1, -1, 0}, {1, -1, 0}, {0, 1, 0},
        {9, -1, 0}, {11, -1, 0}, {10, 1, 0},
    };
    mesh.indices = {0, 1, 2, 3, 4, 5};
    mesh.bounds.setBounds(SbVec3f(-1, -1, 0), SbVec3f(11, 1, 0));
    mesh.progressiveCuts.resize(3);
    for (ProgressiveTriangleCut & cut : mesh.progressiveCuts) {
        cut.indexCount = 6;
        cut.positionCount = 6;
        cut.quantization = {16, 16, 0};
    }
    mesh.progressiveMinimumCut = 0;
    mesh.progressiveResidentCut = 2;
    ProgressiveTriangleCluster first;
    first.bounds.setBounds(SbVec3f(-1, -1, 0), SbVec3f(1, 1, 0));
    first.residentCut = 2;
    first.ranges.push_back({0, 3, 0});
    ProgressiveTriangleCluster second;
    second.bounds.setBounds(SbVec3f(9, -1, 0), SbVec3f(11, 1, 0));
    second.residentCut = 2;
    second.ranges.push_back({3, 3, 2});
    mesh.progressiveClusters = {first, second};

    PartGeometryBuilder geometry;
    geometry.shaded = mesh;
    std::unordered_map<PartId, std::shared_ptr<const PartGeometry>> parts;
    parts.emplace(part, admitGeometry(std::move(geometry)));
    SbMatrix identity;
    identity.makeIdentity();
    auto entry = makeEntry(part, instance, mesh.bounds, identity);
    entry.lodCut = 2;
    CadInstanceBVH instances;
    instances.build({entry});
    std::unordered_map<PartId, CadPartTriBVH> triangles;
    const SbLine ray(SbVec3f(10, 0, 5), SbVec3f(10, 0, 4));
    EXPECT_FALSE(CadPickQuery::pickTriangle(
        ray, instances, parts, triangles, 0.05f, 0).valid);
    const CadPickResult rich = CadPickQuery::pickTriangle(
        ray, instances, parts, triangles, 0.05f, 2);
    ASSERT_TRUE(rich.valid);
    EXPECT_EQ(rich.instanceId, instance);
    EXPECT_EQ(rich.primType, CadPickResult::TRIANGLE);
}

TEST(CadPicking, PointPicksApplyTransformsAndPreserveProducerIds)
{
    const PartId part = CadIdBuilder::partId("point-part");
    const InstanceId instance = CadIdBuilder::childInstance(
        CadIdBuilder::rootInstance(), "points", 0, 0);
    PointRep points;
    points.positions = {{-1, 0, 0}, {2, 0, 0}};
    points.pointIds = {17, 23};
    points.bounds.setBounds(SbVec3f(-1, 0, 0), SbVec3f(2, 0, 0));
    PartGeometryBuilder geometry;
    geometry.points = points;
    std::unordered_map<PartId, std::shared_ptr<const PartGeometry>> parts;
    parts.emplace(part, admitGeometry(std::move(geometry)));

    SbMatrix transform;
    transform.setTranslate(SbVec3f(4, 3, 0));
    SbBox3f world_bounds = points.bounds;
    world_bounds.transform(transform);
    CadInstanceBVH instances;
    instances.build({makeEntry(part, instance, world_bounds, transform)});

    const CadPickResult hit = CadPickQuery::pickPoint(
        SbLine(SbVec3f(6.02f, 3, 5), SbVec3f(6.02f, 3, 4)),
        instances, parts, 0.05f);
    ASSERT_TRUE(hit.valid);
    EXPECT_EQ(hit.instanceId, instance);
    EXPECT_EQ(hit.primType, CadPickResult::POINT);
    EXPECT_EQ(hit.primIndex0, 23u);
    EXPECT_FALSE(CadPickQuery::pickPoint(
        SbLine(SbVec3f(6.2f, 3, 5), SbVec3f(6.2f, 3, 4)),
        instances, parts, 0.05f).valid);
}

} // namespace
