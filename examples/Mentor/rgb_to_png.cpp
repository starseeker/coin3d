/*
 * rgb_to_png - Convert SGI RGB image files to PNG for repository storage
 *
 * The PNG encoding preserves exact pixel values (lossless), enabling
 * round-trip recovery of the original RGB pixel data for image comparison.
 *
 * Usage: rgb_to_png input.rgb output.png
 *
 * SGI RGB format details:
 *   - Magic number: 0x01da
 *   - Pixel data in planar format: all R, then G, then B
 *   - Rows stored bottom-to-top (first row in file = bottom of image)
 * PNG output:
 *   - Interleaved RGB, 8 bits per channel
 *   - Rows stored top-to-bottom (standard PNG order)
 *   - No color space transformation (raw pixel data preserved exactly)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include "lodepng.h"

/* Read a 16-bit big-endian unsigned short */
static unsigned short read_be_u16(FILE *fp) {
    unsigned char buf[2];
    if (fread(buf, 1, 2, fp) != 2) return 0;
    return (unsigned short)((buf[0] << 8) | buf[1]);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s input.rgb output.png\n", argv[0]);
        return 1;
    }

    /* Open input SGI RGB file */
    FILE *fp_in = fopen(argv[1], "rb");
    if (!fp_in) {
        fprintf(stderr, "Error: Cannot open input file: %s\n", argv[1]);
        return 1;
    }

    /* Read SGI RGB header (512 bytes total) */
    unsigned short magic   = read_be_u16(fp_in);
    unsigned char  storage = (unsigned char)fgetc(fp_in); /* 0=verbatim, 1=RLE */
    unsigned char  bpc     = (unsigned char)fgetc(fp_in); /* bytes per channel */
    read_be_u16(fp_in); /* dimension field - not used */
    unsigned short xsize   = read_be_u16(fp_in);          /* width */
    unsigned short ysize   = read_be_u16(fp_in);          /* height */
    unsigned short zsize   = read_be_u16(fp_in);          /* channels */
    /* skip pixmin, pixmax, dummy1, imagename, colormap, dummy2 */
    fseek(fp_in, 512, SEEK_SET);

    if (magic != 0x01da) {
        fprintf(stderr, "Error: Not a valid SGI RGB file (magic=0x%04x)\n", magic);
        fclose(fp_in);
        return 1;
    }
    if (storage != 0) {
        fprintf(stderr, "Error: RLE-compressed SGI RGB files are not supported\n");
        fclose(fp_in);
        return 1;
    }
    if (bpc != 1) {
        fprintf(stderr, "Error: Only 1 byte-per-channel SGI RGB files are supported\n");
        fclose(fp_in);
        return 1;
    }
    if (zsize < 3) {
        fprintf(stderr, "Error: Expected at least 3 channels, got %d\n", (int)zsize);
        fclose(fp_in);
        return 1;
    }

    int width  = (int)xsize;
    int height = (int)ysize;

    /* Read planar pixel data: all R, then all G, then all B
     * SGI RGB stores rows bottom-to-top. */
    std::vector<unsigned char> planes[3];
    int i;
    for (i = 0; i < 3; i++) {
        planes[i].resize(width * height);
        if (fread(planes[i].data(), 1, width * height, fp_in) != (size_t)(width * height)) {
            fprintf(stderr, "Error: Failed to read channel %d data\n", i);
            fclose(fp_in);
            return 1;
        }
    }
    fclose(fp_in);

    /* Build interleaved RGB rows (top-to-bottom for PNG output).
     * SGI RGB row 0 = bottom of image, so we reverse row order. */
    std::vector<unsigned char> rgb_data(width * height * 3);

    int y;
    for (y = 0; y < height; y++) {
        /* SGI stores bottom row first; PNG stores top row first */
        int src_row = (height - 1) - y;
        int x;
        for (x = 0; x < width; x++) {
            rgb_data[(y * width + x) * 3 + 0] = planes[0][src_row * width + x]; /* R */
            rgb_data[(y * width + x) * 3 + 1] = planes[1][src_row * width + x]; /* G */
            rgb_data[(y * width + x) * 3 + 2] = planes[2][src_row * width + x]; /* B */
        }
    }

    /* Write PNG file using lodepng */
    unsigned error = lodepng_encode24_file(argv[2], rgb_data.data(),
                                           (unsigned)width, (unsigned)height);

    if (error) {
        fprintf(stderr, "Error: lodepng failed to write %s: %s\n",
                argv[2], lodepng_error_text(error));
        return 1;
    }

    return 0;
}
