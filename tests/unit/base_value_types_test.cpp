#include <gtest/gtest.h>

#include <Inventor/SbColor.h>
#include <Inventor/SbColor4f.h>
#include <Inventor/SbDict.h>
#include <Inventor/SbImage.h>
#include <Inventor/SbName.h>
#include <Inventor/SbPList.h>
#include <Inventor/SbString.h>
#include <Inventor/SbTime.h>
#include <Inventor/SbVec2d.h>
#include <Inventor/SbVec2i32.h>
#include <Inventor/SbVec2s.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbVec3i32.h>
#include <Inventor/SbVec3s.h>
#include <Inventor/SbVec4d.h>
#include <Inventor/SbVec4i32.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

namespace {

int dictionary_callback_count = 0;

void countDictionaryEntry(SbDictKeyType, void *)
{
    ++dictionary_callback_count;
}

} // namespace

TEST(BaseStrings, StringNameAndTimeExposeStableValueSemantics)
{
    const SbString greeting("hello");
    EXPECT_EQ(greeting.getLength(), 5);
    EXPECT_STREQ(greeting.getString(), "hello");
    const SbString phrase = greeting + " world";
    EXPECT_EQ(phrase.getLength(), 11);
    EXPECT_EQ(phrase.getSubString(6, 10), SbString("world"));
    EXPECT_EQ(SbString(42), SbString("42"));
    SbString concatenated("foo");
    concatenated += SbString("bar");
    EXPECT_EQ(concatenated, SbString("foobar"));
    SbString formatted;
    formatted.sprintf("val=%d", 42);
    EXPECT_EQ(formatted, SbString("val=42"));
    EXPECT_EQ(SbString("hello world").find("world"), 6);

    const SbName name("myNode");
    EXPECT_STREQ(name.getString(), "myNode");
    EXPECT_EQ(name.getLength(), 6);
    EXPECT_EQ(name, SbName("myNode"));
    EXPECT_NE(name, SbName("other"));
    EXPECT_TRUE(!SbName::empty());
    EXPECT_TRUE(SbName::isIdentStartChar('A'));
    EXPECT_TRUE(SbName::isIdentChar('_'));
    EXPECT_TRUE(SbName::isIdentChar('0'));
    EXPECT_FALSE(SbName::isIdentStartChar('5'));
    EXPECT_EQ(SbName("").getLength(), 0);

    const SbTime duration(1.5);
    const SbTime half_second(0.5);
    EXPECT_DOUBLE_EQ(duration.getValue(), 1.5);
    EXPECT_DOUBLE_EQ((duration + half_second).getValue(), 2.0);
    EXPECT_DOUBLE_EQ((duration - half_second).getValue(), 1.0);
    EXPECT_DOUBLE_EQ((duration * 2.0).getValue(), 3.0);
    EXPECT_NEAR((duration / 3.0).getValue(), 0.5, 1e-6);
    EXPECT_GT(duration, half_second);
    EXPECT_EQ(duration.getMsecValue(), 1500ul);
    SbTime configured;
    configured.setValue(3.14);
    EXPECT_NEAR(configured.getValue(), 3.14, 1e-6);
    configured.setValue(7, 500000);
    EXPECT_DOUBLE_EQ(configured.getValue(), 7.5);
    EXPECT_GT(SbTime(65.0).format().getLength(), 0);
    EXPECT_GT(SbTime::getTimeOfDay().getValue(), 0.0);
}

TEST(BaseTime, MillisecondConversionDefinesAllOutOfRangeInputs)
{
    const int value_bits = std::numeric_limits<unsigned long>::digits;
    // 2^(N-3) seconds is exactly 125 complete 2^N millisecond ranges.
    const SbTime overflowing(std::ldexp(1.0, value_bits - 3));
    EXPECT_EQ(overflowing.getMsecValue(), 0ul);

    // Preserve unsigned-style wrapping without relying on undefined
    // floating-to-integer conversions.
    EXPECT_EQ(SbTime(-0.001).getMsecValue(),
              std::numeric_limits<unsigned long>::max());
    EXPECT_EQ(SbTime(std::numeric_limits<double>::infinity()).getMsecValue(),
              0ul);
    EXPECT_EQ(SbTime(std::numeric_limits<double>::quiet_NaN()).getMsecValue(),
              0ul);
}

TEST(BaseDictionary, SupportsLookupRemovalEnumerationAndPointerLists)
{
    SbDict dictionary;
    int first = 42;
    int second = 99;
    ASSERT_TRUE(dictionary.enter(1, &first));
    ASSERT_TRUE(dictionary.enter(2, &second));
    void * found = nullptr;
    ASSERT_TRUE(dictionary.find(1, found));
    EXPECT_EQ(found, &first);
    EXPECT_FALSE(dictionary.find(999, found));
    ASSERT_TRUE(dictionary.remove(1));
    EXPECT_FALSE(dictionary.find(1, found));

    dictionary_callback_count = 0;
    dictionary.applyToAll(countDictionaryEntry);
    EXPECT_EQ(dictionary_callback_count, 1);

    SbPList keys;
    SbPList values;
    dictionary.makePList(keys, values);
    EXPECT_EQ(keys.getLength(), 1);
    EXPECT_EQ(values.getLength(), 1);
    dictionary.clear();
    EXPECT_FALSE(dictionary.find(2, found));
}

