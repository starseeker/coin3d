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

/*!
  \class SoText2 SoText2.h Inventor/nodes/SoText2.h
  \brief The SoText2 class is a node type for visualizing 2D text aligned with the camera plane.

  \ingroup coin_nodes

  SoText2 text is not scaled according to the distance from the
  camera, and is not influenced by rotation or scaling as 3D
  primitives are. If these are properties that you want the text to
  have, you should instead use an SoText3 or SoAsciiText node.

  Note that even though the size of the 2D text is not influenced by
  the distance from the camera, the text is still subject to the usual
  rules with regard to the depth buffer, so it \e will be obscured by
  graphics laying in front of it.

  The text will be \e positioned according to the current transformation.
  The x origin of the text is the first pixel of the leftmost character
  of the text. The y origin of the text is the baseline of the first line
  of text (the baseline being the imaginary line on which all upper case
  characters are standing).

  The size of the fonts on screen is decided from the SoFont::size
  field of a preceding SoFont-node in the scene graph, which specifies
  the size in pixel dimensions. This value sets the approximate
  vertical dimension of the letters.  The default value if no
  SoFont-nodes are used, is 10.

  One important issue about rendering performance: since the
  positioning and rendering of an SoText2 node depends on the current
  viewport and camera, having SoText2 nodes in the scene graph will
  lead to a cache dependency on the previously encountered
  SoCamera-node. This can have severe influence on the rendering
  performance, since if the camera is above the SoText2's nearest
  parent SoSeparator, the SoSeparator will not be able to cache the
  geometry under it.

  (Even worse rendering performance will be forced if the
  SoSeparator::renderCaching flag is explicitly set to \c ON, as the
  SoSeparator node will then continuously generate and destruct the
  same cache as the camera moves.)

  SoText2 nodes are therefore best positioned under their own
  SoSeparator node, outside areas in the scene graph that otherwise
  contains static geometry.

  Also note that SoText2 nodes cache the ids and positions of each glyph
  bitmap used to render \c string. This means that \c USE of a \c DEF'ed
  SoText2 node, with a different font, will be noticeably slower than using
  two separate SoText2 nodes, one for each font, since it will have to
  recalculate glyph bitmap ids and positions for each call to \c GLrender().

  SoScale nodes cannot be used to influence the dimensions of the
  rendering output of SoText2 nodes.

  <b>FILE FORMAT/DEFAULTS:</b>
  \code
    Text2 {
        string ""
        spacing 1
        justification LEFT
    }
  \endcode

  \sa SoFont, SoFontStyle, SoText3, SoAsciiText
*/

#include <Inventor/nodes/SoText2.h>
#include "config.h"

#include <climits>
#include <cstring>
#include <vector>


#include "../base/SbUtf8.h" // Modern UTF-8 support

#include <Inventor/SbBox2s.h>
#include <Inventor/SbLine.h>
#include <Inventor/SbPlane.h>
#include <Inventor/SbString.h>
#include <Inventor/SoPickedPoint.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/actions/SoGetPrimitiveCountAction.h>
#include <Inventor/actions/SoRayPickAction.h>
#include <Inventor/bundles/SoMaterialBundle.h>
#include <Inventor/details/SoTextDetail.h>
#include <Inventor/elements/SoCullElement.h>
#include <Inventor/elements/SoFontNameElement.h>
#include <Inventor/elements/SoFontSizeElement.h>
#include <Inventor/elements/SoGLCacheContextElement.h>
#include <Inventor/elements/SoComplexityTypeElement.h>
#include <Inventor/elements/SoComplexityElement.h>
#include <Inventor/elements/SoLazyElement.h>
#include <Inventor/elements/SoModelMatrixElement.h>
#include <Inventor/elements/SoProjectionMatrixElement.h>
#include <Inventor/elements/SoViewingMatrixElement.h>
#include <Inventor/elements/SoViewVolumeElement.h>
#include <Inventor/elements/SoViewportRegionElement.h>
#include <Inventor/errors/SoDebugError.h>
#include <Inventor/sensors/SoFieldSensor.h>
#include <Inventor/elements/SoCacheElement.h>
#include <Inventor/elements/SoMultiTextureEnabledElement.h>
#include <Inventor/elements/SoGLMultiTextureEnabledElement.h>

#ifdef OBOL_THREADSAFE
#include <Inventor/threads/SbMutex.h>
#endif // OBOL_THREADSAFE

#include "nodes/SoSubNodeP.h"
#include "caches/SoGlyphCache.h"

// The "lean and mean" define is a workaround for a Cygwin bug: when
// windows.h is included _after_ one of the X11 or GLX headers above
// (as it is indirectly from Inventor/system/gl.h), compilation of
// winspool.h (included from windows.h) will bail out with an error
// message due to the use of "Status" as a struct member ("Status" is
// probably #defined somewhere in the X11 or GLX header files).
//
// The WIN32_LEAN_AND_MEAN causes windows.h to not include winspool.h.
//
//        -mortene
#define WIN32_LEAN_AND_MEAN
#include <Inventor/system/gl.h>
#undef WIN32_LEAN_AND_MEAN
// UPDATE, FIXME: due to some reorganization of header files GL/glx.h
// should not be included anywhere for this source code file any
// more. This means the hack above should no longer be necessary. To
// test, try building this file with g++ on a Cygwin system where both
// windows.h and GL/glx.h are available. If that works fine, remove
// the "#define WIN32_LEAN_AND_MEAN" hack. 20030625 mortene.

#include <Inventor/SbFont.h>
#include "../misc/SoEnvironment.h"

/*!
  \enum SoText2::Justification
  Used to specify horizontal string alignment.
*/
/*!
  \var SoText2::Justification SoText2::LEFT
  Left edges of strings are aligned.
*/
/*!
  \var SoText2::Justification SoText2::RIGHT
  Right edges of strings are aligned.
*/
/*!
  \var SoText2::Justification SoText2::CENTER
  Centers of strings are aligned.
*/

/*!
  \var SoMFString SoText2::string

  The set of strings to render.  Each string in the multiple value
  field will be rendered on a separate line.

  The default value of the field is a single empty string.
*/
/*!
  \var SoSFFloat SoText2::spacing

  Vertical spacing between the baselines of two consecutive horizontal lines.
  Default value is 1.0, which means that it is equal to the vertical size of
  the highest character in the bitmap alphabet.
*/
/*!
  \var SoSFEnum SoText2::justification

  Determines horizontal alignment of text strings.

  If justification is set to SoText2::LEFT, the left edge of the first string
  is at the origin and all strings are aligned with their left edges.
  If set to SoText2::RIGHT, the right edge of the first string is
  at the origin and all strings are aligned with their right edges. Otherwise,
  if set to SoText2::CENTER, the center of the first string is at the
  origin and all strings are aligned with their centers.
  The origin is always located at the baseline of the first line of text.

  Default value is SoText2::LEFT.
*/
/*!
  \var SoSFBool SoText2::depthTest

  Controls whether the text pixels are tested against the depth buffer.

  When TRUE (the default), SoText2 follows the same depth-buffer rules as
  other geometry — text will be hidden by closer objects, matching upstream
  Coin behaviour.

  Set to FALSE to render the text always on top of all previously drawn 3D
  geometry, regardless of depth.  This matches the classic SGI OpenInventor
  2.1 behaviour and is useful for annotation labels that must always be
  readable (e.g. interactive handle markers produced by
  SoProceduralShape::buildSelectionDisplay()).

  Default value is TRUE.
*/

