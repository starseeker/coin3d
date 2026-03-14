/* include/Inventor/system/gl_stub.h
 *
 * Minimal OpenGL type definitions, constants, and no-op function stubs
 * for OBOL_NO_OPENGL builds.
 *
 * In a no-OpenGL build the Obol library uses only custom context drivers
 * (nanort, embree, vulkan, ...).  All OpenGL rendering dispatch wrappers
 * (SoGLContext_gl*) are compiled to no-ops; the GL-dispatch implementation
 * (glue/gl.cpp) is excluded.  This header supplies just enough of the GL
 * type system and core constants so that every remaining source file that
 * includes <Inventor/system/gl.h> can compile without touching any real
 * OpenGL headers or libraries.
 *
 * ALL values are taken from the OpenGL 1.1 specification and are stable
 * across all platforms and GL implementations.
 */

#ifndef OBOL_GL_STUB_H
#define OBOL_GL_STUB_H

/* -------------------------------------------------------------------------
 * Basic GL scalar types
 * These are just aliases for standard C types; no GL library is needed.
 * --------------------------------------------------------------------- */

typedef unsigned int    GLenum;
typedef unsigned char   GLboolean;
typedef unsigned int    GLbitfield;
typedef signed   char   GLbyte;
typedef short           GLshort;
typedef int             GLint;
typedef unsigned char   GLubyte;
typedef unsigned short  GLushort;
typedef unsigned int    GLuint;
typedef int             GLsizei;
typedef float           GLfloat;
typedef float           GLclampf;
typedef double          GLdouble;
typedef double          GLclampd;
typedef void            GLvoid;

/* Platform-pointer-sized integer types (used in VBO/VAO paths) */
#if defined(__PTRDIFF_TYPE__)
typedef __PTRDIFF_TYPE__  GLintptr;
typedef __PTRDIFF_TYPE__  GLsizeiptr;
#elif defined(_WIN64)
typedef signed   __int64  GLintptr;
typedef signed   __int64  GLsizeiptr;
#else
typedef signed   long     GLintptr;
typedef signed   long     GLsizeiptr;
#endif

/* GLSL handle type */
typedef char              OBOL_GLchar;
typedef OBOL_GLchar       GLcharARB;

/* -------------------------------------------------------------------------
 * Core GL 1.0 / 1.1 constants
 * (Values from the Khronos OpenGL 1.1 specification; never change.)
 * --------------------------------------------------------------------- */

/* Boolean */
#ifndef GL_FALSE
#  define GL_FALSE                        0
#endif
#ifndef GL_TRUE
#  define GL_TRUE                         1
#endif

/* Data types */
#ifndef GL_BYTE
#  define GL_BYTE                         0x1400
#endif
#ifndef GL_UNSIGNED_BYTE
#  define GL_UNSIGNED_BYTE                0x1401
#endif
#ifndef GL_SHORT
#  define GL_SHORT                        0x1402
#endif
#ifndef GL_UNSIGNED_SHORT
#  define GL_UNSIGNED_SHORT               0x1403
#endif
#ifndef GL_INT
#  define GL_INT                          0x1404
#endif
#ifndef GL_UNSIGNED_INT
#  define GL_UNSIGNED_INT                 0x1405
#endif
#ifndef GL_FLOAT
#  define GL_FLOAT                        0x1406
#endif
#ifndef GL_DOUBLE
#  define GL_DOUBLE                       0x140A
#endif
#ifndef GL_2_BYTES
#  define GL_2_BYTES                      0x1407
#endif
#ifndef GL_3_BYTES
#  define GL_3_BYTES                      0x1408
#endif
#ifndef GL_4_BYTES
#  define GL_4_BYTES                      0x1409
#endif

/* Primitives */
#ifndef GL_POINTS
#  define GL_POINTS                       0x0000
#endif
#ifndef GL_LINES
#  define GL_LINES                        0x0001
#endif
#ifndef GL_LINE_LOOP
#  define GL_LINE_LOOP                    0x0002
#endif
#ifndef GL_LINE_STRIP
#  define GL_LINE_STRIP                   0x0003
#endif
#ifndef GL_TRIANGLES
#  define GL_TRIANGLES                    0x0004
#endif
#ifndef GL_TRIANGLE_STRIP
#  define GL_TRIANGLE_STRIP               0x0005
#endif
#ifndef GL_TRIANGLE_FAN
#  define GL_TRIANGLE_FAN                 0x0006
#endif
#ifndef GL_QUADS
#  define GL_QUADS                        0x0007
#endif
#ifndef GL_QUAD_STRIP
#  define GL_QUAD_STRIP                   0x0008
#endif
#ifndef GL_POLYGON
#  define GL_POLYGON                      0x0009
#endif

/* Vertex arrays */
#ifndef GL_VERTEX_ARRAY
#  define GL_VERTEX_ARRAY                 0x8074
#endif
#ifndef GL_NORMAL_ARRAY
#  define GL_NORMAL_ARRAY                 0x8075
#endif
#ifndef GL_COLOR_ARRAY
#  define GL_COLOR_ARRAY                  0x8076
#endif
#ifndef GL_INDEX_ARRAY
#  define GL_INDEX_ARRAY                  0x8077
#endif
#ifndef GL_TEXTURE_COORD_ARRAY
#  define GL_TEXTURE_COORD_ARRAY          0x8078
#endif
#ifndef GL_EDGE_FLAG_ARRAY
#  define GL_EDGE_FLAG_ARRAY              0x8079
#endif

/* Matrix modes */
#ifndef GL_MATRIX_MODE
#  define GL_MATRIX_MODE                  0x0BA0
#endif
#ifndef GL_MODELVIEW
#  define GL_MODELVIEW                    0x1700
#endif
#ifndef GL_PROJECTION
#  define GL_PROJECTION                   0x1701
#endif
#ifndef GL_TEXTURE
#  define GL_TEXTURE                      0x1702
#endif
#ifndef GL_COLOR
#  define GL_COLOR                        0x1800
#endif

