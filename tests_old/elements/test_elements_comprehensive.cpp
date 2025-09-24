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

#include "utils/test_common.h"
#include <Inventor/elements/SoElement.h>
#include <Inventor/elements/SoModelMatrixElement.h>
#include <Inventor/elements/SoViewingMatrixElement.h>
#include <Inventor/elements/SoProjectionMatrixElement.h>
#include <Inventor/elements/SoLazyElement.h>
#include <Inventor/elements/SoLightModelElement.h>
#include <Inventor/elements/SoEnvironmentElement.h>
#include <Inventor/elements/SoMaterialBindingElement.h>
#include <Inventor/elements/SoNormalBindingElement.h>
#include <Inventor/elements/SoViewVolumeElement.h>
#include <Inventor/elements/SoViewportRegionElement.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbViewVolume.h>
#include <Inventor/SbViewportRegion.h>

using namespace CoinTestUtils;

TEST_CASE("SoElement basic functionality", "[elements][SoElement]") {
    CoinTestFixture fixture;
    
    SECTION("Element type system") {
        // Test element type identification
        SoType element_type = SoElement::getClassTypeId();
        CHECK(element_type != SoType::badType());
        // Note: Type names may include class prefixes
        
        // Test specific element types
        SoType model_matrix_type = SoModelMatrixElement::getClassTypeId();
        CHECK(model_matrix_type.isDerivedFrom(element_type));
        // Note: Type names may include class prefixes
    }
    
    SECTION("Element initialization") {
        // Elements should be properly initialized
        SoType model_type = SoModelMatrixElement::getClassTypeId();
        CHECK(model_type != SoType::badType());
        
        SoType lazy_type = SoLazyElement::getClassTypeId();
        CHECK(lazy_type != SoType::badType());
    }
}

TEST_CASE("SoModelMatrixElement functionality", "[elements][SoModelMatrixElement]") {
    CoinTestFixture fixture;
    
    SECTION("Model matrix manipulation") {
        SoGLRenderAction action(SbViewportRegion(100, 100));
        SoState* state = action.getState();
        
        // Get initial matrix (should be identity)
        const SbMatrix& initial_matrix = SoModelMatrixElement::get(state);
        SbMatrix identity;
        identity.makeIdentity();
        
        // Check if matrix is close to identity
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                CHECK(fabs(initial_matrix[i][j] - identity[i][j]) < 1e-6);
            }
        }
        
        // Set a translation matrix
        SbMatrix translation;
        translation.setTranslate(SbVec3f(1.0f, 2.0f, 3.0f));
        SoModelMatrixElement::set(state, nullptr, translation);
        
        const SbMatrix& new_matrix = SoModelMatrixElement::get(state);
        CHECK(fabs(new_matrix[3][0] - 1.0f) < 1e-6);
        CHECK(fabs(new_matrix[3][1] - 2.0f) < 1e-6);
        CHECK(fabs(new_matrix[3][2] - 3.0f) < 1e-6);
    }
    
    SECTION("Matrix multiplication") {
        SoGLRenderAction action(SbViewportRegion(100, 100));
        SoState* state = action.getState();
        
        // Apply translation
        SbMatrix translation;
        translation.setTranslate(SbVec3f(1.0f, 0.0f, 0.0f));
        SoModelMatrixElement::mult(state, nullptr, translation);
        
        const SbMatrix& result = SoModelMatrixElement::get(state);
        
        // Verify transformation was applied
        CHECK(result != SbMatrix::identity());
    }
}

TEST_CASE("SoLazyElement functionality", "[elements][SoLazyElement]") {
    CoinTestFixture fixture;
    
    SECTION("Lazy element properties") {
        SoGLRenderAction action(SbViewportRegion(100, 100));
        SoState* state = action.getState();
        
        // Test that lazy element can be accessed
        // SoLazyElement handles material properties in modern Coin
        const SoLazyElement* lazy = SoLazyElement::getInstance(state);
        CHECK(lazy != nullptr);
        
        // Basic test that element exists and can be used
        // Detailed functionality depends on internal implementation
    }
}

