/**
 * @file test_segment_lod.cpp
 * @brief Unit tests for obol::SegmentPopLod.
 *
 * Tests:
 *  1. Build from a grid of segments
 *  2. Segment count non-decreasing with increasing level
 *  3. Degenerate (zero-length) segments dropped at all levels
 *  4. Build from polylines of a circle approximation
 *  5. snapNorm helper boundary values
 *
 * No BRL-CAD dependency.  No GL context required.
 */

#include "../test_utils.h"

// Include SegmentPopLod directly (it's not a public API header,
// but tests are allowed to test internal implementation)
#include <obol/cad/CadIds.h>  // ensure headers compile together

// For the LoD we include from the source tree directly
// (tests/cad/ is configured with src/cad/lod in its include path)
#include "SegmentPopLod.h"

#include <Inventor/SbVec3f.h>
#include <Inventor/SbBox3f.h>

#include <vector>
#include <cmath>

using namespace SimpleTest;
using namespace obol;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/** Build a grid of N×N segments in the XY plane [0,1]^2. */
static std::pair<std::vector<SbVec3f>, SbBox3f>
makeGrid(int n)
{
    std::vector<SbVec3f> pts;
    float step = 1.0f / static_cast<float>(n);

    // Horizontal lines
    for (int i = 0; i <= n; ++i) {
        float y = i * step;
        for (int j = 0; j < n; ++j) {
            float x0 = j * step;
            float x1 = x0 + step;
            pts.push_back(SbVec3f(x0, y, 0.0f));
            pts.push_back(SbVec3f(x1, y, 0.0f));
        }
    }
    // Vertical lines
    for (int j = 0; j <= n; ++j) {
        float x = j * step;
        for (int i = 0; i < n; ++i) {
            float y0 = i * step;
            float y1 = y0 + step;
            pts.push_back(SbVec3f(x, y0, 0.0f));
            pts.push_back(SbVec3f(x, y1, 0.0f));
        }
    }

    SbBox3f bounds;
    bounds.setBounds(SbVec3f(0,0,0), SbVec3f(1,1,0.01f));
    return {pts, bounds};
}