/* Depth buffer */
#ifndef GL_NEVER
#  define GL_NEVER                        0x0200
#endif
#ifndef GL_LESS
#  define GL_LESS                         0x0201
#endif
#ifndef GL_EQUAL
#  define GL_EQUAL                        0x0202
#endif
#ifndef GL_LEQUAL
#  define GL_LEQUAL                       0x0203
#endif
#ifndef GL_GREATER
#  define GL_GREATER                      0x0204
#endif
#ifndef GL_NOTEQUAL
#  define GL_NOTEQUAL                     0x0205
#endif
#ifndef GL_GEQUAL
#  define GL_GEQUAL                       0x0206
#endif
#ifndef GL_ALWAYS
#  define GL_ALWAYS                       0x0207
#endif
#ifndef GL_DEPTH_FUNC
#  define GL_DEPTH_FUNC                   0x0B74
#endif
#ifndef GL_DEPTH_CLEAR_VALUE
#  define GL_DEPTH_CLEAR_VALUE            0x0B73
#endif
#ifndef GL_DEPTH_TEST
#  define GL_DEPTH_TEST                   0x0B71
#endif
#ifndef GL_DEPTH_BITS
#  define GL_DEPTH_BITS                   0x0D56
#endif
#ifndef GL_DEPTH_BUFFER_BIT
#  define GL_DEPTH_BUFFER_BIT             0x00000100
#endif
#ifndef GL_DEPTH_COMPONENT
#  define GL_DEPTH_COMPONENT              0x1902
#endif

/* Accumulation buffer */
#ifndef GL_ACCUM_RED_BITS
#  define GL_ACCUM_RED_BITS               0x0D58
#endif
#ifndef GL_ACCUM_GREEN_BITS
#  define GL_ACCUM_GREEN_BITS             0x0D59
#endif
#ifndef GL_ACCUM_BLUE_BITS
#  define GL_ACCUM_BLUE_BITS              0x0D5A
#endif
#ifndef GL_ACCUM_ALPHA_BITS
#  define GL_ACCUM_ALPHA_BITS             0x0D5B
#endif
#ifndef GL_ACCUM_CLEAR_VALUE
#  define GL_ACCUM_CLEAR_VALUE            0x0B80
#endif
#ifndef GL_ACCUM
#  define GL_ACCUM                        0x0100
#endif
#ifndef GL_LOAD
#  define GL_LOAD                         0x0101
#endif
#ifndef GL_RETURN
#  define GL_RETURN                       0x0102
#endif
#ifndef GL_MULT
#  define GL_MULT                         0x0103
#endif
#ifndef GL_ADD
#  define GL_ADD                          0x0104
#endif

/* Color buffer */
#ifndef GL_NONE
#  define GL_NONE                         0
#endif
#ifndef GL_FRONT_LEFT
#  define GL_FRONT_LEFT                   0x0400
#endif
#ifndef GL_FRONT_RIGHT
#  define GL_FRONT_RIGHT                  0x0401
#endif
#ifndef GL_BACK_LEFT
#  define GL_BACK_LEFT                    0x0402
#endif
#ifndef GL_BACK_RIGHT
#  define GL_BACK_RIGHT                   0x0403
#endif
#ifndef GL_FRONT
#  define GL_FRONT                        0x0404
#endif
#ifndef GL_BACK
#  define GL_BACK                         0x0405
#endif
#ifndef GL_LEFT
#  define GL_LEFT                         0x0406
#endif
#ifndef GL_RIGHT
#  define GL_RIGHT                        0x0407
#endif
#ifndef GL_FRONT_AND_BACK
#  define GL_FRONT_AND_BACK               0x0408
#endif
#ifndef GL_AUX0
#  define GL_AUX0                         0x0409
#endif
#ifndef GL_AUX1
#  define GL_AUX1                         0x040A
#endif
#ifndef GL_AUX2
#  define GL_AUX2                         0x040B
#endif
#ifndef GL_AUX3
#  define GL_AUX3                         0x040C
#endif
#ifndef GL_COLOR_BUFFER_BIT
#  define GL_COLOR_BUFFER_BIT             0x00004000
#endif
#ifndef GL_ACCUM_BUFFER_BIT
#  define GL_ACCUM_BUFFER_BIT             0x00000200
#endif
#ifndef GL_STENCIL_BUFFER_BIT
#  define GL_STENCIL_BUFFER_BIT           0x00000400
#endif

/* Clear */
#ifndef GL_DRAW_BUFFER
#  define GL_DRAW_BUFFER                  0x0C01
#endif
#ifndef GL_READ_BUFFER
#  define GL_READ_BUFFER                  0x0C02
#endif
#ifndef GL_COLOR_CLEAR_VALUE
#  define GL_COLOR_CLEAR_VALUE            0x0C22
#endif

/* Alpha test */
#ifndef GL_ALPHA_TEST
#  define GL_ALPHA_TEST                   0x0BC0
#endif
#ifndef GL_ALPHA_TEST_FUNC
#  define GL_ALPHA_TEST_FUNC              0x0BC1
#endif
#ifndef GL_ALPHA_TEST_REF
#  define GL_ALPHA_TEST_REF               0x0BC2
#endif

/* Blending */
#ifndef GL_BLEND
#  define GL_BLEND                        0x0BE2
#endif
#ifndef GL_BLEND_SRC
#  define GL_BLEND_SRC                    0x0BE1
#endif
#ifndef GL_BLEND_DST
#  define GL_BLEND_DST                    0x0BE0
#endif
#ifndef GL_ZERO
#  define GL_ZERO                         0
#endif
#ifndef GL_ONE
#  define GL_ONE                          1
#endif
#ifndef GL_SRC_COLOR
#  define GL_SRC_COLOR                    0x0300
#endif
#ifndef GL_ONE_MINUS_SRC_COLOR
#  define GL_ONE_MINUS_SRC_COLOR          0x0301
#endif
#ifndef GL_SRC_ALPHA
#  define GL_SRC_ALPHA                    0x0302
#endif
#ifndef GL_ONE_MINUS_SRC_ALPHA
#  define GL_ONE_MINUS_SRC_ALPHA          0x0303
#endif
#ifndef GL_DST_ALPHA
#  define GL_DST_ALPHA                    0x0304
#endif
#ifndef GL_ONE_MINUS_DST_ALPHA
#  define GL_ONE_MINUS_DST_ALPHA          0x0305
#endif
#ifndef GL_DST_COLOR
#  define GL_DST_COLOR                    0x0306
#endif
#ifndef GL_ONE_MINUS_DST_COLOR
#  define GL_ONE_MINUS_DST_COLOR          0x0307
#endif
#ifndef GL_SRC_ALPHA_SATURATE
#  define GL_SRC_ALPHA_SATURATE           0x0308
#endif

