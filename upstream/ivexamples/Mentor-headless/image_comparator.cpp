/*
 *
 *  Copyright (C) 2000 Silicon Graphics, Inc.  All Rights Reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  Further, this software is distributed without any warranty that it is
 *  free of the rightful claim of any third person regarding infringement
 *  or the like.  Any license provided herein, whether implied or
 *  otherwise, applies only to this software file.  Patent licenses, if
 *  any, provided herein do not apply to combinations of this program with
 *  other software, or any other product whatsoever.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 *  Mountain View, CA  94043, or:
 *
 *  http://www.sgi.com
 *
 *  For further information regarding this notice, see:
 *
 *  http://oss.sgi.com/projects/GenInfo/NoticeExplan/
 *
 */

/*
 * Image comparison utility for testing headless rendering
 * 
 * Compares two images (SGI RGB or PNG format) using:
 * 1. Pixel-perfect comparison (exact match)
 * 2. Perceptual hash comparison (approximate match for rendering variations)
 * 
 * PNG support: PNG files are decoded to raw RGB pixel data for comparison.
 * This allows PNG-compressed control images to be compared against
 * SGI RGB runtime output without any lossy conversion.
 *
 * Returns 0 if images match within threshold, 1 otherwise
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>

#ifdef HAVE_LIBPNG
#include <png.h>
#endif

// Default thresholds - should match CMake defaults in CMakeLists.txt
// IMAGE_COMPARISON_HASH_THRESHOLD and IMAGE_COMPARISON_RMSE_THRESHOLD
const int DEFAULT_HASH_THRESHOLD = 5;      // 0-64 range
const double DEFAULT_RMSE_THRESHOLD = 5.0; // 0-255 range

// SGI RGB image header structure
struct RGBHeader {
    unsigned short magic;      // 0x01da
    unsigned char storage;     // 0=verbatim, 1=RLE
    unsigned char bpc;         // bytes per pixel channel (1 or 2)
    unsigned short dimension;  // 1, 2, or 3
    unsigned short xsize;      // width
    unsigned short ysize;      // height
    unsigned short zsize;      // number of channels (1=grayscale, 3=RGB, 4=RGBA)
    unsigned int pixmin;       // minimum pixel value
    unsigned int pixmax;       // maximum pixel value (usually 255)
    unsigned int dummy1;       // unused
    char imagename[80];        // image name
    unsigned int colormap;     // colormap ID (0=normal)
    char dummy2[404];          // padding to 512 bytes
};

// Read a 16-bit big-endian short
static unsigned short read_short(FILE* fp) {
    unsigned char buf[2];
    if (fread(buf, 1, 2, fp) != 2) return 0;
    return (buf[0] << 8) | buf[1];
}

// Read a 32-bit big-endian int
static unsigned int read_int(FILE* fp) {
    unsigned char buf[4];
    if (fread(buf, 1, 4, fp) != 4) return 0;
    return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}

// Read RGB header
static bool read_rgb_header(FILE* fp, RGBHeader& header) {
    fseek(fp, 0, SEEK_SET);
    
    header.magic = read_short(fp);
    header.storage = fgetc(fp);
    header.bpc = fgetc(fp);
    header.dimension = read_short(fp);
    header.xsize = read_short(fp);
    header.ysize = read_short(fp);
    header.zsize = read_short(fp);
    header.pixmin = read_int(fp);
    header.pixmax = read_int(fp);
    header.dummy1 = read_int(fp);
    
    if (fread(header.imagename, 1, 80, fp) != 80) return false;
    header.imagename[79] = '\0';
    
    header.colormap = read_int(fp);
    if (fread(header.dummy2, 1, 404, fp) != 404) return false;
    
    return header.magic == 0x01da;
}

// Read uncompressed RGB image data
// SGI RGB stores data in planar format (all R, then G, then B) and rows
// are ordered bottom-to-top. We convert to interleaved RGB and flip rows
// to top-to-bottom so the layout matches PNG-decoded data for comparison.
static bool read_rgb_data(FILE* fp, const RGBHeader& header, std::vector<unsigned char>& data) {
    int width = header.xsize;
    int height = header.ysize;
    int channels = header.zsize;
    
    data.resize(width * height * channels);
    
    if (header.storage == 0) {
        // Verbatim (uncompressed): read each planar channel
        std::vector<unsigned char> plane_data(width * height);
        
        for (int c = 0; c < channels; c++) {
            if (fread(plane_data.data(), 1, width * height, fp) != (size_t)(width * height)) {
                return false;
            }
            
            // Convert planar to interleaved, flipping rows so that row 0 in
            // data[] is the top of the image (matching PNG row order).
            // SGI RGB row 0 = bottom of image, so src_row = (height-1) - y.
            for (int y = 0; y < height; y++) {
                int src_row = (height - 1) - y;
                for (int x = 0; x < width; x++) {
                    data[(y * width + x) * channels + c] = plane_data[src_row * width + x];
                }
            }
        }
    } else {
        fprintf(stderr, "Error: RLE compressed RGB files not supported.\n");
        return false;
    }
    
    return true;
}

// Load RGB image
static bool load_rgb_image(const char* filename, int& width, int& height, int& channels,
                           std::vector<unsigned char>& data) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open file %s\n", filename);
        return false;
    }
    
    RGBHeader header;
    if (!read_rgb_header(fp, header)) {
        fprintf(stderr, "Error: Invalid RGB file %s\n", filename);
        fclose(fp);
        return false;
    }
    
    width = header.xsize;
    height = header.ysize;
    channels = header.zsize;
    
    if (!read_rgb_data(fp, header, data)) {
        fprintf(stderr, "Error: Failed to read image data from %s\n", filename);
        fclose(fp);
        return false;
    }
    
    fclose(fp);
    return true;
}

// Load PNG image (decoded to interleaved RGB/RGBA pixel data)
// Returns pixel data in the same interleaved format as load_rgb_image
#ifdef HAVE_LIBPNG
static bool load_png_image(const char* filename, int& width, int& height, int& channels,
                           std::vector<unsigned char>& data) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open file %s\n", filename);
        return false;
    }

    // Check PNG signature
    unsigned char sig[8];
    if (fread(sig, 1, 8, fp) != 8 || png_sig_cmp(sig, 0, 8) != 0) {
        fprintf(stderr, "Error: Not a valid PNG file: %s\n", filename);
        fclose(fp);
        return false;
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fprintf(stderr, "Error: png_create_read_struct failed\n");
        fclose(fp);
        return false;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        fprintf(stderr, "Error: png_create_info_struct failed\n");
        png_destroy_read_struct(&png, NULL, NULL);
        fclose(fp);
        return false;
    }

    if (setjmp(png_jmpbuf(png))) {
        fprintf(stderr, "Error: PNG read error in %s\n", filename);
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return false;
    }

    png_init_io(png, fp);
    png_set_sig_bytes(png, 8);
    png_read_info(png, info);

    width = (int)png_get_image_width(png, info);
    height = (int)png_get_image_height(png, info);
    png_byte color_type = png_get_color_type(png, info);
    png_byte bit_depth = png_get_bit_depth(png, info);

    // Normalize to 8-bit RGB
    if (bit_depth == 16)
        png_set_strip_16(png);
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);
    // Strip alpha channel - we only want RGB for comparison with SGI RGB images
    if (color_type == PNG_COLOR_TYPE_RGBA || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_strip_alpha(png);
    // Convert grayscale to RGB
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

    png_read_update_info(png, info);
    channels = (int)png_get_channels(png, info);

    // Allocate row pointers and read image
    std::vector<png_bytep> row_pointers(height);
    int rowbytes = (int)png_get_rowbytes(png, info);
    data.resize(height * rowbytes);
    for (int y = 0; y < height; y++)
        row_pointers[y] = data.data() + y * rowbytes;

    png_read_image(png, row_pointers.data());
    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);
    return true;
}
#endif /* HAVE_LIBPNG */

