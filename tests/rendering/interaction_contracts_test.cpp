#include "framework/render_fixture.h"

#include <gtest/gtest.h>

#include <Inventor/SbTime.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoInteraction.h>
#include <Inventor/actions/SoHandleEventAction.h>
#include <Inventor/actions/SoRayPickAction.h>
#include <Inventor/SoPickedPoint.h>
#include <Inventor/draggers/SoDragger.h>
#include <Inventor/draggers/SoTranslate1Dragger.h>
#include <Inventor/events/SoButtonEvent.h>
#include <Inventor/events/SoLocation2Event.h>
#include <Inventor/events/SoMouseButtonEvent.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTransform.h>

namespace {

void copyDraggerMotion(void * user_data, SoDragger * dragger)
{
    static_cast<SoTransform *>(user_data)->setMatrix(dragger->getMotionMatrix());
}

bool sendButton(SoNode * root, const SbViewportRegion & viewport,
                const SoButtonEvent::State state, const SbVec2s & position)
{
    SoMouseButtonEvent event;
    event.setButton(SoMouseButtonEvent::BUTTON1);
    event.setState(state);
    event.setPosition(position);
    event.setTime(SbTime::getTimeOfDay());
    SoHandleEventAction action(viewport);
    action.setEvent(&event);
    action.apply(root);
    return action.isHandled();
}

void sendMotion(SoNode * root, const SbViewportRegion & viewport,
                const SbVec2s & position)
{
    SoLocation2Event event;
    event.setPosition(position);
    event.setTime(SbTime::getTimeOfDay());
    SoHandleEventAction action(viewport);
    action.setEvent(&event);
    action.apply(root);
}

TEST(OSMesaInteractionContracts, MouseDragMovesTranslateDraggerAndAttachedTransform)
{
    constexpr int width = 256;
    constexpr int height = 256;
    const SbViewportRegion viewport(width, height);

    auto * root = new SoSeparator;
    root->ref();
    auto * camera = new SoPerspectiveCamera;
    camera->position.setValue(0.0f, 0.0f, 8.0f);
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);

    auto * attached_transform = new SoTransform;
    attached_transform->translation.setValue(0.0f, 0.0f, -1.0f);
    auto * geometry = new SoSeparator;
    geometry->addChild(attached_transform);
    auto * material = new SoMaterial;
    material->diffuseColor.setValue(0.5f, 0.7f, 0.5f);
    geometry->addChild(material);
    geometry->addChild(new SoCube);
    root->addChild(geometry);

    auto * dragger = new SoTranslate1Dragger;
    dragger->addMotionCallback(copyDraggerMotion, attached_transform);
    root->addChild(dragger);
    camera->viewAll(root, viewport);

    ObolTestSupport::RenderFixture fixture(width, height);
    ASSERT_TRUE(fixture.available());
    ASSERT_TRUE(fixture.render(root));

    const float initial_x = attached_transform->translation.getValue()[0];
    SbVec2s pick_position(128, 128);
    bool found_dragger_pick = false;
    for (short y = 16; y < height && !found_dragger_pick; y += 4) {
        for (short x = 16; x < width; x += 4) {
            SoRayPickAction picker(viewport);
            picker.setPoint(SbVec2s(x, y));
            picker.setPickAll(TRUE);
            picker.apply(root);
            const SoPickedPointList & points = picker.getPickedPointList();
            for (int index = 0; index < points.getLength(); ++index) {
                if (points[index]->getPath()->findNode(dragger) >= 0) {
                    pick_position.setValue(x, y);
                    found_dragger_pick = true;
                    break;
                }
            }
            if (found_dragger_pick) break;
        }
    }
    ASSERT_TRUE(found_dragger_pick);
    const SbVec2s end_position(static_cast<short>(pick_position[0] + 40),
                               pick_position[1]);
    EXPECT_TRUE(sendButton(root, viewport, SoButtonEvent::DOWN, pick_position));
    for (int step = 1; step <= 8; ++step) {
        const short x = static_cast<short>(pick_position[0] + step * 5);
        sendMotion(root, viewport, SbVec2s(x, pick_position[1]));
    }
    sendButton(root, viewport, SoButtonEvent::UP, end_position);

    EXPECT_NE(attached_transform->translation.getValue()[0], initial_x);
    EXPECT_NEAR(attached_transform->translation.getValue()[0],
                dragger->getMotionMatrix()[3][0], 1.0e-4f);
    ASSERT_TRUE(fixture.render(root));
    EXPECT_GT(fixture.nonBackgroundPixels(), 100u);
    root->unref();
}

} // namespace
