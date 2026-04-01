/*
 * cad_large_assembly_viewer.cpp  –  Interactive FLTK demo: Large-Scale CAD Assembly
 *
 * Overview
 * ────────
 * This application demonstrates SoCADAssembly rendering a very large
 * hierarchical CAD model – up to 10 000 instances of complex triangle-mesh
 * parts – interactively in real time using POP Level-of-Detail (LoD).
 *
 * It is designed to answer the question: "Is what we have in place with
 * SoCADAssembly adequate for handling very large CAD systems?"
 *
 * Assembly structure
 * ──────────────────
 * The scene is a parametric, procedurally-generated aerospace-style assembly:
 *
 *   • S × S × S "cells" arranged in a 3-D grid (S chosen from target count)
 *   • Each cell contains PARTS_PER_CELL instances drawn from five part types:
 *       A  "precision"  – icosphere sub-divided 3×  (1 280 triangles)
 *       B  "standard"   – icosphere sub-divided 2×  (  320 triangles)
 *       C  "detail"     – icosphere sub-divided 1×  (   80 triangles)
 *       D  "fastener"   – icosphere sub-divided 0×  (   20 triangles)
 *       E  "panel"      – box mesh               (   12 triangles + 12 edges)
 *
 * Part types A–D share a common mesh (one PartId each); only the per-instance
 * transform and color override differ.  This mirrors real CAD assemblies where
 * one part model can appear thousands of times with different placements.
 *
 * LoD benefit
 * ───────────
 * With lodEnabled=TRUE, SoCADAssembly applies POP quantisation:
 *   • Instances close to the camera show full-detail meshes.
 *   • Instances farther away show coarser representations.
 *   • The total GL draw-call work decreases substantially at large scales.
 * The toolbar shows render time so the LoD benefit is visible directly.
 *
 * Controls
 * ────────
 *   Left-drag   → orbit camera
 *   Right-drag  → dolly (zoom in/out)
 *   Scroll      → zoom
 *   [LoD]       → toggle POP Level-of-Detail on/off
 *   [Mode]      → cycle draw mode (Wireframe → Shaded → Shaded+Edges)
 *   [Proj]      → toggle projection (Perspective ↔ Orthographic)
 *   [Scale]     → rebuild the assembly at a different instance count
 *                 (500 / 2 000 / 5 000 / 10 000)
 *   [Reset View]→ restore default camera
 *   [Save RGB…] → write the current frame to a .rgb file
 *
 * Building
 * ────────
 *   cmake … -DOBOL_BUILD_CAD_LARGE_VIEWER=ON
 *   Requires FLTK and OBOL_USE_SYSTEM_GL or OBOL_USE_SWRAST.
 */

#include <Inventor/SoDB.h>
#include <Inventor/nodekits/SoNodeKit.h>
#include <Inventor/SoInteraction.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbRotation.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbColor.h>
#include <Inventor/SbMatrix.h>

#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoLightModel.h>
#include <Inventor/nodes/SoBaseColor.h>

#include <Inventor/actions/SoGetBoundingBoxAction.h>

#include <obol/cad/SoCADAssembly.h>
#include <obol/cad/CadIds.h>

/* BasicFLTKContextManager – FLTK GL context for off-screen rendering */
#include "fltk_context_manager.h"

/* FLTK */
#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Value_Slider.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/fl_draw.H>
#include <FL/fl_ask.H>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>
#include <vector>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <chrono>

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

/* =========================================================================
 * Configuration
 * ========================================================================= */

/** Number of instances placed in each grid cell.
 *  4 precision + 4 standard + 4 detail + 6 fasteners + 2 panels = 20. */
static constexpr int PARTS_PER_CELL = 20;

/** Animation period (unused here; kept for consistency with other viewers). */
static constexpr double FRAME_INTERVAL = 1.0 / 30.0;

/** World-space spacing between grid cell origins. */
static constexpr float CELL_SPACING = 8.0f;

/* =========================================================================
 * Geometry helpers
 * ========================================================================= */

/** Simple triangle-mesh description. */
struct MeshData {
    std::vector<SbVec3f>  positions;
    std::vector<SbVec3f>  normals;    ///< per-vertex; may be empty
    std::vector<uint32_t> indices;    ///< triangle list
    SbBox3f               bounds;
};

/** Wire-edge description (12 edges of a unit cube). */
static const float BOX_EDGES[12][2][3] = {
    // Bottom face
    {{0,0,0},{1,0,0}}, {{1,0,0},{1,0,1}}, {{1,0,1},{0,0,1}}, {{0,0,1},{0,0,0}},
    // Top face
    {{0,1,0},{1,1,0}}, {{1,1,0},{1,1,1}}, {{1,1,1},{0,1,1}}, {{0,1,1},{0,1,0}},
    // Vertical pillars
    {{0,0,0},{0,1,0}}, {{1,0,0},{1,1,0}}, {{1,0,1},{1,1,1}}, {{0,0,1},{0,1,1}},
};

static void normaliseVec(SbVec3f& v)
{
    float len = v.length();
    if (len > 1e-7f) v /= len;
}

static SbVec3f midpointOnSphere(const SbVec3f& a, const SbVec3f& b)
{
    SbVec3f m = (a + b) * 0.5f;
    normaliseVec(m);
    return m;
}

/**
 * Build an icosphere with the given number of sub-divisions.
 * Level 0 = icosahedron (20 triangles); each level multiplies by 4.
 */