// Determine file type by extension and load image accordingly
static bool load_image(const char* filename, int& width, int& height, int& channels,
                       std::vector<unsigned char>& data) {
    const char* ext = strrchr(filename, '.');
#ifdef HAVE_LIBPNG
    if (ext && (strcmp(ext, ".png") == 0 || strcmp(ext, ".PNG") == 0))
        return load_png_image(filename, width, height, channels, data);
#endif
    return load_rgb_image(filename, width, height, channels, data);
}

// Compute perceptual hash of an image
// Uses a simplified average hash algorithm:
// 1. Resize to 8x8 (conceptually)
// 2. Convert to grayscale
// 3. Compute average brightness
// 4. Create hash based on pixels above/below average
static unsigned long long compute_perceptual_hash(const std::vector<unsigned char>& data,
                                                   int width, int height, int channels) {
    const int HASH_SIZE = 8;
    std::vector<unsigned char> grayscale(HASH_SIZE * HASH_SIZE);
    
    // Sample image at 8x8 grid and convert to grayscale
    for (int y = 0; y < HASH_SIZE; y++) {
        for (int x = 0; x < HASH_SIZE; x++) {
            int src_x = (x * width) / HASH_SIZE;
            int src_y = (y * height) / HASH_SIZE;
            int src_idx = (src_y * width + src_x) * channels;
            
            // Convert to grayscale
            unsigned char gray;
            if (channels >= 3) {
                // RGB to grayscale: 0.299*R + 0.587*G + 0.114*B
                gray = (unsigned char)(0.299 * data[src_idx] +
                                      0.587 * data[src_idx + 1] +
                                      0.114 * data[src_idx + 2]);
            } else {
                gray = data[src_idx];
            }
            
            grayscale[y * HASH_SIZE + x] = gray;
        }
    }
    
    // Compute average
    unsigned int sum = 0;
    for (int i = 0; i < HASH_SIZE * HASH_SIZE; i++) {
        sum += grayscale[i];
    }
    unsigned char avg = sum / (HASH_SIZE * HASH_SIZE);
    
    // Create hash: use strict > so that a uniform/black image (avg==0) produces
    // hash 0 instead of all-1s.  The `>=` form would set all bits for an
    // all-zero sample set (since 0 >= 0 is true), producing a spuriously large
    // Hamming distance when compared against a nearly-identical image whose
    // sampled pixels happen to hit one or two bright spots (raising the average
    // above zero and suppressing those bits).
    unsigned long long hash = 0;
    for (int i = 0; i < HASH_SIZE * HASH_SIZE; i++) {
        if (grayscale[i] > avg) {
            hash |= (1ULL << i);
        }
    }
    
    return hash;
}

