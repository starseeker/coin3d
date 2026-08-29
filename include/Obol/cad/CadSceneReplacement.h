#ifndef OBOL_CAD_SCENE_REPLACEMENT_H
#define OBOL_CAD_SCENE_REPLACEMENT_H

/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistribution of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistribution in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES ARE DISCLAIMED.
\**************************************************************************/

/** @file CadSceneReplacement.h @brief Complete-scene mutation results. */

#include <Obol/cad/CadGeometryValidation.h>
#include <Obol/cad/CadSceneValidation.h>

namespace Obol {

/** Validation domain which rejected a complete retained-scene replacement. */
enum class CadSceneReplacementError {
    Valid = 0,
    Geometry,
    Instances,
    ResourceUnavailable
};

/**
 * Result of replacing a retained scene.
 *
 * A geometry or instance rejection identifies the corresponding subordinate
 * validation.  ResourceUnavailable reports candidate-construction failure
 * before commit.  Every rejected replacement leaves the preceding scene
 * unchanged.
 */
struct [[nodiscard]] CadSceneReplacementResult {
    CadSceneReplacementError error = CadSceneReplacementError::Valid;
    CadGeometryValidation geometry;
    CadSceneValidation instances;

    bool valid() const noexcept
    {
        return error == CadSceneReplacementError::Valid;
    }

    explicit operator bool() const noexcept
    {
        return valid();
    }
};

OBOL_DLL_API const char *cadSceneReplacementErrorName(
    CadSceneReplacementError error) noexcept;

} // namespace Obol

#endif // OBOL_CAD_SCENE_REPLACEMENT_H
