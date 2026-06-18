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
 *  http://www.sgi.com
 *
 *  For further information regarding this notice, see:
 *
 *  http://oss.sgi.com/projects/GenInfo/NoticeExplan/
 *
 */

/*
 * Headless version of Inventor Mentor example 6.1
 * 
 * Original: Text - renders globe with 2D text labels
 * Headless: Renders sphere with 2D text from multiple angles
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cstdio>

namespace {

obol::Transform translation(float x, float y, float z)
{
    obol::Transform transform;
    transform.translation = {x, y, z};
    return transform;
}

obol::Text2D label(const char * text)
{
    obol::Text2D result;
    result.text = text;
    result.fontName = "Times";
    result.fontSize = 24.0f;
    return result;
}

bool renderView(obol::OffscreenRenderer & renderer,
                obol::Scene & scene,
                const char * filename,
                const obol::Vec3 & position)
{
    obol::PerspectiveCamera camera;
    camera.position = position;
    camera.target = {0.0f, 0.0f, 0.0f};
    camera.verticalFieldOfViewRadians = 0.55f;
    scene.setCamera(camera);
    const obol::FrameResult result = renderer.render(scene);
    return result.success && renderer.writeRGB(filename);
}

} // namespace

int main(int argc, char **argv)
{
    initCoinHeadless();

    obol::Scene scene;
    obol::DirectionalLight light;
    light.direction = {-0.5f, -0.7f, -1.0f};
    scene.addDirectionalLight(light);
    scene.addPrimitive(obol::Primitive::Sphere);
    scene.addText2D(label("AFRICA"), obol::Material{}, translation(0.25f, 0.0f, 1.25f));
    scene.addText2D(label("ASIA"), obol::Material{}, translation(0.8f, 0.6f, 0.5f));

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::OffscreenRenderer renderer(backend, target);
    renderer.setBackgroundColor({0.0f, 0.0f, 0.0f, 1.0f});

    const char *baseFilename = (argc > 1) ? argv[1] : "06.1.Text";
    char filename[256];

    snprintf(filename, sizeof(filename), "%s_front.rgb", baseFilename);
    if (!renderView(renderer, scene, filename, {0.0f, 0.0f, 5.0f})) {
        fprintf(stderr, "Error: Failed to render front Text view with Obol v2 API\n");
        return 1;
    }

    snprintf(filename, sizeof(filename), "%s_angle.rgb", baseFilename);
    if (!renderView(renderer, scene, filename, {3.0f, 2.0f, 4.0f})) {
        fprintf(stderr, "Error: Failed to render angled Text view with Obol v2 API\n");
        return 1;
    }

    printf("Rendered globe Text2 labels [Obol v2]\n");
    return 0;
}