static MeshData buildIcosphere(int subdivisions)
{
    const float t = (1.0f + std::sqrt(5.0f)) / 2.0f;
    std::vector<SbVec3f> pts = {
        SbVec3f(-1, t, 0), SbVec3f( 1, t, 0), SbVec3f(-1,-t, 0), SbVec3f( 1,-t, 0),
        SbVec3f( 0,-1, t), SbVec3f( 0, 1, t), SbVec3f( 0,-1,-t), SbVec3f( 0, 1,-t),
        SbVec3f( t, 0,-1), SbVec3f( t, 0, 1), SbVec3f(-t, 0,-1), SbVec3f(-t, 0, 1)
    };
    for (auto& v : pts) normaliseVec(v);

    std::vector<uint32_t> idx = {
        0,11,5, 0,5,1, 0,1,7, 0,7,10, 0,10,11,
        1,5,9,  5,11,4, 11,10,2, 10,7,6, 7,1,8,
        3,9,4,  3,4,2,  3,2,6,  3,6,8,  3,8,9,
        4,9,5,  2,4,11, 6,2,10, 8,6,7,  9,8,1
    };

    for (int s = 0; s < subdivisions; ++s) {
        std::vector<uint32_t> newIdx;
        newIdx.reserve(idx.size() * 4);
        std::unordered_map<uint64_t, uint32_t> cache;

        auto getMid = [&](uint32_t a, uint32_t b) -> uint32_t {
            uint64_t key = (static_cast<uint64_t>(std::min(a,b)) << 32) |
                           static_cast<uint64_t>(std::max(a,b));
            auto it = cache.find(key);
            if (it != cache.end()) return it->second;
            SbVec3f m = midpointOnSphere(pts[a], pts[b]);
            uint32_t mid = static_cast<uint32_t>(pts.size());
            pts.push_back(m);
            cache[key] = mid;
            return mid;
        };

        for (size_t i = 0; i < idx.size(); i += 3) {
            uint32_t v0 = idx[i], v1 = idx[i+1], v2 = idx[i+2];
            uint32_t a = getMid(v0, v1);
            uint32_t b = getMid(v1, v2);
            uint32_t c = getMid(v2, v0);
            newIdx.insert(newIdx.end(), {v0,a,c, v1,b,a, v2,c,b, a,b,c});
        }
        idx = std::move(newIdx);
    }

    MeshData mesh;
    mesh.positions = pts;
    mesh.normals   = pts;   // sphere normals == positions (unit sphere)
    mesh.indices   = idx;
    mesh.bounds.setBounds(SbVec3f(-1,-1,-1), SbVec3f(1,1,1));
    return mesh;
}

/** Build a flat box mesh spanning [-hw,hw] × [-hh,hh] × [-hd,hd]. */
static MeshData buildBoxMesh(float hw, float hh, float hd)
{
    MeshData m;
    // 8 vertices
    m.positions = {
        {-hw,-hh,-hd}, { hw,-hh,-hd}, { hw, hh,-hd}, {-hw, hh,-hd},
        {-hw,-hh, hd}, { hw,-hh, hd}, { hw, hh, hd}, {-hw, hh, hd},
    };
    // Per-face normals assigned per triangle (flat shading from per-face)
    // 6 faces × 2 triangles = 12 triangles
    m.indices = {
        0,1,2, 0,2,3,   // -Z face
        4,6,5, 4,7,6,   // +Z face
        0,4,5, 0,5,1,   // -Y face
        3,2,6, 3,6,7,   // +Y face
        0,3,7, 0,7,4,   // -X face
        1,5,6, 1,6,2,   // +X face
    };
    // Simple vertex normals (average of adjacent faces — good enough for LoD)
    m.normals.assign(8, SbVec3f(0,0,0));
    static const SbVec3f faceNormals[6] = {
        {0,0,-1},{0,0,1},{0,-1,0},{0,1,0},{-1,0,0},{1,0,0}
    };
    for (int f = 0; f < 6; ++f) {
        int base = f * 6;
        for (int k = 0; k < 6; ++k)
            m.normals[m.indices[base+k]] += faceNormals[f];
    }
    for (auto& n : m.normals) normaliseVec(n);
    m.bounds.setBounds(SbVec3f(-hw,-hh,-hd), SbVec3f(hw,hh,hd));
    return m;
}

/** Build the 12 edge polylines of a unit box [-hw,hw]^3. */
static obol::WireRep buildBoxWire(float hw)
{
    obol::WireRep wire;
    for (int e = 0; e < 12; ++e) {
        obol::WirePolyline poly;
        const float* p0 = BOX_EDGES[e][0];
        const float* p1 = BOX_EDGES[e][1];
        // Map [0,1] to [-hw, hw]
        auto s = [&](const float* p) {
            return SbVec3f((p[0] - 0.5f) * 2.0f * hw,
                           (p[1] - 0.5f) * 2.0f * hw,
                           (p[2] - 0.5f) * 2.0f * hw);
        };
        poly.points.push_back(s(p0));
        poly.points.push_back(s(p1));
        poly.edgeId = static_cast<uint32_t>(e + 1);
        wire.polylines.push_back(std::move(poly));
    }
    wire.bounds.setBounds(SbVec3f(-hw,-hw,-hw), SbVec3f(hw,hw,hw));
    return wire;
}

/* =========================================================================
 * Assembly state
 * ========================================================================= */

struct AssemblyState {
    SoSeparator* root         = nullptr;
    SoCamera*    camera       = nullptr;
    SoCADAssembly*       assembly     = nullptr;
    int                  numInstances = 0;
    int                  numParts     = 0;
    int                  gridSize     = 0;
};

static AssemblyState s_asm;

/* =========================================================================
 * Part IDs (one per geometry type)
 * ========================================================================= */

