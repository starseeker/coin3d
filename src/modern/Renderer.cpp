#include <Obol/scene/Renderer.h>

#include <Inventor/SbColor.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/nodes/SoSeparator.h>

#include <memory>
#include <utility>

namespace obol {
namespace {

RenderCapabilities queryCapabilities(RenderBackend * backend,
                                     SoOffscreenRenderer & renderer,
                                     SoDB::ContextManager * manager)
{
    RenderCapabilities caps;
    if (backend) {
        caps.backendKind = backend->kind();
        caps.backendName = backend->name() ? backend->name() : "";
    }
    if (!manager) {
        return caps;
    }

    renderer.getOpenGLVersion(caps.glMajor, caps.glMinor, caps.glRelease);
    caps.known = caps.glMajor > 0;
    caps.openGL2 = caps.glMajor > 2 || (caps.glMajor == 2 && caps.glMinor >= 0);
    caps.openGL3 = caps.glMajor > 3 || (caps.glMajor == 3 && caps.glMinor >= 0);
    caps.framebufferObjects = renderer.hasFramebufferObjectSupport() == TRUE;
    caps.shaders = renderer.isOpenGLExtensionSupported("GL_ARB_shader_objects") == TRUE ||
                   caps.openGL2;

    unsigned int maxW = 0;
    unsigned int maxH = 0;
    manager->maxOffscreenDimensions(maxW, maxH);
    caps.softwareRasterizer = (maxW >= 16384 && maxH >= 16384);

    return caps;
}

void addDiagnostic(std::vector<RenderDiagnostic> & diagnostics,
                   DiagnosticSeverity severity,
                   const std::string & message)
{
    RenderDiagnostic diagnostic;
    diagnostic.severity = severity;
    diagnostic.message = message;
    diagnostics.push_back(diagnostic);
}

SoOffscreenRenderer::Components toLegacyComponents(PixelFormat format)
{
    switch (format) {
    case PixelFormat::Luminance:
        return SoOffscreenRenderer::LUMINANCE;
    case PixelFormat::LuminanceAlpha:
        return SoOffscreenRenderer::LUMINANCE_TRANSPARENCY;
    case PixelFormat::RGB:
        return SoOffscreenRenderer::RGB;
    case PixelFormat::RGBA:
        return SoOffscreenRenderer::RGB_TRANSPARENCY;
    }
    return SoOffscreenRenderer::RGB;
}

bool supportsOpenGLCallbacks(RenderBackendKind kind,
                             SoDB::ContextManager * manager)
{
    if (!manager) {
        return false;
    }

    return kind == RenderBackendKind::OpenGL ||
           kind == RenderBackendKind::OpenGL2SWRast ||
           kind == RenderBackendKind::Unknown;
}

RenderTarget makeTarget(unsigned int width, unsigned int height)
{
    RenderTarget target;
    target.width = width;
    target.height = height;
    target.pixelFormat = PixelFormat::RGB;
    return target;
}

} // namespace

RenderBackend::~RenderBackend() = default;

ContextManagerBackend::ContextManagerBackend(SoDB::ContextManager * manager,
                                             RenderBackendKind backendKind,
                                             const char * backendName)
    : manager_(manager)
    , kind_(backendKind)
    , name_(backendName ? backendName : "context-manager")
{
}

ContextManagerBackend::~ContextManagerBackend() = default;

SoDB::ContextManager *
ContextManagerBackend::legacyContextManager()
{
    return manager_;
}

RenderBackendKind
ContextManagerBackend::kind() const
{
    return kind_;
}

const char *
ContextManagerBackend::name() const
{
    return name_.c_str();
}

struct OffscreenRenderer::Impl {
    Impl(RenderBackend & backendIn, const RenderTarget & targetIn)
        : backend(&backendIn)
        , manager(backendIn.legacyContextManager())
        , target(targetIn)
        , renderer(manager, SbViewportRegion(targetIn.width, targetIn.height))
    {
        renderer.setComponents(toLegacyComponents(target.pixelFormat));
    }

    Impl(std::unique_ptr<RenderBackend> ownedBackendIn,
         const RenderTarget & targetIn)
        : ownedBackend(std::move(ownedBackendIn))
        , backend(ownedBackend.get())
        , manager(backend ? backend->legacyContextManager() : nullptr)
        , target(targetIn)
        , renderer(manager, SbViewportRegion(targetIn.width, targetIn.height))
    {
        renderer.setComponents(toLegacyComponents(target.pixelFormat));
    }

