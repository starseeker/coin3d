#include <Obol/scene/Renderer.h>
#include <Obol/scene/ScenePacketGeometry.h>

#include <Inventor/SbColor.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/nodes/SoSeparator.h>

#include <cstdio>
#include <memory>
#include <utility>

namespace obol {
namespace {

SoDB::ContextManager *
toRendererLegacyContext(NativeContextHandle handle)
{
    return static_cast<SoDB::ContextManager *>(handle);
}

RenderCapabilities queryCapabilities(RenderBackend * backend,
                                     SoOffscreenRenderer & renderer,
                                     SoDB::ContextManager * manager)
{
    RenderCapabilities caps = backend ? backend->capabilities() : RenderCapabilities{};
    if (backend) {
        if (caps.backendKind == RenderBackendKind::Unknown) {
            caps.backendKind = backend->kind();
        }
        if (caps.backendName.empty() && backend->name()) {
            caps.backendName = backend->name();
        }
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

size_t componentCount(PixelFormat format)
{
    switch (format) {
    case PixelFormat::Luminance:
        return 1;
    case PixelFormat::LuminanceAlpha:
        return 2;
    case PixelFormat::RGB:
        return 3;
    case PixelFormat::RGBA:
        return 4;
    }
    return 3;
}

bool writeByte(FILE * file, unsigned char value)
{
    return std::fputc(value, file) != EOF;
}

bool writeBigEndianShort(FILE * file, unsigned short value)
{
    return writeByte(file, static_cast<unsigned char>((value >> 8) & 0xff)) &&
           writeByte(file, static_cast<unsigned char>(value & 0xff));
}

bool writePacketRGB(const char * filename,
                    const unsigned char * pixels,
                    const RenderTarget & target)
{
    if (!filename || !pixels) {
        return false;
    }

    FILE * file = std::fopen(filename, "wb");
    if (!file) {
        return false;
    }

    const size_t components = componentCount(target.pixelFormat);
    if (!writeBigEndianShort(file, 0x01da) ||
        !writeByte(file, 0x00) ||
        !writeByte(file, 0x01) ||
        !writeBigEndianShort(file, components == 1 ? 0x0002 : 0x0003) ||
        !writeBigEndianShort(file, static_cast<unsigned short>(target.width)) ||
        !writeBigEndianShort(file, static_cast<unsigned short>(target.height)) ||
        !writeBigEndianShort(file, static_cast<unsigned short>(components)) ||
        !writeBigEndianShort(file, 0x0000) ||
        !writeBigEndianShort(file, 0x0000) ||
        !writeBigEndianShort(file, 0x0000) ||
        !writeBigEndianShort(file, 0x00ff)) {
        std::fclose(file);
        return false;
    }

    unsigned char headerRest[488] = {};
    const char name[] = "Obol packet renderer";
    for (size_t i = 0; i + 1 < sizeof(name) && i < 80; ++i) {
        headerRest[i] = static_cast<unsigned char>(name[i]);
    }
    if (std::fwrite(headerRest, 1, sizeof(headerRest), file) != sizeof(headerRest)) {
        std::fclose(file);
        return false;
    }

    for (size_t channel = 0; channel < components; ++channel) {
        for (unsigned int y = 0; y < target.height; ++y) {
            for (unsigned int x = 0; x < target.width; ++x) {
                const size_t source =
                    (static_cast<size_t>(y) * target.width + x) *
                        components +
                    channel;
                if (!writeByte(file, pixels[source])) {
                    std::fclose(file);
                    return false;
                }
            }
        }
    }

    return std::fclose(file) == 0;
}

} // namespace

RenderBackend::~RenderBackend() = default;

NativeContextHandle
RenderBackend::legacyContextHandle()
{
    return nullptr;
}

RenderCapabilities
RenderBackend::capabilities() const
{
    RenderCapabilities caps;
    caps.backendKind = kind();
    caps.backendName = name() ? name() : "";
    return caps;
}

bool
RenderBackend::renderPacket(const ScenePacket & packet,
                            const RenderTarget &,
                            const RenderOptions &,
                            const Color &,
                            std::vector<unsigned char> &,
                            std::vector<RenderDiagnostic> & diagnostics)
{
    std::vector<PacketGeometryDiagnostic> geometryDiagnostics;
    inspectPacketGeometrySupport(packet, &geometryDiagnostics);
    for (const PacketGeometryDiagnostic & geometryDiagnostic :
         geometryDiagnostics) {
        addDiagnostic(diagnostics,
                      geometryDiagnostic.severity ==
                              PacketGeometryDiagnosticSeverity::Error
                          ? DiagnosticSeverity::Error
                          : DiagnosticSeverity::Warning,
                      geometryDiagnostic.message);
    }
    addDiagnostic(diagnostics, DiagnosticSeverity::Error,
                  "Render backend does not implement packet rendering and did not provide a legacy context.");
    return false;
}

ContextManagerBackend::ContextManagerBackend(NativeContextHandle manager,
                                             RenderBackendKind backendKind,
                                             const char * backendName)
    : manager_(manager)
    , kind_(backendKind)
    , name_(backendName ? backendName : "context-manager")
{
}

ContextManagerBackend::~ContextManagerBackend() = default;

NativeContextHandle
ContextManagerBackend::legacyContextHandle()
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
        , manager(toRendererLegacyContext(backendIn.legacyContextHandle()))
        , target(targetIn)
        , renderer(manager, SbViewportRegion(targetIn.width, targetIn.height))
    {
        renderer.setComponents(toLegacyComponents(target.pixelFormat));
    }

    Impl(std::unique_ptr<RenderBackend> ownedBackendIn,
         const RenderTarget & targetIn)
        : ownedBackend(std::move(ownedBackendIn))
        , backend(ownedBackend.get())
        , manager(backend ? toRendererLegacyContext(backend->legacyContextHandle()) : nullptr)
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
    std::vector<unsigned char> packetPixels;
    bool hasPacketPixels = false;
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

OffscreenRenderer::OffscreenRenderer(NativeContextHandle manager,
                                     const RenderTarget & target)
    : impl_(new Impl(std::unique_ptr<RenderBackend>(
                         new ContextManagerBackend(manager)),
                     target))
{
}

OffscreenRenderer::OffscreenRenderer(NativeContextHandle manager,
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
    impl_->packetPixels.clear();
    impl_->hasPacketPixels = false;
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
    impl_->packetPixels.clear();
    impl_->hasPacketPixels = false;
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

    if (!impl_->manager) {
        impl_->packetPixels.clear();
        impl_->hasPacketPixels = false;
        result.success =
            impl_->backend &&
            impl_->backend->renderPacket(scene.capturePacket(),
                                         impl_->target,
                                         options,
                                         impl_->background,
                                         impl_->packetPixels,
                                         result.diagnostics);
        const size_t requiredBytes =
            static_cast<size_t>(impl_->target.width) *
            static_cast<size_t>(impl_->target.height) *
            componentCount(impl_->target.pixelFormat);
        if (result.success && impl_->packetPixels.size() < requiredBytes) {
            addDiagnostic(result.diagnostics, DiagnosticSeverity::Error,
                          "Packet renderer returned fewer pixels than the render target requires.");
            result.success = false;
        }
        impl_->hasPacketPixels = result.success;
        return result;
    }

    impl_->packetPixels.clear();
    impl_->hasPacketPixels = false;
    std::unique_ptr<SoSeparator, void(*)(SoSeparator *)> root(
        static_cast<SoSeparator *>(scene.createLegacySceneGraph()),
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
    if (impl_->hasPacketPixels && !impl_->packetPixels.empty()) {
        return impl_->packetPixels.data();
    }
    if (!impl_->manager) {
        return nullptr;
    }
    return impl_->renderer.getBuffer();
}

bool
OffscreenRenderer::writeRGB(const char * filename) const
{
    if (impl_->hasPacketPixels && !impl_->packetPixels.empty()) {
        return writePacketRGB(filename, impl_->packetPixels.data(), impl_->target);
    }
    if (!impl_->manager) {
        return false;
    }
    return impl_->renderer.writeToRGB(filename) == TRUE;
}

} // namespace obol
