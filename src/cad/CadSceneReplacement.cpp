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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES ARE DISCLAIMED.
\**************************************************************************/

#include <Obol/cad/CadSceneReplacement.h>

const char *
Obol::cadSceneReplacementErrorName(CadSceneReplacementError error) noexcept
{
    switch (error) {
    case CadSceneReplacementError::Valid: return "valid";
    case CadSceneReplacementError::Geometry: return "geometry";
    case CadSceneReplacementError::Instances: return "instances";
    case CadSceneReplacementError::ResourceUnavailable:
        return "resource-unavailable";
    }
    return "unknown";
}