    std::unique_ptr<RenderBackend> ownedBackend;
    RenderBackend * backend = nullptr;
    SoDB::ContextManager * manager = nullptr;
    RenderTarget target;
    Color background = {0.0f, 0.0f, 0.0f, 1.0f};
    SoOffscreenRenderer renderer;
};

OffscreenRenderer::OffscreenRenderer(RenderBackend & backend,
                                     const RenderTarget & target)
    : impl_(new Impl(backend, target))
{
}

OffscreenRenderer::OffscreenRenderer(RenderBackend & backend,
                                     unsigned int width,
                                     unsigned int height)
    : impl_(new Impl(backend, makeTarget(width, height)))
{
}

OffscreenRenderer::OffscreenRenderer(SoDB::ContextManager * manager,
                                     const RenderTarget & target)
    : impl_(new Impl(std::unique_ptr<RenderBackend>(
                         new ContextManagerBackend(manager)),
                     target))
{
}

OffscreenRenderer::OffscreenRenderer(SoDB::ContextManager * manager,
                                     unsigned int width,
                                     unsigned int height)
    : impl_(new Impl(std::unique_ptr<RenderBackend>(
                         new ContextManagerBackend(manager)),
                     makeTarget(width, height)))
{
}

OffscreenRenderer::~OffscreenRenderer()
{
    delete impl_;
}

void
OffscreenRenderer::setSize(unsigned int width, unsigned int height)
{
    impl_->target.width = width;
    impl_->target.height = height;
    impl_->renderer.setViewportRegion(SbViewportRegion(width, height));
}

unsigned int
OffscreenRenderer::width() const
{
    return impl_->target.width;
}

unsigned int
OffscreenRenderer::height() const
{
    return impl_->target.height;
}

PixelFormat
OffscreenRenderer::pixelFormat() const
{
    return impl_->target.pixelFormat;
}

void
OffscreenRenderer::setRenderTarget(const RenderTarget & target)
{
    impl_->target = target;
    impl_->renderer.setViewportRegion(SbViewportRegion(target.width, target.height));
    impl_->renderer.setComponents(toLegacyComponents(target.pixelFormat));
}

RenderTarget
OffscreenRenderer::renderTarget() const
{
    return impl_->target;
}

void
OffscreenRenderer::setBackgroundColor(const Color & color)
{
    impl_->background = color;
    impl_->renderer.setBackgroundColor(SbColor(color.r, color.g, color.b));
}

Color
OffscreenRenderer::backgroundColor() const
{
    return impl_->background;
}

FrameResult
OffscreenRenderer::render(const Scene & scene, const RenderOptions & options)
{
    FrameResult result;
    result.target = impl_->target;
    result.capabilities = queryCapabilities(impl_->backend,
                                            impl_->renderer,
                                            impl_->manager);

    if (options.nativeShaders) {
        addDiagnostic(result.diagnostics, DiagnosticSeverity::Warning,
                      "Native shader requests are backend-specific in v2; the initial renderer uses fixed-function/Phong fallback.");
    }
    if (options.advancedTransparency) {
        addDiagnostic(result.diagnostics, DiagnosticSeverity::Warning,
                      "Advanced transparency is optional; the initial renderer uses the backend default transparency mode.");
    }
    if (options.shadows && !result.capabilities.framebufferObjects) {
        addDiagnostic(result.diagnostics, DiagnosticSeverity::Warning,
                      "Shadow rendering requested, but framebuffer object support is unavailable or unknown; rendering without shadows.");
    }

    if (scene.hasObjects(SceneQuery{SceneObjectType::OpenGLCallback,
                                    SceneObjectCategory::BackendNative}) &&
        !supportsOpenGLCallbacks(result.capabilities.backendKind,
                                 impl_->manager)) {
        addDiagnostic(result.diagnostics, DiagnosticSeverity::Error,
                      "Scene contains backend-native OpenGL callbacks, but the active backend does not provide an OpenGL context.");
        result.success = false;
        return result;
    }

    std::unique_ptr<SoSeparator, void(*)(SoSeparator *)> root(
        scene.createLegacySceneGraph(),
        [](SoSeparator * node) {
            if (node) node->unref();
        });

    result.success = impl_->renderer.render(root.get()) == TRUE;
    if (!result.success) {
        addDiagnostic(result.diagnostics, DiagnosticSeverity::Error,
                      "Renderer failed to produce a frame.");
    }
    return result;
}

const unsigned char *
OffscreenRenderer::pixels() const
{
    return impl_->renderer.getBuffer();
}

bool
OffscreenRenderer::writeRGB(const char * filename) const
{
    return impl_->renderer.writeToRGB(filename) == TRUE;
}

} // namespace obol