// Compute Hamming distance between two hashes
static int hamming_distance(unsigned long long hash1, unsigned long long hash2) {
    unsigned long long xor_result = hash1 ^ hash2;
    int distance = 0;
    
    while (xor_result) {
        distance += xor_result & 1;
        xor_result >>= 1;
    }
    
    return distance;
}

// Compute RMSE (Root Mean Square Error) between two images
static double compute_rmse(const std::vector<unsigned char>& data1,
                          const std::vector<unsigned char>& data2) {
    if (data1.size() != data2.size()) return -1.0;
    
    double sum = 0.0;
    for (size_t i = 0; i < data1.size(); i++) {
        int diff = (int)data1[i] - (int)data2[i];
        sum += diff * diff;
    }
    
    return sqrt(sum / data1.size());
}

// Print usage
static void print_usage(const char* prog) {
    fprintf(stderr, "Usage: %s [options] <reference_image> <test_image>\n", prog);
    fprintf(stderr, "\nSupported formats: .rgb (SGI RGB), .png (PNG, requires libpng)\n");
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  -t, --threshold <N>    Set perceptual hash threshold (0-64, default: 5)\n");
    fprintf(stderr, "                         Lower = stricter, Higher = more tolerant\n");
    fprintf(stderr, "  -r, --rmse <N>         Set RMSE threshold (default: 5.0)\n");
    fprintf(stderr, "                         Lower = stricter, Higher = more tolerant\n");
    fprintf(stderr, "  -s, --strict           Use pixel-perfect comparison only\n");
    fprintf(stderr, "  -v, --verbose          Print detailed comparison metrics\n");
    fprintf(stderr, "  -h, --help             Print this help message\n");
    fprintf(stderr, "\nReturns:\n");
    fprintf(stderr, "  0 if images match within threshold\n");
    fprintf(stderr, "  1 if images differ beyond threshold\n");
    fprintf(stderr, "  2 if error occurred\n");
}

