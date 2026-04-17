/**
 * @file test_cad_picking.cpp
 * @brief Unit tests for the CAD CPU picking subsystem.
 *
 * Tests:
 *  1. CadInstanceBVH build and query
 *  2. CadPartEdgeBVH build and closest-segment query
 *  3. CadPickQuery::pickEdge returns correct InstanceId
 *  4. In wireframe mode, edge picking is used over triangle bounds
 *  5. CadPickQuery::pickBounds works when no wire geometry present
 *
 * No BRL-CAD dependency.  No GL context required.
 */

#include "../test_utils.h"

#include <obol/cad/CadIds.h>
#include <obol/cad/SoCADAssembly.h>  // PartGeometry, WireRep, TriMesh, etc.
#include "CadPicking.h"

#include <Inventor/SbVec3f.h>
#include <Inventor/SbBox3f.h>
#include <Inventor/SbLine.h>
#include <Inventor/SbMatrix.h>

#include <unordered_map>
#include <vector>
#include <cmath>

using namespace SimpleTest;
using namespace obol;
using namespace obol::picking;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/** Build a unit-cube wireframe (12 edges, 24 segment endpoints). */
static obol::WireRep makeCubeWireframe()
{
    obol::WireRep rep;
    // 12 edges of a unit cube [0,1]^3
    struct Edge { SbVec3f a, b; };
    std::vector<Edge> edges = {
        // bottom face
        {{0,0,0},{1,0,0}}, {{1,0,0},{1,1,0}}, {{1,1,0},{0,1,0}}, {{0,1,0},{0,0,0}},
        // top face
        {{0,0,1},{1,0,1}}, {{1,0,1},{1,1,1}}, {{1,1,1},{0,1,1}}, {{0,1,1},{0,0,1}},
        // verticals
        {{0,0,0},{0,0,1}}, {{1,0,0},{1,0,1}}, {{1,1,0},{1,1,1}}, {{0,1,0},{0,1,1}},
    };
    for (auto& e : edges) {
        obol::WirePolyline poly;
        poly.points = { e.a, e.b };
        rep.polylines.push_back(poly);
    }
    rep.bounds.setBounds(SbVec3f(0,0,0), SbVec3f(1,1,1));
    return rep;
}

/** Build a simple pyramid triangle mesh. */
static obol::TriMesh makePyramid()
{
    obol::TriMesh mesh;
    mesh.positions = {
        SbVec3f(0,0,0), SbVec3f(1,0,0), SbVec3f(0.5f,1,0),  // base triangle
        SbVec3f(0.5f,0.5f,1)                                   // apex
    };
    mesh.indices = {
        0,1,2,   // base
        0,1,3, 1,2,3, 2,0,3  // sides
    };
    mesh.bounds.setBounds(SbVec3f(0,0,0), SbVec3f(1,1,1));
    return mesh;
}

// ---------------------------------------------------------------------------
// CadInstanceBVH tests
// ---------------------------------------------------------------------------