static obol::PartId s_pidPrecision;   // icosphere sub=3  (1280 tris)
static obol::PartId s_pidStandard;    // icosphere sub=2  ( 320 tris)
static obol::PartId s_pidDetail;      // icosphere sub=1  (  80 tris)
static obol::PartId s_pidFastener;    // icosphere sub=0  (  20 tris)
static obol::PartId s_pidPanel;       // box mesh          (  12 tris + 12 edges)

/** Triangle counts per part type (for stats display). */
static constexpr size_t TRIS_PRECISION = 1280;
static constexpr size_t TRIS_STANDARD  =  320;
static constexpr size_t TRIS_DETAIL    =   80;
static constexpr size_t TRIS_FASTENER  =   20;
static constexpr size_t TRIS_PANEL     =   12;

/** Breakdown per cell:  4 + 4 + 4 + 6 + 2 = 20 */
static constexpr int CELL_PRECISION = 4;
static constexpr int CELL_STANDARD  = 4;
static constexpr int CELL_DETAIL    = 4;
static constexpr int CELL_FASTENER  = 6;
static constexpr int CELL_PANELS    = 2;

/* =========================================================================
 * Build large assembly
 * ========================================================================= */

/**
 * Populate the given SoCADAssembly with parts and instances that simulate
 * a large CAD assembly.
 *
 * The assembly is a S×S×S grid of "cells".  S is chosen so that
 *   S^3 × PARTS_PER_CELL ≈ targetInstances.
 *
 * @param asm_     SoCADAssembly to populate (already added to scene root).
 * @param target   Approximate desired instance count.
 * @return Actual number of instances created.
 */
