/*
 * Public NanoRT-backed context manager.
 *
 * The implementation is compiled into Obol so users do not inherit NanoRT's
 * headers, implementation details, or worker-thread machinery in every
 * translation unit that includes this file.
 */

#ifndef OBOL_SO_NANORT_CONTEXT_MANAGER_H
#define OBOL_SO_NANORT_CONTEXT_MANAGER_H

#include <Inventor/SoDB.h>

class SoNode;

/*!
  \class SoNanoRTContextManager
  \brief CPU ray-tracing context manager backed by NanoRT.

  The implementation is part of the Obol library; applications do not need to
  include or link NanoRT directly.  Install an instance in SoDB::init(), or
  pass it to an explicit-manager SoOffscreenRenderer constructor.  The manager
  must outlive every renderer that uses it.

  An instance owns a scene cache and a persistent row-worker pool.  Its methods
  are not safe to call concurrently; use one manager per concurrent renderer.
*/
class OBOL_DLL_API SoNanoRTContextManager : public SoDB::ContextManager {
public:
  SoNanoRTContextManager();
  ~SoNanoRTContextManager() override;

  SoNanoRTContextManager(const SoNanoRTContextManager &) = delete;
  SoNanoRTContextManager & operator=(const SoNanoRTContextManager &) = delete;

  void * createOffscreenContext(unsigned int width,
                                unsigned int height) override;
  SbBool makeContextCurrent(void * context) override;
  void restorePreviousContext(void * context) override;
  void destroyContext(void * context) override;

  void resetCache();
  // Sets the full-resolution viewport used to size proxy geometry during a
  // reduced-resolution render.  (0, 0) restores render-size tracking.  Values
  // outside SbViewportRegion's positive short range also restore tracking.
  void setDisplayViewport(unsigned int width, unsigned int height);

  // pixels must address width * height * components bytes.  Supported layouts
  // are luminance, luminance-alpha, RGB, and RGBA for components 1 through 4.
  SbBool renderScene(SoNode * scene,
                     unsigned int width,
                     unsigned int height,
                     unsigned char * pixels,
                     unsigned int components,
                     const float backgroundRGB[3]) override;

private:
  class Impl;
  Impl * impl;
};

#endif // OBOL_SO_NANORT_CONTEXT_MANAGER_H