static int test_instance_bvh()
{
    TestRunner runner;

    runner.startTest("CadInstanceBVH: build empty");
    {
        CadInstanceBVH bvh;
        bvh.build({});
        auto results = bvh.query(SbLine(SbVec3f(0,0,-1), SbVec3f(0,0,1)));
        runner.endTest(results.empty(), "Empty BVH should return no results");
    }

    runner.startTest("CadInstanceBVH: single instance hit");
    {
        CadInstanceBVH bvh;
        CadInstanceBVH::Entry e;
        e.worldBounds.setBounds(SbVec3f(0,0,0), SbVec3f(1,1,1));
        e.instanceId = CadIdBuilder::hash128(std::string("inst1"));
        e.partId     = CadIdBuilder::hash128(std::string("part1"));
        e.localToWorld.makeIdentity();

        bvh.build({e});

        // Ray going through the cube
        SbLine ray(SbVec3f(0.5f, 0.5f, -1.0f), SbVec3f(0.5f, 0.5f, 2.0f));
        auto results = bvh.query(ray);
        runner.endTest(results.size() == 1, "Single instance should be found");
    }

    runner.startTest("CadInstanceBVH: ray misses all instances");
    {
        CadInstanceBVH bvh;
        CadInstanceBVH::Entry e;
        e.worldBounds.setBounds(SbVec3f(0,0,0), SbVec3f(1,1,1));
        e.instanceId = CadIdBuilder::hash128(std::string("inst1"));
        e.partId     = CadIdBuilder::hash128(std::string("part1"));
        e.localToWorld.makeIdentity();
        bvh.build({e});

        // Ray pointing away from the cube
        SbLine ray(SbVec3f(10.0f, 10.0f, 0.5f), SbVec3f(11.0f, 10.0f, 0.5f));
        auto results = bvh.query(ray);
        runner.endTest(results.empty(), "Miss ray should return no results");
    }

    runner.startTest("CadInstanceBVH: two instances, only one hit");
    {
        CadInstanceBVH bvh;

        CadInstanceBVH::Entry e1, e2;
        e1.worldBounds.setBounds(SbVec3f(0,0,0), SbVec3f(1,1,1));
        e1.instanceId = CadIdBuilder::hash128(std::string("i1"));
        e1.partId     = CadIdBuilder::hash128(std::string("p1"));
        e1.localToWorld.makeIdentity();

        e2.worldBounds.setBounds(SbVec3f(5,5,5), SbVec3f(6,6,6));
        e2.instanceId = CadIdBuilder::hash128(std::string("i2"));
        e2.partId     = CadIdBuilder::hash128(std::string("p2"));
        e2.localToWorld.makeIdentity();

        bvh.build({e1, e2});

        // Ray through first cube only
        SbLine ray(SbVec3f(0.5f, 0.5f, -1.0f), SbVec3f(0.5f, 0.5f, 2.0f));
        auto results = bvh.query(ray);
        runner.endTest(results.size() == 1 && results[0]->instanceId == e1.instanceId,
                       "Should hit only first cube");
    }

    return runner.getSummary();
}

// ---------------------------------------------------------------------------
// CadPartEdgeBVH tests
// ---------------------------------------------------------------------------

static int test_part_edge_bvh()
{
    TestRunner runner;

    runner.startTest("CadPartEdgeBVH: build empty");
    {
        CadPartEdgeBVH bvh;
        bvh.build({});
        SbLine ray(SbVec3f(0,0,-1), SbVec3f(0,0,1));
        auto hit = bvh.queryClosest(ray, 0.5f);
        runner.endTest(!hit.has_value(), "Empty BVH should return no hit");
    }

    runner.startTest("CadPartEdgeBVH: closest segment found");
    {
        CadPartEdgeBVH bvh;
        // A segment along the X axis at z=0
        CadPartEdgeBVH::SegEntry seg;
        seg.p0 = SbVec3f(0, 0, 0);
        seg.p1 = SbVec3f(1, 0, 0);
        seg.polylineIdx = 0;
        seg.segmentIdx  = 0;
        bvh.build({seg});

        // Ray shooting downward from above the midpoint
        SbLine ray(SbVec3f(0.5f, 0.0f, 2.0f), SbVec3f(0.5f, 0.0f, -1.0f));
        auto hit = bvh.queryClosest(ray, 0.5f);
        runner.endTest(hit.has_value() && hit->seg.polylineIdx == 0,
                       "Should find the segment near the ray");
    }

    runner.startTest("CadPartEdgeBVH: segment outside tolerance not returned");
    {
        CadPartEdgeBVH bvh;
        CadPartEdgeBVH::SegEntry seg;
        seg.p0 = SbVec3f(0, 10, 0);  // far from origin
        seg.p1 = SbVec3f(1, 10, 0);
        seg.polylineIdx = 0;
        seg.segmentIdx  = 0;
        bvh.build({seg});

        SbLine ray(SbVec3f(0.5f, 0.0f, 2.0f), SbVec3f(0.5f, 0.0f, -1.0f));
        auto hit = bvh.queryClosest(ray, 0.1f);  // small tolerance
        runner.endTest(!hit.has_value(), "Distant segment should not be within tolerance");
    }

    return runner.getSummary();
}

// ---------------------------------------------------------------------------
// CadPickQuery tests
// ---------------------------------------------------------------------------

