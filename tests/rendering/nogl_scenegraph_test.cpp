#include <gtest/gtest.h>

#include <Inventor/SbLine.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbVec2f.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbViewVolume.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoPrimitiveVertex.h>
#include <Inventor/actions/SoCallbackAction.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoShape.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoTranslation.h>

namespace {

void countTriangles(void * userdata,
                    SoCallbackAction *,
                    const SoPrimitiveVertex *,
                    const SoPrimitiveVertex *,
                    const SoPrimitiveVertex *)
{
    ++*static_cast<int *>(userdata);
}

class NoOpenGLSceneGraph : public ::testing::Test {
protected:
    void SetUp() override
    {
        root = new SoSeparator;
        root->ref();

        camera = new SoPerspectiveCamera;
        camera->position.setValue(0.0f, 0.0f, 6.0f);
        camera->nearDistance = 0.1f;
        camera->farDistance = 100.0f;
        root->addChild(camera);
        root->addChild(new SoDirectionalLight);

        addShape(new SoSphere, -3.0f);
        addShape(new SoCube, -1.0f);
        addShape(new SoCone, 1.0f);
        addShape(new SoCylinder, 3.0f);
    }

    void TearDown() override
    {
        root->unref();
    }

    void addShape(SoNode * shape, float x)
    {
        SoSeparator * branch = new SoSeparator;
        SoTranslation * translation = new SoTranslation;
        translation->translation.setValue(x, 0.0f, 0.0f);
        branch->addChild(translation);
        branch->addChild(shape);
        root->addChild(branch);
    }

    SoSeparator * root = nullptr;
    SoPerspectiveCamera * camera = nullptr;
};

TEST_F(NoOpenGLSceneGraph, TraversalAndSearchRemainFunctional)
{
    int triangleCount = 0;
    SoCallbackAction callback;
    callback.addTriangleCallback(SoShape::getClassTypeId(), countTriangles,
                                 &triangleCount);
    callback.apply(root);
    EXPECT_GT(triangleCount, 0);

    SoSearchAction search;
    search.setType(SoSphere::getClassTypeId());
    search.setInterest(SoSearchAction::FIRST);
    search.apply(root);
    ASSERT_NE(search.getPath(), nullptr);
    EXPECT_TRUE(search.getPath()->getTail()->isOfType(
        SoSphere::getClassTypeId()));
}

TEST_F(NoOpenGLSceneGraph, CameraAndMatrixMathRemainFunctional)
{
    const SbViewportRegion viewport(320, 240);
    camera->viewAll(root, viewport);
    const SbViewVolume volume =
        camera->getViewVolume(viewport.getViewportAspectRatio());
    SbLine ray;
    volume.projectPointToLine(SbVec2f(0.5f, 0.5f), ray);
    EXPECT_LT(ray.getDirection()[2], -0.5f);

    const SbVec3f point(1.0f, 2.0f, 3.0f);
    SbVec3f transformed;
    SbMatrix::identity().multVecMatrix(point, transformed);
    EXPECT_LT((transformed - point).length(), 1.0e-5f);
}

} // namespace