TEST(BaseColor, HsvAndPackedValuesRoundTrip)
{
    SbColor red;
    red.setHSVValue(0.0f, 1.0f, 1.0f);
    EXPECT_NEAR(red[0], 1.0f, 1e-3f);
    EXPECT_NEAR(red[1], 0.0f, 1e-3f);
    EXPECT_NEAR(red[2], 0.0f, 1e-3f);
    float hue = 0.0f;
    float saturation = 0.0f;
    float value = 0.0f;
    red.getHSVValue(hue, saturation, value);
    EXPECT_NEAR(hue, 0.0f, 1e-3f);
    EXPECT_NEAR(saturation, 1.0f, 1e-3f);
    EXPECT_NEAR(value, 1.0f, 1e-3f);

    const std::uint32_t packed = red.getPackedValue(0.25f);
    SbColor unpacked;
    float transparency = 0.0f;
    unpacked.setPackedValue(packed, transparency);
    EXPECT_NEAR(unpacked[0], 1.0f, 0.01f);
    EXPECT_NEAR(transparency, 0.25f, 0.01f);

    SbColor green;
    green.setHSVValue(1.0f / 3.0f, 1.0f, 1.0f);
    EXPECT_NEAR(green[0], 0.0f, 0.01f);
    EXPECT_NEAR(green[1], 1.0f, 0.01f);
    EXPECT_NEAR(green[2], 0.0f, 0.01f);

    const float cyan_hsv[] = {0.5f, 0.8f, 0.9f};
    SbColor cyan;
    cyan.setHSVValue(cyan_hsv);
    float recovered_hsv[3] = {};
    cyan.getHSVValue(recovered_hsv);
    EXPECT_NEAR(recovered_hsv[0], cyan_hsv[0], 0.01f);
    EXPECT_NEAR(recovered_hsv[1], cyan_hsv[1], 0.01f);
    EXPECT_NEAR(recovered_hsv[2], cyan_hsv[2], 0.01f);

    SbColor opaque_red;
    float opaque_transparency = 1.0f;
    opaque_red.setPackedValue(0xFF0000FF, opaque_transparency);
    EXPECT_NEAR(opaque_red[0], 1.0f, 0.01f);
    EXPECT_NEAR(opaque_red[1], 0.0f, 0.01f);
    EXPECT_NEAR(opaque_red[2], 0.0f, 0.01f);
    EXPECT_NEAR(opaque_transparency, 0.0f, 0.01f);

    SbColor4f color4(0.5f, 0.5f, 0.5f, 0.75f);
    EXPECT_FLOAT_EQ(color4[3], 0.75f);
    const SbColor4f sum = color4 + SbColor4f(0.2f, 0.2f, 0.2f, 0.25f);
    EXPECT_NEAR(sum[0], 0.7f, 0.01f);
    EXPECT_NE(color4.getPackedValue(), 0u);
}

TEST(BaseImage, StoresTwoAndThreeDimensionalPixelData)
{
    SbImage image;
    const SbVec2s size(4, 4);
    unsigned char pixels[4 * 4 * 3];
    for (int index = 0; index < 4 * 4 * 3; ++index) {
        pixels[index] = static_cast<unsigned char>(index);
    }
    image.setValue(size, 3, pixels);
    SbVec2s actual_size;
    int components = 0;
    const unsigned char * actual = image.getValue(actual_size, components);
    EXPECT_EQ(actual_size, size);
    EXPECT_EQ(components, 3);
    ASSERT_NE(actual, nullptr);
    EXPECT_EQ(actual[5], pixels[5]);
    EXPECT_TRUE(image.hasData());

    SbImage volume;
    const SbVec3s volume_size(2, 2, 2);
    const unsigned char voxels[] = {0, 32, 64, 96, 128, 160, 192, 224};
    volume.setValue(volume_size, 1, voxels);
    EXPECT_EQ(volume.getSize(), volume_size);
}

TEST(BaseImage, EqualityIncludesPixelContents)
{
    const SbVec2s size(2, 2);
    const unsigned char original[] = {0, 1, 2, 3};
    const unsigned char changed[] = {0, 1, 2, 4};
    SbImage first;
    SbImage second;
    first.setValue(size, 1, original);
    second.setValue(size, 1, original);
    EXPECT_EQ(first, second);
    second.setValue(size, 1, changed);
    EXPECT_NE(first, second);
}

TEST(BaseIntegerVectors, FloatingPointConversionsAreDefinedAndSaturating)
{
    constexpr double above_max =
        static_cast<double>(std::numeric_limits<std::int32_t>::max()) + 1024.0;
    constexpr double below_min =
        static_cast<double>(std::numeric_limits<std::int32_t>::min()) - 1024.0;

    SbVec2i32 vector2;
    vector2.setValue(SbVec2d(above_max, below_min));
    EXPECT_EQ(vector2[0], std::numeric_limits<std::int32_t>::max());
    EXPECT_EQ(vector2[1], std::numeric_limits<std::int32_t>::min());

    SbVec3i32 vector3;
    vector3.setValue(SbVec3f(
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::quiet_NaN()));
    EXPECT_EQ(vector3[0], std::numeric_limits<std::int32_t>::max());
    EXPECT_EQ(vector3[1], std::numeric_limits<std::int32_t>::min());
    EXPECT_EQ(vector3[2], 0);

    SbVec4i32 vector4;
    vector4.setValue(SbVec4d(1.9, -1.9, 42.0, above_max));
    EXPECT_EQ(vector4, SbVec4i32(1, -1, 42,
                                std::numeric_limits<std::int32_t>::max()));

    SbVec4i32 scaled(1, -1, 7, -7);
    scaled *= std::numeric_limits<double>::infinity();
    EXPECT_EQ(scaled[0], std::numeric_limits<std::int32_t>::max());
    EXPECT_EQ(scaled[1], std::numeric_limits<std::int32_t>::min());
    EXPECT_EQ(scaled[2], std::numeric_limits<std::int32_t>::max());
    EXPECT_EQ(scaled[3], std::numeric_limits<std::int32_t>::min());
}