TEST_CASE("SoLightModelElement functionality", "[elements][SoLightModelElement]") {
    CoinTestFixture fixture;
    
    SECTION("Light model settings") {
        SoGLRenderAction action(SbViewportRegion(100, 100));
        SoState* state = action.getState();
        
        // Test setting light model
        SoLightModelElement::set(state, SoLightModelElement::PHONG);
        SoLightModelElement::Model model = SoLightModelElement::get(state);
        CHECK(model == SoLightModelElement::PHONG);
        
        // Test different light model
        SoLightModelElement::set(state, SoLightModelElement::BASE_COLOR);
        model = SoLightModelElement::get(state);
        CHECK(model == SoLightModelElement::BASE_COLOR);
    }
}

TEST_CASE("SoEnvironmentElement functionality", "[elements][SoEnvironmentElement]") {
    CoinTestFixture fixture;
    
    SECTION("Environment settings") {
        SoGLRenderAction action(SbViewportRegion(100, 100));
        SoState* state = action.getState();
        
        // Set environment properties
        float ambient_intensity = 0.3f;
        SbColor ambient_color(0.2f, 0.2f, 0.2f);
        SbVec3f attenuation(1.0f, 0.0f, 0.0f);
        SoEnvironmentElement::FogType fog_type = SoEnvironmentElement::NONE;
        SbColor fog_color(1, 1, 1);
        
        SoEnvironmentElement::set(state, nullptr, ambient_intensity, ambient_color,
                                  attenuation, fog_type, fog_color, 0.0f, 0.0f);
        
        // Verify environment was set
        float result_intensity;
        SbColor result_color;
        SbVec3f result_attenuation;
        int32_t result_fog;
        SbColor result_fog_color;
        float fog_visibility, fog_start;
        
        SoEnvironmentElement::get(state, result_intensity, result_color,
                                  result_attenuation, result_fog,
                                  result_fog_color, fog_visibility, fog_start);
        
        CHECK(fabs(result_intensity - ambient_intensity) < 1e-6);
        CHECK(fabs(result_color[0] - ambient_color[0]) < 1e-6);
        CHECK(result_fog == (int32_t)fog_type);
    }
}

TEST_CASE("SoViewVolumeElement functionality", "[elements][SoViewVolumeElement]") {
    CoinTestFixture fixture;
    
    SECTION("View volume settings") {
        SoGLRenderAction action(SbViewportRegion(100, 100));
        SoState* state = action.getState();
        
        // Create a perspective view volume
        SbViewVolume vv;
        vv.perspective(45.0f * M_PI / 180.0f, 1.0f, 1.0f, 10.0f);
        
        SoViewVolumeElement::set(state, nullptr, vv);
        
        const SbViewVolume& result = SoViewVolumeElement::get(state);
        
        // Basic verification that view volume was set
        CHECK(result.getProjectionType() == SbViewVolume::PERSPECTIVE);
        CHECK(fabs(result.getNearDist() - 1.0f) < 1e-6);
    }
}

TEST_CASE("SoViewportRegionElement functionality", "[elements][SoViewportRegionElement]") {
    CoinTestFixture fixture;
    
    SECTION("Viewport region settings") {
        SoGLRenderAction action(SbViewportRegion(100, 100));
        SoState* state = action.getState();
        
        // Set viewport region
        SbViewportRegion vp(200, 150);
        SoViewportRegionElement::set(state, vp);
        
        const SbViewportRegion& result = SoViewportRegionElement::get(state);
        
        CHECK(result.getViewportSizePixels()[0] == 200);
        CHECK(result.getViewportSizePixels()[1] == 150);
    }
}