class SoText2P {
public:
  SoText2P(SoText2 * textnode) : maxwidth(0), master(textnode)
  {
    this->bbox.makeEmpty();
    this->font = new SbFont();  // Initialize with ProFont default
  }

  SbBool getQuad(SoState * state, SbVec3f & v0, SbVec3f & v1,
                 SbVec3f & v2, SbVec3f & v3);
  void flushGlyphCache();
  void buildGlyphCache(SoState * state);
  SbBool shouldBuildGlyphCache(SoState * state);
  void dumpBuffer(unsigned char * buffer, SbVec2s size, SbVec2s pos, SbBool mono);
  void computeBBox(SoAction * action, SbBox3f & box, SbVec3f & center);
  void updateFont(SoState * state);  // Update SbFont from state elements
  static void setRasterPos3f(GLfloat x, GLfloat y, GLfloat z);
  int buildGlyphQuads(SoState * state, std::vector<SbVec3f> & quads);


  SbList <int> stringwidth;
  int maxwidth;
  SbList< SbList<SbVec2s> > positions;
  SbBox2s bbox;

  SoGlyphCache * cache;
  SbFont * font;  // Direct SbFont for modern usage
  SoFieldSensor * spacingsensor;
  SoFieldSensor * stringsensor;
  unsigned char * pixel_buffer;
  int pixel_buffer_size;

  static void sensor_cb(void * userdata, SoSensor * OBOL_UNUSED_ARG(s)) {
    SoText2P * thisp = (SoText2P*) userdata;
    thisp->lock();
    if (thisp->cache) thisp->cache->invalidate();
    thisp->unlock();
  }
  void lock(void) {
#ifdef OBOL_THREADSAFE
    this->mutex.lock();
#endif // OBOL_THREADSAFE
  }
  void unlock(void) {
#ifdef OBOL_THREADSAFE
    this->mutex.unlock();
#endif // OBOL_THREADSAFE
  }
private:
#ifdef OBOL_THREADSAFE
  // FIXME: a mutex for every instance seems a bit excessive,
  // especially since Microsoft Windows might have rather strict limits on the
  // total amount of mutex resources a process (or even a user) can
  // allocate. so consider making this a class-wide instance instead.
  // -mortene.
  SbMutex mutex;
#endif // OBOL_THREADSAFE
  SoText2 * master;
};

#define PRIVATE(p) (p->pimpl)
#define PUBLIC(p) (p->master)

// *************************************************************************

SO_NODE_SOURCE(SoText2);

/*!
  Constructor.
*/
SoText2::SoText2(void)
{
  PRIVATE(this) = new SoText2P(this);

  SO_NODE_INTERNAL_CONSTRUCTOR(SoText2);

  SO_NODE_ADD_FIELD(string, (""));
  SO_NODE_ADD_FIELD(spacing, (1.0f));
  SO_NODE_ADD_FIELD(justification, (SoText2::LEFT));
  SO_NODE_ADD_FIELD(depthTest, (TRUE));

  SO_NODE_DEFINE_ENUM_VALUE(Justification, LEFT);
  SO_NODE_DEFINE_ENUM_VALUE(Justification, RIGHT);
  SO_NODE_DEFINE_ENUM_VALUE(Justification, CENTER);
  SO_NODE_SET_SF_ENUM_TYPE(justification, Justification);

  PRIVATE(this)->stringsensor = new SoFieldSensor(SoText2P::sensor_cb, PRIVATE(this));
  PRIVATE(this)->stringsensor->attach(&this->string);
  PRIVATE(this)->stringsensor->setPriority(0);
  PRIVATE(this)->spacingsensor = new SoFieldSensor(SoText2P::sensor_cb, PRIVATE(this));
  PRIVATE(this)->spacingsensor->attach(&this->spacing);
  PRIVATE(this)->spacingsensor->setPriority(0);
  PRIVATE(this)->cache = NULL;
  PRIVATE(this)->pixel_buffer = NULL;
  PRIVATE(this)->pixel_buffer_size = 0;
}

/*!
  Destructor.
*/
SoText2::~SoText2()
{
  if (PRIVATE(this)->cache) PRIVATE(this)->cache->unref();
  delete PRIVATE(this)->font;  // Clean up SbFont
  delete[] PRIVATE(this)->pixel_buffer;
  delete PRIVATE(this)->stringsensor;
  delete PRIVATE(this)->spacingsensor;

  PRIVATE(this)->flushGlyphCache();
  delete PRIVATE(this);
}

/*!
  \copydetails SoNode::initClass(void)
*/
void
SoText2::initClass(void)
{
  SO_NODE_INTERNAL_INIT_CLASS(SoText2, SO_FROM_INVENTOR_2_1);
}

// **************************************************************************