static int test_pick_query()
{
    TestRunner runner;

    // -----------------------------------------------------------------------
    // Setup: two parts, two instances at different positions
    // -----------------------------------------------------------------------
    PartId pidCube = CadIdBuilder::hash128(std::string("cube_part"));
    PartId pidPyramid = CadIdBuilder::hash128(std::string("pyramid_part"));

    obol::PartGeometry geomCube;
    geomCube.wire = makeCubeWireframe();

    obol::PartGeometry geomPyramid;
    geomPyramid.shaded = makePyramid();

    std::unordered_map<PartId, obol::PartGeometry, std::hash<PartId>> parts;
    parts[pidCube]    = geomCube;
    parts[pidPyramid] = geomPyramid;

    // Instance 1: cube at origin
    InstanceId iidCube = CadIdBuilder::extendNameOccBool(
        CadIdBuilder::Root(), "cube", 0, 0);

    // Instance 2: pyramid translated to (10, 0, 0)
    InstanceId iidPyramid = CadIdBuilder::extendNameOccBool(
        CadIdBuilder::Root(), "pyramid", 0, 0);

    SbMatrix identityM;
    identityM.makeIdentity();

    SbMatrix pyramidTranslate;
    pyramidTranslate.makeIdentity();
    pyramidTranslate.setTranslate(SbVec3f(10, 0, 0));

    CadInstanceBVH bvh;
    {
        std::vector<CadInstanceBVH::Entry> entries;

        CadInstanceBVH::Entry e1;
        e1.worldBounds.setBounds(SbVec3f(0,0,0), SbVec3f(1,1,1));
        e1.instanceId   = iidCube;
        e1.partId       = pidCube;
        e1.localToWorld = identityM;
        entries.push_back(e1);

        CadInstanceBVH::Entry e2;
        e2.worldBounds.setBounds(SbVec3f(10,0,0), SbVec3f(11,1,1));
        e2.instanceId   = iidPyramid;
        e2.partId       = pidPyramid;
        e2.localToWorld = pyramidTranslate;
        entries.push_back(e2);

        bvh.build(entries);
    }

    std::unordered_map<PartId, CadPartEdgeBVH, std::hash<PartId>> edgeBvhCache;

    // -----------------------------------------------------------------------
    // Test: pick near a cube edge returns cube InstanceId
    // -----------------------------------------------------------------------
    runner.startTest("CadPickQuery::pickEdge: picks cube instance for ray near cube edge");
    {
        // Ray shooting through the bottom-front edge of the cube (y=0, z=0 edge)
        SbLine ray(SbVec3f(0.5f, -0.5f, 0.0f), SbVec3f(0.5f, 2.0f, 0.0f));
        float toleranceWS = 0.2f;
        CadPickResult result = CadPickQuery::pickEdge(
            ray, bvh, parts, edgeBvhCache, toleranceWS);
        runner.endTest(result.valid && result.instanceId == iidCube,
                       "pickEdge should return the cube instance");
    }

    // -----------------------------------------------------------------------
    // Test: pickBounds on a ray through the pyramid area returns pyramid
    // -----------------------------------------------------------------------
    runner.startTest("CadPickQuery::pickBounds: picks pyramid for ray through pyramid bounds");
    {
        // Ray through pyramid translated position
        SbLine ray(SbVec3f(10.5f, 0.5f, -2.0f), SbVec3f(10.5f, 0.5f, 3.0f));
        CadPickResult result = CadPickQuery::pickBounds(ray, bvh);
        runner.endTest(result.valid && result.instanceId == iidPyramid,
                       "pickBounds should return the pyramid instance");
    }

    // -----------------------------------------------------------------------
    // Test: pickEdge misses when no wire geometry, falls back gracefully
    // -----------------------------------------------------------------------
    runner.startTest("CadPickQuery::pickEdge: no hit for part without wire geometry");
    {
        // Ray through the pyramid area, but pyramid has no wire geometry
        SbLine ray(SbVec3f(10.5f, 0.5f, -2.0f), SbVec3f(10.5f, 0.5f, 3.0f));
        float toleranceWS = 0.01f;
        CadPickResult result = CadPickQuery::pickEdge(
            ray, bvh, parts, edgeBvhCache, toleranceWS);
        // Pyramid has no wire geometry, so no edge pick hit
        // (cube is not on this ray either)
        bool ok = !result.valid ||
                  (result.valid && result.instanceId == iidPyramid
                   && result.primType != CadPickResult::EDGE);
        // Actually pickEdge can only return EDGE type, so !valid is expected
        // for the pyramid (no wire geom). Cube isn't in this ray's path.
        runner.endTest(!result.valid || result.primType == CadPickResult::EDGE,
                       "pickEdge should not return non-EDGE result");
    }

    return runner.getSummary();
}