static int populateAssembly(SoCADAssembly* asm_, int target)
{
    // Compute grid size
    int S = std::max(1, static_cast<int>(
        std::ceil(std::cbrt(static_cast<double>(target) / PARTS_PER_CELL))));

    // Register part geometries
    {
        obol::PartGeometry geom;
        MeshData ico3 = buildIcosphere(3);
        obol::TriMesh tm;
        tm.positions = ico3.positions;
        tm.normals   = ico3.normals;
        tm.indices   = ico3.indices;
        tm.bounds    = ico3.bounds;
        geom.shaded  = tm;
        s_pidPrecision = obol::CadIdBuilder::hash128("precision_part");
        asm_->upsertPart(s_pidPrecision, geom);
    }
    {
        obol::PartGeometry geom;
        MeshData ico2 = buildIcosphere(2);
        obol::TriMesh tm;
        tm.positions = ico2.positions;
        tm.normals   = ico2.normals;
        tm.indices   = ico2.indices;
        tm.bounds    = ico2.bounds;
        geom.shaded  = tm;
        s_pidStandard = obol::CadIdBuilder::hash128("standard_part");
        asm_->upsertPart(s_pidStandard, geom);
    }
    {
        obol::PartGeometry geom;
        MeshData ico1 = buildIcosphere(1);
        obol::TriMesh tm;
        tm.positions = ico1.positions;
        tm.normals   = ico1.normals;
        tm.indices   = ico1.indices;
        tm.bounds    = ico1.bounds;
        geom.shaded  = tm;
        s_pidDetail = obol::CadIdBuilder::hash128("detail_part");
        asm_->upsertPart(s_pidDetail, geom);
    }
    {
        obol::PartGeometry geom;
        MeshData ico0 = buildIcosphere(0);
        obol::TriMesh tm;
        tm.positions = ico0.positions;
        tm.normals   = ico0.normals;
        tm.indices   = ico0.indices;
        tm.bounds    = ico0.bounds;
        geom.shaded  = tm;
        s_pidFastener = obol::CadIdBuilder::hash128("fastener_part");
        asm_->upsertPart(s_pidFastener, geom);
    }
    {
        obol::PartGeometry geom;
        // Panel: wide flat box representing a structural skin panel
        MeshData box = buildBoxMesh(1.8f, 0.15f, 1.8f);
        obol::TriMesh tm;
        tm.positions = box.positions;
        tm.normals   = box.normals;
        tm.indices   = box.indices;
        tm.bounds    = box.bounds;
        geom.shaded  = tm;
        // Feature-edge wire overlay for the panel
        geom.wire    = buildBoxWire(1.8f);
        s_pidPanel   = obol::CadIdBuilder::hash128("panel_part");
        asm_->upsertPart(s_pidPanel, geom);
    }

    // Color palette for visual variety
    static const SbColor4f PALETTE[] = {
        {0.70f, 0.82f, 0.95f, 1.f},  // steel blue
        {0.60f, 0.85f, 0.60f, 1.f},  // light green
        {0.95f, 0.80f, 0.40f, 1.f},  // warm gold
        {0.85f, 0.55f, 0.55f, 1.f},  // copper rose
        {0.75f, 0.65f, 0.90f, 1.f},  // lavender
        {0.50f, 0.90f, 0.85f, 1.f},  // cyan
        {0.95f, 0.70f, 0.30f, 1.f},  // orange
        {0.80f, 0.80f, 0.80f, 1.f},  // silver
    };
    static constexpr int NPALETTE = static_cast<int>(sizeof(PALETTE)/sizeof(PALETTE[0]));

    obol::InstanceId rootId = obol::CadIdBuilder::Root();
    int instIdx = 0;

    // Batch update for efficiency
    asm_->beginUpdate();

    for (int ix = 0; ix < S; ++ix) {
        for (int iy = 0; iy < S; ++iy) {
            for (int iz = 0; iz < S; ++iz) {
                // Cell origin in world space
                SbVec3f cellOrigin(
                    ix * CELL_SPACING,
                    iy * CELL_SPACING,
                    iz * CELL_SPACING);

                // Color for this cell (deterministic from cell index)
                int colorIdx = (ix + iy * 3 + iz * 7) % NPALETTE;

                // Helper lambda: insert one instance at local offset from cell
                auto addInst = [&](obol::PartId pid,
                                   SbVec3f localPos,
                                   float scale,
                                   const SbColor4f& color) {
                    char name[64];
                    snprintf(name, sizeof(name), "inst_%d", instIdx);
                    obol::InstanceRecord rec;
                    rec.part            = pid;
                    rec.parent          = rootId;
                    rec.childName       = name;
                    rec.occurrenceIndex = static_cast<uint32_t>(instIdx);
                    rec.boolOp          = 0;

                    SbMatrix xf;
                    xf.makeIdentity();
                    xf.setTranslate(cellOrigin + localPos);
                    // Apply uniform scale via matrix multiplication
                    SbMatrix scaleM;
                    scaleM.makeIdentity();
                    scaleM[0][0] = scale;
                    scaleM[1][1] = scale;
                    scaleM[2][2] = scale;
                    rec.localToRoot = scaleM * xf;

                    rec.style.hasColorOverride = true;
                    rec.style.color            = color;
                    asm_->upsertInstanceAuto(rec);
                    ++instIdx;
                };

                const SbColor4f& col = PALETTE[colorIdx];

                // 4 precision parts: corners of a sub-unit
                float pc = CELL_SPACING * 0.25f;
                addInst(s_pidPrecision, {-pc, 0.f, -pc}, 0.65f, col);
                addInst(s_pidPrecision, { pc, 0.f, -pc}, 0.65f, PALETTE[(colorIdx+1)%NPALETTE]);
                addInst(s_pidPrecision, {-pc, 0.f,  pc}, 0.65f, PALETTE[(colorIdx+2)%NPALETTE]);
                addInst(s_pidPrecision, { pc, 0.f,  pc}, 0.65f, PALETTE[(colorIdx+3)%NPALETTE]);

                // 4 standard parts: offset in Y
                float sc = CELL_SPACING * 0.20f;
                addInst(s_pidStandard, {-sc, sc, -sc}, 0.50f, PALETTE[(colorIdx+4)%NPALETTE]);
                addInst(s_pidStandard, { sc, sc, -sc}, 0.50f, PALETTE[(colorIdx+5)%NPALETTE]);
                addInst(s_pidStandard, {-sc, sc,  sc}, 0.50f, PALETTE[(colorIdx+6)%NPALETTE]);
                addInst(s_pidStandard, { sc, sc,  sc}, 0.50f, PALETTE[(colorIdx+7)%NPALETTE]);

                // 4 detail parts: midpoints
                addInst(s_pidDetail, {0.f, -sc, 0.f}, 0.38f, col);
                addInst(s_pidDetail, {-sc, -sc, 0.f}, 0.38f, PALETTE[(colorIdx+2)%NPALETTE]);
                addInst(s_pidDetail, { sc, -sc, 0.f}, 0.38f, PALETTE[(colorIdx+4)%NPALETTE]);
                addInst(s_pidDetail, {0.f, -sc, sc},  0.38f, PALETTE[(colorIdx+6)%NPALETTE]);

                // 6 fasteners: small items scattered around cell
                float fc = CELL_SPACING * 0.30f;
                static const SbVec3f fpos[6] = {
                    {-fc, 0.f, 0.f}, {fc, 0.f, 0.f},
                    {0.f, fc, 0.f},  {0.f, -fc, 0.f},
                    {0.f, 0.f, -fc}, {0.f, 0.f, fc}
                };
                SbColor4f fastCol(0.90f, 0.88f, 0.75f, 1.f); // brass-ish
                for (int k = 0; k < CELL_FASTENER; ++k)
                    addInst(s_pidFastener, fpos[k], 0.20f, fastCol);

                // 2 panels: one horizontal, one vertical
                SbColor4f panelCol(0.55f, 0.65f, 0.80f, 0.95f); // blue-grey panels
                addInst(s_pidPanel, {0.f, -sc * 1.5f, 0.f}, 1.0f, panelCol);
                addInst(s_pidPanel, {0.f,  sc * 1.5f, 0.f}, 1.0f,
                        PALETTE[(colorIdx+3)%NPALETTE]);
            }
        }
    }

    asm_->endUpdate();

    return instIdx;
}

/** Build the full large-assembly scene graph and return the state. */
static AssemblyState buildLargeScene(int targetInstances, int width, int height)
{
    AssemblyState st;

    SoSeparator* root = new SoSeparator;
    root->ref();
    st.root = root;

    /* ---- Camera ---- */
    SoPerspectiveCamera* cam = new SoPerspectiveCamera;
    root->addChild(cam);
    st.camera = cam;

    /* ---- Lights: two directional for good depth perception ---- */
    {
        SoDirectionalLight* lt = new SoDirectionalLight;
        lt->direction.setValue(-1.0f, -1.5f, -1.0f);
        lt->color.setValue(1.0f, 1.0f, 0.95f);
        lt->intensity.setValue(0.80f);
        root->addChild(lt);
    }
    {
        SoDirectionalLight* lt = new SoDirectionalLight;
        lt->direction.setValue(1.0f, 0.5f, 0.8f);
        lt->color.setValue(0.35f, 0.45f, 0.75f);
        lt->intensity.setValue(0.30f);
        root->addChild(lt);
    }

    /* ---- SoCADAssembly ---- */
    SoCADAssembly* asm_ = new SoCADAssembly;
    asm_->drawMode.setValue(SoCADAssembly::SHADED);
    asm_->lodEnabled.setValue(TRUE);
    root->addChild(asm_);
    st.assembly = asm_;

    /* ---- Populate ---- */
    st.numInstances = populateAssembly(asm_, targetInstances);
    st.numParts     = static_cast<int>(asm_->partCount());
    st.gridSize     = std::max(1, static_cast<int>(
        std::ceil(std::cbrt(static_cast<double>(targetInstances) / PARTS_PER_CELL))));

    /* ---- Frame the scene ---- */
    SbViewportRegion vp(static_cast<short>(width), static_cast<short>(height));
    cam->viewAll(root, vp);
    SbVec3f pos = cam->position.getValue();
    cam->position.setValue(pos * 1.4f);
    float dist = cam->focalDistance.getValue() * 1.4f;
    cam->focalDistance.setValue(dist);
    cam->nearDistance.setValue(dist * 0.001f);
    cam->farDistance.setValue(dist * 2000.0f);

    return st;
}

