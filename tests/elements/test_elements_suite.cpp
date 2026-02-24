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
 * @file test_elements_suite.cpp
 * @brief Tests for Coin3D element classes.
 *
 * Covers:
 *   Static metadata  - getClassTypeId, getClassStackIndex, getDefault
 *   Node field tests - SoDrawStyle, SoComplexity, SoNormalBinding,
 *                      SoShapeHints, SoFont, SoPickStyle, SoUnits
 *   Scene traversal  - SoCallbackAction over a simple scene graph
 */

#include "../test_utils.h"

#include <Inventor/elements/SoDrawStyleElement.h>
#include <Inventor/elements/SoComplexityElement.h>
#include <Inventor/elements/SoComplexityTypeElement.h>
#include <Inventor/elements/SoNormalBindingElement.h>
#include <Inventor/elements/SoTextureCoordinateBindingElement.h>
#include <Inventor/elements/SoShapeHintsElement.h>
#include <Inventor/elements/SoPolygonOffsetElement.h>
#include <Inventor/elements/SoPointSizeElement.h>
#include <Inventor/elements/SoUnitsElement.h>
#include <Inventor/elements/SoFontNameElement.h>
#include <Inventor/elements/SoFontSizeElement.h>
#include <Inventor/elements/SoPickStyleElement.h>
#include <Inventor/elements/SoOverrideElement.h>
#include <Inventor/SoType.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/nodes/SoComplexity.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/nodes/SoTextureCoordinateBinding.h>
#include <Inventor/nodes/SoShapeHints.h>
#include <Inventor/nodes/SoPolygonOffset.h>
#include <Inventor/nodes/SoFont.h>
#include <Inventor/nodes/SoPickStyle.h>
#include <Inventor/nodes/SoUnits.h>
#include <Inventor/actions/SoCallbackAction.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/SbViewportRegion.h>

using namespace SimpleTest;

// ---------------------------------------------------------------------------
// Traversal callback — simply records that it was called
// ---------------------------------------------------------------------------
static int g_callbackCount = 0;