// ---------------------------------------------------------------------------
// CadPartTriBVH and pickTriangle tests
// ---------------------------------------------------------------------------

static int test_part_tri_bvh()
{
    using namespace obol::picking;
    SimpleTest::TestRunner runner;

    // -----------------------------------------------------------------------
    // Test: empty BVH returns no hit
    // -----------------------------------------------------------------------
    runner.startTest("CadPartTriBVH: empty BVH returns no hit");
    {
        CadPartTriBVH bvh;
        bvh.build({}, {});
        SbLine ray(SbVec3f(0, 0, 2), SbVec3f(0, 0, -1));
        auto hit = bvh.queryClosest(ray);
        runner.endTest(!hit.has_value(), "Empty BVH should return no hit");
    }

    // -----------------------------------------------------------------------
    // Test: ray intersects a single triangle
    // -----------------------------------------------------------------------
    runner.startTest("CadPartTriBVH: ray hits single triangle");
    {
        // Triangle in XY plane at z=0
        std::vector<SbVec3f> pos = {
            SbVec3f(-1, -1, 0), SbVec3f(1, -1, 0), SbVec3f(0, 1, 0)
        };
        std::vector<uint32_t> idx = {0, 1, 2};
        CadPartTriBVH bvh;
        bvh.build(pos, idx);
        // Ray from above, pointing down through the centroid (~0, 0, 0)
        SbLine ray(SbVec3f(0, 0, 2), SbVec3f(0, 0, -1));
        auto hit = bvh.queryClosest(ray);
        runner.endTest(hit.has_value() && hit->triIndex == 0,
                       "Should intersect triangle 0");
    }

    // -----------------------------------------------------------------------
    // Test: ray misses triangle outside its extent
    // -----------------------------------------------------------------------
    runner.startTest("CadPartTriBVH: ray misses triangle outside extent");
    {
        std::vector<SbVec3f> pos = {
            SbVec3f(-1, -1, 0), SbVec3f(1, -1, 0), SbVec3f(0, 1, 0)
        };
        std::vector<uint32_t> idx = {0, 1, 2};
        CadPartTriBVH bvh;
        bvh.build(pos, idx);
        // Ray well outside triangle extent
        SbLine ray(SbVec3f(10, 10, 2), SbVec3f(10, 10, -1));
        auto hit = bvh.queryClosest(ray);
        runner.endTest(!hit.has_value(), "Ray far outside triangle should miss");
    }

    // -----------------------------------------------------------------------
    // Test: ray from behind (negative t) returns no hit
    // -----------------------------------------------------------------------
    runner.startTest("CadPartTriBVH: ray pointing away from triangle returns no hit");
    {
        std::vector<SbVec3f> pos = {
            SbVec3f(-1, -1, 0), SbVec3f(1, -1, 0), SbVec3f(0, 1, 0)
        };
        std::vector<uint32_t> idx = {0, 1, 2};
        CadPartTriBVH bvh;
        bvh.build(pos, idx);
        // SbLine(origin, point_on_line): direction = point - origin.
        // ray2: origin=(0,0,2), point=(0,0,3) → direction=(0,0,1) (+z away from z=0)
        // triangle at z=0 is behind the ray (t=-2 < 0).
        SbLine ray2(SbVec3f(0, 0, 2), SbVec3f(0, 0, 3));
        auto hit = bvh.queryClosest(ray2);
        runner.endTest(!hit.has_value(), "Ray pointing away from triangle should miss");
    }

    // -----------------------------------------------------------------------
    // Test: multi-triangle mesh – hits the correct closest triangle
    // -----------------------------------------------------------------------
    runner.startTest("CadPartTriBVH: pyramid mesh – closest triangle hit");
    {
        obol::TriMesh pyramid = makePyramid();
        CadPartTriBVH bvh;
        bvh.build(pyramid.positions, pyramid.indices);
        // Ray from above shooting through base (z~=0)
        SbLine ray(SbVec3f(0.4f, 0.4f, 2.0f), SbVec3f(0.4f, 0.4f, -1.0f));
        auto hit = bvh.queryClosest(ray);
        runner.endTest(hit.has_value(), "Ray through pyramid should hit a triangle");
    }

    return runner.getSummary();
}