// doc in super
void
SoText2::GLRender(SoGLRenderAction * action)
{
  if (!this->shouldGLRender(action)) return;

  SoState * state = action->getState();

  state->push();
  SoLazyElement::setLightModel(state, SoLazyElement::BASE_COLOR);

  PRIVATE(this)->lock();
  PRIVATE(this)->buildGlyphCache(state);
  SoCacheElement::addCacheDependency(state, PRIVATE(this)->cache);

  // Update SbFont with current state (direct usage instead of bridge)
  PRIVATE(this)->updateFont(state);

  // Render only if bbox not outside cull planes.
  SbBox3f box;
  SbVec3f center;
  PRIVATE(this)->computeBBox(action, box, center);
  if (!SoCullElement::cullTest(state, box, TRUE)) {
    SoMaterialBundle mb(action);
    mb.sendFirst();
    SbVec3f nilpoint(0.0f, 0.0f, 0.0f);
    const SbMatrix & mat = SoModelMatrixElement::get(state);
    const SbMatrix & projmatrix = (mat * SoViewingMatrixElement::get(state) *
                                   SoProjectionMatrixElement::get(state));
    const SbViewportRegion & vp = SoViewportRegionElement::get(state);
    SbVec2s vpsize = vp.getViewportSizePixels();

    projmatrix.multVecMatrix(nilpoint, nilpoint);
    nilpoint[0] = (nilpoint[0] + 1.0f) * 0.5f * vpsize[0];
    nilpoint[1] = (nilpoint[1] + 1.0f) * 0.5f * vpsize[1];

    SbVec2s bbsize = PRIVATE(this)->bbox.getSize();
    const SbVec2s& bbmin = PRIVATE(this)->bbox.getMin();
    const SbVec2s& bbmax = PRIVATE(this)->bbox.getMax();

    float textscreenoffsetx = nilpoint[0]+bbmin[0];
    switch (this->justification.getValue()) {
    case SoText2::LEFT:
      break;
    case SoText2::RIGHT:
      textscreenoffsetx = nilpoint[0] + bbmin[0] - PRIVATE(this)->maxwidth;
      break;
    case SoText2::CENTER:
      textscreenoffsetx = (nilpoint[0] + bbmin[0] - PRIVATE(this)->maxwidth / 2.0f);
      break;
    }

    // Set new state.
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, vpsize[0], 0, vpsize[1], -1.0f, 1.0f);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);

    float fontsize = SoFontSizeElement::get(state);
    int xpos = 0;
    int ypos = 0;
    int rasterx, rastery;
    int ix=0, iy=0;
    int bitmappos[2];
    int bitmapsize[2];
    const unsigned char * buffer = NULL;
    uint32_t prevglyphchar = 0;

    const int nrlines = this->string.getNum();

    // get the current diffuse color
    const SbColor & diffuse = SoLazyElement::getDiffuse(state, 0);
    unsigned char red   = (unsigned char) (diffuse[0] * 255.0f);
    unsigned char green = (unsigned char) (diffuse[1] * 255.0f);
    unsigned char blue  = (unsigned char) (diffuse[2] * 255.0f);
    const unsigned int alpha = (unsigned int)((1.0f - SoLazyElement::getTransparency(state, 0)) * 256);

    state->push();

    // disable textures for all units
    SoGLMultiTextureEnabledElement::disableAll(state);

    glPushAttrib(GL_ENABLE_BIT | GL_PIXEL_MODE_BIT | GL_COLOR_BUFFER_BIT | GL_TEXTURE_BIT);
    glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);

    // Optionally draw on top of all geometry, regardless of depth.
    // glPushAttrib(GL_ENABLE_BIT) above will restore GL_DEPTH_TEST on pop.
    if (!this->depthTest.getValue()) glDisable(GL_DEPTH_TEST);

    SbBool drawPixelBuffer = FALSE;

    for (int i = 0; i < nrlines; i++) {
      SbString str = this->string[i];
      switch (this->justification.getValue()) {
      case SoText2::LEFT:
        xpos = 0;
        break;
      case SoText2::RIGHT:
        xpos = PRIVATE(this)->maxwidth - PRIVATE(this)->stringwidth[i];
        break;
      case SoText2::CENTER:
        xpos = (PRIVATE(this)->maxwidth - PRIVATE(this)->stringwidth[i]) / 2;
        break;
      }

      int kerningx = 0;
      int advancex = 0;

      const char * p = str.getString();
      size_t length = coin_utf8_validate_length(p);

      for (unsigned int strcharidx = 0; strcharidx < length; strcharidx++) {
        uint32_t glyphidx = 0;

        glyphidx = coin_utf8_get_char(p);
        p = coin_utf8_next_char(p);

        // Use enhanced glyph caching for better performance
        SbGlyph2D * glyph = PRIVATE(this)->cache->getGlyph2D(glyphidx, PRIVATE(this)->font);
        if (glyph) {
          // Use cached glyph data
          buffer = glyph->bitmap;
          bitmapsize[0] = glyph->size[0];
          bitmapsize[1] = glyph->size[1];
          bitmappos[0] = glyph->bearing[0];
          bitmappos[1] = glyph->bearing[1];
          
          ix = bitmapsize[0];
          iy = bitmapsize[1];
          
          // Advance from cached data
          advancex = (int)glyph->advance[0];
          
          // Kerning (calculate between current and previous character)
          if (strcharidx > 0 && prevglyphchar != 0) {
            SbVec2f kern = PRIVATE(this)->font->getGlyphKerning(prevglyphchar, glyphidx);
            kerningx = (int)kern[0];
          }
        } else {
          // Fallback to direct SbFont calls if caching fails
          SbVec2s bitmapsize_sb, bitmapbearing;
          buffer = PRIVATE(this)->font->getGlyphBitmap(glyphidx, bitmapsize_sb, bitmapbearing);
          bitmapsize[0] = bitmapsize_sb[0];
          bitmapsize[1] = bitmapsize_sb[1];
          bitmappos[0] = bitmapbearing[0];
          bitmappos[1] = bitmapbearing[1];
          ix = bitmapsize[0];
          iy = bitmapsize[1];
          SbVec2f advance = PRIVATE(this)->font->getGlyphAdvance(glyphidx);
          advancex = (int)advance[0];
          if (strcharidx > 0 && prevglyphchar != 0) {
            SbVec2f kern = PRIVATE(this)->font->getGlyphKerning(prevglyphchar, glyphidx);
            kerningx = (int)kern[0];
          }
        }

        rasterx = xpos + kerningx + bitmappos[0];
        rastery = ypos + (bitmappos[1] - bitmapsize[1]);

        if (buffer) {
          // SbFont uses grayscale rendering, not mono
          if (FALSE) { // Never mono with SbFont
            SoText2P::setRasterPos3f((float)rasterx + textscreenoffsetx, (float)rastery + (int)nilpoint[1], -nilpoint[2]);
            glBitmap(ix,iy,0,0,0,0,(const GLubyte *)buffer);
          }
          else {
            if (!drawPixelBuffer) {
              int numpixels = bbsize[0] * bbsize[1];
              if (numpixels > PRIVATE(this)->pixel_buffer_size) {
                delete[] PRIVATE(this)->pixel_buffer;
                PRIVATE(this)->pixel_buffer = new unsigned char[numpixels*4];
                PRIVATE(this)->pixel_buffer_size = numpixels;
              }
              memset(PRIVATE(this)->pixel_buffer, 0, numpixels * 4);
              drawPixelBuffer = TRUE;
            }

            int memx = rasterx - bbmin[0];
            int memy = bbsize[1] - (bbmax[1] - rastery - 1) - 1;

            if (memx >= 0 && memx + bitmapsize[0] <= bbsize[0] &&
                memy >= 0 && memy + bitmapsize[1] <= bbsize[1]) {

              const unsigned char * src = buffer;
              int rowstride = bbsize[0] * 4;

              // stb_truetype bitmaps are stored top-to-bottom (row 0 = top of glyph).
              // glDrawPixels reads the pixel buffer bottom-to-top (row 0 = bottom of image).
              // Fill in reverse row order so the glyph appears right-side up.
              for (int y = 0; y < iy; y++) {
                // src row y (from top) → pixel buffer row (memy + iy - 1 - y)
                unsigned char * dst = PRIVATE(this)->pixel_buffer +
                  ((memy + iy - 1 - y) * bbsize[0] + memx) * 4;
                for (int x = 0; x < ix; x++) {
                  *dst++ = red; *dst++ = green; *dst++ = blue;
                  // Use the full stb_truetype grayscale coverage value as alpha.
                  // This preserves anti-aliased edges for smooth, high-quality text.
                  // GL_ALPHA_TEST (removed above) is NOT needed here: GL_BLEND alone
                  // gives correct results and works in both compat and core profiles.
                  // For overlapping glyphs, keep the highest alpha seen so far.
                  const int srcval = *src;
                  const int blended = ((256 - srcval) * (int)(*dst) + srcval * (int)(alpha < 256u ? alpha : 255u)) >> 8;
                  const unsigned char newval = (unsigned char)(blended < 255 ? blended : 255);
                  if (newval > *dst) *dst = newval;
                  src++; dst++;
                }
              }
            } else {
              static SbBool once = TRUE;
              if (once) {
                SoDebugError::post("SoText2::GLRender",
                                   "Unable to copy glyph to memory buffer. Position [%d,%d], size [%d,%d], buffer size [%d,%d]",
                                   memx, memy, bitmapsize[0], bitmapsize[1], bbsize[0], bbsize[1]);
                once = FALSE;
              }
            }
          }
        }

        xpos += (advancex + kerningx);

        // Track the previous character for kerning
        prevglyphchar = glyphidx;
      }

      ypos -= (int)(((int) fontsize) * this->spacing.getValue());
    }

    if (drawPixelBuffer) {
      // Composite the RGBA glyph buffer onto the framebuffer using a
      // texture-mapped quad.  This approach is modelled after the glfontstash
      // rendering backend (reworked for Obol / struetype) and replaces the
      // deprecated glDrawPixels path, which suffers from raster-position
      // clipping artefacts on several GL implementations (including OSMesa).
      //
      // The pixel buffer is stored bottom-to-top (GL/OpenGL convention):
      //   row 0   = bottom of text bbox  (texcoord t = 0)
      //   row h-1 = top    of text bbox  (texcoord t = 1)
      // The ortho projection set above is Y-up (bottom=0, top=vpsize[1]), so
      // the quad vertex coordinates map directly to screen pixels without any
      // additional flipping.
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

      // Bottom-left of the bounding box in screen-space (Y=0 at viewport bottom).
      // Round to integer pixel boundaries so the ProFont bitmap glyphs stay
      // pixel-sharp (matches the integer snap that glDrawPixels required).
      const int   rastery_base = (int)floor(nilpoint[1] + 0.5f) - bbsize[1] + bbmax[1];
      const float qx  = (float)(int)floor(textscreenoffsetx + 0.5f);
      const float qy  = (float)rastery_base;
      const float qx1 = qx  + (float)bbsize[0];
      const float qy1 = qy  + (float)bbsize[1];
      // Preserve the anchor's depth so depth-buffered scenes work correctly.
      const float qz  = -nilpoint[2];

      GLuint texid = 0;
      glGenTextures(1, &texid);
      if (texid) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texid);
        // Nearest filtering: glyphs are pixel-aligned so interpolation would
        // only blur edges.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                     bbsize[0], bbsize[1], 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, PRIVATE(this)->pixel_buffer);

        // GL_REPLACE: use the texture RGBA as-is so the pre-baked material
        // colour in the RGB channels and the struetype coverage in the alpha
        // channel pass straight through to the blending stage.
        glEnable(GL_TEXTURE_2D);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex3f(qx,  qy,  qz); /* bottom-left  */
        glTexCoord2f(1.0f, 0.0f); glVertex3f(qx1, qy,  qz); /* bottom-right */
        glTexCoord2f(1.0f, 1.0f); glVertex3f(qx1, qy1, qz); /* top-right    */
        glTexCoord2f(0.0f, 1.0f); glVertex3f(qx,  qy1, qz); /* top-left     */
        glEnd();

        glDisable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        glDeleteTextures(1, &texid);
      }
    }

    // pop old state
    glPopClientAttrib();
    glPopAttrib();
    state->pop();

    glPixelStorei(GL_UNPACK_ALIGNMENT,4);
    // Pop old GL matrix state.
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
  }

  PRIVATE(this)->unlock();

  state->pop();

  // don't auto cache SoText2 nodes.
  SoGLCacheContextElement::shouldAutoCache(action->getState(),
                                           SoGLCacheContextElement::DONT_AUTO_CACHE);
}

