#include <Obol/scene/SceneIO.h>

#include <Inventor/SoDB.h>
#include <Inventor/SoInput.h>
#include <Inventor/SoOutput.h>
#include <Inventor/actions/SoWriteAction.h>
#include <Inventor/nodes/SoSeparator.h>

#include <memory>
#include <sstream>

namespace obol {
namespace {

SoDB::ContextManager *
toSceneIOLegacyContext(NativeContextHandle handle)
{
    return static_cast<SoDB::ContextManager *>(handle);
}

} // namespace

bool
SceneIO::readInventorString(const std::string & input,
                            Scene & scene,
                            NativeContextHandle manager)
{
    SoInput in;
    if (manager) {
        in.setContextManager(toSceneIOLegacyContext(manager));
    }
    in.setBuffer(input.data(), input.size());

    SoSeparator * root = SoDB::readAll(&in);
    if (!root) {
        return false;
    }

    root->ref();
    scene = Scene::fromLegacySceneGraph(root);
    root->unref();
    return true;
}

bool
SceneIO::readInventorFile(const char * filename,
                          Scene & scene,
                          NativeContextHandle manager)
{
    if (!filename) {
        return false;
    }

    SoInput in;
    if (manager) {
        in.setContextManager(toSceneIOLegacyContext(manager));
    }
    if (!in.openFile(filename)) {
        return false;
    }

    SoSeparator * root = SoDB::readAll(&in);
    in.closeFile();
    if (!root) {
        return false;
    }

    root->ref();
    scene = Scene::fromLegacySceneGraph(root);
    root->unref();
    return true;
}

SceneObjectId
SceneIO::addInventorString(const std::string & input,
                           Scene & scene,
                           const Transform & transform,
                           SceneGroupId parent,
                           NativeContextHandle manager)
{
    SoInput in;
    if (manager) {
        in.setContextManager(toSceneIOLegacyContext(manager));
    }
    in.setBuffer(input.data(), input.size());

    SoSeparator * root = SoDB::readAll(&in);
    if (!root) {
        return InvalidSceneObjectId;
    }

    root->ref();
    const SceneObjectId object = scene.addLegacySceneGraph(root, transform, parent);
    root->unref();
    return object;
}

SceneObjectId
SceneIO::addInventorFile(const char * filename,
                         Scene & scene,
                         const Transform & transform,
                         SceneGroupId parent,
                         NativeContextHandle manager)
{
    if (!filename) {
        return InvalidSceneObjectId;
    }

    SoInput in;
    if (manager) {
        in.setContextManager(toSceneIOLegacyContext(manager));
    }
    if (!in.openFile(filename)) {
        return InvalidSceneObjectId;
    }

    SoSeparator * root = SoDB::readAll(&in);
    in.closeFile();
    if (!root) {
        return InvalidSceneObjectId;
    }

    root->ref();
    const SceneObjectId object = scene.addLegacySceneGraph(root, transform, parent);
    root->unref();
    return object;
}

bool
SceneIO::writeInventorString(const Scene & scene, std::string & output)
{
    std::unique_ptr<SoSeparator, void(*)(SoSeparator *)> root(
        static_cast<SoSeparator *>(scene.createLegacySceneGraph()),
        [](SoSeparator * node) {
            if (node) node->unref();
        });

    std::ostringstream stream;
    SoOutput out;
    out.setBinary(FALSE);
    out.setStream(&stream);
    SoWriteAction action(&out);
    action.apply(root.get());

    output = stream.str();
    return !output.empty();
}

bool
SceneIO::writeInventorFile(const Scene & scene, const char * filename)
{
    if (!filename) {
        return false;
    }

    std::unique_ptr<SoSeparator, void(*)(SoSeparator *)> root(
        static_cast<SoSeparator *>(scene.createLegacySceneGraph()),
        [](SoSeparator * node) {
            if (node) node->unref();
        });

    SoOutput out;
    out.setBinary(FALSE);
    if (!out.openFile(filename)) {
        return false;
    }
    SoWriteAction action(&out);
    action.apply(root.get());
    out.closeFile();
    return true;
}

} // namespace obol
