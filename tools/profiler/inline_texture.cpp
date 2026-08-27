#include <Inventor/SoDB.h>
#include <Inventor/SoInput.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/actions/SoWriteAction.h>

#include <Inventor/SoFullPath.h>

#include <cassert>
#include <iostream>

/*
  rolvs 20071107
  Standalone profiler-asset helper. Build with OBOL_BUILD_TOOLS=ON and use
  it to resolve SoTexture2 filenames into inline image data before writing
  the scene back to stdout.
*/

namespace {

int
inlineTextures(void)
{
  SoInput in;
  SoSeparator * root = SoDB::readAll(&in);
  if (!root) {
    std::cerr << "inline_texture: could not read an Inventor scene from stdin\n";
    return 1;
  }

  root->ref();

  SoSearchAction searchaction;
  searchaction.setType(SoTexture2::getClassTypeId());
  searchaction.setSearchingAll(TRUE);
  searchaction.setInterest(SoSearchAction::ALL);

  searchaction.apply(root);

  const SoPathList & pl = searchaction.getPaths();
  for (int i = 0; i < pl.getLength(); ++i) {
    SoFullPath * fp = static_cast<SoFullPath *>(pl[i]);
    SoTexture2 * tex = static_cast<SoTexture2 *>(fp->getTail());
    assert(tex->getTypeId() == SoTexture2::getClassTypeId());
    tex->image.touch();
  }

  SoWriteAction wa;
  wa.apply(root);

  root->unref();
  return 0;
}

} // namespace

int
main(void)
{
  // This utility only parses and writes scene data; it deliberately uses the
  // documented no-rendering initialization mode. Keep all Inventor objects in
  // inlineTextures() so their destructors run before SoDB::finish().
  SoDB::init(nullptr);
  const int result = inlineTextures();
  SoDB::finish();
  return result;
}
