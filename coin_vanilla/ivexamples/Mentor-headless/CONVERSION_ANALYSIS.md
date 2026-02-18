# Mentor Examples Conversion Analysis

This document categorizes all Inventor Mentor examples for conversion to headless rendering. The goal is to identify which examples can be converted to produce reference images for testing Coin's core scene graph functionality.

## Conversion Categories

### ✅ Ready to Convert - Basic Scene Graph
These examples use only core scene graph elements and can be directly converted with minimal changes.

### ⚠️  Needs Interaction Simulation
These examples require user interaction (mouse, keyboard) which must be simulated procedurally.

### ❌ Skip - VRML/XML Specific
Examples that rely on VRML97 or XML features should be skipped as they're being removed in the Coin rework.

### ❌ Skip - GUI Framework Specific
Examples that are tightly coupled to specific GUI toolkits (Motif/Xt widgets) and can't be reasonably converted.

## Detailed Analysis

### Chapter 2: Introduction to Inventor
| Example | Status | Notes |
|---------|--------|-------|
| 02.1.HelloCone | ✅ Done | Simple cone rendering |
| 02.2.EngineSpin | ✅ Done | Animation via rotation - render multiple frames |
| 02.3.Trackball | ⚠️  Convert | Trackball manipulator - simulate rotation sequence |
| 02.4.Examiner | ⚠️  Convert | Examiner viewer - render from predefined viewpoints |

### Chapter 3: Nodes and Groups
| Example | Status | Notes |
|---------|--------|-------|
| 03.1.Molecule | ✅ Done | Water molecule |
| 03.2.Robot | ✅ Done | Robot with shared geometry |
| 03.3.Naming | ✅ Ready | Named nodes - straightforward conversion |

### Chapter 4: Cameras and Lights
| Example | Status | Notes |
|---------|--------|-------|
| 04.1.Cameras | ✅ Ready | Multiple cameras - render from each camera |
| 04.2.Lights | ✅ Ready | Multiple light types - render variations |

### Chapter 5: Shapes and Properties  
| Example | Status | Notes |
|---------|--------|-------|
| 05.1.FaceSet | ✅ Ready | Face set geometry |
| 05.2.IndexedFaceSet | ✅ Ready | Indexed face set |
| 05.3.TriangleStripSet | ✅ Ready | Triangle strips |
| 05.4.QuadMesh | ✅ Ready | Quad mesh |
| 05.5.Binding | ✅ Ready | Material/normal binding |
| 05.6.TransformOrdering | ✅ Ready | Transform ordering effects |

### Chapter 6: Text
| Example | Status | Notes |
|---------|--------|-------|
| 06.1.Text | ✅ Ready | 2D text |
| 06.2.Simple3DText | ✅ Ready | Simple 3D text |
| 06.3.Complex3DText | ✅ Ready | Complex 3D text with justification |

### Chapter 7: Textures
| Example | Status | Notes |
|---------|--------|-------|
| 07.1.BasicTexture | ✅ Ready | Basic texture mapping |
| 07.2.TextureCoordinates | ✅ Ready | Explicit texture coordinates |
| 07.3.TextureFunction | ✅ Ready | Texture functions |

### Chapter 8: Curves and Surfaces
| Example | Status | Notes |
|---------|--------|-------|
| 08.1.BSCurve | ✅ Ready | B-spline curve |
| 08.2.UniCurve | ✅ Ready | Uniform B-spline |
| 08.3.BezSurf | ✅ Ready | Bezier surface |
| 08.4.TrimSurf | ✅ Ready | Trimmed NURBS surface |

### Chapter 9: Applying Actions
| Example | Status | Notes |
|---------|--------|-------|
| 09.1.Print | ✅ Ready | Print action - output to console |
| 09.2.Texture | ✅ Ready | Offscreen rendering to texture (already uses SoOffscreenRenderer!) |
| 09.3.Search | ✅ Ready | Search action - verify nodes found |
| 09.4.PickAction | ⚠️  Convert | Pick action - simulate pick at specific coordinates |
| 09.5.GenSph | ✅ Ready | Generate sphere callback |

### Chapter 10: Handling Events and Selection
| Example | Status | Notes |
|---------|--------|-------|
| 10.1.addEventCB | ✅ Enhanced | Event callbacks - uses simulateKeyPress from manipulator pattern |
| 10.2.setEventCB | ❌ Skip | Xt-specific event handling |
| 10.3and4.MotifList | ❌ Skip | Motif-specific widget |
| 10.5.SelectionCB | ✅ Enhanced | Selection callbacks - documented event vs programmatic approaches |
| 10.6.PickFilterTopLevel | ✅ Done | Pick filtering - programmatic picks |
| 10.7.PickFilterManip | ✅ Done | Manipulator pick filtering |
| 10.8.PickFilterNodeKit | ❌ Skip | NodeKit-specific, Xt-dependent |

### Chapter 11: File I/O
| Example | Status | Notes |
|---------|--------|-------|
| 11.1.ReadFile | ✅ Ready | Read .iv files - use embedded scenes |
| 11.2.ReadString | ✅ Ready | Parse from string buffer |

