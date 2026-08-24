#include "framework/test_context.h"

#include <gtest/gtest.h>

#include <Inventor/SbMatrix.h>
#include <Inventor/SbRotation.h>
#include <Inventor/SbLine.h>
#include <Inventor/SbDPLine.h>
#include <Inventor/SbVec3d.h>
#include <Inventor/SbVec4f.h>
#include <Inventor/SbVec3f.h>

#include <cmath>

namespace {

TEST(BaseMath, IdentityMatrixPreservesPoint)
{
    ObolTestSupport::initializeObol();
    const SbVec3f input(1.25f, -2.0f, 7.5f);
    SbVec3f output;

    SbMatrix::identity().multVecMatrix(input, output);

    EXPECT_NEAR(output[0], input[0], 1e-6f);
    EXPECT_NEAR(output[1], input[1], 1e-6f);
    EXPECT_NEAR(output[2], input[2], 1e-6f);
}

TEST(BaseMath, RotationMapsAxisAsExpected)
{
    const SbRotation quarter_turn(SbVec3f(0.0f, 0.0f, 1.0f),
                                  static_cast<float>(std::acos(-1.0) / 2.0));
    SbVec3f output;

    quarter_turn.multVec(SbVec3f(1.0f, 0.0f, 0.0f), output);

    EXPECT_NEAR(output[0], 0.0f, 1e-5f);
    EXPECT_NEAR(output[1], 1.0f, 1e-5f);
    EXPECT_NEAR(output[2], 0.0f, 1e-5f);
}

TEST(BaseMath, DecomposesComposesAndSolvesMatrices)
{
    SbVec3f translation;
    SbRotation rotation;
    SbVec3f scale;
    SbRotation scale_orientation;
    SbMatrix::identity().getTransform(translation, rotation, scale, scale_orientation,
                                      SbVec3f(0.0f, 0.0f, 0.0f));
    EXPECT_NEAR(translation.length(), 0.0f, 1.0e-6f);
    EXPECT_EQ(scale, SbVec3f(1.0f, 1.0f, 1.0f));

    SbMatrix left_composed;
    left_composed.setTranslate(SbVec3f(0.0f, 2.0f, 0.0f));
    SbMatrix x_translation;
    x_translation.setTranslate(SbVec3f(1.0f, 0.0f, 0.0f));
    left_composed.multLeft(x_translation);
    SbVec3f origin;
    left_composed.multVecMatrix(SbVec3f(0.0f, 0.0f, 0.0f), origin);
    EXPECT_EQ(origin, SbVec3f(1.0f, 2.0f, 0.0f));

    SbMatrix right_composed;
    right_composed.setTranslate(SbVec3f(1.0f, 0.0f, 0.0f));
    right_composed.multRight(left_composed);
    right_composed.multVecMatrix(SbVec3f(0.0f, 0.0f, 0.0f), origin);
    EXPECT_EQ(origin, SbVec3f(2.0f, 2.0f, 0.0f));

    SbMatrix system(2, 1, 0, 0,
                    1, 3, 1, 0,
                    0, 1, 2, 0,
                    0, 0, 0, 1);
    int pivot_indices[4] = {};
    float parity = 0.0f;
    ASSERT_TRUE(system.LUDecomposition(pivot_indices, parity));
    float solution[] = {1.0f, 1.0f, 1.0f, 0.0f};
    system.LUBackSubstitution(pivot_indices, solution);
    EXPECT_NEAR(solution[0], 0.6f, 1.0e-5f);
    EXPECT_NEAR(solution[1], -0.2f, 1.0e-5f);
    EXPECT_NEAR(solution[2], 0.6f, 1.0e-5f);

    SbMatrix modified = SbMatrix::identity();
    EXPECT_TRUE(SbMatrix::identity().equals(modified, 1.0e-6f));
    modified[0][0] = 2.0f;
    EXPECT_FALSE(SbMatrix::identity().equals(modified, 1.0e-6f));
}

TEST(BaseMath, AdvancedMatrixOperationsRetainDeterminantsAndTransforms)
{
    const SbMatrix identity = SbMatrix::identity();
    EXPECT_FLOAT_EQ(identity.det3(), 1.0f);
    EXPECT_FLOAT_EQ(identity.det4(), 1.0f);

    SbMatrix scale;
    scale.setScale(SbVec3f(2.0f, 3.0f, 4.0f));
    EXPECT_NEAR(scale.det4(), 24.0f, 1e-4f);

    const SbMatrix values(1, 2, 3, 4,
                          5, 6, 7, 8,
                          9, 10, 11, 12,
                          13, 14, 15, 16);
    const SbMatrix transposed = values.transpose();
    EXPECT_FLOAT_EQ(transposed[0][1], 5.0f);
    EXPECT_FLOAT_EQ(transposed[1][0], 2.0f);

    SbMatrix transformed;
    const SbVec3f translation(1.0f, 2.0f, 3.0f);
    const SbVec3f scale_factor(2.0f, 2.0f, 2.0f);
    transformed.setTransform(translation,
                             SbRotation(SbVec3f(0, 1, 0), 0.7853981634f),
                             scale_factor);
    SbVec3f extracted_translation;
    SbVec3f extracted_scale;
    SbRotation extracted_rotation;
    SbRotation extracted_scale_orientation;
    transformed.getTransform(extracted_translation, extracted_rotation, extracted_scale,
                             extracted_scale_orientation);
    EXPECT_NEAR(extracted_translation[0], translation[0], 1e-4f);
    EXPECT_NEAR(extracted_translation[1], translation[1], 1e-4f);
    EXPECT_NEAR(extracted_translation[2], translation[2], 1e-4f);
    EXPECT_NEAR(extracted_scale[0], scale_factor[0], 1e-4f);

    SbVec4f homogeneous_result;
    SbMatrix translation_matrix = SbMatrix::identity();
    translation_matrix[3][0] = 5.0f;
    translation_matrix.multVecMatrix(SbVec4f(1.0f, 0.0f, 0.0f, 1.0f), homogeneous_result);
    EXPECT_FLOAT_EQ(homogeneous_result[0], 6.0f);
    EXPECT_FLOAT_EQ(homogeneous_result[3], 1.0f);

    SbDPLine x_axis(SbVec3d(0.0, 0.0, 0.0), SbVec3d(1.0, 0.0, 0.0));
    SbDPLine y_axis(SbVec3d(0.0, 1.0, 0.0), SbVec3d(0.0, -1.0, 0.0));
    SbVec3d closest_x;
    SbVec3d closest_y;
    EXPECT_TRUE(x_axis.getClosestPoints(y_axis, closest_x, closest_y));
    EXPECT_NEAR(closest_x[0], 0.0, 1e-9);
    EXPECT_NEAR(closest_y[1], 0.0, 1e-9);

    SbLine line(SbVec3f(0.0f, 0.0f, 0.0f), SbVec3f(1.0f, 0.0f, 0.0f));
    SbLine translated_line;
    translation_matrix.multLineMatrix(line, translated_line);
    EXPECT_NEAR(translated_line.getPosition()[0], 5.0f, 1e-5f);
    EXPECT_NEAR(translated_line.getDirection()[0], 1.0f, 1e-5f);

    float raw[4][4];
    translation_matrix.getValue(raw);
    SbMatrix restored;
    restored.setValue(raw);
    EXPECT_TRUE(restored.equals(translation_matrix, 1e-6f));
}

} // namespace