/* =========================================================================
 * Off-screen renderer singleton
 * ========================================================================= */

static BasicFLTKContextManager* s_ctx_mgr  = nullptr;
static SoOffscreenRenderer*     s_renderer = nullptr;

static SoOffscreenRenderer* getRenderer(int w, int h)
{
    if (!s_renderer) {
        SbViewportRegion vp(w, h);
        s_renderer = new SoOffscreenRenderer(s_ctx_mgr, vp);
    }
    return s_renderer;
}

/* =========================================================================
 * RenderPanel  –  FLTK box that renders the Obol scene via off-screen path
 * ========================================================================= */

class RenderPanel : public Fl_Box {
public:
    explicit RenderPanel(int X, int Y, int W, int H)
        : Fl_Box(X, Y, W, H, "")
        , fltk_img_(nullptr)
        , dragging_(false), drag_btn_(0), last_x_(0), last_y_(0)
        , last_render_ms_(0.0)
    {
        box(FL_FLAT_BOX);
        color(FL_BLACK);
    }

    ~RenderPanel() { delete fltk_img_; }

    double lastRenderMs() const { return last_render_ms_; }

    void updateSceneCenter()
    {
        if (!s_asm.root) return;
        SoGetBoundingBoxAction bba(SbViewportRegion(w(), h()));
        bba.apply(s_asm.root);
        SbBox3f bbox = bba.getBoundingBox();
        if (!bbox.isEmpty())
            scene_center_ = bbox.getCenter();
        else
            scene_center_.setValue(0.f, 0.f, 0.f);
    }

    void refreshRender()
    {
        if (!s_asm.root) { redraw(); return; }

        int pw = std::max(w(), 1);
        int ph = std::max(h(), 1);

        SoOffscreenRenderer* r = getRenderer(pw, ph);
        SbViewportRegion vp(pw, ph);
        r->setViewportRegion(vp);
        r->setComponents(SoOffscreenRenderer::RGB_TRANSPARENCY);
        r->setBackgroundColor(SbColor(0.08f, 0.09f, 0.14f));
        r->clearBackgroundGradient();

        using Clock = std::chrono::high_resolution_clock;
        using Ms    = std::chrono::duration<double, std::milli>;
        auto t0 = Clock::now();
        bool ok = (r->render(s_asm.root) == TRUE);
        last_render_ms_ = Ms(Clock::now() - t0).count();

        if (!ok) {
            status_ = "Render failed (no GL context)";
            redraw();
            return;
        }

        const unsigned char* src = r->getBuffer();
        if (!src) { redraw(); return; }

        /* SoOffscreenRenderer fills bottom-to-top RGBA; FLTK wants top-down RGB. */
        display_buf_.resize(static_cast<size_t>(pw) * ph * 3);
        for (int row = 0; row < ph; ++row) {
            const uint8_t* s = src + static_cast<size_t>(ph - 1 - row) * pw * 4;
            uint8_t*       d = display_buf_.data() + static_cast<size_t>(row) * pw * 3;
            for (int col = 0; col < pw; ++col) {
                d[0] = s[0]; d[1] = s[1]; d[2] = s[2];
                s += 4; d += 3;
            }
        }

        delete fltk_img_;
        fltk_img_ = new Fl_RGB_Image(display_buf_.data(), pw, ph, 3);
        status_.clear();
        redraw();
    }

    void saveRGB()
    {
        if (!s_asm.root) { fl_message("No scene loaded."); return; }
        const char* path = fl_file_chooser("Save snapshot", "*.rgb", "cad_large.rgb");
        if (!path) return;
        int pw = std::max(w(), 1), ph = std::max(h(), 1);
        SoOffscreenRenderer* r = getRenderer(pw, ph);
        SbViewportRegion vp(pw, ph);
        r->setViewportRegion(vp);
        r->setComponents(SoOffscreenRenderer::RGB_TRANSPARENCY);
        r->setBackgroundColor(SbColor(0.08f, 0.09f, 0.14f));
        r->clearBackgroundGradient();
        if (r->render(s_asm.root) && r->writeToRGB(path))
            fl_message("Saved: %s", path);
        else
            fl_message("Save failed.");
    }

    void draw() override
    {
        fl_rectf(x(), y(), w(), h(), FL_BLACK);
        if (fltk_img_)
            fltk_img_->draw(x(), y(), w(), h(), 0, 0);
        else {
            fl_color(FL_WHITE); fl_font(FL_HELVETICA, 14);
            fl_draw("Building assembly…", x() + 8, y() + h()/2);
        }
        if (!status_.empty()) {
            fl_color(fl_rgb_color(255, 80, 80)); fl_font(FL_HELVETICA, 12);
            fl_draw(status_.c_str(), x() + 6, y() + h() - 6);
        }
    }