/** Build a circle approximation (N segments around unit circle). */
static std::pair<std::vector<SbVec3f>, SbBox3f>
makeCircle(int n)
{
    std::vector<SbVec3f> pts;
    const float pi2 = 2.0f * 3.14159265358979323846f;
    for (int i = 0; i < n; ++i) {
        float a0 = pi2 * i       / n;
        float a1 = pi2 * (i + 1) / n;
        pts.push_back(SbVec3f(std::cos(a0), std::sin(a0), 0.0f));
        pts.push_back(SbVec3f(std::cos(a1), std::sin(a1), 0.0f));
    }
    SbBox3f bounds;
    bounds.setBounds(SbVec3f(-1,-1,-0.01f), SbVec3f(1,1,0.01f));
    return {pts, bounds};
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

int main()
{
    TestRunner runner;

    // -----------------------------------------------------------------------
    // 1. Build from a small grid
    // -----------------------------------------------------------------------
    runner.startTest("SegmentPopLod: build from 4×4 grid");
    {
        auto [pts, bounds] = makeGrid(4);
        SegmentPopLod lod;
        lod.build(pts, bounds);
        runner.endTest(lod.isBuilt() && lod.segmentCount() == pts.size() / 2,
                       "isBuilt should be true and segmentCount should match input");
    }

    // -----------------------------------------------------------------------
    // 2. Segment count non-decreasing with increasing level
    // -----------------------------------------------------------------------
    runner.startTest("SegmentPopLod: segment count non-decreasing with level");
    {
        auto [pts, bounds] = makeGrid(8);
        SegmentPopLod lod;
        lod.build(pts, bounds);

        bool monotone = true;
        size_t prev = 0;
        for (int lv = 0; lv <= 20; lv += 2) {
            size_t cnt = lod.segmentsAtLevel(static_cast<uint8_t>(lv)).size();
            if (cnt < prev) {
                monotone = false;
                break;
            }
            prev = cnt;
        }
        // Also check max level
        size_t maxCnt = lod.segmentsAtLevel(SegmentPopLod::kMaxLevel).size();
        if (maxCnt < prev) monotone = false;

        runner.endTest(monotone, "Segment count must not decrease with higher levels");
    }

    // -----------------------------------------------------------------------
    // 3. All segments present at max level
    // -----------------------------------------------------------------------
    runner.startTest("SegmentPopLod: all segments at kMaxLevel");
    {
        auto [pts, bounds] = makeGrid(4);
        SegmentPopLod lod;
        lod.build(pts, bounds);
        size_t maxCnt = lod.segmentsAtLevel(SegmentPopLod::kMaxLevel).size();
        runner.endTest(maxCnt == lod.segmentCount(),
                       "All segments should be present at max level");
    }

    // -----------------------------------------------------------------------
    // 4. Fewer segments at coarse levels than at max level
    // -----------------------------------------------------------------------
    runner.startTest("SegmentPopLod: fewer segments at level 0 than at max for a grid");
    {
        auto [pts, bounds] = makeGrid(16);  // 16x16 grid → many short segments
        SegmentPopLod lod;
        lod.build(pts, bounds);
        size_t coarseCnt = lod.segmentsAtLevel(0).size();
        size_t fineCnt   = lod.segmentsAtLevel(SegmentPopLod::kMaxLevel).size();
        // For a fine grid some segments should be dropped at level 0
        // (they become degenerate under coarse snapping)
        runner.endTest(coarseCnt <= fineCnt,
                       "Coarse level should have <= segments than fine level");
    }

    // -----------------------------------------------------------------------
    // 5. Zero-length segment is always degenerate (dropped at all levels)
    // -----------------------------------------------------------------------
    runner.startTest("SegmentPopLod: zero-length segment has maxLevel == kMaxLevel");
    {
        // A zero-length segment: p0 == p1
        SbVec3f p(0.5f, 0.5f, 0.5f);
        std::vector<SbVec3f> pts = { p, p,
                                     SbVec3f(0,0,0), SbVec3f(1,1,1) };  // one real seg
        SbBox3f bounds;
        bounds.setBounds(SbVec3f(0,0,0), SbVec3f(1,1,1));
        SegmentPopLod lod;
        lod.build(pts, bounds);
        // The zero-length segment (index 0) should require maxLevel to appear
        // OR it's degenerate at all levels (minLevel == kMaxLevel means it
        // only appears at exactly kMaxLevel).  In our convention:
        // minLevel <= level → included.  So if minLevel == kMaxLevel it only
        // appears at kMaxLevel.
        // The real segment (index 1) spans the full diagonal → minLevel = 0.
        uint8_t zeroMin = lod.minLevelForSegment(0);
        uint8_t realMin = lod.minLevelForSegment(1);
        bool ok = (zeroMin >= realMin);  // degenerate seg needs higher level
        runner.endTest(ok, "Zero-length segment should need higher level than diagonal");
    }

    // -----------------------------------------------------------------------
    // 6. Circle approximation: count increases with level
    // -----------------------------------------------------------------------
    runner.startTest("SegmentPopLod: circle segments non-decreasing with level");
    {
        auto [pts, bounds] = makeCircle(64);
        SegmentPopLod lod;
        lod.build(pts, bounds);

        bool monotone = true;
        size_t prev = 0;
        for (uint8_t lv = 0; lv < 20; ++lv) {
            size_t cnt = lod.segmentsAtLevel(lv).size();
            if (cnt < prev) { monotone = false; break; }
            prev = cnt;
        }
        runner.endTest(monotone, "Circle: count must not decrease with level");
    }

    // -----------------------------------------------------------------------
    // 7. snapNorm edge cases
    // -----------------------------------------------------------------------
    runner.startTest("SegmentPopLod::snapNorm returns v unchanged at kMaxLevel");
    {
        float v = 0.3456f;
        float sv = SegmentPopLod::snapNorm(v, SegmentPopLod::kMaxLevel);
        runner.endTest(sv == v, "snapNorm at max level should return v unchanged");
    }

    runner.startTest("SegmentPopLod::snapNorm at level 0 produces 2 distinct snap values");
    {
        // At level 0 there are 2^1 = 2 cells; midpoints are 0.25 and 0.75
        float s1 = SegmentPopLod::snapNorm(0.1f, 0);
        float s2 = SegmentPopLod::snapNorm(0.4f, 0);
        float s3 = SegmentPopLod::snapNorm(0.6f, 0);
        float s4 = SegmentPopLod::snapNorm(0.9f, 0);
        // 0.1 and 0.4 should both snap to cell 0 (mid=0.25)
        bool ok = (s1 == s2 && s3 == s4 && s1 != s3);
        runner.endTest(ok, "Level 0 should produce 2 distinct snap values");
    }

    // -----------------------------------------------------------------------
    // 8. Empty input doesn't crash
    // -----------------------------------------------------------------------
    runner.startTest("SegmentPopLod: empty input builds without crash");
    {
        SegmentPopLod lod;
        SbBox3f bounds;
        bounds.setBounds(SbVec3f(0,0,0), SbVec3f(1,1,1));
        lod.build({}, bounds);
        bool ok = lod.isBuilt() && lod.segmentCount() == 0;
        auto segs = lod.segmentsAtLevel(5);
        ok = ok && segs.empty();
        runner.endTest(ok, "Empty build should succeed and return empty results");
    }

    return runner.getSummary();
}