/* Logic op */
#ifndef GL_LOGIC_OP_MODE
#  define GL_LOGIC_OP_MODE                0x0BF0
#endif
#ifndef GL_INDEX_LOGIC_OP
#  define GL_INDEX_LOGIC_OP               0x0BF1
#endif
#ifndef GL_COLOR_LOGIC_OP
#  define GL_COLOR_LOGIC_OP               0x0BF2
#endif
#ifndef GL_CLEAR
#  define GL_CLEAR                        0x1500
#endif
#ifndef GL_AND
#  define GL_AND                          0x1501
#endif
#ifndef GL_AND_REVERSE
#  define GL_AND_REVERSE                  0x1502
#endif
#ifndef GL_COPY
#  define GL_COPY                         0x1503
#endif
#ifndef GL_AND_INVERTED
#  define GL_AND_INVERTED                 0x1504
#endif
#ifndef GL_NOOP
#  define GL_NOOP                         0x1505
#endif
#ifndef GL_XOR
#  define GL_XOR                          0x1506
#endif
#ifndef GL_OR
#  define GL_OR                           0x1507
#endif
#ifndef GL_NOR
#  define GL_NOR                          0x1508
#endif
#ifndef GL_EQUIV
#  define GL_EQUIV                        0x1509
#endif
#ifndef GL_INVERT
#  define GL_INVERT                       0x150A
#endif
#ifndef GL_OR_REVERSE
#  define GL_OR_REVERSE                   0x150B
#endif
#ifndef GL_COPY_INVERTED
#  define GL_COPY_INVERTED                0x150C
#endif
#ifndef GL_OR_INVERTED
#  define GL_OR_INVERTED                  0x150D
#endif
#ifndef GL_NAND
#  define GL_NAND                         0x150E
#endif
#ifndef GL_SET
#  define GL_SET                          0x150F
#endif

/* Stencil */
#ifndef GL_STENCIL_TEST
#  define GL_STENCIL_TEST                 0x0B90
#endif
#ifndef GL_STENCIL_FUNC
#  define GL_STENCIL_FUNC                 0x0B92
#endif
#ifndef GL_STENCIL_VALUE_MASK
#  define GL_STENCIL_VALUE_MASK           0x0B93
#endif
#ifndef GL_STENCIL_FAIL
#  define GL_STENCIL_FAIL                 0x0B94
#endif
#ifndef GL_STENCIL_PASS_DEPTH_FAIL
#  define GL_STENCIL_PASS_DEPTH_FAIL      0x0B95
#endif
#ifndef GL_STENCIL_PASS_DEPTH_PASS
#  define GL_STENCIL_PASS_DEPTH_PASS      0x0B96
#endif
#ifndef GL_STENCIL_REF
#  define GL_STENCIL_REF                  0x0B97
#endif
#ifndef GL_STENCIL_WRITEMASK
#  define GL_STENCIL_WRITEMASK            0x0B98
#endif
#ifndef GL_STENCIL_BITS
#  define GL_STENCIL_BITS                 0x0D57
#endif
#ifndef GL_STENCIL_CLEAR_VALUE
#  define GL_STENCIL_CLEAR_VALUE          0x0B91
#endif
#ifndef GL_KEEP
#  define GL_KEEP                         0x1E00
#endif
#ifndef GL_REPLACE
#  define GL_REPLACE                      0x1E01
#endif
#ifndef GL_INCR
#  define GL_INCR                         0x1E02
#endif
#ifndef GL_DECR
#  define GL_DECR                         0x1E03
#endif

/* Color, material */
#ifndef GL_COLOR_MATERIAL
#  define GL_COLOR_MATERIAL               0x0B57
#endif
#ifndef GL_COLOR_MATERIAL_FACE
#  define GL_COLOR_MATERIAL_FACE          0x0B55
#endif
#ifndef GL_COLOR_MATERIAL_PARAMETER
#  define GL_COLOR_MATERIAL_PARAMETER     0x0B56
#endif
#ifndef GL_AMBIENT
#  define GL_AMBIENT                      0x1200
#endif
#ifndef GL_DIFFUSE
#  define GL_DIFFUSE                      0x1201
#endif
#ifndef GL_SPECULAR
#  define GL_SPECULAR                     0x1202
#endif
#ifndef GL_POSITION
#  define GL_POSITION                     0x1203
#endif
#ifndef GL_SPOT_DIRECTION
#  define GL_SPOT_DIRECTION               0x1204
#endif
#ifndef GL_SPOT_EXPONENT
#  define GL_SPOT_EXPONENT                0x1205
#endif
#ifndef GL_SPOT_CUTOFF
#  define GL_SPOT_CUTOFF                  0x1206
#endif
#ifndef GL_CONSTANT_ATTENUATION
#  define GL_CONSTANT_ATTENUATION         0x1207
#endif
#ifndef GL_LINEAR_ATTENUATION
#  define GL_LINEAR_ATTENUATION           0x1208
#endif
#ifndef GL_QUADRATIC_ATTENUATION
#  define GL_QUADRATIC_ATTENUATION        0x1209
#endif
#ifndef GL_EMISSION
#  define GL_EMISSION                     0x1600
#endif
#ifndef GL_SHININESS
#  define GL_SHININESS                    0x1601
#endif
#ifndef GL_AMBIENT_AND_DIFFUSE
#  define GL_AMBIENT_AND_DIFFUSE          0x1602
#endif
#ifndef GL_COLOR_INDEXES
#  define GL_COLOR_INDEXES                0x1603
#endif

/* Lighting */
#ifndef GL_LIGHTING
#  define GL_LIGHTING                     0x0B50
#endif
#ifndef GL_LIGHT_MODEL_LOCAL_VIEWER
#  define GL_LIGHT_MODEL_LOCAL_VIEWER     0x0B51
#endif
#ifndef GL_LIGHT_MODEL_TWO_SIDE
#  define GL_LIGHT_MODEL_TWO_SIDE         0x0B52
#endif
#ifndef GL_LIGHT_MODEL_AMBIENT
#  define GL_LIGHT_MODEL_AMBIENT          0x0B53
#endif
#ifndef GL_LIGHT_MODEL_COLOR_CONTROL
#  define GL_LIGHT_MODEL_COLOR_CONTROL    0x81F8
#endif
#ifndef GL_SINGLE_COLOR
#  define GL_SINGLE_COLOR                 0x81F9
#endif
#ifndef GL_SEPARATE_SPECULAR_COLOR
#  define GL_SEPARATE_SPECULAR_COLOR      0x81FA
#endif
#ifndef GL_SHADE_MODEL
#  define GL_SHADE_MODEL                  0x0B54
#endif
#ifndef GL_FLAT
#  define GL_FLAT                         0x1D00
#endif
#ifndef GL_SMOOTH
#  define GL_SMOOTH                       0x1D01
#endif
#ifndef GL_LIGHT0
#  define GL_LIGHT0                       0x4000
#endif
#ifndef GL_LIGHT1
#  define GL_LIGHT1                       0x4001
#endif
#ifndef GL_LIGHT2
#  define GL_LIGHT2                       0x4002
#endif
#ifndef GL_LIGHT3
#  define GL_LIGHT3                       0x4003
#endif
#ifndef GL_LIGHT4
#  define GL_LIGHT4                       0x4004
#endif
#ifndef GL_LIGHT5
#  define GL_LIGHT5                       0x4005
#endif
#ifndef GL_LIGHT6
#  define GL_LIGHT6                       0x4006
#endif
#ifndef GL_LIGHT7
#  define GL_LIGHT7                       0x4007
#endif
#ifndef GL_MAX_LIGHTS
#  define GL_MAX_LIGHTS                   0x0D31
#endif