// **************************************************************************

// doc in super
void
SoText2::computeBBox(SoAction * action, SbBox3f & box, SbVec3f & center)
{
  PRIVATE(this)->lock();
  PRIVATE(this)->computeBBox(action, box, center);
  SoCacheElement::addCacheDependency(action->getState(), PRIVATE(this)->cache);
  PRIVATE(this)->unlock();
}

// doc in super
void
SoText2::rayPick(SoRayPickAction * action)
{
  if (!this->shouldRayPick(action)) return;

  PRIVATE(this)->lock();
  PRIVATE(this)->buildGlyphCache(action->getState());
  action->setObjectSpace();

  SbVec3f v0, v1, v2, v3;
  if (!PRIVATE(this)->getQuad(action->getState(), v0, v1, v2, v3)) {
    PRIVATE(this)->unlock();
    return; // empty
  }

  SbVec3f isect;
  SbVec3f bary;
  SbBool front;
  SbBool hit = action->intersect(v0, v1, v2, isect, bary, front);
  if (!hit) hit = action->intersect(v0, v2, v3, isect, bary, front);

  if (hit && action->isBetweenPlanes(isect)) {
    // find normalized 2D hitpoint on quad
    float h = (v3-v0).length();
    float w = (v1-v0).length();
    SbLine horiz(v2,v3);
    SbVec3f ptonline = horiz.getClosestPoint(isect);
    float vdist = (ptonline-isect).length();
    vdist /= h;

    SbLine vert(v0,v3);
    ptonline = vert.getClosestPoint(isect);
    float hdist = (ptonline-isect).length();
    hdist /= w;

    // find which string was hit
    const int numstr = this->string.getNum();
    float fonth =  1.0f / float(numstr);
    int stringidx = (numstr - SbClamp(int(vdist/fonth), 0, numstr-1)) - 1;

    int maxlen = 0;
    int i;
    for (i = 0; i < numstr; i++) {
      int len = this->string[i].getLength();
      if (len > maxlen) maxlen = len;
    }

    // find the character
    int charidx = -1;
    int strlength = this->string[stringidx].getLength();
    short minx, miny, maxx, maxy;
    PRIVATE(this)->bbox.getBounds(minx, miny, maxx, maxy);
    float bbwidth = (float)(maxx - minx);
    float strleft = (bbwidth - PRIVATE(this)->stringwidth[stringidx]) / bbwidth;
    float strright = 1.0;
    switch (this->justification.getValue()) {
    case LEFT:
      strleft = 0.0;
      strright = PRIVATE(this)->stringwidth[stringidx] / bbwidth;
      break;
    case RIGHT:
      break;
    case CENTER:
      strleft /= 2.0;
      strright = 1.0f - strleft;
      break;
    default:
      assert(0 && "SoText2::rayPick: unknown justification");
    }

    float charleft, charright;
    for (i=0; i<strlength; i++) {
      charleft = strleft + PRIVATE(this)->positions[stringidx][i][0] / bbwidth;
      charright = (i==strlength-1 ? strright : strleft + (PRIVATE(this)->positions[stringidx][i+1][0] / bbwidth));
      if (hdist >= charleft && hdist <= charright) {
        charidx = i;
        i = strlength;
      }
    }


    if (charidx >= 0 && charidx < strlength) { // we have a hit!
      SoPickedPoint * pp = action->addIntersection(isect);
      if (pp) {
        SoTextDetail * detail = new SoTextDetail;
        detail->setStringIndex(stringidx);
        detail->setCharacterIndex(charidx);
        pp->setDetail(detail, this);
        pp->setMaterialIndex(0);
        pp->setObjectNormal(SbVec3f(0.0f, 0.0f, 1.0f));
      }
    }
  }
  PRIVATE(this)->unlock();
}

