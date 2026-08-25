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

    const SbVec2s sub_dims[] = {SbVec2s(1, 1), SbVec2s(1, 1)};
    const SbVec2s sub_offsets[] = {SbVec2s(0, 0), SbVec2s(3, 3)};
    unsigned char first_pixel[] = {9};
    unsigned char second_pixel[] = {11};
    unsigned char * blocks[] = {first_pixel, second_pixel};
    image.setSubValues(sub_dims, sub_offsets, 2, blocks);
    EXPECT_TRUE(image.hasSubTextures(count));
    EXPECT_EQ(count, 2);
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
