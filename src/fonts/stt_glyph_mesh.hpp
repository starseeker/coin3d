/*
  stt_glyph_mesh.hpp
  Header-only C++ helper to:
    - Extract struetype (stb_truetype) glyph outlines
    - Flatten quadratic/cubic curves into polylines
    - Triangulate filled glyphs with mapbox/earcut.hpp
    - Provide glyph metrics for layout (advance, kerning, bbox, font VMetrics)

  Requirements:
    - Include "struetype.h" before this file (https://github.com/starseeker/struetype)
    - Have mapbox/earcut.hpp available and includable
    - C++11 or newer

  Typical use:
    #include "struetype.h"
    #include "stt_glyph_mesh.hpp"

    stt_fontinfo font;
    stt_InitFont(&font, fontData, fontSize, stt_GetFontOffsetForIndex(fontData, 0));

    sttmesh::FontVMetrics v = sttmesh::get_font_vmetrics(font, pixelHeight);
    float scale = stt_ScaleForPixelHeight(&font, pixelHeight);

    sttmesh::GlyphBuildConfig cfg;
    cfg.scale  = scale;
    cfg.epsilon = 0.5f;   // curve flattening tolerance in output units (pixels if scale=ScaleForPixelHeight)
    cfg.flipY  = false;   // set true if you want Y-down screen coords

    sttmesh::GlyphMesh g = sttmesh::build_glyph_mesh(font, stt_FindGlyphIndex(&font, 'A'), cfg);

    // g.positions: 2D vertices (x,y)
    // g.indices: triangle index list (uint32_t)
    // g.outlineContours: spans for outline polylines (for stroking, if needed)
    // g.metrics.advance, g.metrics.leftSideBearing (scaled)
    // g.bbox: scaled glyph bbox (x0,y0,x1,y1)

  License: MIT OR Public Domain (Unlicense), same spirit as stb/struetype.

  Copyright (c) 2025
*/

#ifndef STT_GLYPH_MESH_HPP_INCLUDED
#define STT_GLYPH_MESH_HPP_INCLUDED

#include <vector>
#include <array>
#include <cstdint>
#include <cmath>
#include <limits>
#include <algorithm>
#include <utility>

#ifndef __INCLUDE_STRUETYPE_H__
#warning "Include struetype.h before including stt_glyph_mesh.hpp"
#endif

#include "earcut.hpp"