/* Normalize, rescale */
#ifndef GL_NORMALIZE
#  define GL_NORMALIZE                    0x0BA1
#endif
#ifndef GL_RESCALE_NORMAL
#  define GL_RESCALE_NORMAL               0x803A
#endif

/* Fog */
#ifndef GL_FOG
#  define GL_FOG                          0x0B60
#endif
#ifndef GL_FOG_MODE
#  define GL_FOG_MODE                     0x0B65
#endif
#ifndef GL_FOG_START
#  define GL_FOG_START                    0x0B63
#endif
#ifndef GL_FOG_END
#  define GL_FOG_END                      0x0B64
#endif
#ifndef GL_FOG_DENSITY
#  define GL_FOG_DENSITY                  0x0B62
#endif
#ifndef GL_FOG_COLOR
#  define GL_FOG_COLOR                    0x0B66
#endif
#ifndef GL_FOG_INDEX
#  define GL_FOG_INDEX                    0x0B61
#endif
#ifndef GL_EXP
#  define GL_EXP                          0x0800
#endif
#ifndef GL_EXP2
#  define GL_EXP2                         0x0801
#endif

/* Scissor, viewport */
#ifndef GL_SCISSOR_TEST
#  define GL_SCISSOR_TEST                 0x0C11
#endif
#ifndef GL_SCISSOR_BOX
#  define GL_SCISSOR_BOX                  0x0C10
#endif
#ifndef GL_VIEWPORT
#  define GL_VIEWPORT                     0x0BA2
#endif

/* Culling */
#ifndef GL_CULL_FACE
#  define GL_CULL_FACE                    0x0B44
#endif
#ifndef GL_CULL_FACE_MODE
#  define GL_CULL_FACE_MODE               0x0B45
#endif
#ifndef GL_FRONT_FACE
#  define GL_FRONT_FACE                   0x0B46
#endif
#ifndef GL_CW
#  define GL_CW                           0x0900
#endif
#ifndef GL_CCW
#  define GL_CCW                          0x0901
#endif

/* Polygon modes */
#ifndef GL_POLYGON_MODE
#  define GL_POLYGON_MODE                 0x0B40
#endif
#ifndef GL_POLYGON_SMOOTH
#  define GL_POLYGON_SMOOTH               0x0B41
#endif
#ifndef GL_POLYGON_STIPPLE
#  define GL_POLYGON_STIPPLE              0x0B42
#endif
#ifndef GL_EDGE_FLAG
#  define GL_EDGE_FLAG                    0x0B43
#endif
#ifndef GL_POINT
#  define GL_POINT                        0x1B00
#endif
#ifndef GL_LINE
#  define GL_LINE                         0x1B01
#endif
#ifndef GL_FILL
#  define GL_FILL                         0x1B02
#endif

/* Point and line attributes */
#ifndef GL_POINT_SMOOTH
#  define GL_POINT_SMOOTH                 0x0B10
#endif
#ifndef GL_POINT_SIZE
#  define GL_POINT_SIZE                   0x0B11
#endif
#ifndef GL_POINT_SIZE_RANGE
#  define GL_POINT_SIZE_RANGE             0x0B12
#endif
#ifndef GL_POINT_SIZE_GRANULARITY
#  define GL_POINT_SIZE_GRANULARITY       0x0B13
#endif
#ifndef GL_LINE_SMOOTH
#  define GL_LINE_SMOOTH                  0x0B20
#endif
#ifndef GL_LINE_WIDTH
#  define GL_LINE_WIDTH                   0x0B21
#endif
#ifndef GL_LINE_WIDTH_RANGE
#  define GL_LINE_WIDTH_RANGE             0x0B22
#endif
#ifndef GL_LINE_WIDTH_GRANULARITY
#  define GL_LINE_WIDTH_GRANULARITY       0x0B23
#endif
#ifndef GL_LINE_STIPPLE
#  define GL_LINE_STIPPLE                 0x0B24
#endif
#ifndef GL_LINE_STIPPLE_PATTERN
#  define GL_LINE_STIPPLE_PATTERN         0x0B25
#endif
#ifndef GL_LINE_STIPPLE_REPEAT
#  define GL_LINE_STIPPLE_REPEAT          0x0B26
#endif

/* Polygon offset */
#ifndef GL_POLYGON_OFFSET_FACTOR
#  define GL_POLYGON_OFFSET_FACTOR        0x8038
#endif
#ifndef GL_POLYGON_OFFSET_UNITS
#  define GL_POLYGON_OFFSET_UNITS         0x2A00
#endif
#ifndef GL_POLYGON_OFFSET_POINT
#  define GL_POLYGON_OFFSET_POINT         0x2A01
#endif
#ifndef GL_POLYGON_OFFSET_LINE
#  define GL_POLYGON_OFFSET_LINE          0x2A02
#endif
#ifndef GL_POLYGON_OFFSET_FILL
#  define GL_POLYGON_OFFSET_FILL          0x8037
#endif