static SoCallbackAction::Response nodeCallback(void * /*data*/,
                                               SoCallbackAction * /*action*/,
                                               const SoNode * /*node*/)
{
    ++g_callbackCount;
    return SoCallbackAction::CONTINUE;
}

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // -----------------------------------------------------------------------
    // 1. SoDrawStyleElement::getClassTypeId
    // -----------------------------------------------------------------------
    runner.startTest("SoDrawStyleElement::getClassTypeId is not badType");
    {
        bool pass = (SoDrawStyleElement::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoDrawStyleElement has bad class type");
    }

    // -----------------------------------------------------------------------
    // 2. SoDrawStyleElement::getClassStackIndex
    // -----------------------------------------------------------------------
    runner.startTest("SoDrawStyleElement::getClassStackIndex >= 0");
    {
        bool pass = (SoDrawStyleElement::getClassStackIndex() >= 0);
        runner.endTest(pass, pass ? "" : "SoDrawStyleElement::getClassStackIndex < 0");
    }

    // -----------------------------------------------------------------------
    // 3. SoDrawStyleElement::getDefault == FILLED
    // -----------------------------------------------------------------------
    runner.startTest("SoDrawStyleElement::getDefault() == FILLED");
    {
        bool pass = (SoDrawStyleElement::getDefault() == SoDrawStyleElement::FILLED);
        runner.endTest(pass, pass ? "" :
            "SoDrawStyleElement default should be FILLED");
    }

    // -----------------------------------------------------------------------
    // 4. SoComplexityElement::getClassTypeId
    // -----------------------------------------------------------------------
    runner.startTest("SoComplexityElement::getClassTypeId is not badType");
    {
        bool pass = (SoComplexityElement::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoComplexityElement has bad class type");
    }

    // -----------------------------------------------------------------------
    // 5. SoComplexityElement::getDefault in range [0, 1]
    // -----------------------------------------------------------------------
    runner.startTest("SoComplexityElement::getDefault() is in [0, 1]");
    {
        float def = SoComplexityElement::getDefault();
        bool pass = (def >= 0.0f && def <= 1.0f);
        runner.endTest(pass, pass ? "" :
            "SoComplexityElement default should be in [0, 1]");
    }

    // -----------------------------------------------------------------------
    // 6. SoNormalBindingElement::getClassTypeId
    // -----------------------------------------------------------------------
    runner.startTest("SoNormalBindingElement::getClassTypeId is not badType");
    {
        bool pass = (SoNormalBindingElement::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoNormalBindingElement has bad class type");
    }

    // -----------------------------------------------------------------------
    // 7. SoNormalBindingElement::getDefault returns a valid binding value
    // -----------------------------------------------------------------------
    runner.startTest("SoNormalBindingElement::getDefault() is a valid binding");
    {
        SoNormalBindingElement::Binding def = SoNormalBindingElement::getDefault();
        // Valid bindings range from OVERALL to PER_VERTEX_INDEXED
        bool pass = (def >= SoNormalBindingElement::OVERALL &&
                     def <= SoNormalBindingElement::PER_VERTEX_INDEXED);
        runner.endTest(pass, pass ? "" :
            "SoNormalBindingElement::getDefault() out of valid binding range");
    }

    // -----------------------------------------------------------------------
    // 8. SoTextureCoordinateBindingElement::getClassTypeId
    // -----------------------------------------------------------------------
    runner.startTest("SoTextureCoordinateBindingElement::getClassTypeId is not badType");
    {
        bool pass = (SoTextureCoordinateBindingElement::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" :
            "SoTextureCoordinateBindingElement has bad class type");
    }

    // -----------------------------------------------------------------------
    // 9. SoShapeHintsElement::getClassTypeId
    // -----------------------------------------------------------------------
    runner.startTest("SoShapeHintsElement::getClassTypeId is not badType");
    {
        bool pass = (SoShapeHintsElement::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoShapeHintsElement has bad class type");
    }

    // -----------------------------------------------------------------------
    // 10. SoPolygonOffsetElement::getClassTypeId
    // -----------------------------------------------------------------------
    runner.startTest("SoPolygonOffsetElement::getClassTypeId is not badType");
    {
        bool pass = (SoPolygonOffsetElement::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoPolygonOffsetElement has bad class type");
    }

    // -----------------------------------------------------------------------
    // 11. SoPointSizeElement::getClassTypeId
    // -----------------------------------------------------------------------
    runner.startTest("SoPointSizeElement::getClassTypeId is not badType");
    {
        bool pass = (SoPointSizeElement::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoPointSizeElement has bad class type");
    }

    // -----------------------------------------------------------------------
    // 12. SoUnitsElement::getClassTypeId
    // -----------------------------------------------------------------------
    runner.startTest("SoUnitsElement::getClassTypeId is not badType");
    {
        bool pass = (SoUnitsElement::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoUnitsElement has bad class type");
    }

    // -----------------------------------------------------------------------
    // 13. SoFontNameElement::getClassTypeId
    // -----------------------------------------------------------------------
    runner.startTest("SoFontNameElement::getClassTypeId is not badType");
    {
        bool pass = (SoFontNameElement::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoFontNameElement has bad class type");
    }

    // -----------------------------------------------------------------------
    // 14. SoFontSizeElement::getClassTypeId
    // -----------------------------------------------------------------------
    runner.startTest("SoFontSizeElement::getClassTypeId is not badType");
    {
        bool pass = (SoFontSizeElement::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoFontSizeElement has bad class type");
    }

    // -----------------------------------------------------------------------
    // 15. SoPickStyleElement::getClassTypeId
    // -----------------------------------------------------------------------
    runner.startTest("SoPickStyleElement::getClassTypeId is not badType");
    {
        bool pass = (SoPickStyleElement::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoPickStyleElement has bad class type");
    }

    // -----------------------------------------------------------------------
    // 16. SoOverrideElement::getClassTypeId
    // -----------------------------------------------------------------------
    runner.startTest("SoOverrideElement::getClassTypeId is not badType");
    {
        bool pass = (SoOverrideElement::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoOverrideElement has bad class type");
    }

    // -----------------------------------------------------------------------
    // 17. SoDrawStyle node traversal: no crash, field value set correctly
    // -----------------------------------------------------------------------
    runner.startTest("SoDrawStyle traversal with SoCallbackAction does not crash");
    {
        SoSeparator * root = new SoSeparator;
        root->ref();

        SoDrawStyle * ds = new SoDrawStyle;
        ds->style = SoDrawStyle::LINES;
        root->addChild(ds);
        root->addChild(new SoCube);

        g_callbackCount = 0;
        SoCallbackAction cba;
        cba.addPreCallback(SoNode::getClassTypeId(), nodeCallback, nullptr);
        cba.apply(root);

        bool pass = (g_callbackCount > 0);
        root->unref();
        runner.endTest(pass, pass ? "" :
            "SoCallbackAction pre-callback was never called");
    }

    runner.startTest("SoDrawStyle::style field set to LINES");
    {
        SoDrawStyle * ds = new SoDrawStyle;
        ds->ref();
        ds->style = SoDrawStyle::LINES;
        bool pass = (ds->style.getValue() == SoDrawStyle::LINES);
        ds->unref();
        runner.endTest(pass, pass ? "" :
            "SoDrawStyle::style field should be LINES after assignment");
    }

    // -----------------------------------------------------------------------
    // 18. SoComplexity node: set complexity field, verify getValue
    // -----------------------------------------------------------------------
    runner.startTest("SoComplexity::complexity field set to 0.8f");
    {
        SoComplexity * cx = new SoComplexity;
        cx->ref();
        cx->value = 0.8f;
        bool pass = (cx->value.getValue() == 0.8f);
        cx->unref();
        runner.endTest(pass, pass ? "" :
            "SoComplexity::value field should be 0.8f after assignment");
    }

    // -----------------------------------------------------------------------
    // 19. SoNormalBinding node: set value field, verify getValue
    // -----------------------------------------------------------------------
    runner.startTest("SoNormalBinding::value field set to PER_FACE");
    {
        SoNormalBinding * nb = new SoNormalBinding;
        nb->ref();
        nb->value = SoNormalBinding::PER_FACE;
        bool pass = (nb->value.getValue() == SoNormalBinding::PER_FACE);
        nb->unref();
        runner.endTest(pass, pass ? "" :
            "SoNormalBinding::value field should be PER_FACE after assignment");
    }

    // -----------------------------------------------------------------------
    // 20. SoShapeHints: set vertexOrdering to COUNTERCLOCKWISE, verify
    // -----------------------------------------------------------------------
    runner.startTest("SoShapeHints::vertexOrdering set to COUNTERCLOCKWISE");
    {
        SoShapeHints * sh = new SoShapeHints;
        sh->ref();
        sh->vertexOrdering = SoShapeHints::COUNTERCLOCKWISE;
        bool pass = (sh->vertexOrdering.getValue() == SoShapeHints::COUNTERCLOCKWISE);
        sh->unref();
        runner.endTest(pass, pass ? "" :
            "SoShapeHints::vertexOrdering should be COUNTERCLOCKWISE after assignment");
    }

    // -----------------------------------------------------------------------
    // 21. SoFont: set name to "Helvetica" and size to 12.0f, verify fields
    // -----------------------------------------------------------------------
    runner.startTest("SoFont::name field set to Helvetica");
    {
        SoFont * font = new SoFont;
        font->ref();
        font->name = SbName("Helvetica");
        bool pass = (strcmp(font->name.getValue().getString(), "Helvetica") == 0);
        font->unref();
        runner.endTest(pass, pass ? "" :
            "SoFont::name field should be 'Helvetica' after assignment");
    }

    runner.startTest("SoFont::size field set to 12.0f");
    {
        SoFont * font = new SoFont;
        font->ref();
        font->size = 12.0f;
        bool pass = (font->size.getValue() == 12.0f);
        font->unref();
        runner.endTest(pass, pass ? "" :
            "SoFont::size field should be 12.0f after assignment");
    }

    // -----------------------------------------------------------------------
    // 22. SoPickStyle: set style to BOUNDING_BOX, verify field
    // -----------------------------------------------------------------------
    runner.startTest("SoPickStyle::style field set to BOUNDING_BOX");
    {
        SoPickStyle * ps = new SoPickStyle;
        ps->ref();
        ps->style = SoPickStyle::BOUNDING_BOX;
        bool pass = (ps->style.getValue() == SoPickStyle::BOUNDING_BOX);
        ps->unref();
        runner.endTest(pass, pass ? "" :
            "SoPickStyle::style field should be BOUNDING_BOX after assignment");
    }

    // -----------------------------------------------------------------------
    // 23. SoUnits: set units to MILLIMETERS, verify field
    // -----------------------------------------------------------------------
    runner.startTest("SoUnits::units field set to MILLIMETERS");
    {
        SoUnits * u = new SoUnits;
        u->ref();
        u->units = SoUnits::MILLIMETERS;
        bool pass = (u->units.getValue() == SoUnits::MILLIMETERS);
        u->unref();
        runner.endTest(pass, pass ? "" :
            "SoUnits::units field should be MILLIMETERS after assignment");
    }

    return runner.getSummary() != 0 ? 1 : 0;
}
