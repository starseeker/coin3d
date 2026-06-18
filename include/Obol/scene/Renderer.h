#ifndef OBOL_SCENE_RENDERER_H
#define OBOL_SCENE_RENDERER_H

/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
\**************************************************************************/

#include <Inventor/SbBasic.h>
#include <Inventor/SoDB.h>

#include <Obol/scene/Scene.h>

#include <string>
#include <vector>

namespace obol {

enum class DiagnosticSeverity {
    Info,
    Warning,
    Error
};

struct RenderDiagnostic {
    DiagnosticSeverity severity = DiagnosticSeverity::Info;
    std::string message;
};

enum class RenderBackendKind {
    Unknown,
    OpenGL,
    OpenGL2SWRast,
    CPU,
    Vulkan,
    Custom
};

struct RenderCapabilities {
    bool known = false;
    RenderBackendKind backendKind = RenderBackendKind::Unknown;
    std::string backendName;
    bool softwareRasterizer = false;
    bool framebufferObjects = false;
    bool shaders = false;
    bool openGL2 = false;
    bool openGL3 = false;
    int glMajor = 0;
    int glMinor = 0;
    int glRelease = 0;
};

struct RenderOptions {
    bool shadows = false;
    bool advancedTransparency = false;
    bool nativeShaders = false;
};

enum class PixelFormat {
    Luminance = 1,
    LuminanceAlpha = 2,
    RGB = 3,
    RGBA = 4
};

struct RenderTarget {
    unsigned int width = 1;
    unsigned int height = 1;
    PixelFormat pixelFormat = PixelFormat::RGB;
};

struct FrameResult {
    bool success = false;
    RenderTarget target;
    RenderCapabilities capabilities;
    std::vector<RenderDiagnostic> diagnostics;
};

class OBOL_DLL_API RenderBackend {
public:
    virtual ~RenderBackend();

    virtual SoDB::ContextManager * legacyContextManager() = 0;
    virtual RenderBackendKind kind() const = 0;
    virtual const char * name() const = 0;
};

class OBOL_DLL_API ContextManagerBackend : public RenderBackend {
public:
    ContextManagerBackend(SoDB::ContextManager * manager,
                          RenderBackendKind backendKind = RenderBackendKind::Unknown,
                          const char * backendName = "context-manager");
    ~ContextManagerBackend() override;

    SoDB::ContextManager * legacyContextManager() override;
    RenderBackendKind kind() const override;
    const char * name() const override;

private:
    SoDB::ContextManager * manager_;
    RenderBackendKind kind_;
    std::string name_;
};

class OBOL_DLL_API OffscreenRenderer {
public:
    OffscreenRenderer(RenderBackend & backend,
                      const RenderTarget & target);

    OffscreenRenderer(RenderBackend & backend,
                      unsigned int width,
                      unsigned int height);

    OffscreenRenderer(SoDB::ContextManager * manager,
                      const RenderTarget & target);

    OffscreenRenderer(SoDB::ContextManager * manager,
                      unsigned int width,
                      unsigned int height);
    ~OffscreenRenderer();

    OffscreenRenderer(const OffscreenRenderer &) = delete;
    OffscreenRenderer & operator=(const OffscreenRenderer &) = delete;

    void setSize(unsigned int width, unsigned int height);
    unsigned int width() const;
    unsigned int height() const;
    PixelFormat pixelFormat() const;

    void setRenderTarget(const RenderTarget & target);
    RenderTarget renderTarget() const;
    void setBackgroundColor(const Color & color);
    Color backgroundColor() const;

    FrameResult render(const Scene & scene,
                       const RenderOptions & options = RenderOptions{});

    const unsigned char * pixels() const;
    bool writeRGB(const char * filename) const;

private:
    struct Impl;
    Impl * impl_;
};

} // namespace obol

#endif // OBOL_SCENE_RENDERER_H