/* Textures */
#ifndef GL_TEXTURE_1D
#  define GL_TEXTURE_1D                   0x0DE0
#endif
#ifndef GL_TEXTURE_2D
#  define GL_TEXTURE_2D                   0x0DE1
#endif
#ifndef GL_TEXTURE_WIDTH
#  define GL_TEXTURE_WIDTH                0x1000
#endif
#ifndef GL_TEXTURE_HEIGHT
#  define GL_TEXTURE_HEIGHT               0x1001
#endif
#ifndef GL_TEXTURE_INTERNAL_FORMAT
#  define GL_TEXTURE_INTERNAL_FORMAT      0x1003
#endif
#ifndef GL_TEXTURE_BORDER_COLOR
#  define GL_TEXTURE_BORDER_COLOR         0x1004
#endif
#ifndef GL_TEXTURE_BORDER
#  define GL_TEXTURE_BORDER               0x1005
#endif
#ifndef GL_TEXTURE_MAG_FILTER
#  define GL_TEXTURE_MAG_FILTER           0x2800
#endif
#ifndef GL_TEXTURE_MIN_FILTER
#  define GL_TEXTURE_MIN_FILTER           0x2801
#endif
#ifndef GL_TEXTURE_WRAP_S
#  define GL_TEXTURE_WRAP_S               0x2802
#endif
#ifndef GL_TEXTURE_WRAP_T
#  define GL_TEXTURE_WRAP_T               0x2803
#endif
#ifndef GL_PROXY_TEXTURE_1D
#  define GL_PROXY_TEXTURE_1D             0x8063
#endif
#ifndef GL_NEAREST
#  define GL_NEAREST                      0x2600
#endif
#ifndef GL_LINEAR
#  define GL_LINEAR                       0x2601
#endif
#ifndef GL_NEAREST_MIPMAP_NEAREST
#  define GL_NEAREST_MIPMAP_NEAREST       0x2700
#endif
#ifndef GL_LINEAR_MIPMAP_NEAREST
#  define GL_LINEAR_MIPMAP_NEAREST        0x2701
#endif
#ifndef GL_NEAREST_MIPMAP_LINEAR
#  define GL_NEAREST_MIPMAP_LINEAR        0x2702
#endif
#ifndef GL_LINEAR_MIPMAP_LINEAR
#  define GL_LINEAR_MIPMAP_LINEAR         0x2703
#endif
#ifndef GL_TEXTURE_ENV
#  define GL_TEXTURE_ENV                  0x2300
#endif
#ifndef GL_TEXTURE_ENV_MODE
#  define GL_TEXTURE_ENV_MODE             0x2200
#endif
#ifndef GL_TEXTURE_ENV_COLOR
#  define GL_TEXTURE_ENV_COLOR            0x2201
#endif
#ifndef GL_MODULATE
#  define GL_MODULATE                     0x2100
#endif
#ifndef GL_DECAL
#  define GL_DECAL                        0x2101
#endif
#ifndef GL_BLEND_COLOR
#  define GL_BLEND_COLOR                  0x8005
#endif
#ifndef GL_CLAMP
#  define GL_CLAMP                        0x2900
#endif
#ifndef GL_REPEAT
#  define GL_REPEAT                       0x2901
#endif
#ifndef GL_MAX_TEXTURE_SIZE
#  define GL_MAX_TEXTURE_SIZE             0x0D33
#endif
#ifndef GL_MAX_TEXTURE_UNITS
#  define GL_MAX_TEXTURE_UNITS            0x84E2
#endif
#ifndef GL_MAX_TEXTURE_COORDS
#  define GL_MAX_TEXTURE_COORDS           0x8871
#endif

/* Pixel formats */
#ifndef GL_COLOR_INDEX
#  define GL_COLOR_INDEX                  0x1900
#endif
#ifndef GL_STENCIL_INDEX
#  define GL_STENCIL_INDEX                0x1901
#endif
#ifndef GL_RED
#  define GL_RED                          0x1903
#endif
#ifndef GL_GREEN
#  define GL_GREEN                        0x1904
#endif
#ifndef GL_BLUE
#  define GL_BLUE                         0x1905
#endif
#ifndef GL_ALPHA
#  define GL_ALPHA                        0x1906
#endif
#ifndef GL_RGB
#  define GL_RGB                          0x1907
#endif
#ifndef GL_RGBA
#  define GL_RGBA                         0x1908
#endif
#ifndef GL_LUMINANCE
#  define GL_LUMINANCE                    0x1909
#endif
#ifndef GL_LUMINANCE_ALPHA
#  define GL_LUMINANCE_ALPHA              0x190A
#endif
#ifndef GL_BITMAP
#  define GL_BITMAP                       0x1A00
#endif

/* Internal pixel formats */
#ifndef GL_ALPHA4
#  define GL_ALPHA4                       0x803B
#endif
#ifndef GL_ALPHA8
#  define GL_ALPHA8                       0x803C
#endif
#ifndef GL_ALPHA12
#  define GL_ALPHA12                      0x803D
#endif
#ifndef GL_ALPHA16
#  define GL_ALPHA16                      0x803E
#endif
#ifndef GL_LUMINANCE4
#  define GL_LUMINANCE4                   0x803F
#endif
#ifndef GL_LUMINANCE8
#  define GL_LUMINANCE8                   0x8040
#endif
#ifndef GL_LUMINANCE12
#  define GL_LUMINANCE12                  0x8041
#endif
#ifndef GL_LUMINANCE16
#  define GL_LUMINANCE16                  0x8042
#endif
#ifndef GL_LUMINANCE4_ALPHA4
#  define GL_LUMINANCE4_ALPHA4            0x8043
#endif
#ifndef GL_LUMINANCE6_ALPHA2
#  define GL_LUMINANCE6_ALPHA2            0x8044
#endif
#ifndef GL_LUMINANCE8_ALPHA8
#  define GL_LUMINANCE8_ALPHA8            0x8045
#endif
#ifndef GL_LUMINANCE12_ALPHA4
#  define GL_LUMINANCE12_ALPHA4           0x8046
#endif
#ifndef GL_LUMINANCE12_ALPHA12
#  define GL_LUMINANCE12_ALPHA12          0x8047
#endif
#ifndef GL_LUMINANCE16_ALPHA16
#  define GL_LUMINANCE16_ALPHA16          0x8048
#endif
#ifndef GL_INTENSITY
#  define GL_INTENSITY                    0x8049
#endif
#ifndef GL_INTENSITY4
#  define GL_INTENSITY4                   0x804A
#endif
#ifndef GL_INTENSITY8
#  define GL_INTENSITY8                   0x804B
#endif
#ifndef GL_INTENSITY12
#  define GL_INTENSITY12                  0x804C
#endif
#ifndef GL_INTENSITY16
#  define GL_INTENSITY16                  0x804D
#endif
#ifndef GL_R3_G3_B2
#  define GL_R3_G3_B2                     0x2A10
#endif
#ifndef GL_RGB4
#  define GL_RGB4                         0x804F
#endif
#ifndef GL_RGB5
#  define GL_RGB5                         0x8050
#endif
#ifndef GL_RGB8
#  define GL_RGB8                         0x8051
#endif
#ifndef GL_RGB10
#  define GL_RGB10                        0x8052
#endif
#ifndef GL_RGB12
#  define GL_RGB12                        0x8053
#endif
#ifndef GL_RGB16
#  define GL_RGB16                        0x8054
#endif
#ifndef GL_RGBA2
#  define GL_RGBA2                        0x8055
#endif
#ifndef GL_RGBA4
#  define GL_RGBA4                        0x8056
#endif
#ifndef GL_RGB5_A1
#  define GL_RGB5_A1                      0x8057
#endif
#ifndef GL_RGBA8
#  define GL_RGBA8                        0x8058
#endif
#ifndef GL_RGB10_A2
#  define GL_RGB10_A2                     0x8059
#endif
#ifndef GL_RGBA12
#  define GL_RGBA12                       0x805A
#endif
#ifndef GL_RGBA16
#  define GL_RGBA16                       0x805B
#endif

