#ifndef OBOL_CAD_CADSHADERSOURCES_H
#define OBOL_CAD_CADSHADERSOURCES_H

/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the conditions in COPYING are
 * met.
\**************************************************************************/

/**
 * @file CadShaderSources.h
 * @brief Private immutable source catalog for the specialized CAD GL tiers.
 *
 * Keeping shader text outside CadRendererGL makes renderer execution and
 * shader maintenance independently reviewable.  These pointers have static
 * storage duration and are never mutated after program startup.
 */

namespace Obol {
namespace internal {

// Stable attribute bindings shared by program construction and the retained
// executors.  A mat4 consumes four consecutive locations beginning at 2.
inline constexpr unsigned int kInstTransformLoc = 2;
inline constexpr unsigned int kInstColorLoc = 6;
inline constexpr unsigned int kInstPopMinLevelLoc = 7;
inline constexpr unsigned int kInstPopMaxFlagsLoc = 8;

extern const char * const kWireVS1;
extern const char * const kWirePopVS1;
extern const char * const kWireFS1;
extern const char * const kProxyPointVS1;
extern const char * const kProxyShadedVS1;
extern const char * const kShadedVS1;
extern const char * const kShadedPopVS1;
extern const char * const kShadedFaceVS1;
extern const char * const kShadedPopFaceVS1;
extern const char * const kShadedFS1;
extern const char * const kShadedDirectionalNormFS1;
extern const char * const kShadedDirectionalFaceFS1;
extern const char * const kShadedFaceDebugFS1;
extern const char * const kShadedNormalDebugFS1;

extern const char * const kWireVS2;
extern const char * const kWirePopVS2;
extern const char * const kWireFS2;
extern const char * const kShadedVS2;
extern const char * const kShadedPopVS2;
extern const char * const kShadedFS2;
extern const char * const kShadedIndirectVS2;
extern const char * const kShadedIndirectFS2;

} // namespace internal
} // namespace Obol

#endif // OBOL_CAD_CADSHADERSOURCES_H
