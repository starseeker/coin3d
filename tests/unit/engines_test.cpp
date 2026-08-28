#include <gtest/gtest.h>

#include <Inventor/SbMatrix.h>
#include <Inventor/SbRotation.h>
#include <Inventor/SbVec2f.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SoType.h>
#include <Inventor/engines/SoCalculator.h>
#include <Inventor/engines/SoBoolOperation.h>
#include <Inventor/engines/SoComposeMatrix.h>
#include <Inventor/engines/SoComposeRotation.h>
#include <Inventor/engines/SoComposeVec2f.h>
#include <Inventor/engines/SoComposeVec3f.h>
#include <Inventor/engines/SoDecomposeMatrix.h>
#include <Inventor/engines/SoDecomposeVec3f.h>
#include <Inventor/engines/SoDecomposeVec2f.h>
#include <Inventor/engines/SoElapsedTime.h>
#include <Inventor/engines/SoGate.h>
#include <Inventor/engines/SoInterpolateFloat.h>
#include <Inventor/engines/SoSelectOne.h>
#include <Inventor/engines/SoTimeCounter.h>
#include <Inventor/fields/SoMFFloat.h>
#include <Inventor/fields/SoMFMatrix.h>
#include <Inventor/fields/SoMFVec2f.h>
#include <Inventor/fields/SoMFVec3f.h>
#include <Inventor/fields/SoSFFloat.h>
#include <Inventor/fields/SoSFBool.h>
#include <Inventor/fields/SoSFRotation.h>
#include <Inventor/fields/SoSFVec3f.h>
#include <Inventor/nodes/SoSphere.h>

// This test deliberately exercises the private generated parser boundary;
// keep internal visibility local so public compatibility headers retain their
// normal behavior in the rest of the grouped unit executable.
#define OBOL_INTERNAL 1
#include "engines/evaluator.h"
#undef OBOL_INTERNAL

#include <atomic>
#include <thread>
#include <vector>

TEST(Engines, ConstructWithValidRuntimeTypes)
{
    auto * calculator = new SoCalculator;
    calculator->ref();
    EXPECT_NE(calculator->getTypeId(), SoType::badType());
    calculator->unref();

    auto * elapsed_time = new SoElapsedTime;
    elapsed_time->ref();
    EXPECT_NE(elapsed_time->getTypeId(), SoType::badType());
    elapsed_time->unref();
}

TEST(Engines, CalculatorEvaluatesConnectedFloatOutput)
{
    auto * calculator = new SoCalculator;
    calculator->ref();
    calculator->a.setValue(3.0f);
    calculator->b.setValue(4.0f);
    calculator->expression.setValue("oa = a + b");

    SoSFFloat result;
    result.connectFrom(&calculator->oa);
    EXPECT_FLOAT_EQ(result.getValue(), 7.0f);
    result.disconnect();
    calculator->unref();
}

TEST(Engines, ConcurrentCalculatorParserKeepsTreesAndErrorsIndependent)
{
    std::atomic<bool> failed{false};
    std::vector<std::thread> threads;
    for (int thread = 0; thread < 8; ++thread) {
        threads.emplace_back([&] {
            for (int iteration = 0; iteration < 200; ++iteration) {
                so_eval_node * tree = so_eval_parse("oa = a + b");
                if (!tree || so_eval_error()) {
                    failed.store(true, std::memory_order_relaxed);
                }
                so_eval_delete(tree);

                tree = so_eval_parse("oa = ;");
                if (tree || !so_eval_error()) {
                    failed.store(true, std::memory_order_relaxed);
                }
                so_eval_delete(tree);
            }
        });
    }
    for (std::thread & thread : threads) thread.join();
    EXPECT_FALSE(failed.load(std::memory_order_relaxed));
}

TEST(Engines, ComposeVec3fPublishesConnectedVectorOutput)
{
    auto * compose = new SoComposeVec3f;
    compose->ref();
    compose->x.setValue(1.0f);
    compose->y.setValue(2.0f);
    compose->z.setValue(3.0f);

    SoSFVec3f result;
    result.connectFrom(&compose->vector);
    const SbVec3f value = result.getValue();
    EXPECT_FLOAT_EQ(value[0], 1.0f);
    EXPECT_FLOAT_EQ(value[1], 2.0f);
    EXPECT_FLOAT_EQ(value[2], 3.0f);
    SoEngineOutputList outputs;
    EXPECT_GE(compose->getOutputs(outputs), 1);
    EXPECT_NE(compose->getOutput("vector"), nullptr);
    result.disconnect();
    compose->unref();
}