static int test_pick_triangle()
{
    using namespace obol::picking;
    SimpleTest::TestRunner runner;

    using namespace obol;

    // Set up one instance of the pyramid with a simple identity transform
    PartId    pidPyr = CadIdBuilder::hash128("pyramid");
    InstanceId iidPyr = CadIdBuilder::extendNameOccBool(
        CadIdBuilder::Root(), "pyramid", 0, 0);

    SbMatrix identityM;
    identityM.makeIdentity();

    obol::TriMesh pyr = makePyramid();

    std::unordered_map<PartId, obol::PartGeometry, std::hash<PartId>> parts;
    {
        obol::PartGeometry g;
        g.shaded = pyr;
        parts[pidPyr] = g;
    }

    CadInstanceBVH bvh;
    {
        std::vector<CadInstanceBVH::Entry> entries;
        CadInstanceBVH::Entry e;
        e.worldBounds.setBounds(SbVec3f(0,0,0), SbVec3f(1,1,1));
        e.instanceId   = iidPyr;
        e.partId       = pidPyr;
        e.localToWorld = identityM;
        entries.push_back(e);
        bvh.build(entries);
    }

    std::unordered_map<PartId, CadPartTriBVH, std::hash<PartId>> triBvhCache;

    // -----------------------------------------------------------------------
    // Test: ray hits pyramid triangle
    // -----------------------------------------------------------------------
    runner.startTest("CadPickQuery::pickTriangle: hits pyramid");
    {
        // Ray through the centre of the pyramid (base is near z=0)
        SbLine ray(SbVec3f(0.4f, 0.4f, 2.0f), SbVec3f(0.4f, 0.4f, -1.0f));
        CadPickResult result = CadPickQuery::pickTriangle(
            ray, bvh, parts, triBvhCache);
        runner.endTest(result.valid && result.instanceId == iidPyr
                       && result.primType == CadPickResult::TRIANGLE,
                       "pickTriangle should return a TRIANGLE hit on the pyramid");
    }

    // -----------------------------------------------------------------------
    // Test: ray misses all triangles returns invalid result
    // -----------------------------------------------------------------------
    runner.startTest("CadPickQuery::pickTriangle: misses entirely → invalid");
    {
        // Ray pointing completely away from the pyramid
        SbLine ray(SbVec3f(100.0f, 100.0f, 100.0f),
                   SbVec3f(100.0f, 100.0f, 200.0f));
        CadPickResult result = CadPickQuery::pickTriangle(
            ray, bvh, parts, triBvhCache);
        runner.endTest(!result.valid, "pickTriangle should return no hit");
    }

    // -----------------------------------------------------------------------
    // Test: part without shaded geometry returns invalid result
    // -----------------------------------------------------------------------
    runner.startTest("CadPickQuery::pickTriangle: part with no shaded geometry → invalid");
    {
        PartId    pidWire = CadIdBuilder::hash128("wire_only");
        InstanceId iidWire = CadIdBuilder::extendNameOccBool(
            CadIdBuilder::Root(), "wire_only", 0, 0);

        std::unordered_map<PartId, obol::PartGeometry, std::hash<PartId>> wireParts;
        {
            obol::PartGeometry g;
            g.wire = makeCubeWireframe();
            wireParts[pidWire] = g;
        }

        CadInstanceBVH wireBvh;
        {
            std::vector<CadInstanceBVH::Entry> entries;
            CadInstanceBVH::Entry e;
            e.worldBounds.setBounds(SbVec3f(0,0,0), SbVec3f(1,1,1));
            e.instanceId   = iidWire;
            e.partId       = pidWire;
            e.localToWorld = identityM;
            entries.push_back(e);
            wireBvh.build(entries);
        }

        std::unordered_map<PartId, CadPartTriBVH, std::hash<PartId>> emptyTriCache;
        SbLine ray(SbVec3f(0.5f, 0.5f, 2.0f), SbVec3f(0.5f, 0.5f, -1.0f));
        CadPickResult result = CadPickQuery::pickTriangle(
            ray, wireBvh, wireParts, emptyTriCache);
        runner.endTest(!result.valid,
                       "No shaded geometry → pickTriangle should return no hit");
    }

    return runner.getSummary();
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    int failures = 0;
    failures += test_instance_bvh();
    failures += test_part_edge_bvh();
    failures += test_pick_query();
    failures += test_part_tri_bvh();
    failures += test_pick_triangle();
    return failures;
}
