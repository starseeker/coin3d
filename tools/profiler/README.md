# Profiler asset tools

Configure Obol with `-DOBOL_BUILD_TOOLS=ON` to build `inline_texture`. The
program reads an Inventor scene from standard input, resolves every
`SoTexture2::filename`, and writes the scene with embedded image data.

`inventify_texturenames.sh OUTPUT.cpp INPUT.svg...` uses that executable and
ImageMagick to generate C++ string arrays for profiler visualization assets.
Set `INLINE_TEXTURE=/path/to/inline_texture` when the build-tree `bin`
directory is not on `PATH`.
