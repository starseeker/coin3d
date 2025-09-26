/*
 *
 *  Copyright (C) 2000 Silicon Graphics, Inc.  All Rights Reserved. 
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  Further, this software is distributed without any warranty that it is
 *  free of the rightful claim of any third person regarding infringement
 *  or the like.  Any license provided herein, whether implied or
 *  otherwise, applies only to this software file.  Patent licenses, if
 *  any, provided herein do not apply to combinations of this program with
 *  other software, or any other product whatsoever.
 * 
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 *  Mountain View, CA  94043, or:
 * 
 *  http://oss.sgi.com 
 * 
 *  For further information regarding this notice, see: 
 * 
 *  http://oss.sgi.com/projects/GenInfo/NoticeExplan/
 *
 */

/*--------------------------------------------------------------
 *  This is a headless adaptation from the Inventor Mentor
 *  chapter 13, example 6.
 *
 *  A calculator engine computes a closed, planar curve.
 *  The output from the engine is connected to the translation
 *  applied to a sphere object, which consequently moves
 *  along the path of the curve. This demonstrates field engines.
 *------------------------------------------------------------*/

#include <iostream>
#include <memory>
#include <cmath>

// OSMesa headers for headless rendering
#ifdef __has_include
  #if __has_include(<OSMesa/osmesa.h>)
    #include <OSMesa/osmesa.h>
    #include <OSMesa/gl.h>
    #define HAVE_OSMESA
  #endif
#endif

// Coin3D headers
#include <Inventor/SoDB.h>
#include <Inventor/SoInteraction.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/engines/SoCalculator.h>
#include <Inventor/engines/SoElapsedTime.h>
#include <Inventor/engines/SoTimeCounter.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoRotationXYZ.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/SbViewportRegion.h>

#ifdef HAVE_OSMESA

// OSMesa context wrapper
struct OSMesaContextData {
    OSMesaContext context;
    std::unique_ptr<unsigned char[]> buffer;
    int width, height;
    
    OSMesaContextData(int w, int h) : width(w), height(h) {
        context = OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, NULL);
        if (context) {
            buffer = std::make_unique<unsigned char[]>(width * height * 4);
        }
    }
    
    ~OSMesaContextData() {
        if (context) OSMesaDestroyContext(context);
    }
    
    bool makeCurrent() {
        if (!context || !buffer) return false;
        
        bool result = OSMesaMakeCurrent(context, buffer.get(), GL_UNSIGNED_BYTE, width, height);
        if (result) {
            // Set Y-axis orientation for proper image output
            OSMesaPixelStore(OSMESA_Y_UP, 0);
        }
        return result;
    }
    
    bool isValid() const { return context != nullptr; }
    
    const unsigned char* getBuffer() const { return buffer.get(); }
};

// OSMesa Context Manager for Coin3D
class OSMesaContextManager : public SoDB::ContextManager {
public:
    virtual void* createOffscreenContext(unsigned int width, unsigned int height) override {
        auto* ctx = new OSMesaContextData(width, height);
        return ctx->isValid() ? ctx : (delete ctx, nullptr);
    }
    
    virtual SbBool makeContextCurrent(void* context) override {
        return context && static_cast<OSMesaContextData*>(context)->makeCurrent() ? TRUE : FALSE;
    }
    
    virtual void restorePreviousContext(void* context) override {
        // OSMesa doesn't need context stacking for single-threaded use
        (void)context;
    }
    
    virtual void destroyContext(void* context) override {
        delete static_cast<OSMesaContextData*>(context);
    }
};

// Save RGB buffer to RGB file using built-in SGI RGB format
bool saveRGB(const std::string& filename, SoOffscreenRenderer* renderer) {
    SbBool result = renderer->writeToRGB(filename.c_str());
    if (result) {
        std::cout << "RGB saved to: " << filename << std::endl;
        return true;
    } else {
        std::cerr << "Error: Could not save RGB file " << filename << std::endl;
        return false;
    }
}

#endif // HAVE_OSMESA