TEST_CASE("SoMaterialBindingElement functionality", "[elements][SoMaterialBindingElement]") {
    CoinTestFixture fixture;
    
    SECTION("Material binding modes") {
        SoGLRenderAction action(SbViewportRegion(100, 100));
        SoState* state = action.getState();
        
        // Test different binding modes
        SoMaterialBindingElement::set(state, nullptr, SoMaterialBindingElement::PER_VERTEX);
        SoMaterialBindingElement::Binding binding = SoMaterialBindingElement::get(state);
        CHECK(binding == SoMaterialBindingElement::PER_VERTEX);
        
        SoMaterialBindingElement::set(state, nullptr, SoMaterialBindingElement::PER_FACE);
        binding = SoMaterialBindingElement::get(state);
        CHECK(binding == SoMaterialBindingElement::PER_FACE);
        
        SoMaterialBindingElement::set(state, nullptr, SoMaterialBindingElement::OVERALL);
        binding = SoMaterialBindingElement::get(state);
        CHECK(binding == SoMaterialBindingElement::OVERALL);
    }
}

TEST_CASE("SoNormalBindingElement functionality", "[elements][SoNormalBindingElement]") {
    CoinTestFixture fixture;
    
    SECTION("Normal binding modes") {
        SoGLRenderAction action(SbViewportRegion(100, 100));
        SoState* state = action.getState();
        
        // Test different binding modes
        SoNormalBindingElement::set(state, nullptr, SoNormalBindingElement::PER_VERTEX);
        SoNormalBindingElement::Binding binding = SoNormalBindingElement::get(state);
        CHECK(binding == SoNormalBindingElement::PER_VERTEX);
        
        SoNormalBindingElement::set(state, nullptr, SoNormalBindingElement::PER_FACE);
        binding = SoNormalBindingElement::get(state);
        CHECK(binding == SoNormalBindingElement::PER_FACE);
    }
}

TEST_CASE("Element state stack behavior", "[elements][state_stack]") {
    CoinTestFixture fixture;
    
    SECTION("State push/pop with model matrix") {
        SoGLRenderAction action(SbViewportRegion(100, 100));
        SoState* state = action.getState();
        
        // Set initial matrix
        SbMatrix initial;
        initial.setTranslate(SbVec3f(1.0f, 0.0f, 0.0f));
        SoModelMatrixElement::set(state, nullptr, initial);
        
        // Push state
        state->push();
        
        // Modify matrix
        SbMatrix modification;
        modification.setTranslate(SbVec3f(0.0f, 1.0f, 0.0f));
        SoModelMatrixElement::mult(state, nullptr, modification);
        
        // Pop state
        state->pop();
        
        const SbMatrix& restored = SoModelMatrixElement::get(state);
        
        // Verify state was restored
        CHECK(restored[3][0] == initial[3][0]);
        CHECK(restored[3][1] == initial[3][1]);
        CHECK(restored[3][2] == initial[3][2]);
    }
    
    SECTION("State push/pop with lazy element") {
        SoGLRenderAction action(SbViewportRegion(100, 100));
        SoState* state = action.getState();
        
        // Push state
        state->push();
        
        // Get lazy element
        const SoLazyElement* lazy = SoLazyElement::getInstance(state);
        CHECK(lazy != nullptr);
        
        // Pop state
        state->pop();
        
        // Verify element still accessible
        const SoLazyElement* restored_lazy = SoLazyElement::getInstance(state);
        CHECK(restored_lazy != nullptr);
    }
}

TEST_CASE("Element dependencies and interactions", "[elements][dependencies]") {
    CoinTestFixture fixture;
    
    SECTION("Matrix elements interaction") {
        SoGLRenderAction action(SbViewportRegion(100, 100));
        SoState* state = action.getState();
        
        // Set model matrix
        SbMatrix model;
        model.setTranslate(SbVec3f(1.0f, 2.0f, 3.0f));
        SoModelMatrixElement::set(state, nullptr, model);
        
        // Set viewing matrix
        SbMatrix viewing;
        viewing.setTranslate(SbVec3f(-5.0f, 0.0f, -10.0f));
        SoViewingMatrixElement::set(state, nullptr, viewing);
        
        // Verify both matrices are set independently
        const SbMatrix& result_model = SoModelMatrixElement::get(state);
        const SbMatrix& result_viewing = SoViewingMatrixElement::get(state);
        
        CHECK(fabs(result_model[3][0] - 1.0f) < 1e-6);
        CHECK(fabs(result_viewing[3][0] - (-5.0f)) < 1e-6);
    }
}