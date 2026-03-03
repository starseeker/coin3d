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

/**
 * @file SoQuadViewport.cpp
 * @brief Implementation of the SoQuadViewport four-quadrant viewport manager.
 *
 * SoQuadViewport composes four SoViewport instances arranged in a 2×2 grid.
 * All non-trivial logic lives in SoViewport; this class only manages the
 * tile layout and shared-scene distribution.
 */

#include <Inventor/SoQuadViewport.h>

#include <Inventor/SoViewport.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/nodes/SoCamera.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/errors/SoDebugError.h>

#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

SoQuadViewport::SoQuadViewport()
    : scene_(nullptr)
    , windowSize_(800, 600)
    , activeQuad_(0)
    , borderWidth_(0)
    , borderColor_(1.0f, 1.0f, 1.0f)   // white
{
    updateViewports();
}

SoQuadViewport::~SoQuadViewport()
{
    // Remove the shared scene from all tiles before their destructors run,
    // so the scene's ref count is brought back down correctly.
    if (scene_) {
        for (int i = 0; i < NUM_QUADS; ++i)
            tiles_[i].setSceneGraph(nullptr);
        scene_->unref();
        scene_ = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Scene graph
// ---------------------------------------------------------------------------

void SoQuadViewport::setSceneGraph(SoNode * newScene)
{
    if (newScene == scene_)
        return;

    // Distribute the new scene to every tile.  SoViewport::setSceneGraph()
    // manages its own ref/unref, so we only hold an extra ref here to keep
    // the scene alive between setSceneGraph() calls even if all tiles are
    // temporarily set to nullptr.
    for (int i = 0; i < NUM_QUADS; ++i)
        tiles_[i].setSceneGraph(newScene);

    if (scene_)  scene_->unref();
    scene_ = newScene;
    if (scene_)  scene_->ref();
}

SoNode * SoQuadViewport::getSceneGraph() const
{
    return scene_;
}

// ---------------------------------------------------------------------------
// Window / viewport
// ---------------------------------------------------------------------------

void SoQuadViewport::setWindowSize(const SbVec2s & size)
{
    windowSize_ = size;
    updateViewports();
}

SbVec2s SoQuadViewport::getWindowSize() const
{
    return windowSize_;
}

SbVec2s SoQuadViewport::getQuadrantSize() const
{
    return SbVec2s(windowSize_[0] / 2, windowSize_[1] / 2);
}

const SbViewportRegion & SoQuadViewport::getViewportRegion(int quad) const
{
    if (quad < 0 || quad >= NUM_QUADS) {
        SoDebugError::post("SoQuadViewport::getViewportRegion",
                           "quad index %d out of range [0,%d)", quad, NUM_QUADS);
        return tiles_[0].getViewportRegion();
    }
    return tiles_[quad].getViewportRegion();
}

// Internal: assign sub-viewport regions to each tile.
//
// Layout (OpenGL origin at bottom-left, window W×H):
//   TOP_LEFT  (0): origin (0,   qH),  size (qW, qH)
//   TOP_RIGHT (1): origin (qW,  qH),  size (qW, qH)
//   BOT_LEFT  (2): origin (0,   0),   size (qW, qH)
//   BOT_RIGHT (3): origin (qW,  0),   size (qW, qH)
void SoQuadViewport::updateViewports()
{
    short W  = windowSize_[0];
    short H  = windowSize_[1];
    short qW = W / 2;
    short qH = H / 2;

    struct { short ox, oy; } origins[NUM_QUADS] = {
        { 0,  qH },   // TOP_LEFT
        { qW, qH },   // TOP_RIGHT
        { 0,  0  },   // BOTTOM_LEFT
        { qW, 0  }    // BOTTOM_RIGHT
    };

    for (int i = 0; i < NUM_QUADS; ++i) {
        SbViewportRegion vp;
        vp.setWindowSize(W, H);
        vp.setViewportPixels(SbVec2s(origins[i].ox, origins[i].oy),
                             SbVec2s(qW, qH));
        tiles_[i].setViewportRegion(vp);
    }
}

// ---------------------------------------------------------------------------
// Per-quadrant SoViewport access
// ---------------------------------------------------------------------------

SoViewport * SoQuadViewport::getViewport(int quad)
{
    if (quad < 0 || quad >= NUM_QUADS) {
        SoDebugError::post("SoQuadViewport::getViewport",
                           "quad index %d out of range [0,%d)", quad, NUM_QUADS);
        return nullptr;
    }
    return &tiles_[quad];
}

const SoViewport * SoQuadViewport::getViewport(int quad) const
{
    if (quad < 0 || quad >= NUM_QUADS) {
        SoDebugError::post("SoQuadViewport::getViewport",
                           "quad index %d out of range [0,%d)", quad, NUM_QUADS);
        return nullptr;
    }
    return &tiles_[quad];
}

// ---------------------------------------------------------------------------
// Camera convenience wrappers
// ---------------------------------------------------------------------------

void SoQuadViewport::setCamera(int quad, SoCamera * camera)
{
    if (quad < 0 || quad >= NUM_QUADS) {
        SoDebugError::post("SoQuadViewport::setCamera",
                           "quad index %d out of range [0,%d)", quad, NUM_QUADS);
        return;
    }
    tiles_[quad].setCamera(camera);
}

SoCamera * SoQuadViewport::getCamera(int quad) const
{
    if (quad < 0 || quad >= NUM_QUADS)
        return nullptr;
    return tiles_[quad].getCamera();
}

// ---------------------------------------------------------------------------
// Active quadrant
// ---------------------------------------------------------------------------

void SoQuadViewport::setActiveQuadrant(int quad)
{
    if (quad < 0 || quad >= NUM_QUADS) {
        SoDebugError::post("SoQuadViewport::setActiveQuadrant",
                           "quad index %d out of range [0,%d)", quad, NUM_QUADS);
        return;
    }
    activeQuad_ = quad;
}

int SoQuadViewport::getActiveQuadrant() const
{
    return activeQuad_;
}

// ---------------------------------------------------------------------------
// View-all convenience wrappers
// ---------------------------------------------------------------------------

void SoQuadViewport::viewAll(int quad)
{
    if (quad < 0 || quad >= NUM_QUADS)
        return;
    tiles_[quad].viewAll();
}

void SoQuadViewport::viewAllQuadrants()
{
    for (int i = 0; i < NUM_QUADS; ++i)
        tiles_[i].viewAll();
}

// ---------------------------------------------------------------------------
// Background colour convenience wrappers
// ---------------------------------------------------------------------------

void SoQuadViewport::setBackgroundColor(int quad, const SbColor & color)
{
    if (quad < 0 || quad >= NUM_QUADS)
        return;
    tiles_[quad].setBackgroundColor(color);
}

const SbColor & SoQuadViewport::getBackgroundColor(int quad) const
{
    if (quad < 0 || quad >= NUM_QUADS)
        return tiles_[0].getBackgroundColor();
    return tiles_[quad].getBackgroundColor();
}

// ---------------------------------------------------------------------------
// Borders
// ---------------------------------------------------------------------------

void SoQuadViewport::setBorderWidth(int pixelWidth)
{
    borderWidth_ = (pixelWidth > 0) ? pixelWidth : 0;
}

int SoQuadViewport::getBorderWidth() const
{
    return borderWidth_;
}

void SoQuadViewport::setBorderColor(const SbColor & color)
{
    borderColor_ = color;
}

const SbColor & SoQuadViewport::getBorderColor() const
{
    return borderColor_;
}

// ---------------------------------------------------------------------------
// Internal: paint horizontal + vertical border lines into a W×H×3 buffer.
// The dividing lines are centred between the quadrants.
//   horizontal: rows  [qH - bLow,  qH + bHigh)   (covering borderWidth rows)
//   vertical:   cols  [qW - bLow,  qW + bHigh)   (covering borderWidth cols)
// ---------------------------------------------------------------------------
void SoQuadViewport::applyBordersToBuffer(unsigned char * buf, int W, int H) const
{
    if (borderWidth_ <= 0 || !buf)
        return;

    const int qW = W / 2;
    const int qH = H / 2;

    const unsigned char bR = (unsigned char)(borderColor_[0] * 255.0f + 0.5f);
    const unsigned char bG = (unsigned char)(borderColor_[1] * 255.0f + 0.5f);
    const unsigned char bB = (unsigned char)(borderColor_[2] * 255.0f + 0.5f);

    // Half-widths: lower half goes left/below the centre, upper half right/above.
    const int bLow  = borderWidth_ / 2;
    const int bHigh = borderWidth_ - bLow;

    // Horizontal band (rows qH-bLow .. qH+bHigh-1)
    for (int y = qH - bLow; y < qH + bHigh; ++y) {
        if (y < 0 || y >= H) continue;
        for (int x = 0; x < W; ++x) {
            unsigned char * p = buf + (y * W + x) * 3;
            p[0] = bR; p[1] = bG; p[2] = bB;
        }
    }

    // Vertical band (cols qW-bLow .. qW+bHigh-1)
    for (int x = qW - bLow; x < qW + bHigh; ++x) {
        if (x < 0 || x >= W) continue;
        for (int y = 0; y < H; ++y) {
            unsigned char * p = buf + (y * W + x) * 3;
            p[0] = bR; p[1] = bG; p[2] = bB;
        }
    }
}

// Internal helper: write a raw interleaved-RGB buffer to an SGI RGB file.
// The buffer is W*H*3 bytes, interleaved RGB, rows bottom-to-top (OpenGL order).
// SGI RGB stores: 512-byte header + planar data (R-plane, G-plane, B-plane),
// rows bottom-to-top within each plane.
static bool writeSGIRGB(const char * path, const unsigned char * buf, int W, int H)
{
    FILE * fp = fopen(path, "wb");
    if (!fp) return false;

    // 512-byte header (big-endian integers where noted)
    unsigned char hdr[512];
    memset(hdr, 0, sizeof(hdr));

    hdr[0] = 0x01; hdr[1] = 0xDA;      // magic
    hdr[2] = 0x00;                       // storage: 0 = verbatim
    hdr[3] = 0x01;                       // bpc: 1 byte per channel per pixel
    hdr[4] = 0x00; hdr[5] = 0x03;       // dimension: 3 (x, y, z)
    hdr[6] = (W >> 8) & 0xFF; hdr[7] = W & 0xFF;           // xsize
    hdr[8] = (H >> 8) & 0xFF; hdr[9] = H & 0xFF;           // ysize
    hdr[10] = 0x00; hdr[11] = 0x03;     // zsize: 3 channels
    // pixmin (bytes 12-15) = 0:  already zeroed
    hdr[19] = 255;                       // pixmax low byte = 255
    // name (bytes 24-103) = "NO NAME\0"
    memcpy(hdr + 24, "NO NAME", 7);
    // colormap (bytes 104-107) = 0:  already zeroed

    if (fwrite(hdr, 1, 512, fp) != 512) { fclose(fp); return false; }

    // Planar pixel data: R channel, then G, then B.
    // Each plane: H rows × W pixels, row 0 = bottom of image.
    for (int c = 0; c < 3; ++c) {
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                if (fputc(buf[(y * W + x) * 3 + c], fp) == EOF) {
                    fclose(fp);
                    return false;
                }
            }
        }
    }

    fclose(fp);
    return true;
}

