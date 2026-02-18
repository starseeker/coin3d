# Testing Headless Examples

## Requirements

The headless examples require a working OpenGL/GLX context for offscreen rendering.

### Option 1: Using Xvfb (X Virtual Framebuffer)

Xvfb provides a virtual X server with GLX support that works well with software rendering (llvmpipe):

```bash
# Install dependencies
sudo apt-get install xvfb mesa-utils libgl1-mesa-dri

# Start Xvfb
Xvfb :99 -screen 0 1024x768x24 -ac +extension GLX +render -noreset &

# Set display environment variable
export DISPLAY=:99

# Enable software rendering (recommended for virtual environments)
export LIBGL_ALWAYS_SOFTWARE=1

# Verify GLX is working
glxinfo | grep "OpenGL renderer"
# Should show: OpenGL renderer string: llvmpipe (LLVM ...)

# Run examples
cd build/output
../bin/02.1.HelloCone
```

### Option 2: Using xvfb-run

For one-off testing, use xvfb-run:

```bash
xvfb-run -a -s "-screen 0 1024x768x24" ./your_example
```

### Option 3: Using native X server

If you have a physical X server running:

```bash
export DISPLAY=:0
./your_example
```

## Building

```bash
cd ivexamples/Mentor-headless
mkdir build && cd build
cmake ..
make
```

## Running

```bash
# With Xvfb already running on :99
export DISPLAY=:99
export LIBGL_ALWAYS_SOFTWARE=1

cd output

# Run individual examples
../bin/02.1.HelloCone
../bin/02.3.Trackball
../bin/09.4.PickAction
# etc.
```

## Troubleshooting

### "Couldn't create GLX context" error

This usually means:
1. No X server is running (start Xvfb)
2. DISPLAY is not set (export DISPLAY=:99)
3. GLX extension is missing (check with `glxinfo`)

### "Failed to render scene" error

After GLX context creation succeeds, rendering failures typically indicate:
1. Missing Mesa/OpenGL drivers
2. Scene graph issues
3. Camera setup problems

Check the example code to ensure the scene has:
- A camera (SoPerspectiveCamera or SoOrthographicCamera)
- A light source (SoDirectionalLight, etc.)
- Geometry to render

## Notes

- Examples generate SGI RGB format images (.rgb files)
- Convert to PNG if needed: `convert image.rgb image.png` (requires ImageMagick)
- Software rendering (llvmpipe) is slower but works reliably in headless environments
- Each example outputs status messages to stdout showing what was rendered
