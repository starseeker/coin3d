#ifndef OBOL_SCENE_SCENEIO_H
#define OBOL_SCENE_SCENEIO_H

/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
\**************************************************************************/

#include <Inventor/SbBasic.h>
#include <Inventor/SoDB.h>

#include <Obol/scene/Scene.h>

#include <string>

namespace obol {

class OBOL_DLL_API SceneIO {
public:
    static bool readInventorString(const std::string & input,
                                   Scene & scene,
                                   SoDB::ContextManager * manager = nullptr);

    static bool readInventorFile(const char * filename,
                                 Scene & scene,
                                 SoDB::ContextManager * manager = nullptr);

    static bool writeInventorString(const Scene & scene,
                                    std::string & output);

    static bool writeInventorFile(const Scene & scene,
                                  const char * filename);
};

} // namespace obol

#endif // OBOL_SCENE_SCENEIO_H
