/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the conditions in COPYING are
 * met.
\**************************************************************************/

#ifndef OBOL_CAD_RENDERER_CONFIGURATION_H
#define OBOL_CAD_RENDERER_CONFIGURATION_H

#include <cstdlib>
#include <string>

/**
 * Immutable renderer policy sampled once for a CadRendererGL lifetime.
 *
 * Keeping environment and diagnostic policy outside executor code prevents
 * repeated getenv calls in frame paths and gives every specialized renderer
 * route one coherent configuration snapshot.
 */
struct CadRendererConfiguration {
    CadRendererConfiguration()
    {
        const auto present = [](const char *name) {
            return std::getenv(name) != nullptr;
        };
        const auto enabled = [](const char *name, bool defaultValue) {
            const char *value = std::getenv(name);
            return value ? value[0] != '0' : defaultValue;
        };
        const auto explicitlyEnabled = [](const char *name) {
            const char *value = std::getenv(name);
            return value && value[0] != '\0' && value[0] != '0';
        };

        softwareGlsl = explicitlyEnabled("OBOL_CAD_SOFTWARE_GLSL");
        lightDebug = explicitlyEnabled("OBOL_CAD_LIGHT_DEBUG");
        if (const char *value = std::getenv("OBOL_CAD_SHADER_DEBUG"))
            if (value[0] != '\0')
                shaderDebug = value;
        renderTiming = present("OBOL_CAD_RENDER_TIMING");
        flatShaded = enabled("OBOL_CAD_FLAT_SHADED", true);
        indirect = enabled("OBOL_CAD_INDIRECT", true);
        indirectDebug = present("OBOL_CAD_INDIRECT_DEBUG");
        flatWire = enabled("OBOL_CAD_FLAT_WIRE", true);
        patchDebug = present("OBOL_CAD_PATCH_DEBUG");
        appendPatch = enabled("OBOL_CAD_APPEND_PATCH", true);
        geometryPatch = enabled("OBOL_CAD_GEOMETRY_PATCH", true);
        lodPatch = enabled("OBOL_CAD_LOD_PATCH", true);
        validateReplay = present("OBOL_CAD_VALIDATE_REPLAY");
        validateGlState = present("OBOL_CAD_VALIDATE_GL_STATE");
        replay = enabled("OBOL_CAD_REPLAY", true);
    }

    bool softwareGlsl = false;
    bool lightDebug = false;
    std::string shaderDebug;
    bool renderTiming = false;
    bool flatShaded = true;
    bool indirect = true;
    bool indirectDebug = false;
    bool flatWire = true;
    bool patchDebug = false;
    bool appendPatch = true;
    bool geometryPatch = true;
    bool lodPatch = true;
    bool validateReplay = false;
    bool validateGlState = false;
    bool replay = true;
};

#endif // OBOL_CAD_RENDERER_CONFIGURATION_H