/* Pixel pack/unpack */
#ifndef GL_UNPACK_SWAP_BYTES
#  define GL_UNPACK_SWAP_BYTES            0x0CF0
#endif
#ifndef GL_UNPACK_LSB_FIRST
#  define GL_UNPACK_LSB_FIRST             0x0CF1
#endif
#ifndef GL_UNPACK_ROW_LENGTH
#  define GL_UNPACK_ROW_LENGTH            0x0CF2
#endif
#ifndef GL_UNPACK_SKIP_ROWS
#  define GL_UNPACK_SKIP_ROWS             0x0CF3
#endif
#ifndef GL_UNPACK_SKIP_PIXELS
#  define GL_UNPACK_SKIP_PIXELS           0x0CF4
#endif
#ifndef GL_UNPACK_ALIGNMENT
#  define GL_UNPACK_ALIGNMENT             0x0CF5
#endif
#ifndef GL_PACK_SWAP_BYTES
#  define GL_PACK_SWAP_BYTES              0x0D00
#endif
#ifndef GL_PACK_LSB_FIRST
#  define GL_PACK_LSB_FIRST               0x0D01
#endif
#ifndef GL_PACK_ROW_LENGTH
#  define GL_PACK_ROW_LENGTH              0x0D02
#endif
#ifndef GL_PACK_SKIP_ROWS
#  define GL_PACK_SKIP_ROWS               0x0D03
#endif
#ifndef GL_PACK_SKIP_PIXELS
#  define GL_PACK_SKIP_PIXELS             0x0D04
#endif
#ifndef GL_PACK_ALIGNMENT
#  define GL_PACK_ALIGNMENT               0x0D05
#endif
#ifndef GL_MAP_COLOR
#  define GL_MAP_COLOR                    0x0D10
#endif
#ifndef GL_MAP_STENCIL
#  define GL_MAP_STENCIL                  0x0D11
#endif
#ifndef GL_INDEX_SHIFT
#  define GL_INDEX_SHIFT                  0x0D12
#endif
#ifndef GL_INDEX_OFFSET
#  define GL_INDEX_OFFSET                 0x0D13
#endif
#ifndef GL_RED_SCALE
#  define GL_RED_SCALE                    0x0D14
#endif
#ifndef GL_RED_BIAS
#  define GL_RED_BIAS                     0x0D15
#endif
#ifndef GL_ZOOM_X
#  define GL_ZOOM_X                       0x0D16
#endif
#ifndef GL_ZOOM_Y
#  define GL_ZOOM_Y                       0x0D17
#endif
#ifndef GL_GREEN_SCALE
#  define GL_GREEN_SCALE                  0x0D18
#endif
#ifndef GL_GREEN_BIAS
#  define GL_GREEN_BIAS                   0x0D19
#endif
#ifndef GL_BLUE_SCALE
#  define GL_BLUE_SCALE                   0x0D1A
#endif
#ifndef GL_BLUE_BIAS
#  define GL_BLUE_BIAS                    0x0D1B
#endif
#ifndef GL_ALPHA_SCALE
#  define GL_ALPHA_SCALE                  0x0D1C
#endif
#ifndef GL_ALPHA_BIAS
#  define GL_ALPHA_BIAS                   0x0D1D
#endif
#ifndef GL_DEPTH_SCALE
#  define GL_DEPTH_SCALE                  0x0D1E
#endif
#ifndef GL_DEPTH_BIAS
#  define GL_DEPTH_BIAS                   0x0D1F
#endif

/* Pixel maps */
#ifndef GL_PIXEL_MAP_I_TO_I
#  define GL_PIXEL_MAP_I_TO_I             0x0C70
#endif
#ifndef GL_PIXEL_MAP_S_TO_S
#  define GL_PIXEL_MAP_S_TO_S             0x0C71
#endif
#ifndef GL_PIXEL_MAP_I_TO_R
#  define GL_PIXEL_MAP_I_TO_R             0x0C72
#endif
#ifndef GL_PIXEL_MAP_I_TO_G
#  define GL_PIXEL_MAP_I_TO_G             0x0C73
#endif
#ifndef GL_PIXEL_MAP_I_TO_B
#  define GL_PIXEL_MAP_I_TO_B             0x0C74
#endif
#ifndef GL_PIXEL_MAP_I_TO_A
#  define GL_PIXEL_MAP_I_TO_A             0x0C75
#endif
#ifndef GL_PIXEL_MAP_R_TO_R
#  define GL_PIXEL_MAP_R_TO_R             0x0C76
#endif
#ifndef GL_PIXEL_MAP_G_TO_G
#  define GL_PIXEL_MAP_G_TO_G             0x0C77
#endif
#ifndef GL_PIXEL_MAP_B_TO_B
#  define GL_PIXEL_MAP_B_TO_B             0x0C78
#endif
#ifndef GL_PIXEL_MAP_A_TO_A
#  define GL_PIXEL_MAP_A_TO_A             0x0C79
#endif

/* Color bits */
#ifndef GL_RED_BITS
#  define GL_RED_BITS                     0x0D52
#endif
#ifndef GL_GREEN_BITS
#  define GL_GREEN_BITS                   0x0D53
#endif
#ifndef GL_BLUE_BITS
#  define GL_BLUE_BITS                    0x0D54
#endif
#ifndef GL_ALPHA_BITS
#  define GL_ALPHA_BITS                   0x0D55
#endif