    int handle(int event) override
    {
        if (!s_asm.camera) return Fl_Box::handle(event);
        const int ev_x = Fl::event_x() - x();
        const int ev_y = Fl::event_y() - y();

        switch (event) {
        case FL_PUSH:
            take_focus();
            dragging_  = true;
            drag_btn_  = Fl::event_button();
            last_x_    = ev_x;
            last_y_    = ev_y;
            return 1;

        case FL_RELEASE:
            dragging_ = false;
            return 1;

        case FL_DRAG:
            if (dragging_) {
                int dx = ev_x - last_x_;
                int dy = ev_y - last_y_;
                last_x_ = ev_x;
                last_y_ = ev_y;

                if (drag_btn_ == 1) {
                    s_asm.camera->orbitCamera(scene_center_,
                                              (float)dx, (float)dy, 0.25f);
                    updateClipping();
                } else if (drag_btn_ == 3) {
                    float dist = s_asm.camera->focalDistance.getValue();
                    dist *= (1.0f + dy * 0.01f);
                    dist = std::max(0.1f, dist);
                    SbVec3f dir = s_asm.camera->position.getValue() - scene_center_;
                    dir.normalize();
                    s_asm.camera->position.setValue(scene_center_ + dir * dist);
                    s_asm.camera->focalDistance.setValue(dist);
                    updateClipping();
                }
                refreshRender();
            }
            return 1;

        case FL_MOUSEWHEEL: {
            float delta = -(float)Fl::event_dy();
            float dist  = s_asm.camera->focalDistance.getValue();
            dist *= (1.0f - delta * 0.10f);
            dist = std::max(0.1f, dist);
            SbVec3f dir = s_asm.camera->position.getValue() - scene_center_;
            dir.normalize();
            s_asm.camera->position.setValue(scene_center_ + dir * dist);
            s_asm.camera->focalDistance.setValue(dist);
            updateClipping();
            refreshRender();
            return 1;
        }

        case FL_FOCUS: case FL_UNFOCUS:
            return 1;

        default:
            break;
        }
        return Fl_Box::handle(event);
    }

    void resize(int X, int Y, int W, int H) override
    {
        Fl_Box::resize(X, Y, W, H);
        refreshRender();
    }

private:
    void updateClipping()
    {
        if (!s_asm.camera) return;
        float d = s_asm.camera->focalDistance.getValue();
        d = std::max(d, 1e-4f);
        s_asm.camera->nearDistance.setValue(d * 0.001f);
        s_asm.camera->farDistance.setValue(d * 2000.0f);
    }

    std::vector<uint8_t> display_buf_;
    Fl_RGB_Image*        fltk_img_;
    std::string          status_;
    bool                 dragging_;
    int                  drag_btn_, last_x_, last_y_;
    double               last_render_ms_;
    SbVec3f              scene_center_{0.f, 0.f, 0.f};
};

/* =========================================================================
 * StatusBar  –  bottom bar with instance/triangle/timing stats
 * ========================================================================= */

class StatusBar : public Fl_Box {
public:
    explicit StatusBar(int X, int Y, int W, int H)
        : Fl_Box(X, Y, W, H, "")
    {
        box(FL_FLAT_BOX);
        color(fl_rgb_color(22, 24, 34));
        align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        labelsize(11);
        labelcolor(fl_rgb_color(160, 210, 160));
    }

    void update(int instances, int parts, double renderMs,
                bool lodOn, const char* drawModeName)
    {
        // Approximate full-detail triangle total (worst-case without LoD)
        size_t totalTris =
            static_cast<size_t>(instances) / PARTS_PER_CELL *
            (CELL_PRECISION * TRIS_PRECISION +
             CELL_STANDARD  * TRIS_STANDARD  +
             CELL_DETAIL    * TRIS_DETAIL    +
             CELL_FASTENER  * TRIS_FASTENER  +
             CELL_PANELS    * TRIS_PANEL);

        snprintf(buf_, sizeof(buf_),
                 "  Instances: %d  |  Parts: %d  |  Full-detail tris: ~%s"
                 "  |  Mode: %s  |  LoD: %s  |  Frame: %.1f ms (%.0f FPS)",
                 instances,
                 parts,
                 formatLarge(totalTris),
                 drawModeName,
                 lodOn ? "ON" : "OFF",
                 renderMs,
                 renderMs > 0.0 ? 1000.0 / renderMs : 0.0);
        label(buf_);
        redraw();
    }

private:
    static const char* formatLarge(size_t n) {
        static char buf[32];
        if (n >= 1000000)
            snprintf(buf, sizeof(buf), "%.1fM", n / 1000000.0);
        else if (n >= 1000)
            snprintf(buf, sizeof(buf), "%.0fK", n / 1000.0);
        else
            snprintf(buf, sizeof(buf), "%zu", n);
        return buf;
    }

    char buf_[512] = {};
};

/* =========================================================================
 * Globals shared between callbacks and the main window
 * ========================================================================= */

static RenderPanel* s_panel   = nullptr;
static StatusBar*   s_status  = nullptr;

/* Current draw mode (cycling through modes). */
static int  s_drawModeIdx = 1;   // 0=WIREFRAME, 1=SHADED, 2=SHADED_WITH_EDGES
static bool s_lodEnabled  = true;
static bool s_perspectiveCamera = true;  // true=perspective, false=orthographic

