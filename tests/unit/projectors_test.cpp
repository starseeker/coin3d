#include <gtest/gtest.h>

#include <Inventor/SbCylinder.h>
#include <Inventor/SbLine.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbPlane.h>
#include <Inventor/SbSphere.h>
#include <Inventor/SbVec2f.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbViewVolume.h>
#include <Inventor/projectors/SbCylinderPlaneProjector.h>
#include <Inventor/projectors/SbCylinderSheetProjector.h>
#include <Inventor/projectors/SbLineProjector.h>
#include <Inventor/projectors/SbPlaneProjector.h>
#include <Inventor/projectors/SbSpherePlaneProjector.h>
#include <Inventor/projectors/SbSphereSheetProjector.h>

#include <cmath>

namespace {

SbViewVolume test_view_volume()
{
    SbViewVolume view;
    view.perspective(1.0471975512f, 1.0f, 0.1f, 100.0f);
    return view;
}

void expect_finite_rotation(const SbRotation & rotation)
{
    SbVec3f axis;
    float angle = 0.0f;
    rotation.getValue(axis, angle);
    EXPECT_TRUE(std::isfinite(angle));
    EXPECT_TRUE(std::isfinite(axis[0]));
    EXPECT_TRUE(std::isfinite(axis[1]));
    EXPECT_TRUE(std::isfinite(axis[2]));
}

TEST(Projectors, PlaneAndLineProjectorsRetainConfigurationAndCopies)
{
    const SbViewVolume view = test_view_volume();
    const SbMatrix identity = SbMatrix::identity();

    SbPlaneProjector plane_projector(SbPlane(SbVec3f(0, 0, 1), -5.0f), FALSE);
    plane_projector.setViewVolume(view);
    plane_projector.setWorkingSpace(identity);
    EXPECT_FALSE(plane_projector.isOrientToEye());
    EXPECT_NEAR(plane_projector.getPlane().getNormal()[2], 1.0f, 1e-5f);
    const SbVec3f plane_point = plane_projector.project(SbVec2f(0.5f, 0.5f));
    EXPECT_TRUE(std::isfinite(plane_point[0]));
    EXPECT_NEAR(plane_point[2], -5.0f, 1e-3f);
    plane_projector.setStartPosition(SbVec2f(0.4f, 0.5f));
    EXPECT_TRUE(std::isfinite(plane_projector.getVector(SbVec2f(0.6f, 0.5f))[0]));
    auto * plane_copy = static_cast<SbPlaneProjector *>(plane_projector.copy());
    ASSERT_NE(plane_copy, nullptr);
    EXPECT_NEAR(plane_copy->getPlane().getDistanceFromOrigin(), -5.0f, 1e-3f);
    delete plane_copy;

    SbLineProjector line_projector;
    line_projector.setViewVolume(view);
    line_projector.setWorkingSpace(identity);
    line_projector.setLine(SbLine(SbVec3f(0, 0, 0), SbVec3f(1, 0, 0)));
    EXPECT_NEAR(line_projector.getLine().getDirection()[0], 1.0f, 1e-5f);
    line_projector.setStartPosition(SbVec2f(0.4f, 0.5f));
    const SbVec3f line_vector = line_projector.getVector(SbVec2f(0.6f, 0.5f));
    (void)line_vector;
    SbVec3f projected;
    EXPECT_TRUE(line_projector.tryProject(SbVec2f(0.5f, 0.5f), 0.01f, projected));
    auto * line_copy = static_cast<SbLineProjector *>(line_projector.copy());
    ASSERT_NE(line_copy, nullptr);
    EXPECT_NEAR(line_copy->getLine().getDirection()[0], 1.0f, 1e-5f);
    delete line_copy;
}

TEST(Projectors, SheetAndPlaneProjectorsProduceStableRotationsAndCopies)
{
    const SbViewVolume view = test_view_volume();
    const SbMatrix identity = SbMatrix::identity();
    const SbCylinder cylinder(SbLine(SbVec3f(0, 0, 0), SbVec3f(0, 1, 0)), 1.0f);
    const SbSphere sphere(SbVec3f(0, 0, 0), 1.0f);

    SbCylinderSheetProjector cylinder_sheet(cylinder, TRUE);
    cylinder_sheet.setViewVolume(view);
    cylinder_sheet.setWorkingSpace(identity);
    expect_finite_rotation(cylinder_sheet.getRotation(
        cylinder_sheet.project(SbVec2f(0.5f, 0.5f)),
        cylinder_sheet.project(SbVec2f(0.6f, 0.5f))));
    auto * cylinder_sheet_copy = static_cast<SbCylinderSheetProjector *>(cylinder_sheet.copy());
    ASSERT_NE(cylinder_sheet_copy, nullptr);
    delete cylinder_sheet_copy;

    SbSphereSheetProjector sphere_sheet(sphere, TRUE);
    sphere_sheet.setViewVolume(view);
    sphere_sheet.setWorkingSpace(identity);
    expect_finite_rotation(sphere_sheet.getRotation(
        sphere_sheet.project(SbVec2f(0.5f, 0.5f)),
        sphere_sheet.project(SbVec2f(0.6f, 0.55f))));
    auto * sphere_sheet_copy = static_cast<SbSphereSheetProjector *>(sphere_sheet.copy());
    ASSERT_NE(sphere_sheet_copy, nullptr);
    delete sphere_sheet_copy;

    SbCylinderPlaneProjector cylinder_plane(cylinder, TRUE);
    cylinder_plane.setViewVolume(view);
    cylinder_plane.setWorkingSpace(identity);
    expect_finite_rotation(cylinder_plane.getRotation(
        cylinder_plane.project(SbVec2f(0.5f, 0.5f)),
        cylinder_plane.project(SbVec2f(0.55f, 0.5f))));
    auto * cylinder_plane_copy = static_cast<SbCylinderPlaneProjector *>(cylinder_plane.copy());
    ASSERT_NE(cylinder_plane_copy, nullptr);
    delete cylinder_plane_copy;

    SbSpherePlaneProjector sphere_plane(sphere, TRUE);
    sphere_plane.setViewVolume(view);
    sphere_plane.setWorkingSpace(identity);
    expect_finite_rotation(sphere_plane.getRotation(
        sphere_plane.project(SbVec2f(0.5f, 0.5f)),
        sphere_plane.project(SbVec2f(0.55f, 0.52f))));
    auto * sphere_plane_copy = static_cast<SbSpherePlaneProjector *>(sphere_plane.copy());
    ASSERT_NE(sphere_plane_copy, nullptr);
    delete sphere_plane_copy;
}

} // namespace
