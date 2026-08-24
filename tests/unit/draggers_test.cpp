#include <gtest/gtest.h>

#include <Inventor/SbBox3f.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/draggers/SoDragger.h>
#include <Inventor/draggers/SoDragPointDragger.h>
#include <Inventor/draggers/SoHandleBoxDragger.h>
#include <Inventor/draggers/SoRotateCylindricalDragger.h>
#include <Inventor/draggers/SoRotateDiscDragger.h>
#include <Inventor/draggers/SoRotateSphericalDragger.h>
#include <Inventor/draggers/SoScale2Dragger.h>
#include <Inventor/draggers/SoScale2UniformDragger.h>
#include <Inventor/draggers/SoScale1Dragger.h>
#include <Inventor/draggers/SoTabPlaneDragger.h>
#include <Inventor/draggers/SoTabBoxDragger.h>
#include <Inventor/draggers/SoTrackballDragger.h>
#include <Inventor/draggers/SoTransformBoxDragger.h>
#include <Inventor/draggers/SoTransformerDragger.h>
#include <Inventor/draggers/SoTranslate1Dragger.h>
#include <Inventor/manips/SoHandleBoxManip.h>
#include <Inventor/manips/SoTrackballManip.h>
#include <Inventor/manips/SoTransformerManip.h>
#include <Inventor/manips/SoTransformManip.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTransform.h>

namespace {

template <typename Dragger>
void expect_dragger_type()
{
    auto * dragger = new Dragger;
    dragger->ref();
    EXPECT_TRUE(dragger->isOfType(SoDragger::getClassTypeId()));
    SoGetBoundingBoxAction bounds(SbViewportRegion(512, 512));
    bounds.apply(dragger);
    dragger->unref();
}

TEST(Draggers, CoreDraggersRetainTypeHierarchyAndFields)
{
    auto * translate = new SoTranslate1Dragger;
    translate->ref();
    translate->translation.setValue(1.0f, 2.0f, 3.0f);
    EXPECT_EQ(translate->translation.getValue()[0], 1.0f);
    EXPECT_TRUE(translate->isOfType(SoDragger::getClassTypeId()));
    translate->unref();

    expect_dragger_type<SoRotateSphericalDragger>();
    expect_dragger_type<SoTrackballDragger>();
    expect_dragger_type<SoHandleBoxDragger>();
    expect_dragger_type<SoTabPlaneDragger>();
    expect_dragger_type<SoTransformerDragger>();
    expect_dragger_type<SoDragPointDragger>();
    expect_dragger_type<SoScale1Dragger>();
    expect_dragger_type<SoScale2Dragger>();
    expect_dragger_type<SoScale2UniformDragger>();
    expect_dragger_type<SoRotateCylindricalDragger>();
    expect_dragger_type<SoRotateDiscDragger>();
    expect_dragger_type<SoTabBoxDragger>();
    expect_dragger_type<SoTransformBoxDragger>();
}

template <typename Manip>
void expect_manip_type_and_dragger()
{
    auto * manip = new Manip;
    manip->ref();
    EXPECT_TRUE(manip->isOfType(SoTransformManip::getClassTypeId()));
    EXPECT_NE(manip->getDragger(), nullptr);
    SoGetBoundingBoxAction bounds(SbViewportRegion(512, 512));
    bounds.apply(manip);
    manip->unref();
}

TEST(Draggers, TransformManipulatorsOwnUsableDraggers)
{
    expect_manip_type_and_dragger<SoHandleBoxManip>();
    expect_manip_type_and_dragger<SoTrackballManip>();

    auto * transformer = new SoTransformerManip;
    transformer->ref();
    EXPECT_TRUE(transformer->isOfType(SoTransformManip::getClassTypeId()));
    transformer->unref();
}

TEST(Draggers, CallbackAndMotionConfigurationApisRetainState)
{
    struct CallbackCounts {
        int value_changed = 0;
        int other_event = 0;
    } counts;

    auto * dragger = new SoTranslate1Dragger;
    dragger->ref();
    dragger->addValueChangedCallback(
        [](void * data, SoDragger *) {
            ++static_cast<CallbackCounts *>(data)->value_changed;
        }, &counts);
    dragger->addOtherEventCallback(
        [](void * data, SoDragger *) {
            ++static_cast<CallbackCounts *>(data)->other_event;
        }, &counts);

    dragger->setMinGesture(3);
    EXPECT_EQ(dragger->getMinGesture(), 3);
    dragger->setProjectorEpsilon(0.001f);
    EXPECT_FLOAT_EQ(dragger->getProjectorEpsilon(), 0.001f);
    dragger->setFrontOnProjector(SoDragger::USE_PICK);
    EXPECT_EQ(dragger->getFrontOnProjector(), SoDragger::USE_PICK);

    SbMatrix translated;
    translated.setTranslate(SbVec3f(2.0f, 0.0f, 0.0f));
    static_cast<SoDragger *>(dragger)->setMotionMatrix(translated);
    dragger->valueChanged();
    EXPECT_GE(counts.value_changed, 1);
    EXPECT_NEAR(dragger->translation.getValue()[0], 2.0f, 1e-4f);
    EXPECT_NEAR(dragger->getMotionMatrix()[3][0], 2.0f, 1e-4f);

    EXPECT_TRUE(dragger->enableValueChangedCallbacks(FALSE));
    EXPECT_FALSE(dragger->enableValueChangedCallbacks(TRUE));
    dragger->unref();
}

TEST(Draggers, ManipulatorsReplaceAndRestoreTransformNodes)
{
    auto * root = new SoSeparator;
    root->ref();
    auto * transform = new SoTransform;
    transform->translation.setValue(1.0f, 2.0f, 3.0f);
    root->addChild(transform);
    root->addChild(new SoCube);

    SoSearchAction find_transform;
    find_transform.setType(SoTransform::getClassTypeId());
    find_transform.setInterest(SoSearchAction::FIRST);
    find_transform.apply(root);
    ASSERT_NE(find_transform.getPath(), nullptr);

    auto * manip = new SoTrackballManip;
    manip->ref();
    ASSERT_TRUE(manip->replaceNode(find_transform.getPath()));

    SoSearchAction find_manip;
    find_manip.setType(manip->getTypeId());
    find_manip.setInterest(SoSearchAction::FIRST);
    find_manip.apply(root);
    ASSERT_NE(find_manip.getPath(), nullptr);
    EXPECT_EQ(find_manip.getPath()->getTail(), manip);

    EXPECT_TRUE(manip->replaceManip(find_manip.getPath(), nullptr));
    SoSearchAction restored;
    restored.setType(SoTransform::getClassTypeId());
    restored.setInterest(SoSearchAction::FIRST);
    restored.apply(root);
    ASSERT_NE(restored.getPath(), nullptr);
    EXPECT_EQ(restored.getPath()->getTail()->getTypeId(), SoTransform::getClassTypeId());

    manip->unref();
    root->unref();
}

} // namespace