TEST(Engines, ComposeAndDecomposeVectorAndMatrixOutputsRoundTrip)
{
    auto * compose_vector = new SoComposeVec2f;
    compose_vector->ref();
    compose_vector->x.set1Value(0, 3.0f);
    compose_vector->y.set1Value(0, 4.0f);
    SoMFVec2f vector;
    vector.connectFrom(&compose_vector->vector);
    ASSERT_EQ(vector.getNum(), 1);
    EXPECT_EQ(vector[0], SbVec2f(3.0f, 4.0f));

    auto * decompose_vector = new SoDecomposeVec2f;
    decompose_vector->ref();
    decompose_vector->vector.set1Value(0, vector[0]);
    SoMFFloat x;
    SoMFFloat y;
    x.connectFrom(&decompose_vector->x);
    y.connectFrom(&decompose_vector->y);
    ASSERT_EQ(x.getNum(), 1);
    ASSERT_EQ(y.getNum(), 1);
    EXPECT_FLOAT_EQ(x[0], 3.0f);
    EXPECT_FLOAT_EQ(y[0], 4.0f);

    auto * compose_matrix = new SoComposeMatrix;
    compose_matrix->ref();
    compose_matrix->translation.set1Value(0, SbVec3f(1.0f, 2.0f, 3.0f));
    compose_matrix->rotation.set1Value(0, SbRotation::identity());
    compose_matrix->scaleFactor.set1Value(0, SbVec3f(1.0f, 1.0f, 1.0f));
    compose_matrix->scaleOrientation.set1Value(0, SbRotation::identity());
    compose_matrix->center.set1Value(0, SbVec3f(0.0f, 0.0f, 0.0f));
    SoMFMatrix matrix;
    matrix.connectFrom(&compose_matrix->matrix);
    ASSERT_EQ(matrix.getNum(), 1);

    auto * decompose_matrix = new SoDecomposeMatrix;
    decompose_matrix->ref();
    decompose_matrix->matrix.set1Value(0, matrix[0]);
    decompose_matrix->center.set1Value(0, SbVec3f(0.0f, 0.0f, 0.0f));
    SoMFVec3f translation;
    translation.connectFrom(&decompose_matrix->translation);
    ASSERT_EQ(translation.getNum(), 1);
    EXPECT_EQ(translation[0], SbVec3f(1.0f, 2.0f, 3.0f));

    translation.disconnect();
    decompose_matrix->unref();
    matrix.disconnect();
    compose_matrix->unref();
    y.disconnect();
    x.disconnect();
    decompose_vector->unref();
    vector.disconnect();
    compose_vector->unref();
}

TEST(Engines, DecomposeVec3fAndComposeRotationPublishExpectedOutputs)
{
    auto * decompose = new SoDecomposeVec3f;
    decompose->ref();
    decompose->vector.set1Value(0, SbVec3f(3.0f, 4.0f, 5.0f));

    SoSFFloat x;
    SoSFFloat y;
    SoSFFloat z;
    x.connectFrom(&decompose->x);
    y.connectFrom(&decompose->y);
    z.connectFrom(&decompose->z);
    EXPECT_FLOAT_EQ(x.getValue(), 3.0f);
    EXPECT_FLOAT_EQ(y.getValue(), 4.0f);
    EXPECT_FLOAT_EQ(z.getValue(), 5.0f);
    x.disconnect();
    y.disconnect();
    z.disconnect();
    decompose->unref();

    auto * compose = new SoComposeRotation;
    compose->ref();
    compose->axis.set1Value(0, SbVec3f(0.0f, 0.0f, 1.0f));
    compose->angle.set1Value(0, 1.57079632679f);
    SoSFRotation rotation;
    rotation.connectFrom(&compose->rotation);
    SbVec3f axis;
    float angle = 0.0f;
    rotation.getValue().getValue(axis, angle);
    EXPECT_NEAR(axis[2], 1.0f, 1e-5f);
    EXPECT_NEAR(angle, 1.57079632679f, 1e-5f);
    rotation.disconnect();
    compose->unref();
}

TEST(Engines, BooleanOperationPublishesAndOrResults)
{
    auto * operation = new SoBoolOperation;
    operation->ref();
    SoSFBool result;
    result.connectFrom(&operation->output);

    operation->a.set1Value(0, TRUE);
    operation->b.set1Value(0, TRUE);
    operation->operation.set1Value(0, SoBoolOperation::A_AND_B);
    EXPECT_TRUE(result.getValue());

    operation->b.set1Value(0, FALSE);
    EXPECT_FALSE(result.getValue());
    operation->operation.set1Value(0, SoBoolOperation::A_OR_B);
    EXPECT_TRUE(result.getValue());
    result.disconnect();
    operation->unref();
}

TEST(Engines, ConnectFieldsAndRetainTimeInterpolationAndRoutingConfiguration)
{
    auto * calculator = new SoCalculator;
    calculator->ref();
    calculator->expression.setValue("oa = a + b");
    calculator->a.setValue(4.0f);
    calculator->b.setValue(2.0f);
    auto * sphere = new SoSphere;
    sphere->ref();
    sphere->radius.connectFrom(&calculator->oa);
    EXPECT_FLOAT_EQ(sphere->radius.getValue(), 6.0f);
    sphere->radius.disconnect();
    sphere->unref();
    calculator->unref();

    auto * counter = new SoTimeCounter;
    counter->ref();
    counter->min.setValue(0);
    counter->max.setValue(10);
    counter->step.setValue(1);
    EXPECT_EQ(counter->min.getValue(), 0);
    EXPECT_EQ(counter->max.getValue(), 10);
    EXPECT_EQ(counter->step.getValue(), 1);
    counter->unref();

    auto * elapsed = new SoElapsedTime;
    elapsed->ref();
    elapsed->speed.setValue(2.0f);
    EXPECT_FLOAT_EQ(elapsed->speed.getValue(), 2.0f);
    elapsed->unref();

    auto * interpolate = new SoInterpolateFloat;
    interpolate->ref();
    interpolate->input0.setValue(0.0f);
    interpolate->input1.setValue(10.0f);
    interpolate->alpha.setValue(0.5f);
    SoSFFloat interpolated;
    interpolated.connectFrom(&interpolate->output);
    EXPECT_FLOAT_EQ(interpolated.getValue(), 5.0f);
    interpolated.disconnect();
    interpolate->unref();

    auto * gate = new SoGate(SoMFFloat::getClassTypeId());
    gate->ref();
    gate->enable.setValue(TRUE);
    EXPECT_TRUE(gate->enable.getValue());
    gate->unref();

    auto * selection = new SoSelectOne(SoMFFloat::getClassTypeId());
    selection->ref();
    selection->index.setValue(0);
    EXPECT_EQ(selection->index.getValue(), 0);
    selection->unref();
}
