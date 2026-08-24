#include "framework/image_assertions.h"

#include <gtest/gtest.h>

#include <cmath>

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

} // namespace