static const char* const DRAW_MODE_NAMES[] = {
    "Wireframe", "Shaded", "Shaded+Edges"
};
static const SoCADAssembly::DrawMode DRAW_MODES[] = {
    SoCADAssembly::WIREFRAME,
    SoCADAssembly::SHADED,
    SoCADAssembly::SHADED_WITH_EDGES,
};

/* Target instance counts for the Scale button */
static const int SCALE_LEVELS[] = { 500, 2000, 5000, 10000 };
static int s_scaleLevelIdx = 1;  // default: 2000

/** Format a scale-level count for the toolbar button (e.g. "500", "2 K", "10 K"). */
static std::string formatScaleLabel(int count)
{
    char buf[32];
    if (count >= 1000)
        snprintf(buf, sizeof(buf), "Scale: %d K", count / 1000);
    else
        snprintf(buf, sizeof(buf), "Scale: %d", count);
    return buf;
}

/** Switch the scene camera between perspective and orthographic projection.
 *  Preserves position, orientation, focal distance, and near/far planes.
 *  For the size parameter: converts heightAngle↔height via the focal distance. */
static void switchCameraType(bool toPerspective)
{
    if (!s_asm.root || !s_asm.camera) return;

    SbVec3f    pos   = s_asm.camera->position.getValue();
    SbRotation ori   = s_asm.camera->orientation.getValue();
    float      focal = s_asm.camera->focalDistance.getValue();
    float      nearD = s_asm.camera->nearDistance.getValue();
    float      farD  = s_asm.camera->farDistance.getValue();

    SoCamera* newCam = nullptr;
    if (toPerspective) {
        SoPerspectiveCamera* pcam = new SoPerspectiveCamera;
        // Recover vertical FOV from the orthographic height
        SoOrthographicCamera* ocam = dynamic_cast<SoOrthographicCamera*>(s_asm.camera);
        if (ocam && focal > 1e-6f)
            pcam->heightAngle.setValue(2.0f * std::atan(ocam->height.getValue() / (2.0f * focal)));
        newCam = pcam;
    } else {
        SoOrthographicCamera* ocam = new SoOrthographicCamera;
        // Derive orthographic height from the perspective vertical FOV
        SoPerspectiveCamera* pcam = dynamic_cast<SoPerspectiveCamera*>(s_asm.camera);
        if (pcam)
            ocam->height.setValue(2.0f * focal * std::tan(pcam->heightAngle.getValue() * 0.5f));
        newCam = ocam;
    }

    newCam->position.setValue(pos);
    newCam->orientation.setValue(ori);
    newCam->focalDistance.setValue(focal);
    newCam->nearDistance.setValue(nearD);
    newCam->farDistance.setValue(farD);

    s_asm.root->replaceChild(s_asm.camera, newCam);
    s_asm.camera = newCam;
}

/* =========================================================================
 * LargeAssemblyWindow  –  main application window
 *
 * Layout:
 *  ┌─────────────────────────────────────────────────────────────────┐
 *  │                    RenderPanel (3-D view)                       │
 *  ├─────────────────────────────────────────────────────────────────┤
 *  │ [LoD] [Mode] [Proj] [Scale] [Reset View] [Save RGB…]           │
 *  ├─────────────────────────────────────────────────────────────────┤
 *  │ Instances: N  |  Full-detail tris: ~NM  |  Frame: N ms (N FPS) │
 *  └─────────────────────────────────────────────────────────────────┘
 * ========================================================================= */

class LargeAssemblyWindow : public Fl_Double_Window {
public:
    static constexpr int TOOLBAR_H = 40;
    static constexpr int STATUS_H  = 22;

    LargeAssemblyWindow(int W, int H)
        : Fl_Double_Window(W, H,
              "Obol – Large-Scale CAD Assembly (SoCADAssembly + LoD)")
    {
        int panel_h = H - TOOLBAR_H - STATUS_H;

        s_panel = new RenderPanel(0, 0, W, panel_h);

        Fl_Group* tb = new Fl_Group(0, panel_h, W, TOOLBAR_H);
        tb->box(FL_FLAT_BOX);
        tb->color(fl_rgb_color(32, 34, 48));

        int bx = 6, by = panel_h + 6, bh = 28;

        lod_btn_ = new Fl_Button(bx, by, 100, bh, "LoD: ON");
        lod_btn_->callback(lodCB, this);
        lod_btn_->labelsize(11);
        lod_btn_->color(fl_rgb_color(40, 130, 60));
        lod_btn_->labelcolor(FL_WHITE);
        bx += 106;

        mode_btn_ = new Fl_Button(bx, by, 120, bh, "Mode: Shaded");
        mode_btn_->callback(modeCB, this);
        mode_btn_->labelsize(11);
        mode_btn_->color(fl_rgb_color(60, 90, 150));
        mode_btn_->labelcolor(FL_WHITE);
        bx += 126;

        proj_btn_ = new Fl_Button(bx, by, 120, bh, "Proj: Persp");
        proj_btn_->callback(projCB, this);
        proj_btn_->labelsize(11);
        proj_btn_->color(fl_rgb_color(50, 110, 120));
        proj_btn_->labelcolor(FL_WHITE);
        bx += 126;

        scale_btn_ = new Fl_Button(bx, by, 120, bh, "Scale: 2 K");
        scale_btn_->copy_label(formatScaleLabel(SCALE_LEVELS[s_scaleLevelIdx]).c_str());
        scale_btn_->callback(scaleCB, this);
        scale_btn_->labelsize(11);
        scale_btn_->color(fl_rgb_color(90, 65, 135));
        scale_btn_->labelcolor(FL_WHITE);
        bx += 126;

        Fl_Button* reset = new Fl_Button(bx, by, 100, bh, "Reset View");
        reset->callback(resetCB, this);
        reset->labelsize(11);
        reset->color(fl_rgb_color(55, 78, 100));
        reset->labelcolor(FL_WHITE);
        bx += 106;

        Fl_Button* save = new Fl_Button(bx, by, 110, bh, "Save RGB\xe2\x80\xa6");
        save->callback(saveCB, this);
        save->labelsize(11);
        save->color(fl_rgb_color(55, 78, 100));
        save->labelcolor(FL_WHITE);

        tb->end();

        s_status = new StatusBar(0, panel_h + TOOLBAR_H, W, STATUS_H);

        end();
        resizable(s_panel);
    }