/* Attribute stack bits */
#ifndef GL_CURRENT_BIT
#  define GL_CURRENT_BIT                  0x00000001
#endif
#ifndef GL_POINT_BIT
#  define GL_POINT_BIT                    0x00000002
#endif
#ifndef GL_LINE_BIT
#  define GL_LINE_BIT                     0x00000004
#endif
#ifndef GL_POLYGON_BIT
#  define GL_POLYGON_BIT                  0x00000008
#endif
#ifndef GL_POLYGON_STIPPLE_BIT
#  define GL_POLYGON_STIPPLE_BIT          0x00000010
#endif
#ifndef GL_PIXEL_MODE_BIT
#  define GL_PIXEL_MODE_BIT               0x00000020
#endif
#ifndef GL_LIGHTING_BIT
#  define GL_LIGHTING_BIT                 0x00000040
#endif
#ifndef GL_FOG_BIT
#  define GL_FOG_BIT                      0x00000080
#endif
#ifndef GL_DEPTH_BUFFER_ENABLE_BIT
#  define GL_DEPTH_BUFFER_ENABLE_BIT      0x00000100
#endif
#ifndef GL_ACCUM_BUFFER_ENABLE_BIT
#  define GL_ACCUM_BUFFER_ENABLE_BIT      0x00000200
#endif
#ifndef GL_STENCIL_BUFFER_BIT2
#  define GL_STENCIL_BUFFER_BIT2          0x00000400
#endif
#ifndef GL_VIEWPORT_BIT
#  define GL_VIEWPORT_BIT                 0x00000800
#endif
#ifndef GL_TRANSFORM_BIT
#  define GL_TRANSFORM_BIT                0x00001000
#endif
#ifndef GL_ENABLE_BIT
#  define GL_ENABLE_BIT                   0x00002000
#endif
#ifndef GL_ALL_ATTRIB_BITS
#  define GL_ALL_ATTRIB_BITS              0xFFFFFFFF
#endif
#ifndef GL_CLIENT_PIXEL_STORE_BIT
#  define GL_CLIENT_PIXEL_STORE_BIT       0x00000001
#endif
#ifndef GL_CLIENT_VERTEX_ARRAY_BIT
#  define GL_CLIENT_VERTEX_ARRAY_BIT      0x00000002
#endif
#ifndef GL_CLIENT_ALL_ATTRIB_BITS
#  define GL_CLIENT_ALL_ATTRIB_BITS       0xFFFFFFFF
#endif

/* Display lists */
#ifndef GL_COMPILE
#  define GL_COMPILE                      0x1300
#endif
#ifndef GL_COMPILE_AND_EXECUTE
#  define GL_COMPILE_AND_EXECUTE          0x1301
#endif
#ifndef GL_LIST_BASE
#  define GL_LIST_BASE                    0x0B32
#endif
#ifndef GL_LIST_INDEX
#  define GL_LIST_INDEX                   0x0B33
#endif
#ifndef GL_LIST_MODE
#  define GL_LIST_MODE                    0x0B30
#endif
#ifndef GL_MAX_LIST_NESTING
#  define GL_MAX_LIST_NESTING             0x0B31
#endif
#ifndef GL_MAX_EVAL_ORDER
#  define GL_MAX_EVAL_ORDER               0x0D30
#endif
#ifndef GL_MAX_ATTRIB_STACK_DEPTH
#  define GL_MAX_ATTRIB_STACK_DEPTH       0x0D35
#endif
#ifndef GL_MAX_CLIENT_ATTRIB_STACK_DEPTH
#  define GL_MAX_CLIENT_ATTRIB_STACK_DEPTH 0x0D3B
#endif
#ifndef GL_MAX_CLIP_PLANES
#  define GL_MAX_CLIP_PLANES              0x0D32
#endif
#ifndef GL_MAX_PROJECTION_STACK_DEPTH
#  define GL_MAX_PROJECTION_STACK_DEPTH   0x0D38
#endif
#ifndef GL_MAX_MODELVIEW_STACK_DEPTH
#  define GL_MAX_MODELVIEW_STACK_DEPTH    0x0D36
#endif
#ifndef GL_MAX_TEXTURE_STACK_DEPTH
#  define GL_MAX_TEXTURE_STACK_DEPTH      0x0D39
#endif
#ifndef GL_MAX_NAME_STACK_DEPTH
#  define GL_MAX_NAME_STACK_DEPTH         0x0D37
#endif

/* Clip planes */
#ifndef GL_CLIP_PLANE0
#  define GL_CLIP_PLANE0                  0x3000
#endif
#ifndef GL_CLIP_PLANE1
#  define GL_CLIP_PLANE1                  0x3001
#endif
#ifndef GL_CLIP_PLANE2
#  define GL_CLIP_PLANE2                  0x3002
#endif
#ifndef GL_CLIP_PLANE3
#  define GL_CLIP_PLANE3                  0x3003
#endif
#ifndef GL_CLIP_PLANE4
#  define GL_CLIP_PLANE4                  0x3004
#endif
#ifndef GL_CLIP_PLANE5
#  define GL_CLIP_PLANE5                  0x3005
#endif

/* Errors */
#ifndef GL_NO_ERROR
#  define GL_NO_ERROR                     0
#endif
#ifndef GL_INVALID_ENUM
#  define GL_INVALID_ENUM                 0x0500
#endif
#ifndef GL_INVALID_VALUE
#  define GL_INVALID_VALUE                0x0501
#endif
#ifndef GL_INVALID_OPERATION
#  define GL_INVALID_OPERATION            0x0502
#endif
#ifndef GL_STACK_OVERFLOW
#  define GL_STACK_OVERFLOW               0x0503
#endif
#ifndef GL_STACK_UNDERFLOW
#  define GL_STACK_UNDERFLOW              0x0504
#endif
#ifndef GL_OUT_OF_MEMORY
#  define GL_OUT_OF_MEMORY                0x0505
#endif

/* String names */
#ifndef GL_VENDOR
#  define GL_VENDOR                       0x1F00
#endif
#ifndef GL_RENDERER
#  define GL_RENDERER                     0x1F01
#endif
#ifndef GL_VERSION
#  define GL_VERSION                      0x1F02
#endif
#ifndef GL_EXTENSIONS
#  define GL_EXTENSIONS                   0x1F03
#endif

/* Feedback buffer type */
#ifndef GL_2D
#  define GL_2D                           0x0600
#endif
#ifndef GL_3D
#  define GL_3D                           0x0601
#endif
#ifndef GL_3D_COLOR
#  define GL_3D_COLOR                     0x0602
#endif
#ifndef GL_3D_COLOR_TEXTURE
#  define GL_3D_COLOR_TEXTURE             0x0603
#endif
#ifndef GL_4D_COLOR_TEXTURE
#  define GL_4D_COLOR_TEXTURE             0x0604
#endif

/* Texture coord name */
#ifndef GL_S
#  define GL_S                            0x2000
#endif
#ifndef GL_T
#  define GL_T                            0x2001
#endif
#ifndef GL_R
#  define GL_R                            0x2002
#endif
#ifndef GL_Q
#  define GL_Q                            0x2003
#endif