// doc in super
void
SoText2::getPrimitiveCount(SoGetPrimitiveCountAction *action)
{
  if (!this->shouldPrimitiveCount(action))
    return;

  action->addNumText(this->string.getNum());
}

// doc in super
void
SoText2::generatePrimitives(SoAction * OBOL_UNUSED_ARG(action))
{
  // This is supposed to be empty. There are no primitives.
}

/*!
  Returns the four world-space corner vertices of the screen-aligned bounding
  quad for this text string.

  The quad is computed using the current model matrix and view volume stored
  in \a state, so this method must be called during scene-graph traversal
  (e.g. from a SoCallbackAction pre-callback or a custom traversal action).

  The returned vertices v0..v3 are in counter-clockwise order:
    v0 = top-left, v1 = top-right, v2 = bottom-right, v3 = bottom-left.

  Returns FALSE if the quad is empty (e.g. empty string or font not loaded).

  This method is intended to provide a path forward for ray-tracing backends
  that cannot rasterise SoText2 directly.  The caller can construct a
  billboarded quad or bounding-box placeholder from the returned vertices.

  \sa SoText3::generatePrimitives()
*/
SbBool
SoText2::getTextQuad(SoState * state,
                     SbVec3f & v0, SbVec3f & v1,
                     SbVec3f & v2, SbVec3f & v3) const
{
  return PRIVATE(this)->getQuad(state, v0, v1, v2, v3);
}

/*!
  Generate per-glyph billboard quads for non-GL rendering backends.

  For each visible glyph, four vertices in object space (counter-clockwise:
  top-left, top-right, bottom-right, bottom-left) are appended to \a quads.
  The quads are screen-aligned at the text anchor depth and sized to the
  pixel extent of each glyph bitmap.  Whitespace characters are skipped.

  \param state  Current traversal state (view volume, model matrix, viewport,
                and font elements must be set up).
  \param quads  Output: 4 × SbVec3f per glyph appended in groups of four.
  \return       Number of quads appended (0 = nothing renderable).

  \sa getTextQuad()
*/
int
SoText2::buildGlyphQuads(SoState * state,
                         std::vector<SbVec3f> & quads) const
{
  PRIVATE(this)->lock();
  int n = PRIVATE(this)->buildGlyphQuads(state, quads);
  PRIVATE(this)->unlock();
  return n;
}