### Chapter 12: Sensors
| Example | Status | Notes |
|---------|--------|-------|
| 12.1.FieldSensor | ✅ Ready | Field sensors - programmatically trigger |
| 12.2.NodeSensor | ✅ Ready | Node sensors - modify and capture |
| 12.3.AlarmSensor | ✅ Ready | Alarm sensor - set specific times |
| 12.4.TimerSensor | ✅ Ready | Timer sensor - capture at intervals |

### Chapter 13: Engines
| Example | Status | Notes |
|---------|--------|-------|
| 13.1.GlobalFlds | ✅ Ready | Global fields |
| 13.2.ElapsedTime | ✅ Ready | Elapsed time engine - set explicit times |
| 13.3.TimeCounter | ✅ Ready | Time counter - render sequence |
| 13.4.Gate | ✅ Ready | Gate engine |
| 13.5.Boolean | ✅ Ready | Boolean engine |
| 13.6.Calculator | ✅ Ready | Calculator engine |
| 13.7.Rotor | ✅ Ready | Rotor engine - render rotation sequence |
| 13.8.Blinker | ✅ Ready | Blinker engine - capture on/off states |

### Chapter 14: Node Kits
| Example | Status | Notes |
|---------|--------|-------|
| 14.1.FrolickingWords | ⚠️  Convert | Animated text - render frame sequence |
| 14.2.Editors | ❌ Skip | Widget-specific editors |
| 14.3.Balance | ✅ Ready | Balance using node kits |

### Chapter 15: Draggers and Manipulators
| Example | Status | Notes |
|---------|--------|-------|
| 15.1.ConeRadius | ✅ Done | Dragger controlling cone via engine - programmatic value setting |
| 15.2.SliderBox | ⚠️  Convert | Slider manipulator - render at positions |
| 15.3.AttachManip | ✅ Done | Attach/detach manipulators - show different manipulator types |
| 15.4.Customize | ⚠️  Convert | Custom manipulator - render variations |

### Chapter 16: Examiner Viewer
| Example | Status | Notes |
|---------|--------|-------|
| 16.1.Overlay | ❌ Skip | Xt-specific overlay planes |
| 16.2.Callback | ❌ Skip | Widget callbacks |
| 16.3.AttachEditor | ❌ Skip | Widget editors |
| 16.4.OneWindow | ❌ Skip | Xt window management |
| 16.5.Examiner | ⚠️  Convert | Examiner customization - render viewpoints |

### Chapter 17: Using Inventor with OpenGL
| Example | Status | Notes |
|---------|--------|-------|
| 17.1.ColorIndex | ❌ Skip | Xt color management |
| 17.2.GLCallback | ⚠️  Convert | GL callbacks - render with custom GL |
| 17.3.GLFloor | ❌ Skip | Xt-specific |

## Conversion Summary

### Priority 1 - Core Scene Graph (38 examples)
- Chapters 3-9: Basic geometry, materials, lights, textures, NURBS
- Should be converted first as they test fundamental Coin functionality

### Priority 2 - Engines and Sensors (12 examples)
- Chapters 12-13: Time-based animations and field connections
- Important for testing dynamic scene graph behavior

### Priority 3 - Advanced Features (10 examples)
- Chapters 14-15: NodeKits, Manipulators
- May require more complex interaction simulation

### Skip (16 examples)
- GUI toolkit specific (Motif/Xt/Widget-based)
- Cannot be reasonably converted without significant rework

## Conversion Patterns

### Pattern 1: Static Geometry
```cpp
// Simple conversion: add camera/light, render once
root->addChild(camera);
root->addChild(light);
root->addChild(sceneContent);
renderToFile(root, "output.rgb");
```

### Pattern 2: Animation/Time-based
```cpp
// Render multiple frames at different times
for (int frame = 0; frame < numFrames; frame++) {
    float time = frame * timeStep;
    timeEngine->setValue(time);
    renderToFile(root, filename);
}
```

### Pattern 3: Multiple Viewpoints
```cpp
// Render from different camera positions
for each viewpoint {
    camera->position.setValue(viewpoint);
    camera->orientation.setValue(orientation);
    renderToFile(root, filename);
}
```

### Pattern 4: Interaction Simulation
```cpp
// Simulate user interactions
manipulator->setValue(position1);
renderToFile(root, "before.rgb");
manipulator->setValue(position2);
renderToFile(root, "after.rgb");
```

## Implementation Roadmap

1. **Phase 1** (Completed): Framework + 4 basic examples
   - headless_utils.h framework
   - HelloCone, EngineSpin, Molecule, Robot

2. **Phase 2**: Core scene graph (Chapters 3-9)
   - Geometry examples (5.x)
   - Material and texture examples (6.x, 7.x)
   - NURBS examples (8.x)
   - Action examples (9.x)

3. **Phase 3**: Engines and sensors (Chapters 12-13)
   - Time-based animations
   - Engine connections

4. **Phase 4**: Advanced features
   - Manipulators with simulated interaction
   - NodeKits

## Testing Strategy

For each converted example:
1. Build and run successfully
2. Generate reference images
3. Verify images show expected content (visual inspection)
4. Document any limitations or differences from original

Future automation:
- Image comparison for regression testing
- Automated test suite running all examples
- CI/CD integration