/* Vertex attributes */
#ifndef GL_CURRENT_COLOR
#  define GL_CURRENT_COLOR                0x0B00
#endif
#ifndef GL_CURRENT_INDEX
#  define GL_CURRENT_INDEX                0x0B01
#endif
#ifndef GL_CURRENT_NORMAL
#  define GL_CURRENT_NORMAL               0x0B02
#endif
#ifndef GL_CURRENT_TEXTURE_COORDS
#  define GL_CURRENT_TEXTURE_COORDS       0x0B03
#endif
#ifndef GL_CURRENT_RASTER_COLOR
#  define GL_CURRENT_RASTER_COLOR         0x0B04
#endif
#ifndef GL_CURRENT_RASTER_INDEX
#  define GL_CURRENT_RASTER_INDEX         0x0B05
#endif
#ifndef GL_CURRENT_RASTER_TEXTURE_COORDS
#  define GL_CURRENT_RASTER_TEXTURE_COORDS 0x0B06
#endif
#ifndef GL_CURRENT_RASTER_POSITION
#  define GL_CURRENT_RASTER_POSITION      0x0B07
#endif
#ifndef GL_CURRENT_RASTER_POSITION_VALID
#  define GL_CURRENT_RASTER_POSITION_VALID 0x0B08
#endif
#ifndef GL_CURRENT_RASTER_DISTANCE
#  define GL_CURRENT_RASTER_DISTANCE      0x0B09
#endif

/* Miscellaneous */
#ifndef GL_DITHER
#  define GL_DITHER                       0x0BD0
#endif
#ifndef GL_AUTO_NORMAL
#  define GL_AUTO_NORMAL                  0x0D80
#endif
#ifndef GL_GENERATE_MIPMAP
#  define GL_GENERATE_MIPMAP              0x8191
#endif

/* Texture generation */
#ifndef GL_TEXTURE_GEN_S
#  define GL_TEXTURE_GEN_S                0x0C60
#endif
#ifndef GL_TEXTURE_GEN_T
#  define GL_TEXTURE_GEN_T                0x0C61
#endif
#ifndef GL_TEXTURE_GEN_R
#  define GL_TEXTURE_GEN_R                0x0C62
#endif
#ifndef GL_TEXTURE_GEN_Q
#  define GL_TEXTURE_GEN_Q                0x0C63
#endif
#ifndef GL_TEXTURE_GEN_MODE
#  define GL_TEXTURE_GEN_MODE             0x2500
#endif
#ifndef GL_OBJECT_PLANE
#  define GL_OBJECT_PLANE                 0x2501
#endif
#ifndef GL_EYE_PLANE
#  define GL_EYE_PLANE                    0x2502
#endif
#ifndef GL_OBJECT_LINEAR
#  define GL_OBJECT_LINEAR                0x2401
#endif
#ifndef GL_EYE_LINEAR
#  define GL_EYE_LINEAR                   0x2400
#endif
#ifndef GL_SPHERE_MAP
#  define GL_SPHERE_MAP                   0x2402
#endif

/* Attribute stack bits (additional) */
#ifndef GL_TEXTURE_BIT
#  define GL_TEXTURE_BIT                  0x00040000
#endif
#ifndef GL_RGBA_MODE
#  define GL_RGBA_MODE                    0x0C31
#endif
#ifndef GL_INDEX_MODE
#  define GL_INDEX_MODE                   0x0C30
#endif

/* -------------------------------------------------------------------------
 * Inline no-op stubs for direct GL query functions that appear in source
 * files compiled in no-GL builds.  All return the safest possible value.
 *
 * NOTE: These are defined with C linkage and as static inline so that
 * including this header in multiple translation units does not produce
 * multiple-definition errors.
 * --------------------------------------------------------------------- */

#ifdef __cplusplus
extern "C" {
#endif

/* glGetError() – return GL_NO_ERROR; no actual GL call */
static inline GLenum obol_stub_glGetError(void) { return GL_NO_ERROR; }
#ifndef glGetError
#  define glGetError() obol_stub_glGetError()
#endif

/* glIsEnabled() – return GL_FALSE; no actual GL call */
static inline GLboolean obol_stub_glIsEnabled(GLenum cap)
{ (void)cap; return GL_FALSE; }
#ifndef glIsEnabled
#  define glIsEnabled(cap) obol_stub_glIsEnabled(cap)
#endif

/* glGetIntegerv() – write zero; no actual GL call */
static inline void obol_stub_glGetIntegerv(GLenum pname, GLint * params)
{ (void)pname; if (params) *params = 0; }
#ifndef glGetIntegerv
#  define glGetIntegerv(pname, params) obol_stub_glGetIntegerv(pname, params)
#endif

/* glGetFloatv() – write zeros; no actual GL call */
static inline void obol_stub_glGetFloatv(GLenum pname, GLfloat * params)
{ (void)pname; if (params) *params = 0.0f; }
#ifndef glGetFloatv
#  define glGetFloatv(pname, params) obol_stub_glGetFloatv(pname, params)
#endif

/* glGetString() – return empty string; no actual GL call */
static inline const GLubyte * obol_stub_glGetString(GLenum name)
{ (void)name; return (const GLubyte *)""; }
#ifndef glGetString
#  define glGetString(name) obol_stub_glGetString(name)
#endif

/* glGetStringi() – return empty string; no actual GL call */
static inline const GLubyte * obol_stub_glGetStringi(GLenum name, GLuint index)
{ (void)name; (void)index; return (const GLubyte *)""; }
#ifndef glGetStringi
#  define glGetStringi(name, index) obol_stub_glGetStringi(name, index)
#endif

/* glFinish() – no-op; no actual GL call */
static inline void obol_stub_glFinish(void) {}
#ifndef glFinish
#  define glFinish() obol_stub_glFinish()
#endif

/* glFlush() – no-op; no actual GL call */
static inline void obol_stub_glFlush(void) {}
#ifndef glFlush
#  define glFlush() obol_stub_glFlush()
#endif

/* glShadeModel() – no-op; no actual GL call */
static inline void obol_stub_glShadeModel(GLenum mode) { (void)mode; }
#ifndef glShadeModel
#  define glShadeModel(mode) obol_stub_glShadeModel(mode)
#endif

/* glGetTexLevelParameteriv() – write zero; no actual GL call */
static inline void obol_stub_glGetTexLevelParameteriv(GLenum target, GLint level,
                                                       GLenum pname, GLint * params)
{ (void)target; (void)level; (void)pname; if (params) *params = 0; }
#ifndef glGetTexLevelParameteriv
#  define glGetTexLevelParameteriv(target, level, pname, params) \
     obol_stub_glGetTexLevelParameteriv(target, level, pname, params)
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

/* Define APIENTRY if not already present (normally from windows.h or gl.h) */
#ifndef APIENTRY
#  define APIENTRY
#endif

/* Define GL_GLEXT_PROTOTYPES guard so extension headers, if accidentally
   included, do not attempt to declare real GL extension function prototypes
   against this stub. */
#ifndef GL_GLEXT_PROTOTYPES
#  define GL_GLEXT_PROTOTYPES 0
#endif

#endif /* OBOL_GL_STUB_H */
