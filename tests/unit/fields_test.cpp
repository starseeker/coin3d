#include <gtest/gtest.h>

#include <Inventor/SbColor.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbRotation.h>
#include <Inventor/SbString.h>
#include <Inventor/SbVec2f.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/fields/SoMFColor.h>
#include <Inventor/fields/SoMFFloat.h>
#include <Inventor/fields/SoMFInt32.h>
#include <Inventor/fields/SoMFString.h>
#include <Inventor/fields/SoMFVec3f.h>
#include <Inventor/fields/SoField.h>
#include <Inventor/fields/SoFieldContainer.h>
#include <Inventor/fields/SoSFBool.h>
#include <Inventor/fields/SoSFColor.h>
#include <Inventor/fields/SoSFFloat.h>
#include <Inventor/fields/SoSFMatrix.h>
#include <Inventor/fields/SoSFNode.h>
#include <Inventor/fields/SoSFRotation.h>
#include <Inventor/fields/SoSFString.h>
#include <Inventor/fields/SoSFVec2f.h>
#include <Inventor/fields/SoSFVec3f.h>
#include <Inventor/engines/SoComposeVec3f.h>
#include <Inventor/engines/SoCalculator.h>
#include <Inventor/engines/SoEngineOutput.h>
#include <Inventor/lists/SoFieldList.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoMaterial.h>

#include <cstdint>

TEST(Fields, ScalarValuesRoundTripWithoutConnections)
{
    SoSFFloat number;
    number.setValue(3.14f);
    EXPECT_FLOAT_EQ(number.getValue(), 3.14f);
    EXPECT_FALSE(number.isConnected());

    SoSFBool enabled;
    enabled.setValue(TRUE);
    EXPECT_EQ(enabled.getValue(), TRUE);

    SoSFString text;
    text.setValue("Obol");
    EXPECT_EQ(text.getValue(), SbString("Obol"));

    SoSFColor color;
    color.setValue(SbColor(1.0f, 0.0f, 0.5f));
    const SbColor actual_color = color.getValue();
    EXPECT_FLOAT_EQ(actual_color[0], 1.0f);
    EXPECT_FLOAT_EQ(actual_color[2], 0.5f);
}

TEST(Fields, VectorAndMatrixValuesPreserveComponents)
{
    SoSFVec2f vector;
    vector.setValue(SbVec2f(3.0f, 4.0f));
    EXPECT_FLOAT_EQ(vector.getValue()[0], 3.0f);
    EXPECT_FLOAT_EQ(vector.getValue()[1], 4.0f);

    SbMatrix matrix = SbMatrix::identity();
    matrix.setTranslate(SbVec3f(1.0f, 2.0f, 3.0f));
    SoSFMatrix matrix_field;
    matrix_field.setValue(matrix);
    EXPECT_FLOAT_EQ(matrix_field.getValue()[3][0], 1.0f);
    EXPECT_FLOAT_EQ(matrix_field.getValue()[3][1], 2.0f);
    EXPECT_FLOAT_EQ(matrix_field.getValue()[3][2], 3.0f);
}

TEST(Fields, MultiFieldsRetainCountOrderAndValues)
{
    SoMFVec3f points;
    const SbVec3f source_points[] = {
        SbVec3f(0.0f, 0.0f, 0.0f),
        SbVec3f(1.0f, 0.0f, 0.0f),
        SbVec3f(0.0f, 1.0f, 0.0f),
    };
    points.setValues(0, 3, source_points);
    ASSERT_EQ(points.getNum(), 3);
    EXPECT_FLOAT_EQ(points[1][0], 1.0f);
    EXPECT_FLOAT_EQ(points[2][1], 1.0f);

    SoMFFloat weights;
    const float source_weights[] = {1.0f, 2.0f, 3.0f, 4.0f};
    weights.setValues(0, 4, source_weights);
    ASSERT_EQ(weights.getNum(), 4);
    EXPECT_FLOAT_EQ(weights[3], 4.0f);

    SoMFString labels;
    labels.set1Value(0, "first");
    labels.set1Value(1, "second");
    ASSERT_EQ(labels.getNum(), 2);
    EXPECT_EQ(labels[0], SbString("first"));
    EXPECT_EQ(labels[1], SbString("second"));

    SoMFColor palette;
    palette.set1Value(0, SbColor(1.0f, 0.0f, 0.0f));
    palette.set1Value(1, SbColor(0.0f, 1.0f, 0.0f));
    ASSERT_EQ(palette.getNum(), 2);
    EXPECT_FLOAT_EQ(palette[1][1], 1.0f);

    SoMFInt32 ids;
    const std::int32_t source_ids[] = {10, 20, 30};
    ids.setValues(0, 3, source_ids);
    ASSERT_EQ(ids.getNum(), 3);
    EXPECT_EQ(ids[1], 20);

    SoMFFloat editable_weights;
    editable_weights.set1Value(0, 1.0f);
    editable_weights.set1Value(1, 2.0f);
    editable_weights.set1Value(2, 3.0f);
    ASSERT_EQ(editable_weights.getNum(), 3);
    EXPECT_FLOAT_EQ(editable_weights[2], 3.0f);
    editable_weights.deleteValues(1, -1);
    EXPECT_EQ(editable_weights.getNum(), 1);
}

