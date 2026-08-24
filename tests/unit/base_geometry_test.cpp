#include <gtest/gtest.h>

#include <Inventor/SbCylinder.h>
#include <Inventor/SbBox3f.h>
#include <Inventor/SbBox2f.h>
#include <Inventor/SbBox2s.h>
#include <Inventor/SbBox3d.h>
#include <Inventor/SbDPLine.h>
#include <Inventor/SbDPMatrix.h>
#include <Inventor/SbDPPlane.h>
#include <Inventor/SbDPRotation.h>
#include <Inventor/SbDPViewVolume.h>
#include <Inventor/SbLine.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbPlane.h>
#include <Inventor/SbRotation.h>
#include <Inventor/SbSphere.h>
#include <Inventor/SbVec2d.h>
#include <Inventor/SbVec2f.h>
#include <Inventor/SbVec2i32.h>
#include <Inventor/SbVec2s.h>
#include <Inventor/SbVec3b.h>
#include <Inventor/SbVec3d.h>
#include <Inventor/SbVec3i32.h>
#include <Inventor/SbVec3s.h>
#include <Inventor/SbVec4f.h>
#include <Inventor/SbVec4i32.h>
#include <Inventor/SbViewVolume.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbXfBox3f.h>

#include <cmath>

namespace {

constexpr float kPi = 3.14159265358979323846f;

void expectNear(const SbVec3f & value, const SbVec3f & expected,
                const float tolerance = 1e-4f)
{
    EXPECT_NEAR(value[0], expected[0], tolerance);
    EXPECT_NEAR(value[1], expected[1], tolerance);
    EXPECT_NEAR(value[2], expected[2], tolerance);
}

TEST(BaseVectors, FloatingPointVariantsProvideArithmeticAndHomogeneousConversion)
{
    const SbVec2f a(3.0f, 4.0f);
    const SbVec2f b(1.0f, 2.0f);
    EXPECT_FLOAT_EQ(a.length(), 5.0f);
    EXPECT_FLOAT_EQ(a.dot(b), 11.0f);
    const SbVec2f sum = a + b;
    EXPECT_FLOAT_EQ(sum[0], 4.0f);
    EXPECT_FLOAT_EQ(sum[1], 6.0f);

    SbVec2f normal = a;
    normal.normalize();
    EXPECT_NEAR(normal.length(), 1.0f, 1e-5f);
    normal.negate();
    EXPECT_NEAR(normal[0], -0.6f, 1e-5f);

    const SbVec4f homogeneous(1.0f, 2.0f, 3.0f, 4.0f);
    EXPECT_FLOAT_EQ(homogeneous.dot(homogeneous), 30.0f);
    SbVec3f projected;
    homogeneous.getReal(projected);
    expectNear(projected, SbVec3f(0.25f, 0.5f, 0.75f));

    const SbVec3d double_vector(3.0, 4.0, 0.0);
    EXPECT_DOUBLE_EQ(double_vector.length(), 5.0);
    const SbVec3d cross = double_vector.cross(SbVec3d(0.0, 0.0, 1.0));
    EXPECT_DOUBLE_EQ(cross[0], 4.0);
    EXPECT_DOUBLE_EQ(cross[1], -3.0);
}

TEST(BaseVectors, IntegralVariantsRetainExactArithmetic)
{
    const SbVec2s short_sum = SbVec2s(3, 4) + SbVec2s(1, 2);
    EXPECT_EQ(short_sum[0], 4);
    EXPECT_EQ(short_sum[1], 6);

    const SbVec2i32 ints(100, 200);
    EXPECT_EQ(ints.dot(SbVec2i32(50, 75)), 20000);
    const SbVec3s short_three(1, 2, 3);
    EXPECT_EQ(short_three.dot(SbVec3s(4, 5, 6)), 32);
    const SbVec3i32 int_three = SbVec3i32(10, 20, 30) + SbVec3i32(1, 2, 3);
    EXPECT_EQ(int_three[0], 11);
    EXPECT_EQ(int_three[2], 33);
    const SbVec4i32 int_four = SbVec4i32(1, 2, 3, 4) + SbVec4i32(2, 2, 2, 2);
    EXPECT_EQ(int_four[3], 6);
    SbVec3b bytes(1, 2, 3);
    bytes.negate();
    EXPECT_EQ(bytes[0], -1);
}

TEST(BaseVectors, IntegralVariantsSupportMutationAndAllComponents)
{
    SbVec2s short_two(3, 4);
    short_two.negate();
    EXPECT_EQ(short_two[0], -3);
    short_two.setValue(5, 6);
    EXPECT_EQ(short_two[1], 6);

    SbVec2i32 int_two(100, 200);
    EXPECT_EQ(int_two.dot(SbVec2i32(50, 75)), 20000);
    int_two.negate();
    EXPECT_EQ(int_two[0], -100);

    SbVec3s short_three(1, 2, 3);
    EXPECT_EQ(short_three - SbVec3s(4, 5, 6), SbVec3s(-3, -3, -3));
    short_three.negate();
    EXPECT_EQ(short_three[2], -3);

    SbVec3i32 int_three(10, 20, 30);
    int_three.negate();
    EXPECT_EQ(int_three[2], -30);
    const SbVec4i32 four_sum = SbVec4i32(1, 2, 3, 4) + SbVec4i32(2, 2, 2, 2);
    EXPECT_EQ(four_sum[0], 3);
    EXPECT_EQ(four_sum[3], 6);
}

TEST(BaseBoxes, FloatDoubleAndShortVariantsCalculateExtentsAndCenters)
{
    SbBox3d double_box;
    EXPECT_TRUE(double_box.isEmpty());
    double_box.extendBy(SbVec3d(1.0, 2.0, 3.0));
    double_box.extendBy(SbVec3d(-1.0, -2.0, -3.0));
    EXPECT_FALSE(double_box.isEmpty());
    EXPECT_NEAR(double_box.getCenter()[0], 0.0, 1e-9);

    SbBox2f float_box;
    float_box.extendBy(SbVec2f(1.0f, 2.0f));
    float_box.extendBy(SbVec2f(-1.0f, -2.0f));
    EXPECT_FLOAT_EQ(float_box.getSize()[0], 2.0f);
    EXPECT_FLOAT_EQ(float_box.getSize()[1], 4.0f);
    EXPECT_TRUE(float_box.hasArea());

    const SbBox2s short_box(SbVec2s(-5, -5), SbVec2s(5, 5));
    EXPECT_FALSE(short_box.isEmpty());
    EXPECT_FLOAT_EQ(short_box.getCenter()[0], 0.0f);
    EXPECT_FLOAT_EQ(short_box.getCenter()[1], 0.0f);

    SbBox2f unit_square(SbVec2f(0.0f, 0.0f), SbVec2f(1.0f, 1.0f));
    EXPECT_EQ(unit_square.getCenter(), SbVec2f(0.5f, 0.5f));
    unit_square.extendBy(SbVec2f(2.0f, 2.0f));
    EXPECT_EQ(unit_square.getMax(), SbVec2f(2.0f, 2.0f));
    EXPECT_TRUE(unit_square.intersect(SbVec2f(1.0f, 1.0f)));

    const SbBox3f volume_box(SbVec3f(0.0f, 0.0f, 0.0f), SbVec3f(2.0f, 3.0f, 4.0f));
    EXPECT_FLOAT_EQ(volume_box.getVolume(), 24.0f);
    float span_minimum = 0.0f;
    float span_maximum = 0.0f;
    volume_box.getSpan(SbVec3f(1.0f, 0.0f, 0.0f), span_minimum, span_maximum);
    EXPECT_FLOAT_EQ(span_minimum, 0.0f);
    EXPECT_FLOAT_EQ(span_maximum, 2.0f);
}

TEST(BaseMatrices, TransformCompositionInverseAndDirectionsFollowTheirContracts)
{
    SbMatrix translation;
    translation.setTranslate(SbVec3f(3.0f, -2.0f, 5.0f));
    SbVec3f moved;
    translation.multVecMatrix(SbVec3f(0, 0, 0), moved);
    expectNear(moved, SbVec3f(3, -2, 5));
    SbVec3f restored;
    translation.inverse().multVecMatrix(moved, restored);
    expectNear(restored, SbVec3f(0, 0, 0));

    SbMatrix scale;
    scale.setScale(SbVec3f(2, 3, 4));
    SbVec3f scaled;
    scale.multVecMatrix(SbVec3f(1, 1, 1), scaled);
    expectNear(scaled, SbVec3f(2, 3, 4));

    SbMatrix combined = translation;
    SbMatrix second_translation;
    second_translation.setTranslate(SbVec3f(0, 2, 0));
    combined.multRight(second_translation);
    SbVec3f combined_origin;
    combined.multVecMatrix(SbVec3f(0, 0, 0), combined_origin);
    expectNear(combined_origin, SbVec3f(3, 0, 5));

    SbVec3f direction;
    translation.multDirMatrix(SbVec3f(1, 0, 0), direction);
    expectNear(direction, SbVec3f(1, 0, 0));
    EXPECT_FLOAT_EQ(SbMatrix::identity().det3(), 1.0f);
    EXPECT_FLOAT_EQ(SbMatrix::identity().det4(), 1.0f);
}

TEST(BaseMatrices, SupportColumnVectorsTransformDecompositionAndLeftComposition)
{
    SbMatrix scale;
    scale.setScale(3.0f);
    SbVec3f column_result;
    scale.multMatrixVec(SbVec3f(1, 2, 0), column_result);
    expectNear(column_result, SbVec3f(3, 6, 0));

    SbMatrix rotation;
    rotation.setRotate(SbRotation(SbVec3f(1, 0, 0), kPi / 4.0f));
    EXPECT_TRUE((rotation * rotation.inverse()).equals(SbMatrix::identity(), 1e-4f));

    const SbVec3f translation(1, 0, 0);
    const SbVec3f scale_factor(2, 2, 2);
    SbMatrix transformed;
    transformed.setTransform(translation, SbRotation::identity(), scale_factor,
                             SbRotation::identity());
    SbVec3f origin;
    transformed.multVecMatrix(SbVec3f(0, 0, 0), origin);
    expectNear(origin, translation);

    SbMatrix centered;
    centered.setTransform(translation, SbRotation::identity(), SbVec3f(1, 1, 1),
                          SbRotation::identity(), SbVec3f(0, 0, 0));
    SbVec3f extracted_translation;
    SbRotation extracted_rotation;
    SbVec3f extracted_scale;
    SbRotation extracted_scale_orientation;
    centered.getTransform(extracted_translation, extracted_rotation, extracted_scale,
                          extracted_scale_orientation, SbVec3f(0, 0, 0));
    expectNear(extracted_translation, translation);

    SbMatrix left_composed;
    left_composed.setTranslate(SbVec3f(1, 0, 0));
    SbMatrix y_translation;
    y_translation.setTranslate(SbVec3f(0, 2, 0));
    left_composed.multLeft(y_translation);
    left_composed.multVecMatrix(SbVec3f(0, 0, 0), origin);
    expectNear(origin, SbVec3f(1, 2, 0));

    SbVec4f homogeneous_result;
    transformed.multVecMatrix(SbVec4f(0.0f, 0.0f, 0.0f, 1.0f), homogeneous_result);
    EXPECT_FLOAT_EQ(homogeneous_result[0], 1.0f);
    EXPECT_FLOAT_EQ(homogeneous_result[3], 1.0f);

    SbMatrix values;
    const float raw_values[4][4] = {
        {1.0f, 2.0f, 3.0f, 4.0f},
        {5.0f, 6.0f, 7.0f, 8.0f},
        {9.0f, 10.0f, 11.0f, 12.0f},
        {13.0f, 14.0f, 15.0f, 16.0f},
    };
    values.setValue(raw_values);
    SbMat round_trip;
    values.getValue(round_trip);
    EXPECT_FLOAT_EQ(round_trip[0][0], 1.0f);
    EXPECT_FLOAT_EQ(round_trip[2][3], 12.0f);

    SbMatrix identity = values;
    identity.makeIdentity();
    EXPECT_TRUE(identity == SbMatrix::identity());
    EXPECT_TRUE(identity != values);

    const float flat_values[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        2.0f, 3.0f, 4.0f, 1.0f,
    };
    SbMatrix flat_matrix;
    flat_matrix.setValue(flat_values);
    flat_matrix.multVecMatrix(SbVec3f(0, 0, 0), origin);
    expectNear(origin, SbVec3f(2, 3, 4));

    SbMatrix rotation_matrix;
    rotation_matrix = SbRotation(SbVec3f(0, 0, 1), kPi / 2.0f);
    rotation_matrix.multVecMatrix(SbVec3f(1, 0, 0), origin);
    expectNear(origin, SbVec3f(0, 1, 0), 1e-3f);

    SbMatrix multiplied = SbMatrix::identity();
    SbMatrix y_shift;
    y_shift.setTranslate(SbVec3f(0, 2, 0));
    multiplied.setTranslate(SbVec3f(1, 0, 0));
    multiplied *= y_shift;
    multiplied.multVecMatrix(SbVec3f(0, 0, 0), origin);
    expectNear(origin, SbVec3f(1, 2, 0));
}

TEST(BaseRotations, AxisAngleMatrixAndSlerpAreConsistent)
{
    const SbRotation quarter_turn(SbVec3f(0, 0, 1), kPi / 2.0f);
    SbVec3f rotated;
    quarter_turn.multVec(SbVec3f(1, 0, 0), rotated);
    expectNear(rotated, SbVec3f(0, 1, 0), 1e-3f);

    SbMatrix matrix;
    quarter_turn.getValue(matrix);
    SbRotation from_matrix(matrix);
    SbVec3f round_trip;
    from_matrix.multVec(SbVec3f(1, 0, 0), round_trip);
    expectNear(round_trip, rotated, 1e-3f);

    const SbRotation from_to(SbVec3f(1, 0, 0), SbVec3f(0, 1, 0));
    from_to.multVec(SbVec3f(1, 0, 0), rotated);
    expectNear(rotated, SbVec3f(0, 1, 0), 1e-3f);

    const SbRotation halfway = SbRotation::slerp(
        SbRotation::identity(), SbRotation(SbVec3f(0, 1, 0), kPi / 2.0f), 0.5f);
    halfway.multVec(SbVec3f(1, 0, 0), rotated);
    EXPECT_NEAR(rotated[0], std::sqrt(0.5f), 1e-3f);
    EXPECT_NEAR(rotated[2], -std::sqrt(0.5f), 1e-3f);

    const SbRotation start(SbVec3f(1, 0, 0), kPi / 4.0f);
    const SbRotation end(SbVec3f(1, 0, 0), 3.0f * kPi / 4.0f);
    SbVec3f endpoint_axis;
    float start_angle = 0.0f;
    float end_angle = 0.0f;
    SbRotation::slerp(start, end, 0.0f).getValue(endpoint_axis, start_angle);
    SbRotation::slerp(start, end, 1.0f).getValue(endpoint_axis, end_angle);
    EXPECT_NEAR(start_angle, kPi / 4.0f, 1e-3f);
    EXPECT_NEAR(end_angle, 3.0f * kPi / 4.0f, 1e-3f);

    const SbRotation inverse = quarter_turn.inverse();
    const SbRotation identity = quarter_turn * inverse;
    identity.multVec(SbVec3f(1, 2, 3), rotated);
    expectNear(rotated, SbVec3f(1, 2, 3), 1e-3f);

    SbRotation from_quaternion;
    const float z_quarter_turn[] = {0.0f, 0.0f, std::sqrt(0.5f), std::sqrt(0.5f)};
    from_quaternion.setValue(z_quarter_turn);
    from_quaternion.scaleAngle(0.5f);
    SbVec3f axis;
    float angle = 0.0f;
    from_quaternion.getValue(axis, angle);
    EXPECT_NEAR(angle, kPi / 4.0f, 1e-3f);

    const SbRotation combined =
        SbRotation(SbVec3f(0, 1, 0), kPi / 4.0f) *
        SbRotation(SbVec3f(0, 1, 0), kPi / 4.0f);
    combined.getValue(axis, angle);
    EXPECT_NEAR(angle, kPi / 2.0f, 1e-3f);

    float q0 = 0.0f;
    float q1 = 0.0f;
    float q2 = 0.0f;
    float q3 = 0.0f;
    quarter_turn.getValue(q0, q1, q2, q3);
    SbRotation copied_quaternion;
    copied_quaternion.setValue(q0, q1, q2, q3);
    SbVec3f copied_result;
    copied_quaternion.multVec(SbVec3f(1, 0, 0), copied_result);
    expectNear(copied_result, SbVec3f(0, 1, 0), 1e-3f);
    EXPECT_NE(copied_quaternion, SbRotation::identity());

    SbRotation mutating_inverse = quarter_turn;
    mutating_inverse.invert();
    const SbRotation mutating_identity = quarter_turn * mutating_inverse;
    mutating_identity.multVec(SbVec3f(2, 3, 4), copied_result);
    expectNear(copied_result, SbVec3f(2, 3, 4), 1e-3f);
    EXPECT_NEAR(quarter_turn[2], std::sqrt(0.5f), 1e-3f);

    SbRotation scaled = quarter_turn;
    scaled *= 0.5f;
    EXPECT_GT(std::sqrt(scaled[0] * scaled[0] + scaled[1] * scaled[1] +
                        scaled[2] * scaled[2] + scaled[3] * scaled[3]), 0.01f);
    const SbRotation equal_rotation(SbVec3f(1, 0, 0), kPi / 4.0f);
    EXPECT_EQ(equal_rotation, SbRotation(SbVec3f(1, 0, 0), kPi / 4.0f));
    EXPECT_NE(equal_rotation, quarter_turn);
}

TEST(BaseGeometry, PlanesLinesSpheresAndCylindersIntersectAnalytically)
{
    const SbPlane plane(SbVec3f(0, 0, 1), 0.0f);
    SbVec3f intersection;
    ASSERT_TRUE(plane.intersect(
        SbLine(SbVec3f(0, 0, -1), SbVec3f(0, 0, 1)), intersection));
    EXPECT_NEAR(intersection[2], 0.0f, 1e-5f);

    const SbLine x_axis(SbVec3f(0, 0, 0), SbVec3f(1, 0, 0));
    expectNear(x_axis.getClosestPoint(SbVec3f(3, 5, 0)), SbVec3f(3, 0, 0));

    const SbSphere sphere(SbVec3f(0, 0, 0), 1.0f);
    SbVec3f enter;
    SbVec3f exit;
    ASSERT_TRUE(sphere.intersect(
        SbLine(SbVec3f(-2, 0, 0), SbVec3f(2, 0, 0)), enter, exit));
    EXPECT_NEAR(enter[0], -1.0f, 1e-4f);
    EXPECT_NEAR(exit[0], 1.0f, 1e-4f);

    SbSphere circumscribed;
    circumscribed.circumscribe(SbBox3f(SbVec3f(-1, -1, -1), SbVec3f(1, 1, 1)));
    EXPECT_NEAR(circumscribed.getRadius(), std::sqrt(3.0f), 1e-5f);
    EXPECT_EQ(circumscribed.getCenter(), SbVec3f(0, 0, 0));
    circumscribed.setCenter(SbVec3f(1, 2, 3));
    circumscribed.setRadius(5.0f);
    EXPECT_EQ(circumscribed.getCenter(), SbVec3f(1, 2, 3));
    EXPECT_FLOAT_EQ(circumscribed.getRadius(), 5.0f);

    const SbCylinder cylinder(SbLine(SbVec3f(0, 0, 0), SbVec3f(0, 1, 0)), 1.0f);
    ASSERT_TRUE(cylinder.intersect(
        SbLine(SbVec3f(-2, 0.5f, 0), SbVec3f(2, 0.5f, 0)), enter, exit));
}

TEST(BaseGeometry, BoxesAndPlanesPreserveSpatialQueriesAndTransforms)
{
    SbBox3f box;
    box.extendBy(SbVec3f(1.0f, 2.0f, 3.0f));
    box.extendBy(SbVec3f(-1.0f, -2.0f, -3.0f));
    box.extendBy(SbBox3f(SbVec3f(2.0f, 2.0f, 2.0f), SbVec3f(4.0f, 4.0f, 4.0f)));
    EXPECT_EQ(box.getMin(), SbVec3f(-1.0f, -2.0f, -3.0f));
    EXPECT_EQ(box.getMax(), SbVec3f(4.0f, 4.0f, 4.0f));
    EXPECT_TRUE(box.intersect(SbVec3f(0.0f, 0.0f, 0.0f)));
    EXPECT_FALSE(box.intersect(SbVec3f(5.0f, 5.0f, 5.0f)));
    EXPECT_TRUE(box.intersect(SbBox3f(SbVec3f(3.0f, 3.0f, 3.0f), SbVec3f(5.0f, 5.0f, 5.0f))));
    EXPECT_FALSE(box.intersect(SbBox3f(SbVec3f(5.0f, 5.0f, 5.0f), SbVec3f(6.0f, 6.0f, 6.0f))));
    expectNear(box.getClosestPoint(SbVec3f(10.0f, 0.0f, 0.0f)), SbVec3f(4.0f, 0.0f, 0.0f));
    float width = 0.0f;
    float height = 0.0f;
    float depth = 0.0f;
    box.getSize(width, height, depth);
    EXPECT_FLOAT_EQ(width, 5.0f);
    EXPECT_FLOAT_EQ(height, 6.0f);
    EXPECT_FLOAT_EQ(depth, 7.0f);
    SbMatrix translation;
    translation.setTranslate(SbVec3f(5.0f, 0.0f, 0.0f));
    box.transform(translation);
    EXPECT_FLOAT_EQ(box.getCenter()[0], 6.5f);

    SbPlane horizontal(SbVec3f(0.0f, 1.0f, 0.0f), 0.0f);
    EXPECT_TRUE(horizontal.isInHalfSpace(SbVec3f(0.0f, 1.0f, 0.0f)));
    EXPECT_FALSE(horizontal.isInHalfSpace(SbVec3f(0.0f, -1.0f, 0.0f)));
    const SbPlane depth_plane(SbVec3f(0.0f, 0.0f, 1.0f), 0.0f);
    EXPECT_FLOAT_EQ(depth_plane.getDistance(SbVec3f(0.0f, 0.0f, 5.0f)), 5.0f);
    SbVec3f plane_intersection;
    ASSERT_TRUE(horizontal.intersect(
        SbLine(SbVec3f(0.0f, 5.0f, 0.0f), SbVec3f(0.0f, -1.0f, 0.0f)),
        plane_intersection));
    EXPECT_NEAR(plane_intersection[1], 0.0f, 1e-5f);
    SbLine plane_intersection_line;
    ASSERT_TRUE(SbPlane(SbVec3f(1.0f, 0.0f, 0.0f), 0.0f).intersect(horizontal,
                                                                      plane_intersection_line));
    EXPECT_GT(plane_intersection_line.getDirection().length(), 0.5f);
    const SbPlane from_points(SbVec3f(-1.0f, 0.0f, 0.0f), SbVec3f(1.0f, 0.0f, 0.0f),
                              SbVec3f(0.0f, 0.0f, 1.0f));
    EXPECT_NEAR(std::fabs(from_points.getNormal()[1]), 1.0f, 0.1f);
    SbPlane translated_plane = depth_plane;
    SbMatrix depth_translation;
    depth_translation.setTranslate(SbVec3f(0.0f, 0.0f, 5.0f));
    translated_plane.transform(depth_translation);
    EXPECT_NEAR(translated_plane.getDistance(SbVec3f(0.0f, 0.0f, 10.0f)), 5.0f, 0.1f);

    SbPlane offset_plane(SbVec3f(0.0f, 0.0f, 1.0f), 0.0f);
    offset_plane.offset(2.0f);
    EXPECT_NEAR(std::fabs(offset_plane.getDistance(SbVec3f(0, 0, 0))), 2.0f, 1e-5f);
}

TEST(BaseGeometry, CylindersAndLinesPreserveAnalyticAndTransformContracts)
{
    const SbLine axis(SbVec3f(0.0f, -10.0f, 0.0f), SbVec3f(0.0f, 10.0f, 0.0f));
    SbCylinder cylinder(axis, 1.0f);
    EXPECT_FLOAT_EQ(cylinder.getRadius(), 1.0f);
    cylinder.setRadius(2.0f);
    EXPECT_FLOAT_EQ(cylinder.getRadius(), 2.0f);
    cylinder.setAxis(SbLine(SbVec3f(0.0f, -5.0f, 0.0f), SbVec3f(0.0f, 5.0f, 0.0f)));

    SbVec3f nearest_hit;
    ASSERT_TRUE(cylinder.intersect(
        SbLine(SbVec3f(0.0f, 0.0f, 5.0f), SbVec3f(0.0f, 0.0f, -1.0f)), nearest_hit));
    EXPECT_NEAR(std::fabs(nearest_hit[2]), 2.0f, 0.1f);
    EXPECT_FALSE(cylinder.intersect(
        SbLine(SbVec3f(5.0f, 0.0f, 5.0f), SbVec3f(5.0f, 0.0f, -1.0f)), nearest_hit));
    SbVec3f enter;
    SbVec3f exit;
    ASSERT_TRUE(cylinder.intersect(
        SbLine(SbVec3f(0.0f, 0.0f, 10.0f), SbVec3f(0.0f, 0.0f, -1.0f)), enter, exit));
    EXPECT_NEAR(enter[2], 2.0f, 0.1f);
    EXPECT_NEAR(exit[2], -2.0f, 0.1f);

    const SbLine diagonal(SbVec3f(1.0f, 2.0f, 3.0f), SbVec3f(4.0f, 5.0f, 6.0f));
    EXPECT_NEAR(diagonal.getDirection().length(), 1.0f, 1e-5f);
    const SbLine x_axis(SbVec3f(0.0f, 0.0f, 0.0f), SbVec3f(1.0f, 0.0f, 0.0f));
    expectNear(x_axis.getClosestPoint(SbVec3f(5.0f, 3.0f, 0.0f)), SbVec3f(5.0f, 0.0f, 0.0f));
    SbVec3f x_axis_closest;
    SbVec3f vertical_closest;
    ASSERT_TRUE(x_axis.getClosestPoints(
        SbLine(SbVec3f(5.0f, 1.0f, 0.0f), SbVec3f(5.0f, -1.0f, 0.0f)),
        x_axis_closest, vertical_closest));
    EXPECT_NEAR(x_axis_closest[0], 5.0f, 0.1f);
    SbMatrix translation;
    translation.setTranslate(SbVec3f(3.0f, 0.0f, 0.0f));
    SbLine transformed_line;
    translation.multLineMatrix(SbLine(SbVec3f(0.0f, 0.0f, 0.0f),
                                      SbVec3f(0.0f, 0.0f, 1.0f)),
                               transformed_line);
    EXPECT_NEAR(transformed_line.getPosition()[0], 3.0f, 1e-5f);

    SbLine configured_line;
    configured_line.setValue(SbVec3f(1.0f, 2.0f, 3.0f), SbVec3f(1.0f, 2.0f, 4.0f));
    EXPECT_EQ(configured_line.getPosition(), SbVec3f(1.0f, 2.0f, 3.0f));
    EXPECT_NEAR(configured_line.getDirection().length(), 1.0f, 1e-5f);
}

TEST(BaseViewVolume, OrthographicAndPerspectiveViewsProvideStableGeometry)
{
    SbViewVolume orthographic;
    orthographic.ortho(-1, 1, -1, 1, 0.1f, 100.0f);
    EXPECT_GT(std::fabs(orthographic.getPlane(0.1f).getNormal()[2]), 0.5f);

    SbViewVolume perspective;
    perspective.perspective(kPi / 2.0f, 1.0f, 0.1f, 100.0f);
    const SbVec3f sight = perspective.getSightPoint(10.0f);
    EXPECT_NEAR(sight[0], 0.0f, 1e-3f);
    EXPECT_NEAR(sight[1], 0.0f, 1e-3f);
}

TEST(BaseViewVolume, ExposesOrthographicDimensionsAndFiniteProjectionMatrices)
{
    SbViewVolume orthographic;
    orthographic.ortho(-2.0f, 2.0f, -2.0f, 2.0f, 1.0f, 100.0f);
    EXPECT_GT(orthographic.getNearDist(), 0.0f);
    EXPECT_GT(orthographic.getWidth(), 0.0f);
    EXPECT_GT(orthographic.getHeight(), 0.0f);
    EXPECT_GT(orthographic.getProjectionDirection().length(), 0.5f);
    EXPECT_TRUE(std::isfinite(orthographic.getProjectionPoint()[0]));

    SbViewVolume perspective;
    perspective.perspective(kPi / 3.0f, 1.5f, 0.1f, 100.0f);
    SbMatrix affine;
    SbMatrix projection;
    perspective.getMatrices(affine, projection);
    SbVec3f transformed;
    affine.multVecMatrix(SbVec3f(0, 0, -10), transformed);
    EXPECT_TRUE(std::isfinite(transformed[0]));
}

TEST(BaseViewVolume, ProjectsAndConstructsStableScreenGeometry)
{
    SbViewVolume orthographic;
    orthographic.ortho(-2.0f, 2.0f, -2.0f, 2.0f, 0.1f, 100.0f);

    SbVec3f screen;
    orthographic.projectToScreen(SbVec3f(0.0f, 0.0f, 0.0f), screen);
    EXPECT_NEAR(screen[0], 0.5f, 0.1f);
    EXPECT_NEAR(screen[1], 0.5f, 0.1f);

    const SbViewVolume narrowed = orthographic.narrow(0.25f, 0.25f, 0.75f, 0.75f);
    const SbVec3f narrowed_sight = narrowed.getSightPoint(10.0f);
    EXPECT_TRUE(std::isfinite(narrowed_sight[0]));
    EXPECT_TRUE(std::isfinite(narrowed_sight[1]));
    EXPECT_TRUE(std::isfinite(narrowed_sight[2]));

    SbViewVolume perspective;
    perspective.perspective(kPi / 2.0f, 1.0f, 0.1f, 100.0f);
    const SbViewVolume box_narrowed = perspective.narrow(
        SbBox3f(SbVec3f(-0.5f, -0.5f, -5.0f), SbVec3f(0.5f, 0.5f, -4.0f)));
    EXPECT_NE(box_narrowed.getWidth(), perspective.getWidth());
    EXPECT_GT(perspective.getWorldToScreenScale(SbVec3f(0, 0, -10), 0.1f), 0.0f);

    const SbVec3f plane_point = perspective.getPlanePoint(10.0f, SbVec2f(0.5f, 0.5f));
    EXPECT_NEAR(plane_point[0], 0.0f, 0.5f);
    EXPECT_NEAR(plane_point[1], 0.0f, 0.5f);

    SbLine projected_line;
    perspective.projectPointToLine(SbVec2f(0.5f, 0.5f), projected_line);
    EXPECT_GT(projected_line.getDirection().length(), 0.5f);
    SbVec3f projected_start;
    SbVec3f projected_end;
    perspective.projectPointToLine(SbVec2f(0.5f, 0.5f), projected_start, projected_end);
    EXPECT_GT((projected_end - projected_start).length(), 0.1f);
    EXPECT_NEAR(orthographic.getCameraSpaceMatrix()[0][0], 1.0f, 0.01f);

    const SbMatrix camera_space = perspective.getCameraSpaceMatrix();
    EXPECT_NEAR(camera_space[3][3], 1.0f, 1e-5f);
    const SbVec2f projected_box = perspective.projectBox(
        SbBox3f(SbVec3f(-0.5f, -0.5f, -5.0f), SbVec3f(0.5f, 0.5f, -4.0f)));
    EXPECT_GE(projected_box[0], 0.0f);
    EXPECT_GE(projected_box[1], 0.0f);
    SbVec3f projected_screen;
    perspective.projectToScreen(SbVec3f(0.0f, 0.0f, -5.0f), projected_screen);
    EXPECT_GE(projected_screen[0], -0.1f);
    EXPECT_LE(projected_screen[0], 1.1f);
    const SbRotation align = perspective.getAlignRotation(FALSE);
    EXPECT_NEAR(std::sqrt(align[0] * align[0] + align[1] * align[1] +
                          align[2] * align[2] + align[3] * align[3]), 1.0f, 1e-3f);
}

TEST(BaseViewVolume, DoublePrecisionVariantProjectsAndProvidesMatrices)
{
    SbDPViewVolume view;
    view.ortho(-1.0, 1.0, -1.0, 1.0, 0.1, 100.0);
    EXPECT_EQ(view.getProjectionType(), SbDPViewVolume::ORTHOGRAPHIC);
    EXPECT_NEAR(view.getWidth(), 2.0, 1e-9);
    EXPECT_GT(std::fabs(view.getPlane(0.1).getNormal()[2]), 0.5f);

    SbDPMatrix affine;
    SbDPMatrix projection;
    view.getMatrices(affine, projection);
    SbVec3d transformed;
    affine.multVecMatrix(SbVec3d(0.0, 0.0, 0.0), transformed);
    EXPECT_TRUE(std::isfinite(transformed[0]));

    SbVec3d screen;
    view.projectToScreen(SbVec3d(0.0, 0.0, 0.0), screen);
    EXPECT_NEAR(screen[0], 0.5, 0.1);
    EXPECT_NEAR(screen[1], 0.5, 0.1);

    view.perspective(M_PI / 2.0, 1.0, 0.1, 100.0);
    EXPECT_EQ(view.getProjectionType(), SbDPViewVolume::PERSPECTIVE);
    const SbVec3d sight = view.getSightPoint(10.0);
    EXPECT_NEAR(sight[0], 0.0, 0.1);
    EXPECT_NEAR(sight[1], 0.0, 0.1);

    SbDPLine projected_line;
    view.projectPointToLine(SbVec2d(0.0, 0.0), projected_line);
    EXPECT_GT(projected_line.getDirection().length(), 0.5);
    SbVec3d projected_start;
    SbVec3d projected_end;
    view.projectPointToLine(SbVec2d(0.0, 0.0), projected_start, projected_end);
    EXPECT_GT((projected_end - projected_start).length(), 0.1);
    EXPECT_GT(view.getWorldToScreenScale(SbVec3d(0.0, 0.0, -10.0), 1.0), 0.0);
    const double original_width = view.getWidth();
    view.scale(2.0);
    EXPECT_NEAR(view.getWidth(), original_width * 2.0, 1e-6);
    EXPECT_NEAR(view.getViewUp()[1], 1.0, 1e-6);

    const SbDPViewVolume narrowed = view.narrow(0.25, 0.25, 0.75, 0.75);
    EXPECT_GT(narrowed.getWidth(), 0.0);

    SbDPViewVolume frustum;
    frustum.frustum(-1.0, 1.0, -1.0, 1.0, 1.0, 100.0);
    EXPECT_GT(frustum.getNearDist(), 0.0);
    EXPECT_NEAR(frustum.getWidth(), 2.0, 1e-6);

    SbDPViewVolume moved;
    moved.perspective(M_PI / 3.0, 1.0, 1.0, 100.0);
    const SbVec3d original_direction = moved.getProjectionDirection();
    moved.rotateCamera(SbDPRotation(SbVec3d(0.0, 1.0, 0.0), M_PI / 4.0));
    EXPECT_GT((moved.getProjectionDirection() - original_direction).length(), 0.1);
    const SbVec3d original_point = moved.getProjectionPoint();
    moved.translateCamera(SbVec3d(5.0, 0.0, 0.0));
    EXPECT_NEAR(moved.getProjectionPoint()[0] - original_point[0], 5.0, 0.1);

    SbDPViewVolume scaled;
    scaled.ortho(-2.0, 2.0, -2.0, 2.0, 1.0, 100.0);
    const double original_height = scaled.getHeight();
    scaled.scaleHeight(0.5);
    EXPECT_NEAR(scaled.getHeight(), original_height * 0.5, 1e-6);
    const double original_scaled_width = scaled.getWidth();
    scaled.scaleWidth(3.0);
    EXPECT_NEAR(scaled.getWidth(), original_scaled_width * 3.0, 1e-6);
    EXPECT_GT(scaled.zVector().length(), 0.5);

    SbDPViewVolume detail;
    detail.perspective(M_PI / 3.0, 1.0, 1.0, 100.0);
    const SbVec3d plane_point = detail.getPlanePoint(5.0, SbVec2d(0.0, 0.0));
    EXPECT_TRUE(std::isfinite(plane_point[0]));
    EXPECT_GT(detail.getWorldToScreenScale(SbVec3d(0.0, 0.0, -10.0), 1.0), 0.0);
    const SbDPRotation align_rotation = detail.getAlignRotation(FALSE);
    double q0 = 0.0;
    double q1 = 0.0;
    double q2 = 0.0;
    double q3 = 0.0;
    align_rotation.getValue(q0, q1, q2, q3);
    EXPECT_NEAR(std::sqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3), 1.0, 1e-6);
    EXPECT_GE(detail.projectBox(
                  SbBox3f(SbVec3f(-1, -1, -5), SbVec3f(1, 1, -4)))[0],
              0.0);
}

