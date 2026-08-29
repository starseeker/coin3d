#ifndef OBOL_IMAGE_ASSERTIONS_H
#define OBOL_IMAGE_ASSERTIONS_H

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace ObolTestSupport {

/** An 8-bit, three-channel image used by backend-specific render references. */
struct RgbImage {
    unsigned int width = 0;
    unsigned int height = 0;
    std::vector<unsigned char> pixels;
};

/** Measurements from comparing two RGB images. */
struct ImageComparison {
    bool compatible = false;
    std::size_t differing_pixels = 0;
    unsigned char max_channel_error = 0;
    double rms_error = 0.0;
};

/** Tolerances are intentional and must be chosen by each render test. */
struct ImageTolerance {
    std::size_t differing_pixels = 0;
    unsigned char max_channel_error = 0;
    double rms_error = 0.0;
};

/** Load a PNG as RGB pixels.  On failure, error receives a human-readable cause. */
std::optional<RgbImage> loadRgbPng(const std::string & path,
                                   std::string * error = nullptr);

/** Save a valid RGB image as PNG.  Intended for failure artifacts. */
bool saveRgbPng(const RgbImage & image, const std::string & path,
                std::string * error = nullptr);

/** Compare identically sized RGB images without applying an implicit tolerance. */
ImageComparison compareRgb(const RgbImage & actual, const RgbImage & expected);

/** Build an RGB image containing the absolute per-channel error. */
std::optional<RgbImage> absoluteDifferenceRgb(const RgbImage & actual,
                                              const RgbImage & expected);

/** Return true only when all requested error limits are met. */
bool isWithinTolerance(const ImageComparison & comparison,
                       const ImageTolerance & tolerance);

/** A concise diagnostic suitable for an assertion failure message. */
std::string describeComparison(const ImageComparison & comparison);

} // namespace ObolTestSupport

#endif // OBOL_IMAGE_ASSERTIONS_H