/*!
  Build a ready-to-composite RGBA pixel buffer for non-GL rendering backends.

  The pixel buffer uses the same binary-threshold alpha as GLRender: each
  pixel is either fully opaque (stb_truetype coverage >= 50%) or transparent.
  Rows are stored bottom-to-top (GL/OpenGL convention).

  \param state   Current traversal state.
  \param pixbuf  Resized and filled with out_w * out_h * 4 RGBA bytes.
  \param out_x   Left edge in viewport pixel coordinates.
  \param out_y   Bottom edge in viewport pixel coordinates.
  \param out_w   Width of the pixel buffer in pixels.
  \param out_h   Height of the pixel buffer in pixels.
  \return        TRUE if any text pixels were written.
*/
SbBool
SoText2::buildPixelBuffer(SoState * state,
                          std::vector<unsigned char> & pixbuf,
                          int & out_x, int & out_y,
                          int & out_w, int & out_h) const
{
  PRIVATE(this)->lock();

  PRIVATE(this)->buildGlyphCache(state);
  PRIVATE(this)->updateFont(state);

  SbVec3f nilpoint(0.0f, 0.0f, 0.0f);
  const SbMatrix & mat = SoModelMatrixElement::get(state);
  const SbMatrix & projmatrix = (mat * SoViewingMatrixElement::get(state) *
                                 SoProjectionMatrixElement::get(state));
  const SbViewportRegion & vp = SoViewportRegionElement::get(state);
  const SbVec2s vpsize = vp.getViewportSizePixels();

  projmatrix.multVecMatrix(nilpoint, nilpoint);
  nilpoint[0] = (nilpoint[0] + 1.0f) * 0.5f * vpsize[0];
  nilpoint[1] = (nilpoint[1] + 1.0f) * 0.5f * vpsize[1];

  const SbVec2s bbsize = PRIVATE(this)->bbox.getSize();
  const SbVec2s & bbmin = PRIVATE(this)->bbox.getMin();
  const SbVec2s & bbmax = PRIVATE(this)->bbox.getMax();

  if (bbsize[0] <= 0 || bbsize[1] <= 0) {
    PRIVATE(this)->unlock();
    return FALSE;
  }

  const int nrlines = this->string.getNum();
  const float fontsize = SoFontSizeElement::get(state);

  const SbColor & diffuse = SoLazyElement::getDiffuse(state, 0);
  const unsigned char red   = (unsigned char)(diffuse[0] * 255.0f);
  const unsigned char green = (unsigned char)(diffuse[1] * 255.0f);
  const unsigned char blue  = (unsigned char)(diffuse[2] * 255.0f);
  const unsigned int  alpha = (unsigned int)((1.0f - SoLazyElement::getTransparency(state, 0)) * 256);

  // Allocate pixel buffer (RGBA, bottom-to-top row order).
  const int numpixels = bbsize[0] * bbsize[1];
  pixbuf.assign(numpixels * 4, 0);

  int xpos = 0, ypos = 0;
  uint32_t prevglyphchar = 0;

  for (int i = 0; i < nrlines; i++) {
    const SbString str = this->string[i];
    switch (this->justification.getValue()) {
    case SoText2::LEFT:   xpos = 0; break;
    case SoText2::RIGHT:  xpos = PRIVATE(this)->maxwidth - PRIVATE(this)->stringwidth[i]; break;
    case SoText2::CENTER: xpos = (PRIVATE(this)->maxwidth - PRIVATE(this)->stringwidth[i]) / 2; break;
    }

    int kerningx = 0, advancex = 0;
    const char * p = str.getString();
    const size_t length = coin_utf8_validate_length(p);

    for (unsigned int ci = 0; ci < length; ci++) {
      const uint32_t glyphidx = coin_utf8_get_char(p);
      p = coin_utf8_next_char(p);

      SbGlyph2D * glyph = PRIVATE(this)->cache->getGlyph2D(glyphidx, PRIVATE(this)->font);
      if (!glyph) continue;

      const unsigned char * buffer = glyph->bitmap;
      const int ix = glyph->size[0];
      const int iy = glyph->size[1];
      if (!buffer || ix <= 0 || iy <= 0) {
        // Whitespace: advance without drawing.
        advancex = (int)glyph->advance[0];
        if (ci > 0 && prevglyphchar != 0) {
          kerningx = (int)PRIVATE(this)->font->getGlyphKerning(prevglyphchar, glyphidx)[0];
        }
        xpos += advancex + kerningx;
        prevglyphchar = glyphidx;
        continue;
      }

      advancex = (int)glyph->advance[0];
      if (ci > 0 && prevglyphchar != 0) {
        kerningx = (int)PRIVATE(this)->font->getGlyphKerning(prevglyphchar, glyphidx)[0];
      }

      const int rasterx = xpos + kerningx + glyph->bearing[0];
      const int rastery  = ypos + (glyph->bearing[1] - iy);
      const int memx = rasterx - bbmin[0];
      const int memy = bbsize[1] - (bbmax[1] - rastery - 1) - 1;

      if (memx >= 0 && memx + ix <= bbsize[0] &&
          memy >= 0 && memy + iy <= bbsize[1]) {
        const unsigned char * src = buffer;
        for (int gy = 0; gy < iy; gy++) {
          unsigned char * dst = pixbuf.data() +
            ((memy + iy - 1 - gy) * bbsize[0] + memx) * 4;
          for (int gx = 0; gx < ix; gx++) {
            *dst++ = red; *dst++ = green; *dst++ = blue;
            // Full stb_truetype grayscale alpha (same as GLRender path).
            const int srcval = *src++;
            const int blended = ((256 - srcval) * (int)(*dst) + srcval * (int)(alpha < 256u ? alpha : 255u)) >> 8;
            const unsigned char newval = (unsigned char)(blended < 255 ? blended : 255);
            if (newval > *dst) *dst = newval;
            dst++;
          }
        }
      }

      xpos += advancex + kerningx;
      prevglyphchar = glyphidx;
    }
    ypos -= (int)((int)fontsize * this->spacing.getValue());
  }

  // Compute the viewport-space bottom-left position of the pixel buffer.
  float textscreenoffsetx = nilpoint[0] + bbmin[0];
  switch (this->justification.getValue()) {
  case SoText2::RIGHT:  textscreenoffsetx = nilpoint[0] + bbmin[0] - PRIVATE(this)->maxwidth; break;
  case SoText2::CENTER: textscreenoffsetx = nilpoint[0] + bbmin[0] - PRIVATE(this)->maxwidth * 0.5f; break;
  default: break;
  }
  const int rastery_base = (int)floor(nilpoint[1] + 0.5f) - bbsize[1] + bbmax[1];

  out_x = (int)floor(textscreenoffsetx + 0.5f);
  out_y = rastery_base;
  out_w = bbsize[0];
  out_h = bbsize[1];

  PRIVATE(this)->unlock();
  return TRUE;
}

// SoText2P methods below

void
SoText2P::flushGlyphCache()
{
  this->stringwidth.truncate(0);
  this->maxwidth=0;
  this->positions.truncate(0);
  this->bbox.makeEmpty();
}

// Calculates a quad around the text in 3D.
//  Return FALSE if the quad is empty.
SbBool
SoText2P::getQuad(SoState * state, SbVec3f & v0, SbVec3f & v1,
                  SbVec3f & v2, SbVec3f & v3)
{
  this->buildGlyphCache(state);

  short xmin, ymin, xmax, ymax;
  this->bbox.getBounds(xmin, ymin, xmax, ymax);

  // FIXME: Why doesn't the SbBox2s have an 'isEmpty()' method as well?
  // (20040308 handegar)
  if (xmax < xmin) return FALSE;

  SbVec3f nilpoint(0.0f, 0.0f, 0.0f);
  const SbMatrix & mat = SoModelMatrixElement::get(state);
  mat.multVecMatrix(nilpoint, nilpoint);

  const SbViewVolume &vv = SoViewVolumeElement::get(state);

  SbVec3f screenpoint;
  vv.projectToScreen(nilpoint, screenpoint);

  const SbViewportRegion & vp = SoViewportRegionElement::get(state);
  SbVec2s vpsize = vp.getViewportSizePixels();

  SbVec2f n0, n1, n2, n3, center;
  SbVec2s sp((short) (screenpoint[0] * vpsize[0]), (short)(screenpoint[1] * vpsize[1]));

  n0 = SbVec2f(float(sp[0] + xmin)/float(vpsize[0]),
               float(sp[1] + ymax)/float(vpsize[1]));
  n1 = SbVec2f(float(sp[0] + xmax)/float(vpsize[0]),
               float(sp[1] + ymax)/float(vpsize[1]));
  n2 = SbVec2f(float(sp[0] + xmax)/float(vpsize[0]),
               float(sp[1] + ymin)/float(vpsize[1]));
  n3 = SbVec2f(float(sp[0] + xmin)/float(vpsize[0]),
               float(sp[1] + ymin)/float(vpsize[1]));

  float w = n1[0]-n0[0];
  float halfw = w*0.5f;
  switch (PUBLIC(this)->justification.getValue()) {
  case SoText2::LEFT:
    break;
  case SoText2::RIGHT:
    n0[0] -= w;
    n1[0] -= w;
    n2[0] -= w;
    n3[0] -= w;
    break;
  case SoText2::CENTER:
    n0[0] -= halfw;
    n1[0] -= halfw;
    n2[0] -= halfw;
    n3[0] -= halfw;
    break;
  default:
    assert(0 && "unknown alignment");
    break;
  }

  // get distance from nilpoint to camera plane
  float dist = -vv.getPlane(0.0f).getDistance(nilpoint);

  // find the four image points in the plane
  v0 = vv.getPlanePoint(dist, n0);
  v1 = vv.getPlanePoint(dist, n1);
  v2 = vv.getPlanePoint(dist, n2);
  v3 = vv.getPlanePoint(dist, n3);

  // test if the quad is outside the view frustum, ignore it in that case
  SbBox3f testbox;
  testbox.extendBy(v0);
  testbox.extendBy(v1);
  testbox.extendBy(v2);
  testbox.extendBy(v3);
  if (!vv.intersect(testbox)) return FALSE;

  // transform back to object space
  SbMatrix inv = mat.inverse();
  inv.multVecMatrix(v0, v0);
  inv.multVecMatrix(v1, v1);
  inv.multVecMatrix(v2, v2);
  inv.multVecMatrix(v3, v3);

  return TRUE;
}