TEST(BaseViewportRegion, RetainsWindowViewportAndDisplayMetrics)
{
    SbViewportRegion viewport(800, 600);
    EXPECT_EQ(viewport.getWindowSize()[0], 800);
    EXPECT_EQ(viewport.getWindowSize()[1], 600);
    EXPECT_NEAR(viewport.getViewportAspectRatio(), 800.0f / 600.0f, 0.01f);
    EXPECT_FLOAT_EQ(viewport.getViewportOrigin()[0], 0.0f);
    EXPECT_FLOAT_EQ(viewport.getViewportOrigin()[1], 0.0f);
    EXPECT_FLOAT_EQ(viewport.getViewportSize()[0], 1.0f);
    EXPECT_FLOAT_EQ(viewport.getViewportSize()[1], 1.0f);

    viewport.setPixelsPerInch(96.0f);
    EXPECT_FLOAT_EQ(viewport.getPixelsPerInch(), 96.0f);
    viewport.setViewportPixels(SbVec2s(10, 10), SbVec2s(200, 150));
    EXPECT_EQ(viewport.getViewportSizePixels()[0], 200);
    EXPECT_EQ(viewport.getViewportSizePixels()[1], 150);
}

TEST(BaseGeometry, TransformedBoxesProjectAndExtendInWorldSpace)
{
    const SbBox3f unit_box(SbVec3f(-1, -1, -1), SbVec3f(1, 1, 1));
    SbXfBox3f transformed(unit_box);
    EXPECT_FLOAT_EQ(transformed.getTransform()[0][0], 1.0f);
    EXPECT_NEAR(transformed.project().getCenter()[0], 0.0f, 1e-5f);

    SbMatrix translation;
    translation.setTranslate(SbVec3f(5.0f, 0.0f, 0.0f));
    transformed.setTransform(translation);
    EXPECT_NEAR(transformed.project().getCenter()[0], 5.0f, 0.1f);

    SbXfBox3f accumulated;
    accumulated.extendBy(SbVec3f(2.0f, 3.0f, 4.0f));
    accumulated.extendBy(SbVec3f(-2.0f, -3.0f, -4.0f));
    EXPECT_NEAR(accumulated.project().getCenter()[0], 0.0f, 0.1f);
    EXPECT_EQ(accumulated.project().getMax(), SbVec3f(2.0f, 3.0f, 4.0f));

    const SbXfBox3f query_box(SbVec3f(-2.0f, -2.0f, -2.0f),
                               SbVec3f(2.0f, 2.0f, 2.0f));
    EXPECT_TRUE(query_box.intersect(SbVec3f(1.0f, 1.0f, 1.0f)));
    EXPECT_FALSE(query_box.intersect(SbVec3f(3.0f, 3.0f, 3.0f)));
}

TEST(BaseGeometry, DoublePrecisionPrimitivesPreserveAnalyticResults)
{
    const SbDPLine line(SbVec3d(0.0, 0.0, 0.0), SbVec3d(1.0, 0.0, 0.0));
    const SbVec3d closest = line.getClosestPoint(SbVec3d(3.0, 5.0, 0.0));
    EXPECT_NEAR(closest[0], 3.0, 1e-9);
    EXPECT_NEAR(closest[1], 0.0, 1e-9);

    const SbDPPlane plane(SbVec3d(0.0, 1.0, 0.0), 0.0);
    EXPECT_NEAR(plane.getDistance(SbVec3d(0.0, 5.0, 0.0)), 5.0, 1e-9);
    EXPECT_TRUE(plane.isInHalfSpace(SbVec3d(0.0, 1.0, 0.0)));

    const SbDPRotation rotation(SbVec3d(0.0, 1.0, 0.0), M_PI / 2.0);
    SbVec3d rotated;
    rotation.multVec(SbVec3d(1.0, 0.0, 0.0), rotated);
    EXPECT_NEAR(rotated[0], 0.0, 1e-6);
    EXPECT_NEAR(rotated[2], -1.0, 1e-6);
}

} // namespace
