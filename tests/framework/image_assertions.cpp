#include "image_assertions.h"

#include <lodepng.h>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace ObolTestSupport {

std::optional<RgbImage> loadRgbPng(const std::string & path, std::string * error)
{
    RgbImage image;
    unsigned error_code = lodepng::decode(image.pixels, image.width, image.height,
                                          path, LCT_RGB, 8);
    if (error_code == 0) return image;

    if (error) {
        *error = "could not load '" + path + "': " + lodepng_error_text(error_code);
    }
    return std::nullopt;
}

ImageComparison compareRgb(const RgbImage & actual, const RgbImage & expected)
{
    ImageComparison comparison;
    const std::size_t expected_bytes =
        static_cast<std::size_t>(expected.width) * expected.height * 3;
    const std::size_t actual_bytes =
        static_cast<std::size_t>(actual.width) * actual.height * 3;
    if (actual.width != expected.width || actual.height != expected.height ||
        actual.pixels.size() != actual_bytes || expected.pixels.size() != expected_bytes) {
        return comparison;
    }

    comparison.compatible = true;
    double squared_error_sum = 0.0;
    for (std::size_t i = 0; i < actual.pixels.size(); i += 3) {
        bool pixel_differs = false;
        for (std::size_t channel = 0; channel != 3; ++channel) {
            const int difference = std::abs(static_cast<int>(actual.pixels[i + channel]) -
                                            static_cast<int>(expected.pixels[i + channel]));
            comparison.max_channel_error = std::max(
                comparison.max_channel_error, static_cast<unsigned char>(difference));
            squared_error_sum += static_cast<double>(difference * difference);
            pixel_differs = pixel_differs || difference != 0;
        }
        if (pixel_differs) ++comparison.differing_pixels;
    }
    if (!actual.pixels.empty()) {
        comparison.rms_error = std::sqrt(squared_error_sum / actual.pixels.size());
    }
    return comparison;
}

bool isWithinTolerance(const ImageComparison & comparison,
                       const ImageTolerance & tolerance)
{
    return comparison.compatible &&
           comparison.differing_pixels <= tolerance.differing_pixels &&
           comparison.max_channel_error <= tolerance.max_channel_error &&
           comparison.rms_error <= tolerance.rms_error;
}

std::string describeComparison(const ImageComparison & comparison)
{
    if (!comparison.compatible) return "image dimensions or RGB buffer sizes differ";

    std::ostringstream stream;
    stream << "differing pixels=" << comparison.differing_pixels
           << ", max channel error=" << static_cast<unsigned int>(comparison.max_channel_error)
           << ", RMS error=" << comparison.rms_error;
    return stream.str();
}

} // namespace ObolTestSupport