// Generate per-glyph billboard quads for non-GL rendering backends.
// Returns the number of quads added to \a quads.
int
SoText2P::buildGlyphQuads(SoState * state, std::vector<SbVec3f> & quads)
{
  this->buildGlyphCache(state);

  const int nrlines = PUBLIC(this)->string.getNum();
  if (nrlines == 0 || this->positions.getLength() != nrlines)
    return 0;

  // View setup: same as getQuad().
  SbVec3f nilpoint(0.0f, 0.0f, 0.0f);
  const SbMatrix & mat = SoModelMatrixElement::get(state);
  mat.multVecMatrix(nilpoint, nilpoint);

  const SbViewVolume & vv = SoViewVolumeElement::get(state);

  SbVec3f screenpoint;
  vv.projectToScreen(nilpoint, screenpoint);

  const SbViewportRegion & vp = SoViewportRegionElement::get(state);
  SbVec2s vpsize = vp.getViewportSizePixels();
  if (vpsize[0] <= 0 || vpsize[1] <= 0) return 0;

  const SbVec2s sp((short)(screenpoint[0] * vpsize[0]),
                   (short)(screenpoint[1] * vpsize[1]));
  const float dist = -vv.getPlane(0.0f).getDistance(nilpoint);
  const SbMatrix inv = mat.inverse();
  const float vpW = static_cast<float>(vpsize[0]);
  const float vpH = static_cast<float>(vpsize[1]);

  int quadsAdded = 0;

  for (int i = 0; i < nrlines; i++) {
    // Apply the same per-line justification offset as GLRender().
    // positions[] is built with xpos=0 (LEFT); adjust here for other modes.
    int xjust = 0;
    switch (PUBLIC(this)->justification.getValue()) {
    case SoText2::RIGHT:
      xjust = this->maxwidth - this->stringwidth[i];
      break;
    case SoText2::CENTER:
      xjust = (this->maxwidth - this->stringwidth[i]) / 2;
      break;
    default: // LEFT
      break;
    }

    const SbList<SbVec2s> & charpos = this->positions[i];
    const SbString str = PUBLIC(this)->string[i];
    const char * p = str.getString();
    const size_t length = coin_utf8_validate_length(p);
    const int nchars = charpos.getLength();
    if (static_cast<int>(length) != nchars) continue;

    for (int j = 0; j < nchars; j++) {
      const uint32_t glyphidx = coin_utf8_get_char(p);
      p = coin_utf8_next_char(p);

      SbGlyph2D * glyph = this->cache->getGlyph2D(glyphidx, this->font);
      if (!glyph || glyph->size[0] <= 0 || glyph->size[1] <= 0)
        continue;  // whitespace or missing glyph bitmap

      // Pixel-space rectangle for this glyph: bottom-left origin + size.
      const SbVec2s & pos = charpos[j];
      const int pxl = sp[0] + pos[0] + xjust;
      const int pyb = sp[1] + pos[1];
      const int pxr = pxl + glyph->size[0];
      const int pyt = pyb + glyph->size[1];

      // Convert to normalized screen coords and unproject to world space.
      const SbVec2f nTL(static_cast<float>(pxl) / vpW, static_cast<float>(pyt) / vpH);
      const SbVec2f nTR(static_cast<float>(pxr) / vpW, static_cast<float>(pyt) / vpH);
      const SbVec2f nBR(static_cast<float>(pxr) / vpW, static_cast<float>(pyb) / vpH);
      const SbVec2f nBL(static_cast<float>(pxl) / vpW, static_cast<float>(pyb) / vpH);

      SbVec3f v0 = vv.getPlanePoint(dist, nTL);
      SbVec3f v1 = vv.getPlanePoint(dist, nTR);
      SbVec3f v2 = vv.getPlanePoint(dist, nBR);
      SbVec3f v3 = vv.getPlanePoint(dist, nBL);

      // Transform back to object space (mirrors getQuad()).
      inv.multVecMatrix(v0, v0);
      inv.multVecMatrix(v1, v1);
      inv.multVecMatrix(v2, v2);
      inv.multVecMatrix(v3, v3);

      quads.push_back(v0);
      quads.push_back(v1);
      quads.push_back(v2);
      quads.push_back(v3);
      ++quadsAdded;
    }
  }

  return quadsAdded;
}


void
SoText2P::dumpBuffer(unsigned char * buffer, SbVec2s size, SbVec2s pos, SbBool mono)
{
  // FIXME: pure debug method, remove. preng 2003-03-18.
  if (!buffer) {
    fprintf(stderr,"bitmap error: buffer pointer NULL.\n");
  } else {
    int rows = size[1];
    int bytes = mono ? size[0] >> 3 : size[0];
    fprintf(stderr, "%s bitmap dump %d * %d bytes at %d, %d:\n",
            mono ? "mono": "gray level", rows, bytes, pos[0], pos[1]);
    for (int y=rows-1; y>=0; y--) {
      for (int byte=0; byte<bytes; byte++) {
        for (int bit=0; bit<8; bit++) {
          fprintf(stderr, "%d", buffer[y*bytes + byte] & 0x80>>bit ? 1 : 0);
        }
      }
      fprintf(stderr,"\n");
    }
  }
}

// FIXME: Use notify() mechanism to detect field changes. For
// Coin3. preng, 2003-03-10.
//
// UPDATE 20030408 mortene: that wouldn't be sufficient, as
// e.g. changes to SoFont and SoFontStyle nodes in the scene graph can
// also have an influence on which glyphs to render.
//
// The best solution would be to create a new cache; SoGlyphCache or
// something. This cache would automatically store SoFont and
// SoFontStyle elements and be marked as invalid when they change
// (that's what caches are for). pederb, 2003-04-08

SbBool
SoText2P::shouldBuildGlyphCache(SoState * state)
{
  if (this->cache == NULL) return TRUE;
  return !this->cache->isValid(state);
}

