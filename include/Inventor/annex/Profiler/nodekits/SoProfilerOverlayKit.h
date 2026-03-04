#ifndef OBOL_SOPROFILEROVERLAYKIT_H
#define OBOL_SOPROFILEROVERLAYKIT_H

#include <Inventor/nodekits/SoBaseKit.h>
#include <Inventor/nodekits/SoSubKit.h>
#include <Inventor/fields/SoSFVec3f.h>

/*!
  \class SoProfilerOverlayKit SoProfilerOverlayKit.h Inventor/annex/Profiler/nodekits/SoProfilerOverlayKit.h
  \brief Node kit that overlays profiling information on the rendered scene.

  \ingroup coin_annex

  SoProfilerOverlayKit adds an HUD-style transparent overlay over
  the scene showing live profiling statistics.

  \sa SoBaseKit, SoProfilerTopKit
*/
class OBOL_DLL_API SoProfilerOverlayKit : public SoBaseKit {
  typedef SoBaseKit inherited;
  SO_KIT_HEADER(SoProfilerOverlayKit);
  SO_KIT_CATALOG_ENTRY_HEADER(topSeparator);
  SO_KIT_CATALOG_ENTRY_HEADER(profilingStats);
  SO_KIT_CATALOG_ENTRY_HEADER(viewportInfo);
  SO_KIT_CATALOG_ENTRY_HEADER(overlayCamera);
  SO_KIT_CATALOG_ENTRY_HEADER(depthTestOff);
  SO_KIT_CATALOG_ENTRY_HEADER(overlaySep);
  SO_KIT_CATALOG_ENTRY_HEADER(depthTestOn);

public:
  static void initClass(void);
  SoProfilerOverlayKit(void);

  SoSFVec3f viewportSize; // output in pixels for internal use

  void addOverlayGeometry(SoNode * node);

protected:
  virtual ~SoProfilerOverlayKit(void);

private:
  struct SoProfilerOverlayKitP * pimpl;
};

#endif // !OBOL_SOPROFILEROVERLAYKIT_H
