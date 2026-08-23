/**************************************************************************\\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the conditions in the Obol
 * license are met.
\\**************************************************************************/

#ifndef OBOL_CAD_SOFTWARE_WIRE_H
#define OBOL_CAD_SOFTWARE_WIRE_H

#include <Obol/cad/SoCADAssembly.h>
#include "CadFramePlan.h"

#include <vector>

class SoState;

struct CadSoftwareWireRenderResult {
    bool rendered = false;
    Obol::CadRenderedWork work;
    size_t subpixelProxyDrawPointCount = 0;
};

CadSoftwareWireRenderResult cadRenderSoftwareWire(
    const Obol::internal::CadFramePlan& plan,
    const SoCADAssembly& assembly, SoState *state,
    const SbMatrix& viewProj,
    const std::vector<Obol::internal::CadSubpixelProxyPoint>&
        subpixelProxyPoints);

#endif // OBOL_CAD_SOFTWARE_WIRE_H
