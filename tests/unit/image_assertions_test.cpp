#include "framework/image_assertions.h"

#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>

namespace {

using ObolTestSupport::ImageTolerance;
using ObolTestSupport::RgbImage;

TEST(ImageAssertions, MeasuresRgbDifferencesWithoutHiddenThresholds)
{
    const RgbImage expected{2, 1, {0, 10, 20, 30, 40, 50}};
    const RgbImage actual{2, 1, {0, 12, 20, 33, 40, 50}};

    const auto comparison = ObolTestSupport::compareRgb(actual, expected);

    ASSERT_TRUE(comparison.compatible);
    EXPECT_EQ(comparison.differing_pixels, 2u);
    EXPECT_EQ(comparison.max_channel_error, 3u);
    EXPECT_DOUBLE_EQ(comparison.rms_error, std::sqrt(13.0 / 6.0));
    EXPECT_FALSE(ObolTestSupport::isWithinTolerance(comparison, ImageTolerance{}));
    EXPECT_TRUE(ObolTestSupport::isWithinTolerance(
        comparison, ImageTolerance{2, 3, std::sqrt(13.0 / 6.0)}));
}

TEST(ImageAssertions, RejectsMismatchedImageShapes)
{
    const RgbImage actual{1, 1, {0, 0, 0}};
    const RgbImage expected{2, 1, {0, 0, 0, 0, 0, 0}};

    const auto comparison = ObolTestSupport::compareRgb(actual, expected);

    EXPECT_FALSE(comparison.compatible);
    EXPECT_EQ(ObolTestSupport::describeComparison(comparison),
              "image dimensions or RGB buffer sizes differ");
}

TEST(ImageAssertions, ProducesAbsolutePerChannelDifferenceImage)
{
    const RgbImage expected{2, 1, {0, 10, 20, 30, 40, 50}};
    const RgbImage actual{2, 1, {4, 8, 20, 27, 45, 40}};

    const auto difference =
        ObolTestSupport::absoluteDifferenceRgb(actual, expected);

    ASSERT_TRUE(difference.has_value());
    EXPECT_EQ(difference->width, 2u);
    EXPECT_EQ(difference->height, 1u);
    EXPECT_EQ(difference->pixels,
              (std::vector<unsigned char>{4, 2, 0, 3, 5, 10}));
}

TEST(ImageAssertions, RejectsDifferenceImagesWithInvalidStorage)
{
    const RgbImage malformed{1, 1, {0, 0}};
    const RgbImage valid{1, 1, {0, 0, 0}};

    EXPECT_FALSE(
        ObolTestSupport::absoluteDifferenceRgb(malformed, valid).has_value());
}

TEST(ImageAssertions, SavesPngThatCanBeLoadedWithoutChangingPixels)
{
    const RgbImage source{2, 1, {0, 10, 20, 30, 40, 50}};
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        "obol_image_assertions_round_trip.png";
    struct RemoveOnExit {
        std::filesystem::path path;
        ~RemoveOnExit()
        {
            std::error_code ignored;
            std::filesystem::remove(path, ignored);
        }
    } cleanup{path};

    std::string error;
    ASSERT_TRUE(ObolTestSupport::saveRgbPng(source, path.string(), &error))
        << error;
    const auto loaded = ObolTestSupport::loadRgbPng(path.string(), &error);
    ASSERT_TRUE(loaded.has_value()) << error;
    EXPECT_EQ(loaded->width, source.width);
    EXPECT_EQ(loaded->height, source.height);
    EXPECT_EQ(loaded->pixels, source.pixels);
}

} // namespace