TEST(Fields, NodeFieldsAndNodeOwnedFieldsPreserveReferencesAndValues)
{
    auto * cube = new SoCube;
    cube->ref();

    SoSFNode node;
    node.setValue(cube);
    EXPECT_EQ(node.getValue(), cube);

    cube->width.setValue(5.0f);
    EXPECT_FLOAT_EQ(cube->width.getValue(), 5.0f);

    node.setValue(nullptr);
    cube->unref();
}

TEST(Fields, ConnectionsPropagateEngineAndFieldValuesUntilDisconnected)
{
    auto * engine = new SoComposeVec3f;
    engine->ref();
    engine->x.set1Value(0, 5.0f);
    engine->y.set1Value(0, 6.0f);
    engine->z.set1Value(0, 7.0f);
    SoSFVec3f vector_target;
    vector_target.connectFrom(&engine->vector);
    EXPECT_EQ(vector_target.getValue(), SbVec3f(5, 6, 7));
    EXPECT_TRUE(vector_target.isConnected());
    vector_target.disconnect();
    EXPECT_FALSE(vector_target.isConnected());
    engine->unref();

    SoSFFloat source;
    SoSFFloat target;
    source.setValue(3.14f);
    target.connectFrom(&source);
    EXPECT_TRUE(target.isConnected());
    EXPECT_TRUE(target.isConnectedFromField());
    EXPECT_FLOAT_EQ(target.getValue(), 3.14f);
    source.setValue(2.71f);
    EXPECT_FLOAT_EQ(target.getValue(), 2.71f);
    target.disconnect();
    EXPECT_FALSE(target.isConnected());
}

