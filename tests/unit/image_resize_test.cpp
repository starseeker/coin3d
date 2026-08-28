#include <gtest/gtest.h>

#include "base/SbImageResize.h"

#include <array>
#include <climits>
#include <memory>

namespace {

TEST(ImageResize, EveryTwoDimensionalFilterPreservesAConstantImage)
{
  constexpr std::array<unsigned char, 12> source = {
    17, 91, 203, 17, 91, 203,
    17, 91, 203, 17, 91, 203
  };
  constexpr std::array<SbImageResizeFilter, 6> filters = {
    SB_IMAGE_RESIZE_FAST,
    SB_IMAGE_RESIZE_BILINEAR,
    SB_IMAGE_RESIZE_FILTER_BELL,
    SB_IMAGE_RESIZE_FILTER_B_SPLINE,
    SB_IMAGE_RESIZE_FILTER_LANCZOS3,
    SB_IMAGE_RESIZE_FILTER_MITCHELL
  };

  for (const SbImageResizeFilter filter : filters) {
    SCOPED_TRACE(static_cast<int>(filter));
    std::unique_ptr<unsigned char[]> resized(
      SbImageResize_resize2D(source.data(), 2, 2, 3, 5, 3, filter));
    ASSERT_NE(resized, nullptr);
    for (size_t pixel = 0; pixel < 15; ++pixel) {
      EXPECT_NEAR(resized[pixel * 3], 17, 1);
      EXPECT_NEAR(resized[pixel * 3 + 1], 91, 1);
      EXPECT_NEAR(resized[pixel * 3 + 2], 203, 1);
    }
  }
}

TEST(ImageResize, ThreeDimensionalResizePreservesConstantVoxels)
{
  constexpr std::array<unsigned char, 8> source = {
    73, 73, 73, 73, 73, 73, 73, 73
  };
  std::unique_ptr<unsigned char[]> resized(
    SbImageResize_resize3D(source.data(), 2, 2, 2, 1, 3, 4, 5,
                          SB_IMAGE_RESIZE_HIGH));
  ASSERT_NE(resized, nullptr);
  for (size_t i = 0; i < 60; ++i) EXPECT_EQ(resized[i], 73);
}

TEST(ImageResize, RejectsInvalidInputAndUnknownFilters)
{
  constexpr unsigned char source[] = { 1, 2, 3, 4 };
  unsigned char destination[16] = {};
  EXPECT_EQ(SbImageResize_resize2D(nullptr, 1, 1, 1, 2, 2), nullptr);
  EXPECT_EQ(SbImageResize_resize2D(source, 0, 1, 1, 2, 2), nullptr);
  EXPECT_EQ(SbImageResize_resize2D(source, 1, 1, 5, 1, 1), nullptr);
  EXPECT_EQ(SbImageResize_resize2D(
    source, INT_MAX, 1, 1, 1, 1, SB_IMAGE_RESIZE_FILTER_LANCZOS3),
    nullptr);
  EXPECT_EQ(SbImageResize_resize3D(source, 1, 1, 1, 5, 1, 1, 1), nullptr);
  EXPECT_FALSE(SbImageResize_resize2D_inplace(
    source, destination, 2, 2, 1, 4, 4,
    static_cast<SbImageResizeFilter>(999)));
  EXPECT_EQ(SbImageResize_resize3D(
    source, 1, 1, 1, 1, 1, 1, 1,
    static_cast<SbImageResizeFilter>(999)), nullptr);
}

} // namespace
