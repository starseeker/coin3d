#include "SoNanoRTContextManagerP.h"

#include <limits>

SoNanoRTContextManager::SoNanoRTContextManager()
  : impl(new Impl)
{
}

SoNanoRTContextManager::~SoNanoRTContextManager()
{
  delete this->impl;
}

void *
SoNanoRTContextManager::createOffscreenContext(unsigned int, unsigned int)
{
  return nullptr;
}

SbBool
SoNanoRTContextManager::makeContextCurrent(void *)
{
  return FALSE;
}

void
SoNanoRTContextManager::restorePreviousContext(void *)
{
}

void
SoNanoRTContextManager::destroyContext(void *)
{
}

void
SoNanoRTContextManager::resetCache()
{
  this->impl->resetCache();
}

void
SoNanoRTContextManager::setDisplayViewport(unsigned int width,
                                           unsigned int height)
{
  const unsigned int maxViewport =
    static_cast<unsigned int>(std::numeric_limits<short>::max());
  if ((width == 0) != (height == 0) ||
      width > maxViewport || height > maxViewport) {
    width = 0;
    height = 0;
  }
  this->impl->setDisplayViewport(width, height);
}

SbBool
SoNanoRTContextManager::renderScene(SoNode * scene,
                                    unsigned int width,
                                    unsigned int height,
                                    unsigned char * pixels,
                                    unsigned int components,
                                    const float backgroundRGB[3])
{
  const unsigned int maxViewport =
    static_cast<unsigned int>(std::numeric_limits<short>::max());
  if (scene == nullptr || pixels == nullptr || backgroundRGB == nullptr ||
      components < 1 || components > 4 || width == 0 || height == 0 ||
      width > maxViewport || height > maxViewport) {
    return FALSE;
  }
  return this->impl->renderScene(scene, width, height, pixels, components,
                                 backgroundRGB);
}