// ---------------------------------------------------------------------------
// Composite rendering
// ---------------------------------------------------------------------------

SbBool SoQuadViewport::writeCompositeToRGB(const char * filename,
                                            SoOffscreenRenderer * quadRenderer)
{
    if (!filename || !quadRenderer)
        return FALSE;

    const int W  = windowSize_[0];
    const int H  = windowSize_[1];
    const int qW = W / 2;
    const int qH = H / 2;

    // Allocate and zero-fill the composite buffer (interleaved RGB, bottom-to-top).
    const int bufBytes = W * H * 3;
    unsigned char * composite = new unsigned char[bufBytes];
    memset(composite, 0, bufBytes);

    // x/y pixel offsets for each quadrant in the composite:
    //   TOP_LEFT  (0): (0,   qH)
    //   TOP_RIGHT (1): (qW,  qH)
    //   BOT_LEFT  (2): (0,   0)
    //   BOT_RIGHT (3): (qW,  0)
    const int xOff[NUM_QUADS] = { 0,  qW, 0,  qW };
    const int yOff[NUM_QUADS] = { qH, qH, 0,  0  };

    // Use a simple qW×qH viewport region for each per-quadrant render so
    // the renderer allocates a qW×qH buffer (not the full W×H window).
    const SbViewportRegion qvp((short)qW, (short)qH);
    bool anyOk = false;

    for (int q = 0; q < NUM_QUADS; ++q) {
        quadRenderer->setViewportRegion(qvp);
        quadRenderer->setBackgroundColor(tiles_[q].getBackgroundColor());
        SbBool ok = quadRenderer->render(tiles_[q].getRoot());
        if (!ok) continue;
        anyOk = true;

        const unsigned char * qBuf = quadRenderer->getBuffer();
        const int cx = xOff[q];
        const int cy = yOff[q];

        for (int r = 0; r < qH; ++r) {
            const unsigned char * src = qBuf + r * qW * 3;
            unsigned char       * dst = composite + ((r + cy) * W + cx) * 3;
            memcpy(dst, src, qW * 3);
        }
    }

    if (!anyOk) {
        delete[] composite;
        return FALSE;
    }

    // Paint border lines over the assembled composite.
    if (borderWidth_ > 0)
        applyBordersToBuffer(composite, W, H);

    SbBool result = writeSGIRGB(filename, composite, W, H) ? TRUE : FALSE;
    delete[] composite;
    return result;
}

// ---------------------------------------------------------------------------
// Rendering convenience wrapper
// ---------------------------------------------------------------------------

SbBool SoQuadViewport::renderQuadrant(int quad, SoOffscreenRenderer * renderer)
{
    if (quad < 0 || quad >= NUM_QUADS || !renderer)
        return FALSE;
    return tiles_[quad].render(renderer);
}

// ---------------------------------------------------------------------------
// Event routing
// ---------------------------------------------------------------------------

SbBool SoQuadViewport::processEvent(const SoEvent * event)
{
    if (!event)
        return FALSE;
    return tiles_[activeQuad_].processEvent(event);
}
