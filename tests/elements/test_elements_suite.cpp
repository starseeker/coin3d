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

using namespace ObolTest;

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

TEST(ElementsSuite, SoDrawStyleElementGetClassTypeIdIsNotBadType)
{
    bool pass = (SoDrawStyleElement::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoDrawStyleElement has bad class type";
}

// -----------------------------------------------------------------------
// 2. SoDrawStyleElement::getClassStackIndex
// -----------------------------------------------------------------------

TEST(ElementsSuite, SoDrawStyleElementGetClassStackIndex0)
{
    bool pass = (SoDrawStyleElement::getClassStackIndex() >= 0);
    EXPECT_TRUE(pass) << "SoDrawStyleElement::getClassStackIndex < 0";
}

// -----------------------------------------------------------------------
// 3. SoDrawStyleElement::getDefault == FILLED
// -----------------------------------------------------------------------

TEST(ElementsSuite, SoDrawStyleElementGetDefaultFILLED)
{
    bool pass = (SoDrawStyleElement::getDefault() == SoDrawStyleElement::FILLED);
    EXPECT_TRUE(pass) << "SoDrawStyleElement default should be FILLED";
}

// -----------------------------------------------------------------------
// 4. SoComplexityElement::getClassTypeId
// -----------------------------------------------------------------------

TEST(ElementsSuite, SoComplexityElementGetClassTypeIdIsNotBadType)
{
    bool pass = (SoComplexityElement::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoComplexityElement has bad class type";
}

// -----------------------------------------------------------------------
// 5. SoComplexityElement::getDefault in range [0, 1]
// -----------------------------------------------------------------------

TEST(ElementsSuite, SoComplexityElementGetDefaultIsIn01)
{
    float def = SoComplexityElement::getDefault();
    bool pass = (def >= 0.0f && def <= 1.0f);
    EXPECT_TRUE(pass) << "SoComplexityElement default should be in [0, 1]";
}

// -----------------------------------------------------------------------
// 6. SoNormalBindingElement::getClassTypeId
// -----------------------------------------------------------------------

TEST(ElementsSuite, SoNormalBindingElementGetClassTypeIdIsNotBadType)
{
    bool pass = (SoNormalBindingElement::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoNormalBindingElement has bad class type";
}

// -----------------------------------------------------------------------
// 7. SoNormalBindingElement::getDefault returns a valid binding value
// -----------------------------------------------------------------------

TEST(ElementsSuite, SoNormalBindingElementGetDefaultIsAValidBinding)
{
    SoNormalBindingElement::Binding def = SoNormalBindingElement::getDefault();
    // Valid bindings range from OVERALL to PER_VERTEX_INDEXED
    bool pass = (def >= SoNormalBindingElement::OVERALL &&
                 def <= SoNormalBindingElement::PER_VERTEX_INDEXED);
    EXPECT_TRUE(pass) << "SoNormalBindingElement::getDefault() out of valid binding range";
}

// -----------------------------------------------------------------------
// 8. SoTextureCoordinateBindingElement::getClassTypeId
// -----------------------------------------------------------------------

TEST(ElementsSuite, SoTextureCoordinateBindingElementGetClassTypeIdIsNotBadType)
{
    bool pass = (SoTextureCoordinateBindingElement::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoTextureCoordinateBindingElement has bad class type";
}

// -----------------------------------------------------------------------
// 9. SoShapeHintsElement::getClassTypeId
// -----------------------------------------------------------------------

TEST(ElementsSuite, SoShapeHintsElementGetClassTypeIdIsNotBadType)
{
    bool pass = (SoShapeHintsElement::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoShapeHintsElement has bad class type";
}

// -----------------------------------------------------------------------
// 10. SoPolygonOffsetElement::getClassTypeId
// -----------------------------------------------------------------------

TEST(ElementsSuite, SoPolygonOffsetElementGetClassTypeIdIsNotBadType)
{
    bool pass = (SoPolygonOffsetElement::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoPolygonOffsetElement has bad class type";
}

// -----------------------------------------------------------------------
// 11. SoPointSizeElement::getClassTypeId
// -----------------------------------------------------------------------

TEST(ElementsSuite, SoPointSizeElementGetClassTypeIdIsNotBadType)
{
    bool pass = (SoPointSizeElement::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoPointSizeElement has bad class type";
}

// -----------------------------------------------------------------------
// 12. SoUnitsElement::getClassTypeId
// -----------------------------------------------------------------------

TEST(ElementsSuite, SoUnitsElementGetClassTypeIdIsNotBadType)
{
    bool pass = (SoUnitsElement::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoUnitsElement has bad class type";
}

// -----------------------------------------------------------------------
// 13. SoFontNameElement::getClassTypeId
// -----------------------------------------------------------------------

TEST(ElementsSuite, SoFontNameElementGetClassTypeIdIsNotBadType)
{
    bool pass = (SoFontNameElement::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoFontNameElement has bad class type";
}

// -----------------------------------------------------------------------
// 14. SoFontSizeElement::getClassTypeId
// -----------------------------------------------------------------------

TEST(ElementsSuite, SoFontSizeElementGetClassTypeIdIsNotBadType)
{
    bool pass = (SoFontSizeElement::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoFontSizeElement has bad class type";
}

// -----------------------------------------------------------------------
// 15. SoPickStyleElement::getClassTypeId
// -----------------------------------------------------------------------

TEST(ElementsSuite, SoPickStyleElementGetClassTypeIdIsNotBadType)
{
    bool pass = (SoPickStyleElement::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoPickStyleElement has bad class type";
}

// -----------------------------------------------------------------------
// 16. SoOverrideElement::getClassTypeId
// -----------------------------------------------------------------------

TEST(ElementsSuite, SoOverrideElementGetClassTypeIdIsNotBadType)
{
    bool pass = (SoOverrideElement::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoOverrideElement has bad class type";
}

// -----------------------------------------------------------------------
// 17. SoDrawStyle node traversal: no crash, field value set correctly
// -----------------------------------------------------------------------

TEST(ElementsSuite, SoDrawStyleTraversalWithSoCallbackActionDoesNotCrash)
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
    EXPECT_TRUE(pass) << "SoCallbackAction pre-callback was never called";
}

TEST(ElementsSuite, SoDrawStyleStyleFieldSetToLINES)
{
    SoDrawStyle * ds = new SoDrawStyle;
    ds->ref();
    ds->style = SoDrawStyle::LINES;
    bool pass = (ds->style.getValue() == SoDrawStyle::LINES);
    ds->unref();
    EXPECT_TRUE(pass) << "SoDrawStyle::style field should be LINES after assignment";
}

// -----------------------------------------------------------------------
// 18. SoComplexity node: set complexity field, verify getValue
// -----------------------------------------------------------------------

TEST(ElementsSuite, SoComplexityComplexityFieldSetTo08f)
{
    SoComplexity * cx = new SoComplexity;
    cx->ref();
    cx->value = 0.8f;
    bool pass = (cx->value.getValue() == 0.8f);
    cx->unref();
    EXPECT_TRUE(pass) << "SoComplexity::value field should be 0.8f after assignment";
}

// -----------------------------------------------------------------------
// 19. SoNormalBinding node: set value field, verify getValue
// -----------------------------------------------------------------------

TEST(ElementsSuite, SoNormalBindingValueFieldSetToPERFACE)
{
    SoNormalBinding * nb = new SoNormalBinding;
    nb->ref();
    nb->value = SoNormalBinding::PER_FACE;
    bool pass = (nb->value.getValue() == SoNormalBinding::PER_FACE);
    nb->unref();
    EXPECT_TRUE(pass) << "SoNormalBinding::value field should be PER_FACE after assignment";
}

// -----------------------------------------------------------------------
// 20. SoShapeHints: set vertexOrdering to COUNTERCLOCKWISE, verify
// -----------------------------------------------------------------------

TEST(ElementsSuite, SoShapeHintsVertexOrderingSetToCOUNTERCLOCKWISE)
{
    SoShapeHints * sh = new SoShapeHints;
    sh->ref();
    sh->vertexOrdering = SoShapeHints::COUNTERCLOCKWISE;
    bool pass = (sh->vertexOrdering.getValue() == SoShapeHints::COUNTERCLOCKWISE);
    sh->unref();
    EXPECT_TRUE(pass) << "SoShapeHints::vertexOrdering should be COUNTERCLOCKWISE after assignment";
}

// -----------------------------------------------------------------------
// 21. SoFont: set name to "Helvetica" and size to 12.0f, verify fields
// -----------------------------------------------------------------------

TEST(ElementsSuite, SoFontNameFieldSetToHelvetica)
{
    SoFont * font = new SoFont;
    font->ref();
    font->name = SbName("Helvetica");
    bool pass = (strcmp(font->name.getValue().getString(), "Helvetica") == 0);
    font->unref();
    EXPECT_TRUE(pass) << "SoFont::name field should be 'Helvetica' after assignment";
}

TEST(ElementsSuite, SoFontSizeFieldSetTo120f)
{
    SoFont * font = new SoFont;
    font->ref();
    font->size = 12.0f;
    bool pass = (font->size.getValue() == 12.0f);
    font->unref();
    EXPECT_TRUE(pass) << "SoFont::size field should be 12.0f after assignment";
}

// -----------------------------------------------------------------------
// 22. SoPickStyle: set style to BOUNDING_BOX, verify field
// -----------------------------------------------------------------------

TEST(ElementsSuite, SoPickStyleStyleFieldSetToBOUNDINGBOX)
{
    SoPickStyle * ps = new SoPickStyle;
    ps->ref();
    ps->style = SoPickStyle::BOUNDING_BOX;
    bool pass = (ps->style.getValue() == SoPickStyle::BOUNDING_BOX);
    ps->unref();
    EXPECT_TRUE(pass) << "SoPickStyle::style field should be BOUNDING_BOX after assignment";
}

// -----------------------------------------------------------------------
// 23. SoUnits: set units to MILLIMETERS, verify field
// -----------------------------------------------------------------------

TEST(ElementsSuite, SoUnitsUnitsFieldSetToMILLIMETERS)
{
    SoUnits * u = new SoUnits;
    u->ref();
    u->units = SoUnits::MILLIMETERS;
    bool pass = (u->units.getValue() == SoUnits::MILLIMETERS);
    u->unref();
    EXPECT_TRUE(pass) << "SoUnits::units field should be MILLIMETERS after assignment";
}
