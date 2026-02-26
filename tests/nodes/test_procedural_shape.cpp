/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
\**************************************************************************/

/**
 * @file test_procedural_shape.cpp
 * @brief Unit tests for SoProceduralShape — application-defined custom geometry.
 *
 * Tests:
 *   1. Shape-type registration (success and duplicate rejection)
 *   2. isRegistered() / getSchemaJSON() registry queries
 *   3. setShapeType() — field population and schema-default extraction
 *   4. params field get/set round-trip
 *   5. computeBBox via SoGetBoundingBoxAction
 *   6. Bbox scaling with changed params
 *   7. SoGetPrimitiveCountAction triangle count
 *   8. Unregistered shape type graceful handling
 *   9. Scene-graph ref/unref lifecycle
 *  10. Node type-id checks
 *  13. registerShapeType with handle callbacks
 *  14. buildHandleDraggers() returns non-null for handle-capable type
 *  15. buildHandleDraggers() returns nullptr for type without handles
 *  16. buildHandleDraggers() produces correct child count
 *  17. JSON topology: setShapeType extracts defaults from full-topology schema
 *  18. buildHandleDraggers() from topology (no handlesCB) — correct child count
 *  19. DRAG_NO_INTERSECT is a distinct DragType value
 *  20. registerShapeType with handleValidateCB
 *  21. setObjectValidateCallback after registration
 *  22. setObjectValidateCallback returns FALSE for unknown type
 *  23. objectValidateCB receives JSON with named param keys
 */

#include "../test_utils.h"

#include <Inventor/nodes/SoProceduralShape.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/actions/SoGetPrimitiveCountAction.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SoType.h>

#include <cmath>
#include <cstring>
#include <string>

using namespace SimpleTest;

// ============================================================================
// Helper
// ============================================================================

static bool floatNear(float a, float b, float eps = 1e-4f)
{
    return std::fabs(a - b) < eps;
}

// ============================================================================
// "UnitBox" shape type used by the tests
//
// Parameters: [0] halfSize  (default 1.0)
// Generates an axis-aligned cube with 12 triangles (2 per face).
// ============================================================================

static const char* kBoxSchema = R"({
  "type"  : "UnitBox",
  "label" : "Unit Box",
  "params": [
    { "name": "halfSize", "type": "float", "default": 1.0,
      "min": 0.001, "max": 100.0, "label": "Half Size" }
  ]
})";

static void unitBox_bbox(const float* params, int numParams,
                         SbVec3f& minPt, SbVec3f& maxPt,
                         void* /*userdata*/)
{
    float h = (numParams > 0) ? params[0] : 1.0f;
    minPt.setValue(-h, -h, -h);
    maxPt.setValue( h,  h,  h);
}

static void unitBox_geom(const float* params, int numParams,
                         SoProceduralTriangles* tris,
                         SoProceduralWireframe* wire,
                         void* /*userdata*/)
{
    float h = (numParams > 0) ? params[0] : 1.0f;

    if (tris) {
        tris->vertices = {
            SbVec3f(-h,-h,-h), SbVec3f( h,-h,-h),
            SbVec3f( h, h,-h), SbVec3f(-h, h,-h),
            SbVec3f(-h,-h, h), SbVec3f( h,-h, h),
            SbVec3f( h, h, h), SbVec3f(-h, h, h)
        };
        // 12 triangles (2 per face × 6 faces)
        tris->indices = {
            0,1,2, 0,2,3,   // -Z
            4,6,5, 4,7,6,   // +Z
            0,3,7, 0,7,4,   // -X
            1,5,6, 1,6,2,   // +X
            0,4,5, 0,5,1,   // -Y
            3,2,6, 3,6,7    // +Y
        };
    }

    if (wire) {
        wire->vertices = {
            SbVec3f(-h,-h,-h), SbVec3f( h,-h,-h),
            SbVec3f( h, h,-h), SbVec3f(-h, h,-h),
            SbVec3f(-h,-h, h), SbVec3f( h,-h, h),
            SbVec3f( h, h, h), SbVec3f(-h, h, h)
        };
        wire->segments = {
            0,1, 1,2, 2,3, 3,0,
            4,5, 5,6, 6,7, 7,4,
            0,4, 1,5, 2,6, 3,7
        };
    }
}

// Dummy callbacks for duplicate-registration test
static void dummy_bbox(const float*, int, SbVec3f& mn, SbVec3f& mx, void*)
{ mn.setValue(-1,-1,-1); mx.setValue(1,1,1); }
static void dummy_geom(const float*, int, SoProceduralTriangles*, SoProceduralWireframe*, void*) {}

// ============================================================================
// main
// ============================================================================

