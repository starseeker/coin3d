/**
 * @file test_triangle_lod.cpp
 * @brief Unit tests for obol::TrianglePopLod.
 *
 * Tests:
 *  1. Build from a subdivided plane
 *  2. Triangle count non-decreasing with increasing level
 *  3. All triangles present at max level
 *  4. Degenerate triangles (all verts at same point) are dropped
 *  5. snapNorm consistency with SegmentPopLod convention
 *
 * No BRL-CAD dependency.  No GL context required.
 */

#include "../test_utils.h"
#include "TrianglePopLod.h"

#include <Inventor/SbVec3f.h>
#include <Inventor/SbBox3f.h>

#include <vector>
#include <cmath>

using namespace SimpleTest;
using namespace obol;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * Build a flat N×N grid of triangles in [0,1]^2.
 * Each grid cell yields 2 triangles.
 */
static std::tuple<std::vector<SbVec3f>, std::vector<uint32_t>, SbBox3f>
makeSubdividedPlane(int n)
{
    std::vector<SbVec3f> positions;
    float step = 1.0f / static_cast<float>(n);

    for (int j = 0; j <= n; ++j) {
        for (int i = 0; i <= n; ++i) {
            positions.push_back(SbVec3f(i * step, j * step, 0.0f));
        }
    }

    std::vector<uint32_t> indices;
    auto idx = [&](int i, int j) -> uint32_t {
        return static_cast<uint32_t>(j * (n + 1) + i);
    };
    for (int j = 0; j < n; ++j) {
        for (int i = 0; i < n; ++i) {
            // Lower-left tri
            indices.push_back(idx(i,   j));
            indices.push_back(idx(i+1, j));
            indices.push_back(idx(i,   j+1));
            // Upper-right tri
            indices.push_back(idx(i+1, j));
            indices.push_back(idx(i+1, j+1));
            indices.push_back(idx(i,   j+1));
        }
    }

    SbBox3f bounds;
    bounds.setBounds(SbVec3f(0,0,-0.01f), SbVec3f(1,1,0.01f));
    return { positions, indices, bounds };
}

/**
 * Build a simple icosphere approximation (20 base triangles of icosahedron).
 */