void
SoText2P::buildGlyphCache(SoState * state)
{
  if (!this->shouldBuildGlyphCache(state)) {
    return;
  }

  this->flushGlyphCache();

  // don't unref the old cache until after we've created the new
  // cache.
  SoGlyphCache * oldcache = this->cache;

  state->push();
  SbBool storedinvalid = SoCacheElement::setInvalid(FALSE);
  this->cache = new SoGlyphCache(state);
  this->cache->ref();
  SoCacheElement::set(state, this->cache);
  this->cache->readFontspec(state);

  // CRITICAL FIX: Update font from state elements before bbox calculation
  // This ensures the font scale matches between bbox calculation and rendering
  SbName fontname = SoFontNameElement::get(state);
  float fontsize = SoFontSizeElement::get(state);
  this->font->setSize(fontsize);
  
  int ypos = 0;
  int maxoverhang = INT_MIN;

  const int nrlines = PUBLIC(this)->string.getNum();

  this->bbox.makeEmpty();

  // Debug: Add bounding box debugging to verify the fix
  auto env_debug = CoinInternal::getEnvironmentVariable("OBOL_DEBUG_BBOX");
  SbBool debug_bbox = env_debug.has_value() && (std::atoi(env_debug->c_str()) > 0);

  for (int i=0; i < nrlines; i++) {
    SbString str = PUBLIC(this)->string[i];
    this->positions.append(SbList<SbVec2s>());

    SbBox2s linebbox;
    linebbox.makeEmpty();  // CRITICAL FIX: Initialize the bounding box
    int xpos = 0;
    int actuallength = 0;
    int kerningx = 0;
    int advancex = 0;
    int bitmapsize[2];
    int bitmappos[2];
    uint32_t prevglyphchar = 0;
    const char * p = str.getString();
    size_t length = coin_utf8_validate_length(p);

    // fetch all glyphs first
    for (unsigned int strcharidx = 0; strcharidx < length; strcharidx++) {
      uint32_t glyphidx = 0;

      glyphidx = coin_utf8_get_char(p);
      p = coin_utf8_next_char(p);

      // Use enhanced glyph caching for bbox calculation as well
      SbGlyph2D * glyph = this->cache->getGlyph2D(glyphidx, this->font);
      if (glyph) {
        // Use cached glyph data for bbox calculation
        bitmapsize[0] = glyph->size[0];
        bitmapsize[1] = glyph->size[1];
        bitmappos[0] = glyph->bearing[0];
        bitmappos[1] = glyph->bearing[1];
        advancex = (int)glyph->advance[0];
        
        // Kerning calculation
        if (strcharidx > 0 && prevglyphchar != 0) {
          SbVec2f kern = this->font->getGlyphKerning(prevglyphchar, glyphidx);
          kerningx = (int)kern[0];
        }
      } else {
        // Fallback to direct SbFont calls
        SbVec2s bitmapsize_sb, bitmapbearing;
        (void)this->font->getGlyphBitmap(glyphidx, bitmapsize_sb, bitmapbearing);
        bitmapsize[0] = bitmapsize_sb[0];
        bitmapsize[1] = bitmapsize_sb[1];
        bitmappos[0] = bitmapbearing[0];
        bitmappos[1] = bitmapbearing[1];
        SbVec2f advance = this->font->getGlyphAdvance(glyphidx);
        advancex = (int)advance[0];
        if (strcharidx > 0 && prevglyphchar != 0) {
          SbVec2f kern = this->font->getGlyphKerning(prevglyphchar, glyphidx);
          kerningx = (int)kern[0];
        }
      }

      SbVec2s pos;
      pos[0] = xpos + kerningx + bitmappos[0];
      pos[1] = ypos + (bitmappos[1] - bitmapsize[1]);

      linebbox.extendBy(pos);
      linebbox.extendBy(pos + SbVec2s(bitmapsize[0], bitmapsize[1]));
      this->positions[i].append(pos);

      actuallength += (advancex + kerningx);

      xpos += (advancex + kerningx);
      prevglyphchar = glyphidx;
    }

    this->bbox.extendBy(linebbox);
    
    // Debug: Show bounding box after extending
    if (debug_bbox && i == 0) {  // Only show for first line to avoid spam
      short bxmin, bymin, bxmax, bymax;
      this->bbox.getBounds(bxmin, bymin, bxmax, bymax);
      printf("DEBUG: After line %d, bbox = (%d,%d) to (%d,%d)\n", i, bxmin, bymin, bxmax, bymax);
      fflush(stdout);
    }
    
    this->stringwidth.append(actuallength);
    if (actuallength > this->maxwidth) this->maxwidth=actuallength;

    // bitmap of last character can end before or beyond starting position of next character
    if (!linebbox.isEmpty())
    {
      int overhang = linebbox.getMax()[0] - actuallength;
      if (overhang > maxoverhang) maxoverhang = overhang;
    }

    ypos -= (int)(((int)fontsize) * PUBLIC(this)->spacing.getValue());
    
  }

  // extent bbox to include maxoverhang at the maxwidth string
  // this is needed for right-aligned text which gets aligned at the maxwidth
  // position, because there can be other strings with bitmaps going beyond
  if (maxoverhang > INT_MIN)
  {
    this->bbox.extendBy(SbVec2s(this->maxwidth + maxoverhang, this->bbox.getMax()[1]));
  }

  state->pop();
  SoCacheElement::setInvalid(storedinvalid);

  if (oldcache) oldcache->unref();
}

void
SoText2P::computeBBox(SoAction * action, SbBox3f & box, SbVec3f & center)
{
  SbVec3f v0, v1, v2, v3;
  // this will cause a cache dependency on the view volume,
  // model matrix and viewport.
  if (!this->getQuad(action->getState(), v0, v1, v2, v3)) {
    return; // empty
  }
  box.makeEmpty();

  box.extendBy(v0);
  box.extendBy(v1);
  box.extendBy(v2);
  box.extendBy(v3);

  center = box.getCenter();
}

// Sets the raster position for GL raster operations.
// Handles the special case where the x/y coordinates are negative
void
SoText2P::setRasterPos3f(GLfloat x, GLfloat y, GLfloat z)
{
  float rpx = x >= 0 ? x : 0;
  int offvp = x < 0 ? 1 : 0;
  float offsetx = x >= 0 ? 0 : x;

  float rpy = y >= 0 ? y : 0;
  offvp = (offvp || y < 0) ? 1 : 0;  // FIXED: Operator precedence bug
  float offsety = y >= 0 ? 0 : y;

  glRasterPos3f(rpx,rpy,z);
  if (offvp) { glBitmap(0, 0, 0, 0,offsetx,offsety, NULL); }
}

// Update SbFont with current font state elements
void
SoText2P::updateFont(SoState * state)
{
  // Get font information from state elements  
  SbName fontname = SoFontNameElement::get(state);
  float fontsize = SoFontSizeElement::get(state);
  
  // Set the size for the SbFont
  this->font->setSize(fontsize);
  
  // For now, we use ProFont as default. In a complete implementation,
  // we could load specific font files based on the fontname parameter.
}

#undef PRIVATE
#undef PUBLIC