namespace sttmesh {

// 2D point
struct Vec2 {
  float x, y;
};

// Span describing one contour's vertex range within a flat points array
struct ContourSpan {
  int start = 0;
  int count = 0;
};

// Flattened outline: contiguous points with contour spans
struct Outline {
  std::vector<Vec2> points;
  std::vector<ContourSpan> contours;
};

// Per-glyph metrics (scaled by cfg.scale)
struct GlyphMetrics {
  float advance = 0.0f;          // horizontal advance (scaled)
  float leftSideBearing = 0.0f;  // left side bearing (scaled)
};

// Scaled glyph bounding box
struct GlyphBBox {
  float x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f;
  bool valid = false;
};

// Resulting mesh for one glyph
struct GlyphMesh {
  std::vector<Vec2> positions;          // 2D positions, z=0 in Coin3D
  std::vector<uint32_t> indices;        // triangle indices into positions
  std::vector<ContourSpan> outlineContours; // for outline rendering (polylines), in same order as original contours
  GlyphMetrics metrics;                 // advance/LSB for layout
  GlyphBBox bbox;                       // scaled bbox for culling/debug
  int glyphIndex = -1;
};

// Font vertical metrics (scaled)
struct FontVMetrics {
  float ascent = 0.0f;
  float descent = 0.0f;
  float lineGap = 0.0f;
};

// Configuration for glyph building
struct GlyphBuildConfig {
  float scale = 1.0f;   // use stt_ScaleForPixelHeight(&font, px) or mapping em->pixels
  float epsilon = 0.5f; // flattening tolerance in output units
  bool  flipY = false;  // set to true if you want Y-down output
};

// ----------- Low-level flattening from struetype shape -------------

inline static float distPointToSegmentSq(const Vec2& a, const Vec2& b, const Vec2& p) {
  float vx = b.x - a.x, vy = b.y - a.y;
  float wx = p.x - a.x, wy = p.y - a.y;
  float vv = vx*vx + vy*vy;
  if (vv <= 0.0f) {
    float dx = p.x - a.x, dy = p.y - a.y;
    return dx*dx + dy*dy;
  }
  float t = (wx*vx + wy*vy) / vv;
  float projx = a.x + t * vx;
  float projy = a.y + t * vy;
  float dx = p.x - projx, dy = p.y - projy;
  return dx*dx + dy*dy;
}

// Adaptive subdivision: quadratic Bezier p0 -> p2 with control p1
inline static void flattenQuadratic(const Vec2& p0, const Vec2& p1, const Vec2& p2,
                                    float epsilon, int depth, std::vector<Vec2>& out)
{
  float d2 = distPointToSegmentSq(p0, p2, p1);
  float eps2 = epsilon * epsilon;
  if (d2 <= eps2 || depth > 16) {
    out.push_back(p2);
    return;
  }
  Vec2 p01 { (p0.x + p1.x)*0.5f, (p0.y + p1.y)*0.5f };
  Vec2 p12 { (p1.x + p2.x)*0.5f, (p1.y + p2.y)*0.5f };
  Vec2 pm  { (p01.x + p12.x)*0.5f, (p01.y + p12.y)*0.5f };
  flattenQuadratic(p0,  p01, pm,  epsilon, depth+1, out);
  flattenQuadratic(pm,  p12, p2,  epsilon, depth+1, out);
}

// Adaptive subdivision: cubic Bezier p0 -> p3 with controls c1, c2
inline static void flattenCubic(const Vec2& p0, const Vec2& c1, const Vec2& c2, const Vec2& p3,
                                float epsilon, int depth, std::vector<Vec2>& out)
{
  float d1 = distPointToSegmentSq(p0, p3, c1);
  float d2 = distPointToSegmentSq(p0, p3, c2);
  float dmax = (d1 > d2) ? d1 : d2;
  float eps2 = epsilon * epsilon;
  if (dmax <= eps2 || depth > 16) {
    out.push_back(p3);
    return;
  }
  Vec2 p01  { (p0.x + c1.x)*0.5f, (p0.y + c1.y)*0.5f };
  Vec2 p12  { (c1.x + c2.x)*0.5f, (c1.y + c2.y)*0.5f };
  Vec2 p23  { (c2.x + p3.x)*0.5f, (c2.y + p3.y)*0.5f };
  Vec2 p012 { (p01.x + p12.x)*0.5f, (p01.y + p12.y)*0.5f };
  Vec2 p123 { (p12.x + p23.x)*0.5f, (p12.y + p23.y)*0.5f };
  Vec2 pm   { (p012.x + p123.x)*0.5f, (p012.y + p123.y)*0.5f };

  flattenCubic(p0,  p01,  p012, pm,  epsilon, depth+1, out);
  flattenCubic(pm,  p123, p23,  p3,  epsilon, depth+1, out);
}

// Extract and flatten a glyph to an Outline
inline static bool buildOutlineFromGlyph(const stt_fontinfo& font,
                                         int glyphIndex,
                                         const GlyphBuildConfig& cfg,
                                         Outline& out)
{
  out.points.clear();
  out.contours.clear();

  stt_vertex* verts = nullptr;
  int n = stt_GetGlyphShape(&font, glyphIndex, &verts);
  if (n <= 0 || !verts) {
    return false; // space or non-drawable glyph
  }

  const float ysign = cfg.flipY ? -1.0f : 1.0f;

  bool hasOpen = false;
  Vec2 pen {0.0f, 0.0f};
  std::vector<Vec2> tmp; tmp.reserve(64);

  auto push_contour = [&](const std::vector<Vec2>& contour) {
    if (contour.size() <= 1) return; // discard degenerate
    int start = (int)out.points.size();
    out.points.insert(out.points.end(), contour.begin(), contour.end());
    out.contours.push_back(ContourSpan{ start, (int)contour.size() });
  };

  for (int i = 0; i < n; i++) {
    stt_vertex v = verts[i];
    switch (v.type) {
      case STT_vmove: {
        if (hasOpen) {
          // Finish previous contour
          if (!tmp.empty()) {
            // Drop trailing duplicate if present
            if (tmp.size() >= 2) {
              const Vec2& a = tmp.front();
              const Vec2& b = tmp.back();
              if (a.x == b.x && a.y == b.y) {
                tmp.pop_back();
              }
            }
            push_contour(tmp);
            tmp.clear();
          }
        }
        pen.x = v.x * cfg.scale;
        pen.y = v.y * cfg.scale * ysign;
        tmp.push_back(pen);
        hasOpen = true;
      } break;

      case STT_vline: {
        Vec2 p1 { v.x * cfg.scale, v.y * cfg.scale * ysign };
        // avoid exact duplicate
        if (tmp.empty() || tmp.back().x != p1.x || tmp.back().y != p1.y) {
          tmp.push_back(p1);
        }
        pen = p1;
      } break;

      case STT_vcurve: {
        Vec2 c  { v.cx * cfg.scale,  v.cy * cfg.scale * ysign };
        Vec2 p1 { v.x  * cfg.scale,  v.y  * cfg.scale * ysign };
        flattenQuadratic(pen, c, p1, cfg.epsilon, 0, tmp);
        pen = p1;
      } break;

      case STT_vcubic: {
        Vec2 c1 { v.cx  * cfg.scale,  v.cy  * cfg.scale * ysign };
        Vec2 c2 { v.cx1 * cfg.scale,  v.cy1 * cfg.scale * ysign };
        Vec2 p1 { v.x   * cfg.scale,  v.y   * cfg.scale * ysign };
        flattenCubic(pen, c1, c2, p1, cfg.epsilon, 0, tmp);
        pen = p1;
      } break;

      default: break;
    }
  }

  if (hasOpen) {
    // Close last contour
    if (!tmp.empty()) {
      if (tmp.size() >= 2) {
        const Vec2& a = tmp.front();
        const Vec2& b = tmp.back();
        if (a.x == b.x && a.y == b.y) {
          tmp.pop_back();
        }
      }
      push_contour(tmp);
      tmp.clear();
    }
  }

  stt_FreeShape(&font, verts);
  return !out.contours.empty();
}

// ----------------- Utilities for grouping rings and tessellation ----------------

inline static double signedArea(const Outline& o, int contourIdx) {
  const ContourSpan& s = o.contours[contourIdx];
  if (s.count < 3) return 0.0;
  const Vec2* p = o.points.data() + s.start;
  double sum = 0.0;
  for (int i = 0; i < s.count; ++i) {
    const Vec2& a = p[i];
    const Vec2& b = p[(i+1) % s.count];
    sum += (double)a.x * (double)b.y - (double)b.x * (double)a.y;
  }
  return 0.5 * sum;
}

inline static bool pointInRing(const Outline& o, int contourIdx, const Vec2& pt) {
  const ContourSpan& s = o.contours[contourIdx];
  if (s.count < 3) return false;
  bool inside = false;
  const Vec2* v = o.points.data() + s.start;
  for (int i = 0, j = s.count - 1; i < s.count; j = i++) {
    const Vec2& a = v[i];
    const Vec2& b = v[j];
    bool cond = ((a.y > pt.y) != (b.y > pt.y));
    if (cond) {
      float xInt = (b.x - a.x) * (pt.y - a.y) / (b.y - a.y + 1e-30f) + a.x;
      if (pt.x < xInt) inside = !inside;
    }
  }
  return inside;
}

// Compute parent ring for each contour using containment. Parent is the smallest-area ring that contains the contour.
inline static void computeContainmentTree(const Outline& o, std::vector<int>& parent, std::vector<int>& depth) {
  const int N = (int)o.contours.size();
  parent.assign(N, -1);
  depth.assign(N, 0);

  // Precompute an interior sample point per contour (first vertex)
  std::vector<Vec2> sample(N);
  for (int i = 0; i < N; ++i) {
    const ContourSpan& s = o.contours[i];
    sample[i] = o.points[s.start]; // first vertex (good enough for simple non-self-intersecting contours)
  }

  // Areas for nearest parent selection
  std::vector<double> absArea(N);
  for (int i = 0; i < N; ++i) {
    absArea[i] = std::fabs(signedArea(o, i));
  }

  for (int j = 0; j < N; ++j) {
    int best = -1;
    double bestArea = std::numeric_limits<double>::infinity();
    for (int i = 0; i < N; ++i) if (i != j) {
      if (pointInRing(o, i, sample[j])) {
        if (absArea[i] > absArea[j] && absArea[i] < bestArea) {
          bestArea = absArea[i];
          best = i;
        }
      }
    }
    parent[j] = best;
  }

  // Compute depth by following parent chain
  for (int i = 0; i < N; ++i) {
    int d = 0, p = parent[i];
    // protect against cycles (shouldn't happen)
    for (int steps = 0; p != -1 && steps < N; ++steps) {
      d++; p = parent[p];
    }
    depth[i] = d;
  }
}

// A group is one outer ring with its immediate hole children (even-odd strategy).
struct RingGroup {
  int outer = -1;
  std::vector<int> holes;
};

// Partition outline into groups for earcut: each even-depth ring becomes an outer; immediate odd-depth children are holes.
// Even-depth descendants beyond +2 are "islands" and become separate outers in subsequent groups.
inline static std::vector<RingGroup> buildRingGroups(const Outline& o) {
  std::vector<RingGroup> groups;
  if (o.contours.empty()) return groups;

  std::vector<int> parent, depth;
  computeContainmentTree(o, parent, depth);
  const int N = (int)o.contours.size();

  // Map outer index to its group index
  std::vector<int> outerToGroup(N, -1);

  for (int i = 0; i < N; ++i) {
    if ((depth[i] % 2) == 0) {
      // even depth -> outer
      RingGroup g; g.outer = i;
      groups.push_back(std::move(g));
      outerToGroup[i] = (int)groups.size() - 1;
    }
  }

  // Assign immediate odd-depth children as holes to their outer
  for (int i = 0; i < N; ++i) {
    int p = parent[i];
    if (p != -1 && depth[i] == depth[p] + 1) {
      // immediate child
      if ((depth[i] % 2) == 1) {
        int gi = outerToGroup[p];
        if (gi >= 0) groups[gi].holes.push_back(i);
      }
      // else it's an island (even depth), handled as outer already
    }
  }

  // Optional: sort holes by area ascending (small to large) - not required, but can help stability
  for (auto& g : groups) {
    std::sort(g.holes.begin(), g.holes.end(), [&](int a, int b) {
      return std::fabs(signedArea(o, a)) < std::fabs(signedArea(o, b));
    });
  }

  return groups;
}

// ------------------------ Triangulation with Earcut ----------------------------

inline static GlyphMesh triangulateGlyph(const Outline& outline,
                                         const GlyphBuildConfig& /*cfg*/,
                                         const GlyphMetrics& metrics,
                                         const GlyphBBox& bbox,
                                         int glyphIndex)
{
  GlyphMesh mesh;
  mesh.metrics = metrics;
  mesh.bbox = bbox;
  mesh.glyphIndex = glyphIndex;

  if (outline.contours.empty()) {
    return mesh; // nothing to draw (e.g., space)
  }

  // Preserve outline contours for stroking/line rendering
  mesh.outlineContours = outline.contours;

  // Build groups
  std::vector<RingGroup> groups = buildRingGroups(outline);

  // For each group, build earcut polygon and accumulate vertices/indices
  using DPoint = std::array<double, 2>;
  using Ring   = std::vector<DPoint>;
  using Poly   = std::vector<Ring>;

  for (const RingGroup& g : groups) {
    if (g.outer < 0) continue;

    Poly poly;
    poly.reserve(1 + g.holes.size());

    auto make_ring = [&](int contourIdx) {
      Ring r;
      const ContourSpan& s = outline.contours[contourIdx];
      r.reserve((size_t)s.count);
      const Vec2* p = outline.points.data() + s.start;
      for (int i = 0; i < s.count; ++i) {
        r.push_back({ (double)p[i].x, (double)p[i].y });
      }
      // Drop trailing duplicate if present (shouldn't be if we cleaned earlier)
      if (r.size() >= 2 && r.front() == r.back()) r.pop_back();
      return r;
    };

    poly.push_back(make_ring(g.outer));
    for (int h : g.holes) {
      poly.push_back(make_ring(h));
    }

    // Earcut indices are local to this poly
    std::vector<uint32_t> local = mapbox::earcut<uint32_t>(poly);

    // Append vertices to global mesh.positions in same order as in 'poly'
    uint32_t base = (uint32_t)mesh.positions.size();
    // Outer ring
    {
      const ContourSpan& s = outline.contours[g.outer];
      const Vec2* p = outline.points.data() + s.start;
      mesh.positions.insert(mesh.positions.end(), p, p + s.count);
    }
    // Holes
    for (int h : g.holes) {
      const ContourSpan& s = outline.contours[h];
      const Vec2* p = outline.points.data() + s.start;
      mesh.positions.insert(mesh.positions.end(), p, p + s.count);
    }

    // Offset local indices and append
    mesh.indices.reserve(mesh.indices.size() + local.size());
    for (uint32_t idx : local) {
      mesh.indices.push_back(base + idx);
    }
  }

  return mesh;
}

// ---------------------- Public API entry-points -------------------------------

inline static GlyphMetrics get_glyph_metrics(const stt_fontinfo& font, int glyphIndex, float scale) {
  GlyphMetrics gm;
  int adv = 0, lsb = 0;
  stt_GetGlyphHMetrics(&font, glyphIndex, &adv, &lsb);
  gm.advance = (float)adv * scale;
  gm.leftSideBearing = (float)lsb * scale;
  return gm;
}

inline static int get_glyph_kerning(const stt_fontinfo& font, int glyphA, int glyphB, float scale) {
  // value is in font units; scale to output units
  int k = stt_GetGlyphKernAdvance(&font, glyphA, glyphB);
  return (int)std::lround((double)k * (double)scale);
}

inline static FontVMetrics get_font_vmetrics(const stt_fontinfo& font, float pixelHeight) {
  FontVMetrics v{};
  int ia=0, id=0, il=0;
  stt_GetFontVMetrics(&font, &ia, &id, &il);
  float scale = stt_ScaleForPixelHeight(&font, pixelHeight);
  v.ascent  = (float)ia * scale;
  v.descent = (float)id * scale;
  v.lineGap = (float)il * scale;
  return v;
}

inline static GlyphBBox get_glyph_bbox(const stt_fontinfo& font, int glyphIndex, const GlyphBuildConfig& cfg) {
  GlyphBBox b{};
  int x0=0,y0=0,x1=0,y1=0;
  if (stt_GetGlyphBox(&font, glyphIndex, &x0, &y0, &x1, &y1)) {
    b.x0 = (float)x0 * cfg.scale;
    b.x1 = (float)x1 * cfg.scale;
    if (cfg.flipY) {
      // Flip Y about baseline: TrueType Y-up -> Y-down
      b.y0 = (float)y0 * cfg.scale * -1.0f;
      b.y1 = (float)y1 * cfg.scale * -1.0f;
      if (b.y0 > b.y1) std::swap(b.y0, b.y1);
    } else {
      b.y0 = (float)y0 * cfg.scale;
      b.y1 = (float)y1 * cfg.scale;
    }
    b.valid = true;
  } else {
    b.valid = false;
  }
  return b;
}

// Build mesh for a glyph index
inline static GlyphMesh build_glyph_mesh(const stt_fontinfo& font,
                                         int glyphIndex,
                                         const GlyphBuildConfig& cfg)
{
  GlyphMesh gm;
  gm.glyphIndex = glyphIndex;

  // Metrics and bbox first (useful even for null outlines like space)
  gm.metrics = get_glyph_metrics(font, glyphIndex, cfg.scale);
  gm.bbox    = get_glyph_bbox(font, glyphIndex, cfg);

  Outline outline;
  if (!buildOutlineFromGlyph(font, glyphIndex, cfg, outline)) {
    // No outline; return empty geometry but with metrics populated
    return gm;
  }

  // Save outline contours for stroking if needed
  gm.outlineContours = outline.contours;

  // Triangulate
  GlyphMesh tess = triangulateGlyph(outline, cfg, gm.metrics, gm.bbox, glyphIndex);
  return tess;
}

// Convenience: build by codepoint
inline static GlyphMesh build_codepoint_mesh(const stt_fontinfo& font,
                                             int codepoint,
                                             const GlyphBuildConfig& cfg)
{
  int glyph = stt_FindGlyphIndex(&font, codepoint);
  return build_glyph_mesh(font, glyph, cfg);
}

} // namespace sttmesh

#endif // STT_GLYPH_MESH_HPP_INCLUDED