    void initScene()
    {
        int pw = std::max(s_panel->w(), 1);
        int ph = std::max(s_panel->h(), 1);

        if (s_asm.root) s_asm.root->unref();
        s_asm = buildLargeScene(SCALE_LEVELS[s_scaleLevelIdx], pw, ph);
        // Restore chosen projection type after rebuild
        if (!s_perspectiveCamera)
            switchCameraType(false);
        s_panel->updateSceneCenter();
        s_panel->refreshRender();
        updateStatus();
    }

private:
    Fl_Button* lod_btn_   = nullptr;
    Fl_Button* mode_btn_  = nullptr;
    Fl_Button* proj_btn_  = nullptr;
    Fl_Button* scale_btn_ = nullptr;

    void updateStatus()
    {
        if (!s_status || !s_asm.assembly) return;
        s_status->update(
            s_asm.numInstances,
            s_asm.numParts,
            s_panel ? s_panel->lastRenderMs() : 0.0,
            s_lodEnabled,
            DRAW_MODE_NAMES[s_drawModeIdx]);
    }

    static void lodCB(Fl_Widget*, void* data)
    {
        auto* self = static_cast<LargeAssemblyWindow*>(data);
        s_lodEnabled = !s_lodEnabled;
        if (s_asm.assembly)
            s_asm.assembly->lodEnabled.setValue(s_lodEnabled ? TRUE : FALSE);
        if (self->lod_btn_) {
            self->lod_btn_->label(s_lodEnabled ? "LoD: ON" : "LoD: OFF");
            self->lod_btn_->color(s_lodEnabled
                ? fl_rgb_color(40, 130, 60)
                : fl_rgb_color(150, 60, 40));
            self->lod_btn_->redraw();
        }
        if (s_panel) s_panel->refreshRender();
        self->updateStatus();
    }

    static void modeCB(Fl_Widget*, void* data)
    {
        auto* self = static_cast<LargeAssemblyWindow*>(data);
        s_drawModeIdx = (s_drawModeIdx + 1) % 3;
        if (s_asm.assembly)
            s_asm.assembly->drawMode.setValue(DRAW_MODES[s_drawModeIdx]);

        static const char* btnLabels[] = {
            "Mode: Wireframe", "Mode: Shaded", "Mode: Shd+Edges"
        };
        if (self->mode_btn_) {
            self->mode_btn_->label(btnLabels[s_drawModeIdx]);
            self->mode_btn_->redraw();
        }
        if (s_panel) s_panel->refreshRender();
        self->updateStatus();
    }

    static void projCB(Fl_Widget*, void* data)
    {
        auto* self = static_cast<LargeAssemblyWindow*>(data);
        s_perspectiveCamera = !s_perspectiveCamera;
        switchCameraType(s_perspectiveCamera);
        if (self->proj_btn_) {
            self->proj_btn_->label(s_perspectiveCamera ? "Proj: Persp" : "Proj: Ortho");
            self->proj_btn_->color(s_perspectiveCamera
                ? fl_rgb_color(50, 110, 120)
                : fl_rgb_color(120, 90, 40));
            self->proj_btn_->redraw();
        }
        if (s_panel) s_panel->refreshRender();
    }

    static void scaleCB(Fl_Widget*, void* data)
    {
        auto* self = static_cast<LargeAssemblyWindow*>(data);
        s_scaleLevelIdx = (s_scaleLevelIdx + 1) % 4;
        int target = SCALE_LEVELS[s_scaleLevelIdx];

        // Update button label using the shared formatter
        if (self->scale_btn_) {
            self->scale_btn_->copy_label(formatScaleLabel(target).c_str());
            self->scale_btn_->redraw();
        }

        // Rebuild the assembly at the new scale
        self->initScene();
    }

    static void resetCB(Fl_Widget*, void* data)
    {
        auto* self = static_cast<LargeAssemblyWindow*>(data);
        self->initScene();
    }

    static void saveCB(Fl_Widget*, void* /*data*/)
    {
        if (s_panel) s_panel->saveRGB();
    }
};

/* =========================================================================
 * main
 * ========================================================================= */

int main(int argc, char** argv)
{
    BasicFLTKContextManager basicMgr;
    s_ctx_mgr = &basicMgr;

    SoDB::init(&basicMgr);
    SoNodeKit::init();
    SoInteraction::init();

    // Register the SoCADAssembly node type before scene construction.
    SoCADAssembly::initClass();

    Fl::scheme("gtk+");
    if (!Fl::visual(FL_RGB | FL_DOUBLE | FL_DEPTH))
        Fl::visual(FL_RGB | FL_DOUBLE);

    LargeAssemblyWindow* win = new LargeAssemblyWindow(1100, 760);
    win->show(argc, argv);
    win->wait_for_expose();

    // Build the initial scene after the window is visible so that the FLTK
    // GL context is already active.
    win->initScene();

    return Fl::run();
}
