#include <gtest/gtest.h>

#include <Inventor/SbVec2s.h>
#include <Inventor/events/SoKeyboardEvent.h>
#include <Inventor/events/SoLocation2Event.h>
#include <Inventor/events/SoMouseButtonEvent.h>

TEST(Events, MouseButtonPressAndReleasePredicatesTrackState)
{
    SoMouseButtonEvent event;
    event.setButton(SoMouseButtonEvent::BUTTON1);
    event.setState(SoButtonEvent::DOWN);
    EXPECT_TRUE(SoMouseButtonEvent::isButtonPressEvent(
        &event, SoMouseButtonEvent::BUTTON1));
    EXPECT_FALSE(SoMouseButtonEvent::isButtonReleaseEvent(
        &event, SoMouseButtonEvent::BUTTON1));

    const SbVec2s position(100, 200);
    event.setPosition(position);
    EXPECT_EQ(event.getPosition(), position);
}

TEST(Events, KeyboardAndLocationEventsRetainTheirValues)
{
    SoKeyboardEvent keyboard;
    keyboard.setKey(SoKeyboardEvent::A);
    keyboard.setState(SoButtonEvent::DOWN);
    EXPECT_TRUE(SoKeyboardEvent::isKeyPressEvent(&keyboard, SoKeyboardEvent::A));
    EXPECT_EQ(keyboard.getKey(), SoKeyboardEvent::A);

    SoLocation2Event location;
    const SbVec2s position(320, 240);
    location.setPosition(position);
    EXPECT_EQ(location.getPosition(), position);
}
