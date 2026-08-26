#include <gtest/gtest.h>

#include <Inventor/SbImage.h>
#include <Inventor/SbName.h>
#include <Inventor/SoInput.h>
#include <Inventor/SoOutput.h>
#include <Inventor/fields/SoSFImage.h>
#include <Inventor/fields/SoSFImage3.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

void * grow_output_buffer(void * pointer, size_t size)
{
    return std::realloc(pointer, size);
}

std::string write_image_field(const SoSFImage & image)
{
    SoOutput output;
    output.setBuffer(nullptr, 1, grow_output_buffer);
    output.setStage(SoOutput::WRITE);
    image.write(&output, SbName("image"));

    void * buffer = nullptr;
    size_t size = 0;
    if (!output.getBuffer(buffer, size)) return {};
    return std::string(static_cast<const char *>(buffer), size);
}

template <typename Integer>
std::string write_binary_integer(const Integer value)
{
    SoOutput output;
    output.setBuffer(nullptr, 1, grow_output_buffer);
    output.setBinary(TRUE);
    output.write(value);

    void * buffer = nullptr;
    size_t size = 0;
    if (!output.getBuffer(buffer, size)) return {};
    return std::string(static_cast<const char *>(buffer), size);
}

} // namespace

TEST(ImageFields, TransparencyAndSubImagesPreserveUsefulImageSemantics)
{
    SoSFImage image;
    const unsigned char opaque_rgba[] = {10, 20, 30, 255};
    image.setValue(SbVec2s(1, 1), 4, opaque_rgba);
    EXPECT_FALSE(image.hasTransparency());

    const unsigned char transparent_rgba[] = {10, 20, 30, 0};
    image.setValue(SbVec2s(1, 1), 4, transparent_rgba);
    EXPECT_TRUE(image.hasTransparency());

    const unsigned char rgb[] = {10, 20, 30};
    image.setValue(SbVec2s(1, 1), 3, rgb);
    EXPECT_FALSE(image.hasTransparency());

    const unsigned char sub_pixels[] = {
        1, 2, 3, 4,
    };
    image.setValue(SbVec2s(4, 4), 1, nullptr);
    image.setSubValue(SbVec2s(2, 2), SbVec2s(1, 1),
                      const_cast<unsigned char *>(sub_pixels));

    int count = 0;
    EXPECT_TRUE(image.hasSubTextures(count));
    EXPECT_EQ(count, 1);
    SbVec2s dims;
    SbVec2s offset;
    unsigned char * stored = image.getSubTexture(0, dims, offset);
    ASSERT_NE(stored, nullptr);
    EXPECT_EQ(dims, SbVec2s(2, 2));
    EXPECT_EQ(offset, SbVec2s(1, 1));
    EXPECT_EQ(std::memcmp(stored, sub_pixels, sizeof(sub_pixels)), 0);
    EXPECT_EQ(image.getSubTexture(-1, dims, offset), nullptr);
    EXPECT_EQ(dims, SbVec2s(0, 0));

    SbVec2s image_size;
    int components = 0;
    const unsigned char * image_pixels = image.getValue(image_size, components);
    ASSERT_NE(image_pixels, nullptr);
    EXPECT_EQ(image_size, SbVec2s(4, 4));
    EXPECT_EQ(components, 1);
    EXPECT_EQ(image_pixels[5], 1);
    EXPECT_EQ(image_pixels[6], 2);
    EXPECT_EQ(image_pixels[9], 3);
    EXPECT_EQ(image_pixels[10], 4);

    const SbVec2s sub_dims[] = {SbVec2s(1, 1), SbVec2s(1, 1)};
    const SbVec2s sub_offsets[] = {SbVec2s(0, 0), SbVec2s(3, 3)};
    unsigned char first_pixel[] = {9};
    unsigned char second_pixel[] = {11};
    unsigned char * blocks[] = {first_pixel, second_pixel};
    image.setSubValues(sub_dims, sub_offsets, 2, blocks);
    EXPECT_TRUE(image.hasSubTextures(count));
    EXPECT_EQ(count, 3);
    image_pixels = image.getValue(image_size, components);
    EXPECT_EQ(image_pixels[0], 9);
    EXPECT_EQ(image_pixels[15], 11);
    EXPECT_EQ(image_pixels[5], 1);

    unsigned char * editable = image.startEditing(image_size, components);
    ASSERT_NE(editable, nullptr);
    editable[5] = 77;
    image.finishEditing();
    EXPECT_TRUE(image.hasSubTextures(count));
    EXPECT_EQ(count, 4);
    stored = image.getSubTexture(3, dims, offset);
    ASSERT_NE(stored, nullptr);
    EXPECT_EQ(dims, SbVec2s(4, 4));
    EXPECT_EQ(offset, SbVec2s(0, 0));
    EXPECT_EQ(stored[5], 77);

    EXPECT_FALSE(image.set("2 2 1 0x01"));
    EXPECT_FALSE(image.hasSubTextures(count));
    EXPECT_EQ(count, 0);

    ASSERT_TRUE(image.set("1 1 1 0x2a"));
    EXPECT_FALSE(image.hasSubTextures(count));
    EXPECT_EQ(count, 0);
    image_pixels = image.getValue(image_size, components);
    ASSERT_NE(image_pixels, nullptr);
    EXPECT_EQ(image_size, SbVec2s(1, 1));
    EXPECT_EQ(image_pixels[0], 42);
}