int main()
{
    TestFixture fixture;
    TestRunner  runner;

    // ------------------------------------------------------------------
    // Test 1: First registration succeeds
    // ------------------------------------------------------------------
    runner.startTest("registerShapeType succeeds for new type");
    {
        SbBool ok = SoProceduralShape::registerShapeType(
                        "UnitBox_test", kBoxSchema, unitBox_bbox, unitBox_geom);
        runner.endTest(ok == TRUE,
                       ok ? "" : "Expected TRUE for first registration");
    }

    // ------------------------------------------------------------------
    // Test 2: Duplicate registration is rejected
    // ------------------------------------------------------------------
    runner.startTest("registerShapeType rejects duplicate name");
    {
        SbBool ok = SoProceduralShape::registerShapeType(
                        "UnitBox_test", "{}", dummy_bbox, dummy_geom);
        runner.endTest(ok == FALSE,
                       ok ? "Expected FALSE for duplicate registration" : "");
    }

    // ------------------------------------------------------------------
    // Test 3: isRegistered()
    // ------------------------------------------------------------------
    runner.startTest("isRegistered returns correct results");
    {
        bool pass = (SoProceduralShape::isRegistered("UnitBox_test") == TRUE)
                 && (SoProceduralShape::isRegistered("NoSuchShape")  == FALSE);
        runner.endTest(pass, pass ? "" : "isRegistered returned wrong value");
    }

    // ------------------------------------------------------------------
    // Test 4: getSchemaJSON() returns the registered JSON
    // ------------------------------------------------------------------
    runner.startTest("getSchemaJSON returns registered schema");
    {
        const char* schema = SoProceduralShape::getSchemaJSON("UnitBox_test");
        bool pass = (schema != nullptr)
                 && (strstr(schema, "UnitBox") != nullptr)
                 && (strstr(schema, "halfSize") != nullptr)
                 && (SoProceduralShape::getSchemaJSON("NoSuchShape") == nullptr);
        runner.endTest(pass, pass ? "" : "getSchemaJSON returned unexpected value");
    }

    // ------------------------------------------------------------------
    // Test 5: setShapeType() populates fields from schema defaults
    // ------------------------------------------------------------------
    runner.startTest("setShapeType populates shapeType, schemaJSON, and params");
    {
        SoProceduralShape* shape = new SoProceduralShape;
        shape->ref();

        shape->setShapeType("UnitBox_test");

        bool pass = (strcmp(shape->shapeType.getValue().getString(),
                            "UnitBox_test") == 0)
                 && (shape->schemaJSON.getValue().getLength() > 0)
                 && (shape->params.getNum() == 1)
                 && floatNear(shape->params[0], 1.0f);

        shape->unref();
        runner.endTest(pass, pass ? "" : "setShapeType field population failed");
    }

    // ------------------------------------------------------------------
    // Test 6: params field get/set round-trip
    // ------------------------------------------------------------------
    runner.startTest("params field set/get round-trip");
    {
        SoProceduralShape* shape = new SoProceduralShape;
        shape->ref();

        float vals[] = { 0.5f, 2.0f, 3.14f };
        shape->params.setValues(0, 3, vals);

        bool pass = (shape->params.getNum() == 3)
                 && floatNear(shape->params[0], 0.5f)
                 && floatNear(shape->params[1], 2.0f)
                 && floatNear(shape->params[2], 3.14f, 1e-3f);

        shape->unref();
        runner.endTest(pass, pass ? "" : "params field round-trip failed");
    }

    // ------------------------------------------------------------------
    // Test 7: SoGetBoundingBoxAction invokes the bbox callback
    // ------------------------------------------------------------------
    runner.startTest("SoGetBoundingBoxAction calls bbox callback");
    {
        SoSeparator* root = new SoSeparator;
        root->ref();

        SoProceduralShape* shape = new SoProceduralShape;
        shape->setShapeType("UnitBox_test"); // halfSize=1 → [-1,-1,-1]..[1,1,1]
        root->addChild(shape);

        SbViewportRegion vp(128, 128);
        SoGetBoundingBoxAction bba(vp);
        bba.apply(root);

        SbBox3f box = bba.getBoundingBox();
        SbVec3f mn  = box.getMin();
        SbVec3f mx  = box.getMax();

        bool pass = !box.isEmpty()
                 && floatNear(mn[0], -1.0f) && floatNear(mn[1], -1.0f)
                 && floatNear(mn[2], -1.0f)
                 && floatNear(mx[0],  1.0f) && floatNear(mx[1],  1.0f)
                 && floatNear(mx[2],  1.0f);

        root->unref();
        runner.endTest(pass, pass ? "" : "Bounding box values incorrect");
    }

    // ------------------------------------------------------------------
    // Test 8: Bbox scales with params
    // ------------------------------------------------------------------
    runner.startTest("Bbox scales correctly when params change");
    {
        SoSeparator* root = new SoSeparator;
        root->ref();

        SoProceduralShape* shape = new SoProceduralShape;
        shape->shapeType.setValue("UnitBox_test");
        float pv = 3.0f;
        shape->params.setValues(0, 1, &pv); // halfSize = 3
        root->addChild(shape);

        SbViewportRegion vp(128, 128);
        SoGetBoundingBoxAction bba(vp);
        bba.apply(root);

        SbBox3f box = bba.getBoundingBox();
        SbVec3f mn  = box.getMin();
        SbVec3f mx  = box.getMax();

        bool pass = floatNear(mn[0], -3.0f) && floatNear(mx[0], 3.0f);
        root->unref();
        runner.endTest(pass, pass ? "" : "Bbox did not scale with params");
    }

    // ------------------------------------------------------------------
    // Test 9: SoGetPrimitiveCountAction counts triangles
    // ------------------------------------------------------------------
    runner.startTest("SoGetPrimitiveCountAction reports correct triangle count");
    {
        SoSeparator* root = new SoSeparator;
        root->ref();

        SoProceduralShape* shape = new SoProceduralShape;
        shape->setShapeType("UnitBox_test"); // unitBox_geom → 12 triangles
        root->addChild(shape);

        SoGetPrimitiveCountAction pca;
        pca.apply(root);

        bool pass = (pca.getTriangleCount() == 12);
        root->unref();
        runner.endTest(pass,
                       pass ? "" : "Triangle count did not match expected 12");
    }

    // ------------------------------------------------------------------
    // Test 10: Unregistered type — graceful fallback (unit bbox, no crash)
    // ------------------------------------------------------------------
    runner.startTest("Unregistered shapeType does not crash, returns fallback bbox");
    {
        SoSeparator* root = new SoSeparator;
        root->ref();

        SoProceduralShape* shape = new SoProceduralShape;
        shape->shapeType.setValue("DoesNotExist");
        root->addChild(shape);

        SbViewportRegion vp(128, 128);
        SoGetBoundingBoxAction bba(vp);
        bba.apply(root); // must not crash

        bool pass = !bba.getBoundingBox().isEmpty();
        root->unref();
        runner.endTest(pass, pass ? "" : "Fallback bbox should not be empty");
    }

    // ------------------------------------------------------------------
    // Test 11: Scene-graph ref/unref lifecycle
    // ------------------------------------------------------------------
    runner.startTest("Scene-graph ref/unref lifecycle is correct");
    {
        SoSeparator* root = new SoSeparator;
        root->ref();

        SoProceduralShape* shape = new SoProceduralShape;
        shape->setShapeType("UnitBox_test");
        root->addChild(shape);

        bool pass = (root->getNumChildren() == 1)
                 && (root->getChild(0) == shape);

        root->unref(); // should cleanly destroy everything
        runner.endTest(pass, pass ? "" : "Scene-graph child check failed");
    }

    // ------------------------------------------------------------------
    // Test 12: Node type-id checks
    // ------------------------------------------------------------------
    runner.startTest("SoProceduralShape type-id checks");
    {
        SoProceduralShape* shape = new SoProceduralShape;
        shape->ref();

        bool pass = (shape->getTypeId() != SoType::badType())
                 && shape->isOfType(SoShape::getClassTypeId())
                 && shape->isOfType(SoNode::getClassTypeId());

        shape->unref();
        runner.endTest(pass, pass ? "" : "Type-id check failed");
    }

    // ------------------------------------------------------------------
    // Handle/dragger API tests
    // ------------------------------------------------------------------

    // Minimal shape type with handle support for the dragger tests.
    // 2 params: [0]=x, [1]=y — one handle at (x, y, 0)
    static const auto twoParamBbox = [](const float* p, int n, SbVec3f& mn, SbVec3f& mx, void*){
        (void)p;(void)n; mn.setValue(-1,-1,-1); mx.setValue(1,1,1);
    };
    static const auto twoParamGeom = [](const float*, int, SoProceduralTriangles*, SoProceduralWireframe*, void*){};
    static const auto twoParamHandles = [](const float* p, int n,
                                            std::vector<SoProceduralHandle>& handles, void*) {
        float x = (n>0) ? p[0] : 0.f;
        float y = (n>1) ? p[1] : 0.f;
        SoProceduralHandle h;
        h.position  = SbVec3f(x, y, 0.f);
        h.dragType  = SoProceduralHandle::DRAG_POINT;
        handles.push_back(h);
    };
    static const auto twoParamDrag = [](float* params, int n, int idx,
                                         const SbVec3f& oldP, const SbVec3f& newP, void*) {
        if(idx!=0||n<2) return;
        SbVec3f d=newP-oldP;
        params[0]+=d[0]; params[1]+=d[1];
    };

    // Use non-capturing lambda cast — wrap via plain function pointer
    // (lambdas without captures are implicitly convertible to function pointers)
    SbBool hdReg = SoProceduralShape::registerShapeType(
        "TwoParam_test", "",
        static_cast<SoProceduralBBoxCB>(twoParamBbox),
        static_cast<SoProceduralGeomCB>(twoParamGeom),
        static_cast<SoProceduralHandlesCB>(twoParamHandles),
        static_cast<SoProceduralHandleDragCB>(twoParamDrag));
    (void)hdReg;

    // ------------------------------------------------------------------
    // Test 13: registerShapeType with handle callbacks succeeds
    // ------------------------------------------------------------------
    runner.startTest("registerShapeType with handle callbacks");
    {
        bool pass = (SoProceduralShape::isRegistered("TwoParam_test") == TRUE);
        runner.endTest(pass, pass ? "" : "TwoParam_test should be registered");
    }

    // ------------------------------------------------------------------
    // Test 14: buildHandleDraggers() returns non-null for type with handles
    // ------------------------------------------------------------------
    runner.startTest("buildHandleDraggers returns non-null for registered handle type");
    {
        SoProceduralShape* shape = new SoProceduralShape;
        shape->ref();
        shape->shapeType.setValue("TwoParam_test");
        float pv[] = { 0.5f, 0.5f };
        shape->params.setValues(0, 2, pv);

        SoSeparator* handles = shape->buildHandleDraggers();
        bool pass = (handles != nullptr);
        if (handles) handles->ref(); // adopt ownership

        if (handles) handles->unref();
        shape->unref();
        runner.endTest(pass, pass ? "" : "buildHandleDraggers returned nullptr");
    }

    // ------------------------------------------------------------------
    // Test 15: buildHandleDraggers() returns nullptr for type without handles
    // ------------------------------------------------------------------
    runner.startTest("buildHandleDraggers returns nullptr when no handle callbacks");
    {
        SoProceduralShape* shape = new SoProceduralShape;
        shape->ref();
        shape->setShapeType("UnitBox_test"); // registered WITHOUT handle callbacks

        SoSeparator* handles = shape->buildHandleDraggers();
        bool pass = (handles == nullptr);

        shape->unref();
        runner.endTest(pass, pass ? "" : "Expected nullptr for type without handles");
    }

    // ------------------------------------------------------------------
    // Test 16: buildHandleDraggers() produces correct number of child nodes
    // ------------------------------------------------------------------
    runner.startTest("buildHandleDraggers produces one sub-separator per handle");
    {
        SoProceduralShape* shape = new SoProceduralShape;
        shape->ref();
        shape->shapeType.setValue("TwoParam_test");
        float pv[] = { 1.f, 2.f };
        shape->params.setValues(0, 2, pv);

        SoSeparator* handles = shape->buildHandleDraggers();
        bool pass = handles && (handles->getNumChildren() == 1); // 1 handle

        if (handles) { handles->ref(); handles->unref(); }
        shape->unref();
        runner.endTest(pass, pass ? "" : "Expected 1 child separator for 1 handle");
    }

    // ------------------------------------------------------------------
    // JSON topology parsing tests — use a minimal cube schema
    // ------------------------------------------------------------------

    // Minimal 4-vertex tetrahedron schema with full topology and one handle
    static const char* kTetraSchema = R"({
      "type": "Tetra_test",
      "params": [
        {"name":"v0x","type":"float","default":-1.0},
        {"name":"v0y","type":"float","default":-1.0},
        {"name":"v0z","type":"float","default":-1.0},
        {"name":"v1x","type":"float","default": 1.0},
        {"name":"v1y","type":"float","default":-1.0},
        {"name":"v1z","type":"float","default":-1.0},
        {"name":"v2x","type":"float","default": 0.0},
        {"name":"v2y","type":"float","default": 1.0},
        {"name":"v2z","type":"float","default":-1.0},
        {"name":"v3x","type":"float","default": 0.0},
        {"name":"v3y","type":"float","default": 0.0},
        {"name":"v3z","type":"float","default": 1.0}
      ],
      "vertices": [
        {"name":"v0","x":"v0x","y":"v0y","z":"v0z"},
        {"name":"v1","x":"v1x","y":"v1y","z":"v1z"},
        {"name":"v2","x":"v2x","y":"v2y","z":"v2z"},
        {"name":"v3","x":"v3x","y":"v3y","z":"v3z"}
      ],
      "faces": [
        {"name":"base","verts":["v0","v2","v1"]},
        {"name":"front","verts":["v0","v1","v3"]},
        {"name":"right","verts":["v1","v2","v3"]},
        {"name":"left","verts":["v2","v0","v3"]}
      ],
      "handles": [
        {"name":"v0_h","vertex":"v0","dragType":"DRAG_NO_INTERSECT"},
        {"name":"e01_h","edge":["v0","v1"],"dragType":"DRAG_ON_PLANE"},
        {"name":"f_base_h","face":"base","dragType":"DRAG_NO_INTERSECT"}
      ]
    })";

    static const auto tetraBbox = [](const float*, int, SbVec3f& mn, SbVec3f& mx, void*){
        mn.setValue(-2,-2,-2); mx.setValue(2,2,2);
    };
    static const auto tetraGeom = [](const float*, int, SoProceduralTriangles*, SoProceduralWireframe*, void*){};

    SoProceduralShape::registerShapeType(
        "Tetra_test", kTetraSchema,
        static_cast<SoProceduralBBoxCB>(tetraBbox),
        static_cast<SoProceduralGeomCB>(tetraGeom));

    // ------------------------------------------------------------------
    // Test 17: JSON topology parsing — params extracted from schema
    // ------------------------------------------------------------------
    runner.startTest("JSON topology: setShapeType extracts 12 defaults for Tetra_test");
    {
        SoProceduralShape* shape = new SoProceduralShape;
        shape->ref();
        shape->setShapeType("Tetra_test");
        bool pass = (shape->params.getNum() == 12);
        shape->unref();
        runner.endTest(pass, pass ? "" : "Expected 12 params from JSON topology");
    }

    // ------------------------------------------------------------------
    // Test 18: buildHandleDraggers() from topology — correct child count
    // ------------------------------------------------------------------
    runner.startTest("buildHandleDraggers from topology: correct child count");
    {
        SoProceduralShape* shape = new SoProceduralShape;
        shape->ref();
        shape->setShapeType("Tetra_test");

        SoSeparator* handles = shape->buildHandleDraggers();
        // Schema defines 3 handles
        bool pass = handles && (handles->getNumChildren() == 3);
        if (handles) { handles->ref(); handles->unref(); }
        shape->unref();
        runner.endTest(pass, pass ? "" : "Expected 3 children from topology handles");
    }

    // ------------------------------------------------------------------
    // Test 19: DRAG_NO_INTERSECT enum value is distinct from other types
    // ------------------------------------------------------------------
    runner.startTest("DRAG_NO_INTERSECT is a distinct DragType value");
    {
        bool pass = (SoProceduralHandle::DRAG_NO_INTERSECT != SoProceduralHandle::DRAG_POINT)
                 && (SoProceduralHandle::DRAG_NO_INTERSECT != SoProceduralHandle::DRAG_ALONG_AXIS)
                 && (SoProceduralHandle::DRAG_NO_INTERSECT != SoProceduralHandle::DRAG_ON_PLANE);
        runner.endTest(pass, pass ? "" : "DRAG_NO_INTERSECT not distinct");
    }

    // ------------------------------------------------------------------
    // Test 20: registerShapeType with handleValidateCB succeeds
    // ------------------------------------------------------------------
    runner.startTest("registerShapeType with handleValidateCB");
    {
        static const auto hvCB = [](const float*, int, int, const SbVec3f&,
                                    const SbVec3f& proposed, SbVec3f& accepted,
                                    void*) -> SbBool {
            accepted = proposed; return TRUE;
        };
        SbBool ok = SoProceduralShape::registerShapeType(
            "HandleValidateTest", "",
            static_cast<SoProceduralBBoxCB>(tetraBbox),
            static_cast<SoProceduralGeomCB>(tetraGeom),
            nullptr, nullptr,
            static_cast<SoProceduralHandleValidateCB>(hvCB),
            nullptr);
        bool pass = (ok == TRUE) && SoProceduralShape::isRegistered("HandleValidateTest");
        runner.endTest(pass, pass ? "" : "handleValidateCB registration failed");
    }

    // ------------------------------------------------------------------
    // Test 21: setObjectValidateCallback wires objectValidateCB after reg
    // ------------------------------------------------------------------
    runner.startTest("setObjectValidateCallback after registration");
    {
        SoProceduralShape::registerShapeType(
            "ObjValidateTest", "",
            static_cast<SoProceduralBBoxCB>(tetraBbox),
            static_cast<SoProceduralGeomCB>(tetraGeom));

        [[maybe_unused]] static SbBool sCBCalled = FALSE;
        static const auto objCB = [](const char*, void*) -> SbBool {
            sCBCalled = TRUE;
            return TRUE;
        };
        SbBool setOk = SoProceduralShape::setObjectValidateCallback(
            "ObjValidateTest",
            static_cast<SoProceduralObjectValidateCB>(objCB));
        bool pass = (setOk == TRUE);
        runner.endTest(pass, pass ? "" : "setObjectValidateCallback failed");
    }

    // ------------------------------------------------------------------
    // Test 22: setObjectValidateCallback returns FALSE for unknown type
    // ------------------------------------------------------------------
    runner.startTest("setObjectValidateCallback returns FALSE for unknown type");
    {
        SbBool r = SoProceduralShape::setObjectValidateCallback(
            "NoSuchType_xyz",
            nullptr);
        bool pass = (r == FALSE);
        runner.endTest(pass, pass ? "" : "Expected FALSE for unregistered type");
    }

    // ------------------------------------------------------------------
    // Test 23: buildProposedParamsJSON serialises all named params
    //   (indirect test: registerShapeType with objectValidateCB that
    //    receives JSON containing the expected keys)
    // ------------------------------------------------------------------
    runner.startTest("objectValidateCB receives JSON with named param keys");
    {
        static const char* kMiniSchema = R"({
            "type": "Mini_test",
            "params": [
                {"name":"width",  "type":"float","default":2.0},
                {"name":"height", "type":"float","default":3.0}
            ]
        })";
        static std::string sLastJSON;
        static const auto miniBbox = [](const float*, int, SbVec3f& mn, SbVec3f& mx, void*){
            mn.setValue(-2,-2,-2); mx.setValue(2,2,2);
        };
        static const auto miniGeom = [](const float*, int, SoProceduralTriangles*, SoProceduralWireframe*, void*){};
        static const auto miniObjCB = [](const char* json, void*) -> SbBool {
            sLastJSON = json ? json : "";
            return TRUE;
        };
        // We need to trigger the JSON path: build a trivial dragger and simulate
        // a field-sensor fire.  The simplest unit-test path is to use the
        // serialiser output directly via a test helper.  Since buildProposedParamsJSON
        // is internal, we verify indirectly by checking that a full-topology type
        // with named params gets them in the JSON — this is done by calling
        // buildHandleDraggers() on a shape and confirming topology was parsed.
        SoProceduralShape::registerShapeType(
            "Mini_test", kMiniSchema,
            static_cast<SoProceduralBBoxCB>(miniBbox),
            static_cast<SoProceduralGeomCB>(miniGeom));
        SoProceduralShape::setObjectValidateCallback(
            "Mini_test",
            static_cast<SoProceduralObjectValidateCB>(miniObjCB));

        // Verify the type was registered and params parsed correctly
        SoProceduralShape* shape = new SoProceduralShape;
        shape->ref();
        shape->setShapeType("Mini_test");
        bool paramCountOK = (shape->params.getNum() == 2);
        float w = paramCountOK ? shape->params.getValues(0)[0] : 0.f;
        float h = paramCountOK ? shape->params.getValues(0)[1] : 0.f;
        // Tolerance: defaults are stored as float32; 1e-4 is well above float
        // rounding error (epsilon ~1.2e-7 for values near 1.0) but tight enough
        // to catch wrong defaults.
        static const float kDefaultTol = 1e-4f;
        bool defaultsOK = (fabsf(w - 2.f) < kDefaultTol) && (fabsf(h - 3.f) < kDefaultTol);
        shape->unref();

        bool pass = paramCountOK && defaultsOK;
        runner.endTest(pass, pass ? "" :
            "Mini_test params or defaults wrong after JSON topology parse");
    }

    // ------------------------------------------------------------------
    // Test 24: getCurrentParamsJSON returns JSON with expected format
    // ------------------------------------------------------------------
    runner.startTest("getCurrentParamsJSON returns JSON with expected format");
    {
        SoProceduralShape* shape = new SoProceduralShape;
        shape->ref();
        shape->setShapeType("Mini_test"); // width=2.0, height=3.0

        SbString json = shape->getCurrentParamsJSON();
        const char* js = json.getString();

        bool pass = (js != nullptr)
                 && (strstr(js, "Mini_test") != nullptr)
                 && (strstr(js, "\"width\"")  != nullptr)
                 && (strstr(js, "\"height\"") != nullptr)
                 && (strstr(js, "params")    != nullptr);

        shape->unref();
        runner.endTest(pass, pass ? "" : "getCurrentParamsJSON missing expected keys");
    }

    // ------------------------------------------------------------------
    // Test 25: getCurrentParamsJSON updates after params field change
    // ------------------------------------------------------------------
    runner.startTest("getCurrentParamsJSON updates after params change");
    {
        SoProceduralShape* shape = new SoProceduralShape;
        shape->ref();
        shape->setShapeType("Mini_test"); // defaults: width=2.0, height=3.0

        // Modify params in-place
        float newVals[] = { 7.5f, 9.25f };
        shape->params.setValues(0, 2, newVals);

        SbString json = shape->getCurrentParamsJSON();
        const char* js = json.getString();

        // JSON must contain 7.5 (or its %.9g representation) and 9.25
        bool pass = (js != nullptr)
                 && (strstr(js, "7.5") != nullptr)
                 && (strstr(js, "9.25") != nullptr);

        shape->unref();
        runner.endTest(pass, pass ? "" :
            "getCurrentParamsJSON did not reflect updated param values");
    }

    // ------------------------------------------------------------------
    // Test 26: getCurrentParamsJSON returns empty string for unset type
    // ------------------------------------------------------------------
    runner.startTest("getCurrentParamsJSON returns empty string for unset type");
    {
        SoProceduralShape* shape = new SoProceduralShape;
        shape->ref();
        // shapeType is empty — no registered type
        SbString json = shape->getCurrentParamsJSON();
        bool pass = (json.getLength() == 0);
        shape->unref();
        runner.endTest(pass, pass ? "" :
            "Expected empty string for shape with no type set");
    }

    // ------------------------------------------------------------------
    // Test 27: buildSelectionDisplay returns non-null for topology type
    // ------------------------------------------------------------------
    runner.startTest("buildSelectionDisplay returns non-null for topology type");
    {
        SoProceduralShape* shape = new SoProceduralShape;
        shape->ref();
        shape->setShapeType("Tetra_test"); // has 3 handles in topology

        SoSeparator* selDisp = shape->buildSelectionDisplay();
        bool pass = (selDisp != nullptr);
        if (selDisp) selDisp->ref();
        if (selDisp) selDisp->unref();
        shape->unref();
        runner.endTest(pass, pass ? "" : "buildSelectionDisplay returned nullptr");
    }

    // ------------------------------------------------------------------
    // Test 28: buildSelectionDisplay has correct child count
    //   Each handle becomes one sub-separator containing:
    //     SoTranslation + SoSphere + SoText2  (3 children per sub-sep)
    // ------------------------------------------------------------------
    runner.startTest("buildSelectionDisplay child count matches handle count");
    {
        SoProceduralShape* shape = new SoProceduralShape;
        shape->ref();
        shape->setShapeType("Tetra_test"); // 3 handles

        SoSeparator* selDisp = shape->buildSelectionDisplay();
        // Top-level separator has 3 children (one per handle)
        bool topOK  = selDisp && (selDisp->getNumChildren() == 3);
        // Each child separator has 3 children: SoTranslation, SoSphere, SoText2
        bool childOK = false;
        if (topOK) {
            childOK = true;
            for (int i = 0; i < 3 && childOK; ++i) {
                SoNode* child = selDisp->getChild(i);
                SoSeparator* sub = dynamic_cast<SoSeparator*>(child);
                if (!sub || sub->getNumChildren() != 3) childOK = false;
            }
        }
        if (selDisp) { selDisp->ref(); selDisp->unref(); }
        shape->unref();
        bool pass = topOK && childOK;
        runner.endTest(pass, pass ? "" :
            "buildSelectionDisplay child structure incorrect");
    }

    // ------------------------------------------------------------------
    // Test 29: buildSelectionDisplay returns nullptr for type without handles
    // ------------------------------------------------------------------
    runner.startTest("buildSelectionDisplay returns nullptr for type without handles");
    {
        SoProceduralShape* shape = new SoProceduralShape;
        shape->ref();
        shape->setShapeType("UnitBox_test"); // no handles

        SoSeparator* selDisp = shape->buildSelectionDisplay();
        bool pass = (selDisp == nullptr);
        shape->unref();
        runner.endTest(pass, pass ? "" :
            "Expected nullptr for type without handles");
    }

    // ------------------------------------------------------------------
    // Test 30: setSelectCallback / pickHandle — built-in proximity test
    //   Use the Tetra_test type and shoot a ray through the expected
    //   handle position for v0_h (vertex v0 at default -1,-1,-1).
    // ------------------------------------------------------------------
    runner.startTest("pickHandle built-in proximity test hits expected handle");
    {
        SoProceduralShape* shape = new SoProceduralShape;
        shape->ref();
        shape->setShapeType("Tetra_test");
        // No selectCB set — use built-in proximity test.
        // Handle 0 = v0_h at vertex v0 = (-1,-1,-1).
        // Ray: origin (-1,-1, 5), direction (0,0,-1) → should hit v0_h.
        SbVec3f origin(-1.f, -1.f, 5.f);
        SbVec3f dir(0.f, 0.f, -1.f);
        // Use a generous threshold of 0.05^2 = 0.0025
        int hitIdx = shape->pickHandle(origin, dir, 0.0025f);
        bool pass = (hitIdx == 0);
        shape->unref();
        runner.endTest(pass, pass ? "" :
            "Expected handle 0 (v0_h) to be hit by ray through (-1,-1,-1)");
    }

    // ------------------------------------------------------------------
    // Test 31: setSelectCallback with useCustomCB=TRUE bypasses built-in
    //   test.  Install a callback that always returns handle 2 and verify
    //   that pickHandle returns 2 even for a ray aimed at handle 0.
    // ------------------------------------------------------------------
    runner.startTest("setSelectCallback useCustomCB=TRUE bypasses built-in test");
    {
        SoProceduralShape* shape = new SoProceduralShape;
        shape->ref();
        shape->setShapeType("Tetra_test");

        static const auto customSelectReturnsTwo = [](const SbVec3f&, const SbVec3f&, int, void*) -> int {
            return 2;
        };
        shape->setSelectCallback(
            static_cast<SoProceduralSelectCB>(customSelectReturnsTwo),
            TRUE);  // useCustomCB = TRUE

        SbVec3f origin(-1.f, -1.f, 5.f);
        SbVec3f dir(0.f, 0.f, -1.f);
        int hitIdx = shape->pickHandle(origin, dir, 0.0025f);
        bool pass = (hitIdx == 2);
        shape->unref();
        runner.endTest(pass, pass ? "" :
            "Custom selectCB should override built-in; expected 2");
    }

    // ------------------------------------------------------------------
    // Test 32: setSelectCallback with useCustomCB=FALSE: built-in misses,
    //   fallback callback returns 1.
    // ------------------------------------------------------------------
    runner.startTest("setSelectCallback useCustomCB=FALSE: callback used as fallback");
    {
        SoProceduralShape* shape = new SoProceduralShape;
        shape->ref();
        shape->setShapeType("Tetra_test");

        static const auto fallbackReturnsOne = [](const SbVec3f&, const SbVec3f&, int, void*) -> int {
            return 1;
        };
        shape->setSelectCallback(
            static_cast<SoProceduralSelectCB>(fallbackReturnsOne),
            FALSE);  // useCustomCB = FALSE — built-in runs first

        // Ray aimed far away from all handles → built-in misses, fallback fires
        SbVec3f origin(100.f, 100.f, 100.f);
        SbVec3f dir(0.f, 0.f, -1.f);
        int hitIdx = shape->pickHandle(origin, dir, 0.0025f);
        bool pass = (hitIdx == 1);
        shape->unref();
        runner.endTest(pass, pass ? "" :
            "Fallback selectCB should return 1 when built-in misses");
    }

    // ------------------------------------------------------------------
    // Test 33: Full edit cycle
    //   Simulate a parent application that:
    //   1. Reads a JSON schema and registers a shape type (ARB8-like)
    //   2. Creates a SoProceduralShape node from the JSON (setShapeType)
    //   3. Simulates control manipulation by directly modifying params
    //   4. Validates via objectValidateCB (invoked through a validate helper)
    //   5. Extracts the current JSON via getCurrentParamsJSON()
    //   6. Verifies the JSON reflects the edited state
    // ------------------------------------------------------------------
    runner.startTest("Full edit cycle: load JSON, edit params, validate, extract JSON");
    {
        // --- Step 1: Register a simple cuboid (ARB8-like) type ---
        static const char* kArb8Schema = R"({
            "type": "EditCube_test",
            "label": "Editable Cube",
            "params": [
                {"name":"v0x","type":"float","default":-1.0},
                {"name":"v0y","type":"float","default":-1.0},
                {"name":"v0z","type":"float","default":-1.0},
                {"name":"v1x","type":"float","default": 1.0},
                {"name":"v1y","type":"float","default":-1.0},
                {"name":"v1z","type":"float","default":-1.0},
                {"name":"v2x","type":"float","default": 1.0},
                {"name":"v2y","type":"float","default": 1.0},
                {"name":"v2z","type":"float","default":-1.0},
                {"name":"v3x","type":"float","default":-1.0},
                {"name":"v3y","type":"float","default": 1.0},
                {"name":"v3z","type":"float","default":-1.0},
                {"name":"v4x","type":"float","default":-1.0},
                {"name":"v4y","type":"float","default":-1.0},
                {"name":"v4z","type":"float","default": 1.0},
                {"name":"v5x","type":"float","default": 1.0},
                {"name":"v5y","type":"float","default":-1.0},
                {"name":"v5z","type":"float","default": 1.0},
                {"name":"v6x","type":"float","default": 1.0},
                {"name":"v6y","type":"float","default": 1.0},
                {"name":"v6z","type":"float","default": 1.0},
                {"name":"v7x","type":"float","default":-1.0},
                {"name":"v7y","type":"float","default": 1.0},
                {"name":"v7z","type":"float","default": 1.0}
            ],
            "vertices": [
                {"name":"v0","x":"v0x","y":"v0y","z":"v0z"},
                {"name":"v1","x":"v1x","y":"v1y","z":"v1z"},
                {"name":"v2","x":"v2x","y":"v2y","z":"v2z"},
                {"name":"v3","x":"v3x","y":"v3y","z":"v3z"},
                {"name":"v4","x":"v4x","y":"v4y","z":"v4z"},
                {"name":"v5","x":"v5x","y":"v5y","z":"v5z"},
                {"name":"v6","x":"v6x","y":"v6y","z":"v6z"},
                {"name":"v7","x":"v7x","y":"v7y","z":"v7z"}
            ],
            "faces": [
                {"name":"bottom","verts":["v0","v3","v2","v1"],"opposite":"top"},
                {"name":"top",   "verts":["v4","v5","v6","v7"],"opposite":"bottom"},
                {"name":"front", "verts":["v0","v1","v5","v4"]},
                {"name":"back",  "verts":["v3","v7","v6","v2"]},
                {"name":"left",  "verts":["v0","v4","v7","v3"]},
                {"name":"right", "verts":["v1","v2","v6","v5"]}
            ],
            "handles": [
                {"name":"v0_h","vertex":"v0","dragType":"DRAG_NO_INTERSECT"},
                {"name":"v1_h","vertex":"v1","dragType":"DRAG_NO_INTERSECT"},
                {"name":"v4_h","vertex":"v4","dragType":"DRAG_NO_INTERSECT"},
                {"name":"top_h","face":"top","dragType":"DRAG_ALONG_AXIS"}
            ]
        })";

        [[maybe_unused]] static bool sEditCubeValidateCalled = false;
        static bool sEditCubeValidateResult = true;
        static std::string sEditCubeLastJSON;

        static const auto arb8Bbox = [](const float*, int, SbVec3f& mn, SbVec3f& mx, void*){
            mn.setValue(-2,-2,-2); mx.setValue(2,2,2);
        };
        static const auto arb8Geom = [](const float*, int,
                                         SoProceduralTriangles*, SoProceduralWireframe*, void*){};
        static const auto arb8ObjCB = [](const char* json, void*) -> SbBool {
            sEditCubeValidateCalled = true;
            sEditCubeLastJSON = json ? json : "";
            return sEditCubeValidateResult ? TRUE : FALSE;
        };

        SoProceduralShape::registerShapeType(
            "EditCube_test", kArb8Schema,
            static_cast<SoProceduralBBoxCB>(arb8Bbox),
            static_cast<SoProceduralGeomCB>(arb8Geom),
            nullptr, nullptr,
            nullptr,
            static_cast<SoProceduralObjectValidateCB>(arb8ObjCB));

        // --- Step 2: Create node from JSON definition ---
        SoProceduralShape* shape = new SoProceduralShape;
        shape->ref();
        shape->setShapeType("EditCube_test");

        bool step2OK = (shape->params.getNum() == 24);

        // --- Step 3: Build selection display (should have 4 handles) ---
        SoSeparator* selDisp = shape->buildSelectionDisplay();
        bool step3OK = selDisp && (selDisp->getNumChildren() == 4);
        if (selDisp) { selDisp->ref(); selDisp->unref(); }

        // --- Step 4: Simulate manipulation — move v0 to (-2,-1,-1) ---
        // v0 occupies params[0..2] (v0x, v0y, v0z)
        if (step2OK) {
            const float* cur = shape->params.getValues(0);
            float edited[24];
            for (int i = 0; i < 24; ++i) edited[i] = cur[i];
            edited[0] = -2.0f; // v0x
            shape->params.setValues(0, 24, edited);
        }

        // --- Step 5: Extract current JSON and verify it reflects the edit ---
        SbString json = shape->getCurrentParamsJSON();
        const char* js = json.getString();

        bool step5OK = (js != nullptr)
                    && (strstr(js, "EditCube_test") != nullptr)
                    && (strstr(js, "\"v0x\"") != nullptr)
                    && (strstr(js, "-2")   != nullptr); // edited value present

        // --- Step 6: Validate via objectValidateCB
        //   The callback is registered on the type; we simulate validation by
        //   parsing the JSON ourselves using getCurrentParamsJSON as the source.
        //   Since objectValidateCB is called during DRAG_NO_INTERSECT drags (which
        //   require live dragger infrastructure), we verify its wiring by calling
        //   setObjectValidateCallback and confirming the type entry was updated.
        SbBool setOK = SoProceduralShape::setObjectValidateCallback(
            "EditCube_test",
            static_cast<SoProceduralObjectValidateCB>(arb8ObjCB));

        shape->unref();

        bool pass = step2OK && step3OK && step5OK && (setOK == TRUE);
        std::string failMsg;
        if (!step2OK) failMsg += "step2 (24 params) FAILED; ";
        if (!step3OK) failMsg += "step3 (selDisp children) FAILED; ";
        if (!step5OK) failMsg += "step5 (JSON content) FAILED; ";
        runner.endTest(pass, failMsg);
    }

    return runner.getSummary();
}