TEST(Fields, ExposeStateAndConnectionMetadata)
{
    SoSFFloat ignored;
    ignored.setValue(3.14f);
    ignored.setIgnored(TRUE);
    EXPECT_TRUE(ignored.isIgnored());
    ignored.setIgnored(FALSE);
    EXPECT_FALSE(ignored.isIgnored());

    auto * connection_cube = new SoCube;
    connection_cube->ref();
    connection_cube->width.enableConnection(FALSE);
    EXPECT_FALSE(connection_cube->width.isConnectionEnabled());
    connection_cube->width.enableConnection(TRUE);
    EXPECT_TRUE(connection_cube->width.isConnectionEnabled());
    connection_cube->unref();

    auto * cube = new SoCube;
    cube->ref();
    EXPECT_TRUE(cube->width.isNotifyEnabled());
    const SbBool notifications_were_enabled = cube->width.enableNotify(FALSE);
    EXPECT_FALSE(cube->width.isNotifyEnabled());
    cube->width.enableNotify(notifications_were_enabled);
    EXPECT_EQ(cube->width.getContainer(), static_cast<SoFieldContainer *>(cube));
    cube->unref();

    SoSFFloat source;
    SoSFFloat target;
    target.connectFrom(&source);
    SoField * connected_field = nullptr;
    EXPECT_TRUE(target.getConnectedField(connected_field));
    EXPECT_EQ(connected_field, &source);
    target.disconnect();

    auto * calculator = new SoCalculator;
    calculator->ref();
    calculator->a.setValue(1.0f);
    calculator->b.setValue(2.0f);
    calculator->expression.setValue("oa = a + b");
    SoSFFloat engine_result;
    engine_result.connectFrom(&calculator->oa);
    EXPECT_TRUE(engine_result.isConnectedFromEngine());
    SoEngineOutput * connected_engine = nullptr;
    EXPECT_TRUE(engine_result.getConnectedEngine(connected_engine));
    EXPECT_EQ(connected_engine, &calculator->oa);
    engine_result.disconnect();
    calculator->unref();

    SoMFFloat values;
    const float source_values[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    values.setValues(0, 5, source_values);
    ASSERT_EQ(values.getNum(), 5);
    const float * stored_values = values.getValues(0);
    ASSERT_NE(stored_values, nullptr);
    EXPECT_FLOAT_EQ(stored_values[2], 3.0f);
}

TEST(Fields, NodeFieldMetadataSupportsLookupEnumerationAndDefaultState)
{
    auto * cube = new SoCube;
    cube->ref();

    SoField * width = cube->getField("width");
    ASSERT_NE(width, nullptr);
    EXPECT_EQ(width, &cube->width);
    EXPECT_EQ(cube->getField("not-a-cube-field"), nullptr);

    SbName field_name;
    EXPECT_TRUE(cube->getFieldName(&cube->width, field_name));
    EXPECT_EQ(field_name, SbName("width"));

    SoFieldList fields;
    EXPECT_GE(cube->getFields(fields), 3);
    EXPECT_TRUE(cube->width.isDefault());
    cube->width.setValue(3.0f);
    EXPECT_FALSE(cube->width.isDefault());
    cube->width.setDefault(TRUE);
    EXPECT_TRUE(cube->width.isDefault());

    SoSFFloat standalone;
    standalone.setValue(1.0f);
    standalone.evaluate();
    EXPECT_FLOAT_EQ(standalone.getValue(), 1.0f);
    cube->unref();
}

TEST(Fields, FieldMutationAndForwardConnectionsPreserveState)
{
    auto * source = new SoMaterial;
    auto * target = new SoMaterial;
    source->ref();
    target->ref();

    source->diffuseColor.setValue(SbColor(0.8f, 0.2f, 0.1f));
    target->diffuseColor.connectFrom(&source->diffuseColor);
    SoFieldList forward_connections;
    EXPECT_EQ(source->diffuseColor.getForwardConnections(forward_connections), 1);
    EXPECT_EQ(forward_connections[0], &target->diffuseColor);
    EXPECT_TRUE(target->diffuseColor.isConnectedFromField());
    EXPECT_TRUE(target->diffuseColor.isConnectionEnabled());
    target->diffuseColor.enableConnection(FALSE);
    EXPECT_FALSE(target->diffuseColor.isConnectionEnabled());
    target->diffuseColor.enableConnection(TRUE);
    target->diffuseColor.disconnect(&source->diffuseColor);
    EXPECT_FALSE(target->diffuseColor.isConnected());

    SbString serialized;
    source->diffuseColor.get(serialized);
    EXPECT_GT(serialized.getLength(), 0);
    EXPECT_TRUE(source->diffuseColor.set("0.9 0.1 0.5"));
    source->diffuseColor.touch();
    source->diffuseColor.setDefault(TRUE);
    EXPECT_TRUE(source->diffuseColor.isDefault());
    source->diffuseColor.setIgnored(TRUE);
    EXPECT_TRUE(source->diffuseColor.isIgnored());
    source->diffuseColor.setIgnored(FALSE);
    source->diffuseColor.enableNotify(FALSE);
    EXPECT_FALSE(source->diffuseColor.isNotifyEnabled());
    source->diffuseColor.enableNotify(TRUE);

    target->unref();
    source->unref();
}

TEST(Fields, CopyAndMutateFieldValuesWithoutChangingTheirOwnership)
{
    auto * source_cube = new SoCube;
    auto * copied_cube = new SoCube;
    source_cube->ref();
    copied_cube->ref();
    source_cube->width.setValue(5.0f);
    copied_cube->width.copyFrom(source_cube->width);
    EXPECT_FLOAT_EQ(copied_cube->width.getValue(), 5.0f);
    EXPECT_TRUE(source_cube->width.isSame(copied_cube->width));
    copied_cube->width.setValue(4.0f);
    EXPECT_FALSE(source_cube->width.isSame(copied_cube->width));
    source_cube->unref();
    copied_cube->unref();

    SoMFFloat values;
    values.set1Value(0, 1.0f);
    values.set1Value(1, 2.0f);
    values.set1Value(2, 3.0f);
    values.insertSpace(1, 1);
    ASSERT_EQ(values.getNum(), 4);
    values.deleteValues(0, 2);
    EXPECT_EQ(values.getNum(), 2);

    SoSFRotation rotation;
    rotation.setValue(SbRotation(SbVec3f(0.0f, 1.0f, 0.0f), 3.14159265358979323846f / 3.0f));
    SbVec3f axis;
    float angle = 0.0f;
    rotation.getValue().getValue(axis, angle);
    EXPECT_NEAR(angle, 3.14159265358979323846f / 3.0f, 1.0e-3f);
}