TEST(ImageFields, InvalidDimensionsDoNotCorruptExistingImages)
{
    SoSFImage image;
    const unsigned char pixel[] = {42};
    image.setValue(SbVec2s(1, 1), 1, pixel);
    image.setValue(SbVec2s(-1, 1), 1, pixel);

    SbVec2s size;
    int components = 0;
    const unsigned char * stored = image.getValue(size, components);
    ASSERT_NE(stored, nullptr);
    EXPECT_EQ(size, SbVec2s(1, 1));
    EXPECT_EQ(components, 1);
    EXPECT_EQ(stored[0], 42);

    SoSFImage3 volume;
    volume.setValue(SbVec3s(1, 1, 1), 1, pixel);
    volume.setValue(SbVec3s(1, 1, -1), 1, pixel);
    SbVec3s volume_size;
    components = 0;
    stored = volume.getValue(volume_size, components);
    ASSERT_NE(stored, nullptr);
    EXPECT_EQ(volume_size, SbVec3s(1, 1, 1));
    EXPECT_EQ(components, 1);

    SbImage base_image;
    base_image.setValue(SbVec2s(1, 1), 1, pixel);
    base_image.setValue(SbVec3s(-1, 1, 0), 1, pixel);
    EXPECT_EQ(base_image.getSize(), SbVec3s(1, 1, 0));
}

TEST(ImageFields, SubImageHistoryIsBoundedWithoutLosingCurrentPixels)
{
    SoSFImage image;
    const unsigned char initial[] = {0};
    image.setValue(SbVec2s(1, 1), 1, initial);

    for (int update = 0; update < 100; ++update) {
        unsigned char pixel[] = {static_cast<unsigned char>(update)};
        image.setSubValue(SbVec2s(1, 1), SbVec2s(0, 0), pixel);
    }

    int retained = 0;
    EXPECT_TRUE(image.hasSubTextures(retained));
    EXPECT_EQ(retained, 64);

    SbVec2s dims;
    SbVec2s offset;
    unsigned char * oldest = image.getSubTexture(0, dims, offset);
    ASSERT_NE(oldest, nullptr);
    EXPECT_EQ(oldest[0], 36);

    SbVec2s size;
    int components = 0;
    const unsigned char * current = image.getValue(size, components);
    ASSERT_NE(current, nullptr);
    EXPECT_EQ(current[0], 99);
}

TEST(ImageFields, NeverWriteOmitsPixelPayload)
{
    SoSFImage image;
    const unsigned char pixel[] = {1, 2, 3, 4};
    image.setValue(SbVec2s(1, 1), 4, pixel);
    const std::string normal = write_image_field(image);
    ASSERT_FALSE(normal.empty());

    image.setNeverWrite(TRUE);
    EXPECT_TRUE(image.isNeverWrite());
    const std::string omitted = write_image_field(image);
    ASSERT_FALSE(omitted.empty());
    EXPECT_NE(omitted.find("image 0 0 0"), std::string::npos);
    EXPECT_LT(omitted.size(), normal.size());
}

TEST(ImageFields, ReadHexAcceptsUint32AndRejectsOverflow)
{
    {
        const char input_text[] = "0xdeadbeef ";
        SoInput input;
        input.setBuffer(input_text, std::strlen(input_text));
        uint32_t value = 0;
        ASSERT_TRUE(input.readHex(value));
        EXPECT_EQ(value, UINT32_C(0xdeadbeef));
    }

    {
        const char input_text[] = "0x100000000 ";
        SoInput input;
        input.setBuffer(input_text, std::strlen(input_text));
        uint32_t value = 0;
        EXPECT_FALSE(input.readHex(value));
    }

    {
        const char input_text[] = "42";
        SoInput input;
        input.setBuffer(input_text, std::strlen(input_text));
        uint32_t value = 0;
        EXPECT_FALSE(input.readHex(value));
    }
}

