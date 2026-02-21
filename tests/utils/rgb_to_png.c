/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
\**************************************************************************/

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
#include <png.h>

/* Read a 16-bit big-endian unsigned short */
static unsigned short read_be_u16(FILE *fp) {
    unsigned char buf[2];
    if (fread(buf, 1, 2, fp) != 2) return 0;
    return (unsigned short)((buf[0] << 8) | buf[1]);
}

/* Read a 32-bit big-endian unsigned int */
static unsigned int read_be_u32(FILE *fp) {
    unsigned char buf[4];
    if (fread(buf, 1, 4, fp) != 4) return 0;
    return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
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
    unsigned short dim     = read_be_u16(fp_in);
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
    unsigned char *planes[3];
    int i;
    for (i = 0; i < 3; i++) {
        planes[i] = (unsigned char *)malloc(width * height);
        if (!planes[i]) {
            fprintf(stderr, "Error: Out of memory\n");
            fclose(fp_in);
            return 1;
        }
        if (fread(planes[i], 1, width * height, fp_in) != (size_t)(width * height)) {
            fprintf(stderr, "Error: Failed to read channel %d data\n", i);
            fclose(fp_in);
            return 1;
        }
    }
    fclose(fp_in);

    /* Build interleaved RGB rows (top-to-bottom for PNG output).
     * SGI RGB row 0 = bottom of image, so we reverse row order. */
    unsigned char **row_pointers = (unsigned char **)malloc(height * sizeof(unsigned char *));
    unsigned char *rgb_data = (unsigned char *)malloc(width * height * 3);
    if (!row_pointers || !rgb_data) {
        fprintf(stderr, "Error: Out of memory\n");
        return 1;
    }

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
        row_pointers[y] = rgb_data + y * width * 3;
    }

    for (i = 0; i < 3; i++) free(planes[i]);

    /* Write PNG file */
    FILE *fp_out = fopen(argv[2], "wb");
    if (!fp_out) {
        fprintf(stderr, "Error: Cannot open output file: %s\n", argv[2]);
        free(row_pointers);
        free(rgb_data);
        return 1;
    }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fprintf(stderr, "Error: png_create_write_struct failed\n");
        fclose(fp_out);
        free(row_pointers);
        free(rgb_data);
        return 1;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        fprintf(stderr, "Error: png_create_info_struct failed\n");
        png_destroy_write_struct(&png, NULL);
        fclose(fp_out);
        free(row_pointers);
        free(rgb_data);
        return 1;
    }

    if (setjmp(png_jmpbuf(png))) {
        fprintf(stderr, "Error: PNG write error\n");
        png_destroy_write_struct(&png, &info);
        fclose(fp_out);
        free(row_pointers);
        free(rgb_data);
        return 1;
    }

    png_init_io(png, fp_out);
    /* Use PNG_COLOR_TYPE_RGB with 8-bit depth - exact lossless representation */
    png_set_IHDR(png, info, width, height, 8,
                 PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    /* Best compression for repository storage */
    png_set_compression_level(png, 9);
    png_write_info(png, info);
    png_write_image(png, row_pointers);
    png_write_end(png, NULL);

    png_destroy_write_struct(&png, &info);
    fclose(fp_out);
    free(row_pointers);
    free(rgb_data);

    return 0;
}
