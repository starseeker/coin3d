#ifndef OBOL_SCENE_SCENEIO_H
#define OBOL_SCENE_SCENEIO_H

/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
\**************************************************************************/

#include <Obol/base/Export.h>
#include <Obol/scene/Scene.h>

#include <string>

namespace obol {

class OBOL_V2_API SceneIO {
public:
    static bool readInventorString(const std::string & input,
                                   Scene & scene,
                                   NativeContextHandle manager = nullptr);

    static bool readInventorFile(const char * filename,
                                 Scene & scene,
                                 NativeContextHandle manager = nullptr);

    static SceneObjectId addInventorString(const std::string & input,
                                           Scene & scene,
                                           const Transform & transform = Transform{},
                                           SceneGroupId parent = RootSceneGroupId,
                                           NativeContextHandle manager = nullptr);

    static SceneObjectId addInventorFile(const char * filename,
                                         Scene & scene,
                                         const Transform & transform = Transform{},
                                         SceneGroupId parent = RootSceneGroupId,
                                         NativeContextHandle manager = nullptr);

    static bool writeInventorString(const Scene & scene,
                                    std::string & output);

    static bool writeInventorFile(const Scene & scene,
                                  const char * filename);
};

} // namespace obol

#endif // OBOL_SCENE_SCENEIO_H