TEST(InputIntegers, AcceptsInt32BoundariesAndRejectsNarrowingOverflow)
{
    const auto read_unsigned = [](const char * text, unsigned int & value) {
        SoInput input;
        input.setBuffer(text, std::strlen(text));
        return input.read(value) != FALSE;
    };
    const auto read_signed = [](const char * text, int & value) {
        SoInput input;
        input.setBuffer(text, std::strlen(text));
        return input.read(value) != FALSE;
    };

    unsigned int unsigned_value = 17;
    EXPECT_TRUE(read_unsigned("4294967295", unsigned_value));
    EXPECT_EQ(unsigned_value, UINT32_MAX);
    unsigned_value = 17;
    EXPECT_FALSE(read_unsigned("4294967296", unsigned_value));
    EXPECT_EQ(unsigned_value, 17u);
    EXPECT_FALSE(read_unsigned("0x100000000", unsigned_value));
    EXPECT_EQ(unsigned_value, 17u);
    EXPECT_FALSE(read_unsigned("-1", unsigned_value));
    EXPECT_EQ(unsigned_value, 17u);

    int signed_value = 17;
    EXPECT_TRUE(read_signed("2147483647", signed_value));
    EXPECT_EQ(signed_value, INT32_MAX);
    EXPECT_TRUE(read_signed("-2147483648", signed_value));
    EXPECT_EQ(signed_value, INT32_MIN);
    signed_value = 17;
    EXPECT_FALSE(read_signed("2147483648", signed_value));
    EXPECT_EQ(signed_value, 17);
    EXPECT_FALSE(read_signed("-2147483649", signed_value));
    EXPECT_EQ(signed_value, 17);

    short short_value = 9;
    {
        SoInput input;
        const char text[] = "32768";
        input.setBuffer(text, sizeof(text) - 1);
        EXPECT_FALSE(input.read(short_value));
        EXPECT_EQ(short_value, 9);
    }
    unsigned short ushort_value = 9;
    {
        SoInput input;
        const char text[] = "65536";
        input.setBuffer(text, sizeof(text) - 1);
        EXPECT_FALSE(input.read(ushort_value));
        EXPECT_EQ(ushort_value, 9);
    }
    int8_t byte_value = 9;
    {
        SoInput input;
        const char text[] = "128";
        input.setBuffer(text, sizeof(text) - 1);
        EXPECT_FALSE(input.readByte(byte_value));
        EXPECT_EQ(byte_value, 9);
    }
    uint8_t ubyte_value = 9;
    {
        SoInput input;
        const char text[] = "256";
        input.setBuffer(text, sizeof(text) - 1);
        EXPECT_FALSE(input.readByte(ubyte_value));
        EXPECT_EQ(ubyte_value, 9);
    }

    const auto binary_short = write_binary_integer(32768);
    ASSERT_FALSE(binary_short.empty());
    SoInput binary_input;
    binary_input.setBuffer(binary_short.data(), binary_short.size());
    ASSERT_TRUE(binary_input.isBinary());
    short_value = 9;
    EXPECT_FALSE(binary_input.read(short_value));
    EXPECT_EQ(short_value, 9);

    const auto binary_ushort = write_binary_integer(65536u);
    ASSERT_FALSE(binary_ushort.empty());
    SoInput binary_unsigned_input;
    binary_unsigned_input.setBuffer(binary_ushort.data(), binary_ushort.size());
    ASSERT_TRUE(binary_unsigned_input.isBinary());
    ushort_value = 9;
    EXPECT_FALSE(binary_unsigned_input.read(ushort_value));
    EXPECT_EQ(ushort_value, 9);

    const auto binary_uint_max = write_binary_integer(UINT32_MAX);
    ASSERT_FALSE(binary_uint_max.empty());
    SoInput binary_uint_input;
    binary_uint_input.setBuffer(binary_uint_max.data(), binary_uint_max.size());
    ASSERT_TRUE(binary_uint_input.isBinary());
    unsigned_value = 0;
    EXPECT_TRUE(binary_uint_input.read(unsigned_value));
    EXPECT_EQ(unsigned_value, UINT32_MAX);
}
