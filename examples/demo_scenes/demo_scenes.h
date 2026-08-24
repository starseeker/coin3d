#ifndef OBOL_DEMO_SCENES_H
#define OBOL_DEMO_SCENES_H

#include <functional>
#include <string>
#include <string_view>
#include <vector>

class SoOffscreenRenderer;
class SoSeparator;

namespace ObolDemo {

/** Backends represented by the multi-renderer viewer. */
struct BackendSupport {
    bool nanort = true;
    bool embree = true;
    bool vulkan = true;
};

/**
 * A user-facing scene, intentionally distinct from a test case.
 *
 * Demo scenes explain a capability visually.  They do not embed assertions,
 * test categories, test runners, image goldens, or scripted test sequences.
 */
struct DemoScene {
    std::string id;
    std::string category;
    std::string title;
    std::string description;
    bool has_scene_interaction = false;
    BackendSupport backends;
    std::function<SoSeparator *(int width, int height)> create;
    std::function<void(SoOffscreenRenderer *)> configure_renderer;
};

const std::vector<DemoScene> & demoScenes();
const DemoScene * findDemoScene(std::string_view id);

} // namespace ObolDemo

#endif // OBOL_DEMO_SCENES_H