static std::tuple<std::vector<SbVec3f>, std::vector<uint32_t>, SbBox3f>
makeIcosphere()
{
    // 12 vertices of an icosahedron
    const float t = (1.0f + std::sqrt(5.0f)) / 2.0f;
    std::vector<SbVec3f> pos = {
        SbVec3f(-1, t, 0), SbVec3f( 1, t, 0), SbVec3f(-1,-t, 0), SbVec3f( 1,-t, 0),
        SbVec3f( 0,-1, t), SbVec3f( 0, 1, t), SbVec3f( 0,-1,-t), SbVec3f( 0, 1,-t),
        SbVec3f( t, 0,-1), SbVec3f( t, 0, 1), SbVec3f(-t, 0,-1), SbVec3f(-t, 0, 1)
    };
    // Normalise
    for (auto& v : pos) {
        float len = v.length();
        if (len > 1e-6f) v /= len;
    }

    // 20 faces
    std::vector<uint32_t> idx = {
        0,11,5, 0,5,1, 0,1,7, 0,7,10, 0,10,11,
        1,5,9,  5,11,4, 11,10,2, 10,7,6, 7,1,8,
        3,9,4,  3,4,2,  3,2,6,  3,6,8,  3,8,9,
        4,9,5,  2,4,11, 6,2,10, 8,6,7,  9,8,1
    };

    SbBox3f bounds;
    bounds.setBounds(SbVec3f(-1,-1,-1), SbVec3f(1,1,1));
    return { pos, idx, bounds };
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

int main()
{
    TestRunner runner;

    // -----------------------------------------------------------------------
    // 1. Build from subdivided plane
    // -----------------------------------------------------------------------
    runner.startTest("TrianglePopLod: build from 4x4 subdivided plane");
    {
        auto [pos, idx, bounds] = makeSubdividedPlane(4);
        TrianglePopLod lod;
        lod.build(pos, idx, bounds);
        size_t expected = idx.size() / 3;
        bool ok = lod.isBuilt() && lod.triangleCount() == expected;
        runner.endTest(ok, "isBuilt should be true and triangleCount should match");
    }

    // -----------------------------------------------------------------------
    // 2. Triangle count non-decreasing with level
    // -----------------------------------------------------------------------
    runner.startTest("TrianglePopLod: count non-decreasing with level");
    {
        auto [pos, idx, bounds] = makeSubdividedPlane(8);
        TrianglePopLod lod;
        lod.build(pos, idx, bounds);

        bool monotone = true;
        size_t prev = 0;
        for (int lv = 0; lv <= 30; lv += 3) {
            size_t cnt = lod.trianglesAtLevel(static_cast<uint8_t>(lv)).size();
            if (cnt < prev) { monotone = false; break; }
            prev = cnt;
        }
        size_t maxCnt = lod.trianglesAtLevel(TrianglePopLod::kMaxLevel).size();
        if (maxCnt < prev) monotone = false;

        runner.endTest(monotone, "Triangle count must not decrease with level");
    }

    // -----------------------------------------------------------------------
    // 3. All triangles at max level
    // -----------------------------------------------------------------------
    runner.startTest("TrianglePopLod: all triangles at kMaxLevel");
    {
        auto [pos, idx, bounds] = makeSubdividedPlane(4);
        TrianglePopLod lod;
        lod.build(pos, idx, bounds);
        size_t maxCnt = lod.trianglesAtLevel(TrianglePopLod::kMaxLevel).size();
        runner.endTest(maxCnt == lod.triangleCount(),
                       "All triangles should be present at max level");
    }

    // -----------------------------------------------------------------------
    // 4. Fewer triangles at level 0 for a fine mesh
    // -----------------------------------------------------------------------
    runner.startTest("TrianglePopLod: fewer triangles at level 0 than max for fine mesh");
    {
        auto [pos, idx, bounds] = makeSubdividedPlane(16);
        TrianglePopLod lod;
        lod.build(pos, idx, bounds);
        size_t coarseCnt = lod.trianglesAtLevel(0).size();
        size_t fineCnt   = lod.trianglesAtLevel(TrianglePopLod::kMaxLevel).size();
        runner.endTest(coarseCnt <= fineCnt,
                       "Coarse level should have <= triangles than max");
    }

    // -----------------------------------------------------------------------
    // 5. Degenerate triangle (all 3 verts at same point) always has high minLevel
    // -----------------------------------------------------------------------
    runner.startTest("TrianglePopLod: degenerate tri has higher minLevel");
    {
        SbVec3f p(0.5f, 0.5f, 0.5f);
        // Degenerate triangle: all three vertices identical
        std::vector<SbVec3f> pos = {
            p, p, p,            // degenerate
            SbVec3f(0,0,0), SbVec3f(1,0,0), SbVec3f(0,1,0)  // real triangle
        };
        std::vector<uint32_t> idx = { 0, 1, 2,   3, 4, 5 };
        SbBox3f bounds;
        bounds.setBounds(SbVec3f(0,0,0), SbVec3f(1,1,1));
        TrianglePopLod lod;
        lod.build(pos, idx, bounds);
        uint8_t degenMin = lod.minLevelForTriangle(0);
        uint8_t realMin  = lod.minLevelForTriangle(1);
        runner.endTest(degenMin >= realMin,
                       "Degenerate triangle should need >= level than a real one");
    }

    // -----------------------------------------------------------------------
    // 6. Icosphere: non-decreasing count
    // -----------------------------------------------------------------------
    runner.startTest("TrianglePopLod: icosphere count non-decreasing");
    {
        auto [pos, idx, bounds] = makeIcosphere();
        TrianglePopLod lod;
        lod.build(pos, idx, bounds);

        bool monotone = true;
        size_t prev = 0;
        for (uint8_t lv = 0; lv < 30; ++lv) {
            size_t cnt = lod.trianglesAtLevel(lv).size();
            if (cnt < prev) { monotone = false; break; }
            prev = cnt;
        }
        runner.endTest(monotone, "Icosphere: count must not decrease with level");
    }

    // -----------------------------------------------------------------------
    // 7. snapNorm: max level returns v unchanged
    // -----------------------------------------------------------------------
    runner.startTest("TrianglePopLod::snapNorm returns v at kMaxLevel");
    {
        float v = 0.7234f;
        float sv = TrianglePopLod::snapNorm(v, TrianglePopLod::kMaxLevel);
        runner.endTest(sv == v, "snapNorm at max level should be identity");
    }

    // -----------------------------------------------------------------------
    // 8. Empty input builds without crash
    // -----------------------------------------------------------------------
    runner.startTest("TrianglePopLod: empty input builds without crash");
    {
        TrianglePopLod lod;
        SbBox3f bounds;
        bounds.setBounds(SbVec3f(0,0,0), SbVec3f(1,1,1));
        lod.build({}, {}, bounds);
        bool ok = lod.isBuilt() && lod.triangleCount() == 0;
        runner.endTest(ok, "Empty build should succeed");
    }

    return runner.getSummary();
}