int
main(int argc, char **argv)
{
#ifdef HAVE_OSMESA
    // Initialize Coin3D with OSMesa context management
    std::unique_ptr<OSMesaContextManager> context_manager = std::make_unique<OSMesaContextManager>();
    SoDB::init(context_manager.get());
    SoInteraction::init();
    
    std::cout << "Calculator Engine: Rose Curve - Headless OSMesa Version" << std::endl;

    SoSeparator *root = new SoSeparator;
    root->ref();

    // Add a camera and light
    SoPerspectiveCamera *myCamera = new SoPerspectiveCamera;
    myCamera->position.setValue(0, 0, 20.0);
    myCamera->nearDistance = 10.0;
    myCamera->farDistance = 30.0;
    root->addChild(myCamera);
    root->addChild(new SoDirectionalLight);

    // Rotate scene slightly to get better view
    SoRotationXYZ *globalRotXYZ = new SoRotationXYZ;
    globalRotXYZ->axis = SoRotationXYZ::X;
    globalRotXYZ->angle = M_PI/7;
    root->addChild(globalRotXYZ);

/////////////////////////////////////////////////////////////
// CODE FOR The Inventor Mentor STARTS HERE  

    // Create a path of cubes to show the trail
    SoSeparator *pathGroup = new SoSeparator;
    root->addChild(pathGroup);
    SoMaterial *pathMaterial = new SoMaterial;
    pathMaterial->diffuseColor.setValue(0.3, 0.3, 0.8);
    pathMaterial->transparency = 0.7;
    pathGroup->addChild(pathMaterial);

    // Draw multiple positions along the curve to show the path
    for (int i = 0; i < 72; i += 4) {  // Every 5 degrees
        float theta = i * M_PI / 180.0f;
        float r = 5.0f * cos(5 * theta);
        float x = r * cos(theta);
        float z = r * sin(theta);
        
        SoSeparator *cubeGroup = new SoSeparator;
        pathGroup->addChild(cubeGroup);
        
        SoTransform *cubeTransform = new SoTransform;
        cubeTransform->translation.setValue(x, 0, z);
        cubeTransform->scaleFactor.setValue(0.2f, 0.2f, 0.2f);
        cubeGroup->addChild(cubeTransform);
        cubeGroup->addChild(new SoCube);
    }

    // Moving object group
    SoSeparator *movingGroup = new SoSeparator;
    root->addChild(movingGroup);

    // Set up the object transformations
    SoTranslation *danceTranslation = new SoTranslation;
    SoTransform *initialTransform = new SoTransform;
    movingGroup->addChild(danceTranslation);
    initialTransform->scaleFactor.setValue(0.5f, 0.5f, 0.5f);
    movingGroup->addChild(initialTransform);
    
    // Add material for the moving sphere
    SoMaterial *sphereMaterial = new SoMaterial;
    sphereMaterial->diffuseColor.setValue(1.0, 0.3, 0.3);  // Red
    movingGroup->addChild(sphereMaterial);
    movingGroup->addChild(new SoSphere);

    // Set up an engine to calculate the motion path:
    // r = 5*cos(5*theta); x = r*cos(theta); z = r*sin(theta)
    // Theta is incremented using a time counter engine,
    // and converted to radians using an expression in
    // the calculator engine.
    SoCalculator *calcXZ = new SoCalculator; 
    calcXZ->ref();
    SoTimeCounter *thetaCounter = new SoTimeCounter;
    thetaCounter->ref();

    thetaCounter->max = 360;
    thetaCounter->step = 4;
    thetaCounter->frequency = 0.075f;

    calcXZ->a.connectFrom(&thetaCounter->output);    
    calcXZ->expression.set1Value(0, "ta=a*M_PI/180"); // theta
    calcXZ->expression.set1Value(1, "tb=5*cos(5*ta)"); // r
    calcXZ->expression.set1Value(2, "td=tb*cos(ta)"); // x 
    calcXZ->expression.set1Value(3, "te=tb*sin(ta)"); // z 
    calcXZ->expression.set1Value(4, "oA=vec3f(td,0,te)"); 
    danceTranslation->translation.connectFrom(&calcXZ->oA);

// CODE FOR The Inventor Mentor ENDS HERE
/////////////////////////////////////////////////////////////

    // Set up offscreen renderer with reasonable size
    const int width = 512;
    const int height = 512;
    SbViewportRegion viewport(width, height);
    SoOffscreenRenderer renderer(viewport);
    renderer.setBackgroundColor(SbColor(0.1f, 0.1f, 0.1f)); // Dark background

    // Make camera see everything
    myCamera->viewAll(root, viewport);

    // Force an evaluation by triggering the engines
    // This sets a specific time for the snapshot
    SoDB::getSensorManager()->processTimerQueue();
    SoDB::getSensorManager()->processDelayQueue(FALSE);
    
    // Render the scene at a specific moment
    SbBool success = renderer.render(root);

    if (success) {
        // Determine output filename
        std::string filename = "Calculator.rgb";
        if (argc > 1) {
            filename = argv[1];
        }
        
        // Save to RGB file using built-in SGI RGB format
        if (saveRGB(filename, &renderer)) {
            std::cout << "Successfully rendered calculator engine rose curve to " << filename << std::endl;
        } else {
            std::cerr << "Error saving RGB file" << std::endl;
            root->unref();
            calcXZ->unref();
            thetaCounter->unref();
            return 1;
        }
    } else {
        std::cerr << "Error: Failed to render scene" << std::endl;
        root->unref();
        calcXZ->unref();
        thetaCounter->unref();
        return 1;
    }

    // Clean up
    root->unref();
    calcXZ->unref();
    thetaCounter->unref();

    return 0;
    
#else
    std::cerr << "Error: OSMesa support not available. Cannot run headless rendering." << std::endl;
    return 1;
#endif
}