int main(int argc, char** argv) {
    // Default parameters (match CMake defaults in CMakeLists.txt)
    int hash_threshold = DEFAULT_HASH_THRESHOLD;
    double rmse_threshold = DEFAULT_RMSE_THRESHOLD;
    bool strict_mode = false;
    bool verbose = false;
    const char* ref_filename = nullptr;
    const char* test_filename = nullptr;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-s" || arg == "--strict") {
            strict_mode = true;
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if ((arg == "-t" || arg == "--threshold") && i + 1 < argc) {
            hash_threshold = atoi(argv[++i]);
            if (hash_threshold < 0 || hash_threshold > 64) {
                fprintf(stderr, "Error: threshold must be between 0 and 64\n");
                return 2;
            }
        } else if ((arg == "-r" || arg == "--rmse") && i + 1 < argc) {
            rmse_threshold = atof(argv[++i]);
            if (rmse_threshold < 0.0) {
                fprintf(stderr, "Error: RMSE threshold must be non-negative\n");
                return 2;
            }
        } else if (arg[0] == '-') {
            fprintf(stderr, "Error: Unknown option %s\n", arg.c_str());
            print_usage(argv[0]);
            return 2;
        } else {
            if (!ref_filename) {
                ref_filename = argv[i];
            } else if (!test_filename) {
                test_filename = argv[i];
            } else {
                fprintf(stderr, "Error: Too many arguments\n");
                print_usage(argv[0]);
                return 2;
            }
        }
    }
    
    if (!ref_filename || !test_filename) {
        fprintf(stderr, "Error: Both reference and test images must be specified\n");
        print_usage(argv[0]);
        return 2;
    }
    
    // Load images
    int ref_width, ref_height, ref_channels;
    int test_width, test_height, test_channels;
    std::vector<unsigned char> ref_data, test_data;
    
    if (!load_image(ref_filename, ref_width, ref_height, ref_channels, ref_data)) {
        return 2;
    }
    
    if (!load_image(test_filename, test_width, test_height, test_channels, test_data)) {
        return 2;
    }
    
    if (verbose) {
        printf("Reference image: %dx%d, %d channels\n", ref_width, ref_height, ref_channels);
        printf("Test image: %dx%d, %d channels\n", test_width, test_height, test_channels);
    }
    
    // Check dimensions
    if (ref_width != test_width || ref_height != test_height) {
        fprintf(stderr, "Error: Image dimensions do not match\n");
        fprintf(stderr, "  Reference: %dx%d\n", ref_width, ref_height);
        fprintf(stderr, "  Test: %dx%d\n", test_width, test_height);
        return 1;
    }
    
    if (ref_channels != test_channels) {
        fprintf(stderr, "Error: Number of channels do not match\n");
        fprintf(stderr, "  Reference: %d\n", ref_channels);
        fprintf(stderr, "  Test: %d\n", test_channels);
        return 1;
    }
    
    // Pixel-perfect comparison
    bool pixel_perfect = (ref_data == test_data);
    
    if (pixel_perfect) {
        if (verbose) {
            printf("Images are pixel-perfect match\n");
        }
        return 0;
    }
    
    if (strict_mode) {
        if (verbose) {
            printf("Images differ (strict mode)\n");
        }
        return 1;
    }
    
    // Perceptual comparison
    unsigned long long ref_hash = compute_perceptual_hash(ref_data, ref_width, ref_height, ref_channels);
    unsigned long long test_hash = compute_perceptual_hash(test_data, test_width, test_height, test_channels);
    int hash_dist = hamming_distance(ref_hash, test_hash);
    
    // RMSE comparison
    double rmse = compute_rmse(ref_data, test_data);
    
    if (verbose) {
        printf("Perceptual hash distance: %d (threshold: %d)\n", hash_dist, hash_threshold);
        printf("RMSE: %.2f (threshold: %.2f)\n", rmse, rmse_threshold);
    }
    
    // Determine if images match within threshold
    bool hash_match = (hash_dist <= hash_threshold);
    bool rmse_match = (rmse <= rmse_threshold);
    
    if (hash_match && rmse_match) {
        if (verbose) {
            printf("Images match within threshold\n");
        }
        return 0;
    } else {
        if (verbose) {
            printf("Images differ beyond threshold\n");
            if (!hash_match) {
                printf("  Perceptual hash exceeded threshold\n");
            }
            if (!rmse_match) {
                printf("  RMSE exceeded threshold\n");
            }
        }
        return 1;
    }
}
