#ifndef OBOL_BASE_EXPORT_H
#define OBOL_BASE_EXPORT_H

/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
\**************************************************************************/

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
# ifdef OBOL_INTERNAL
#  ifdef OBOL_MAKE_DLL
#   define OBOL_V2_API __declspec(dllexport)
#  endif
# else
#  ifdef OBOL_DLL
#   define OBOL_V2_API __declspec(dllimport)
#  endif
# endif
#endif

#ifndef OBOL_V2_API
# define OBOL_V2_API
#endif

namespace obol {

using NativeContextHandle = void *;
using NativeNodeHandle = void *;
using NativeSceneGraphHandle = void *;

} // namespace obol

#endif // OBOL_BASE_EXPORT_H
