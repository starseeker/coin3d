/*
 * stt_iosevka.cpp -- stb_truetype direct-render test for Iosevka Aile TTC font
 *
 * Loads the Iosevka Aile Regular TTC font via SbFont::loadFont() and renders
 * several text strings directly to a PNG image WITHOUT going through any
 * OpenGL layer.  The purpose is to:
 *
 *   1. Verify that struetype (stb_truetype) can correctly parse a TrueType
 *      Collection (.ttc) file using the proper stt_GetFontOffsetForIndex
 *      offset calculation.
 *   2. Confirm that rendered glyphs are Iosevka-shaped (taller, narrower
 *      strokes than the embedded ProFont fallback).
 *   3. Provide a visual reference image that does not require a display
 *      server or OpenGL context.
 *
 * The font file path is supplied at compile time via the OBOL_FONTS_DIR
 * preprocessor define (set by CMake) or falls back to a relative path.
 *
 * Usage: stt_iosevka [output.png]
 *
 * Exit 0 on success, 1 on any error (font load failure or write error).
 */

#include <Inventor/SbFont.h>
#include <Inventor/SbVec2s.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

#ifndef OBOL_FONTS_DIR
#define OBOL_FONTS_DIR ""
#endif

/* Compile-time concatenation: OBOL_FONTS_DIR must be non-empty (set by CMake).
 * If OBOL_FONTS_DIR is empty the path will start with '/' and fopen() will
 * fail gracefully; the test will exit with code 1 and a clear error message. */
static const char k_iosevka_path[] =
    OBOL_FONTS_DIR "/Iosevka/IosevkaAile-Regular.ttc";

static const int IMG_W    = 500;
static const int IMG_H    = 220;
static const int CHANNELS = 4;

// Composite a glyph bitmap over an opaque-black canvas
static void blit_glyph(unsigned char *canvas, int cw, int ch,
                        const unsigned char *bitmap, int bw, int bh,
                        int dst_x, int dst_y,
                        unsigned char cr, unsigned char cg, unsigned char cb)
{
    for (int gy = 0; gy < bh; gy++) {
        int dy = dst_y + gy;
        if (dy < 0 || dy >= ch) continue;
        for (int gx = 0; gx < bw; gx++) {
            int dx = dst_x + gx;
            if (dx < 0 || dx >= cw) continue;
            const int srcval = bitmap[gy * bw + gx];
            if (srcval == 0) continue;
            unsigned char *dst = canvas + (dy * cw + dx) * CHANNELS;
            dst[0] = (unsigned char)((srcval * (int)cr + (255 - srcval) * (int)dst[0]) / 255);
            dst[1] = (unsigned char)((srcval * (int)cg + (255 - srcval) * (int)dst[1]) / 255);
            dst[2] = (unsigned char)((srcval * (int)cb + (255 - srcval) * (int)dst[2]) / 255);
        }
    }
}

struct TextRow {
    float       size;
    const char *text;
    int         x_start;
    int         y_top;
    unsigned char r, g, b;
};

static const TextRow k_rows[] = {
    { 22.0f, "Iosevka Aile",   5,   5, 255, 220,  60 },
    { 18.0f, "Hello World",    5,  45,  80, 220, 255 },
    { 16.0f, "OpenGL 3D Text", 5,  85, 100, 255, 120 },
    { 14.0f, "AaBbCc 012345",  5, 120, 255, 120, 180 },
    { 12.0f, "struetype .ttc", 5, 150, 200, 200, 200 },
    {  0.0f, NULL,             0,   0,   0,   0,   0 }
};

int main(int argc, char **argv)
{
    const char *outpath = (argc > 1) ? argv[1] : "/tmp/stt_iosevka.png";

    unsigned char *canvas =
        (unsigned char *)malloc((size_t)IMG_W * IMG_H * CHANNELS);
    if (!canvas) {
        fprintf(stderr, "stt_iosevka: out of memory\n");
        return 1;
    }
    /* Opaque dark background */
    for (int i = 0; i < IMG_W * IMG_H; i++) {
        canvas[i * CHANNELS + 0] = 20;
        canvas[i * CHANNELS + 1] = 20;
        canvas[i * CHANNELS + 2] = 30;
        canvas[i * CHANNELS + 3] = 255;
    }

    int any_failed = 0;

    for (int ri = 0; k_rows[ri].text != NULL; ri++) {
        SbFont font(k_iosevka_path);
        if (!font.loadFont(k_iosevka_path)) {
            fprintf(stderr, "stt_iosevka: failed to load font: %s\n",
                    k_iosevka_path);
            any_failed = 1;
            break;
        }
        font.setSize(k_rows[ri].size);

        int x = k_rows[ri].x_start;
        const char *p = k_rows[ri].text;
        while (*p) {
            const unsigned char ch = static_cast<unsigned char>(*p++);
            SbVec2s sz, bearing;
            const unsigned char *bitmap = font.getGlyphBitmap(ch, sz, bearing);
            const SbVec2f adv = font.getGlyphAdvance(ch);

            if (bitmap && sz[0] > 0 && sz[1] > 0) {
                const int dst_x = x + bearing[0];
                const int dst_y = k_rows[ri].y_top
                                  + static_cast<int>(k_rows[ri].size)
                                  - bearing[1];
                blit_glyph(canvas, IMG_W, IMG_H,
                           bitmap, sz[0], sz[1],
                           dst_x, dst_y,
                           k_rows[ri].r, k_rows[ri].g, k_rows[ri].b);
            }
            x += static_cast<int>(adv[0]);
        }
    }

    if (any_failed) {
        free(canvas);
        return 1;
    }

    const int written = stbi_write_png(outpath,
                                       IMG_W, IMG_H, CHANNELS,
                                       canvas, IMG_W * CHANNELS);
    free(canvas);

    if (written) {
        printf("stt_iosevka: saved %s (%dx%d)\n", outpath, IMG_W, IMG_H);
        return 0;
    }
    fprintf(stderr, "stt_iosevka: failed to write %s\n", outpath);
    return 1;
}
