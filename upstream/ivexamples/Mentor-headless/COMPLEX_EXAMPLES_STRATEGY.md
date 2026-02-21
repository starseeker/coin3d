# Complex Examples Conversion Strategy

This document provides detailed implementation strategies for converting the remaining complex Mentor examples that require interaction simulation.

## Overview

After completing all straightforward examples (41/66 = 62%), the remaining examples fall into these categories:
- **Viewer/Camera simulation** (Medium) - 2 examples
- **Pick/Selection simulation** (Medium-High) - 4 examples
- **Event simulation** (High) - 1 example
- **Manipulator simulation** (Very High) - 4 examples
- **GUI-specific** (Skip) - 14 examples

Total convertible remaining: ~11 examples

---

## 1. Viewer/Camera Simulation (Medium Complexity)

### Examples
- 02.3.Trackball
- 02.4.Examiner

### Strategy

These examples demonstrate camera manipulation that would normally be done via mouse dragging. We can simulate this by generating camera paths programmatically.

### Implementation Approach

```cpp
// Trackball simulation - orbital camera movement
void renderTrackballSequence(SoSeparator *root, SoPerspectiveCamera *camera) {
    const float radius = 10.0f;
    const float height = 2.0f;
    const int numFrames = 16;
    
    for (int i = 0; i < numFrames; i++) {
        float angle = (2.0 * M_PI * i) / numFrames;
        
        // Orbital position
        camera->position.setValue(
            radius * cos(angle),
            height,
            radius * sin(angle)
        );
        
        // Always point at center
        camera->pointAt(SbVec3f(0, 0, 0));
        
        renderToFile(root, filename);
    }
}

// Examiner simulation - tumble, dolly, pan
void renderExaminerSequence(SoSeparator *root, SoPerspectiveCamera *camera) {
    // Initial position
    camera->viewAll(root, viewport);
    renderToFile(root, "examiner_initial.rgb");
    
    // Tumble around (rotate camera)
    for (int i = 0; i < 8; i++) {
        float angle = M_PI / 4 * i;
        // Rotate camera around Y axis
        rotateCamera(camera, angle, 0);
        renderToFile(root, filename);
    }
    
    // Dolly in/out (zoom)
    SbVec3f origPos = camera->position.getValue();
    for (float scale = 0.5f; scale <= 2.0f; scale += 0.5f) {
        camera->position.setValue(origPos * scale);
        renderToFile(root, filename);
    }
}
```

### Utilities Needed

```cpp
// headless_camera_utils.h
void orbitCamera(SoCamera *camera, const SbVec3f &center, 
                 float radius, float azimuth, float elevation);
void rotateCamera(SoCamera *camera, float yaw, float pitch);
void dollyCameraToward(SoCamera *camera, const SbVec3f &target, float factor);
void panCamera(SoCamera *camera, float dx, float dy);
```

### Estimated Effort
- **2-3 hours per example**
- **Priority: Medium** - Good for demonstrating camera control

---

## 2. Pick/Selection Simulation (Medium-High Complexity)

### Examples
- 09.4.PickAction
- 10.5.SelectionCB  
- 10.6.PickFilterTopLevel
- 10.7.PickFilterManip

### Strategy

These examples use picking to select objects in the scene. We can simulate this by:
1. Calculating world space positions of objects
2. Converting to screen coordinates
3. Performing pick actions at those coordinates
4. Rendering before/after selection states

### Implementation Approach

```cpp
// Pick action simulation
void simulatePickAtObject(SoSeparator *root, SoPath *objectPath, 
                          SoPerspectiveCamera *camera, 
                          const SbViewportRegion &viewport) {
    // Get object's world position
    SoGetBoundingBoxAction bboxAction(viewport);
    bboxAction.apply(objectPath);
    SbBox3f bbox = bboxAction.getBoundingBox();
    SbVec3f center = bbox.getCenter();
    
    // Convert world to screen coordinates
    SbVec2s screenPos = worldToScreen(center, camera, viewport);
    
    // Perform pick action
    SoRayPickAction pickAction(viewport);
    pickAction.setPoint(screenPos);
    pickAction.apply(root);
    
    // Get picked path
    SoPickedPoint *pickedPoint = pickAction.getPickedPoint();
    if (pickedPoint != NULL) {
        SoPath *pickedPath = pickedPoint->getPath();
        printf("Picked: %s\n", pickedPath->getTail()->getName().getString());
        
        // Highlight the picked object
        highlightPath(pickedPath);
        renderToFile(root, "picked.rgb");
    }
}

// Selection callback simulation
void simulateSelection(SoSeparator *root, SoSelection *selection) {
    // Find all selectable nodes (e.g., all cubes)
    SoSearchAction search;
    search.setType(SoCube::getClassTypeId());  // Note: May be getTypeId() in some Coin versions
    search.setInterest(SoSearchAction::ALL);
    search.apply(root);
    
    SoPathList &paths = search.getPaths();
    
    // Render unselected state
    renderToFile(root, "unselected.rgb");
    
    // Select each object and render
    for (int i = 0; i < paths.getLength(); i++) {
        selection->select(paths[i]);
        renderToFile(root, filename);
        selection->deselect(paths[i]);
    }
}
```

### Pick Filtering Example

```cpp
// Custom pick filter callback - only allows top-level objects
static SoPath *
topLevelPickFilter(void *, const SoPickedPoint *pick) {
    if (pick == NULL) return NULL;
    
    SoPath *path = pick->getPath();
    // Return path with only root and first child (top level)
    if (path->getLength() > 2) {
        path = path->copy(0, 2);  // Keep only first 2 nodes
    }
    return path;
}

// 10.6.PickFilterTopLevel - only pick top-level objects
void simulatePickFiltering(SoSeparator *root) {
    // Set up pick filter callback
    SoSelection *selection = new SoSelection;
    selection->policy = SoSelection::SINGLE;
    
    // Attach custom pick filter
    selection->setPickFilterCallback(topLevelPickFilter, NULL);
    
    // Attempt to pick nested objects
    // Only top-level should be selectable
    for each object in scene {
        attemptPick(object);
        renderToFile(root, filename);
    }
}
```

### Utilities Needed

```cpp
// headless_pick_utils.h
SbVec2s worldToScreen(const SbVec3f &worldPos, SoCamera *camera,
                      const SbViewportRegion &viewport);
SbVec3f getObjectCenter(SoPath *path);
void performPick(SoSeparator *root, const SbVec2s &screenPos,
                 const SbViewportRegion &viewport);
void highlightPath(SoPath *path);  // Add emissive material
```

### Estimated Effort
- **3-4 hours per example**
- **Priority: High** - Important for testing selection functionality

---

## 3. Event Simulation (High Complexity)

### Examples
- 10.1.addEventCB

### Strategy

This example uses keyboard events to control the scene. We simulate by:
1. Creating SoEvent objects programmatically
2. Setting up SoHandleEventAction
3. Dispatching events to event callbacks
4. Capturing state changes

### Implementation Approach

```cpp
// Event simulation
void simulateKeyboardEvents(SoSeparator *root, SoEventCallback *eventCB) {
    SbViewportRegion viewport(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    
    // Render initial state
    renderToFile(root, "initial.rgb");
    
    // Simulate arrow key presses
    const SoKeyboardEvent::Key keys[] = {
        SoKeyboardEvent::UP_ARROW,
        SoKeyboardEvent::DOWN_ARROW,
        SoKeyboardEvent::LEFT_ARROW,
        SoKeyboardEvent::RIGHT_ARROW
    };
    
    for (int i = 0; i < 4; i++) {
        // Create keyboard event
        SoKeyboardEvent keyEvent;
        keyEvent.setKey(keys[i]);
        keyEvent.setState(SoButtonEvent::DOWN);
        keyEvent.setTime(SbTime::getTimeOfDay());
        
        // Create and apply event action
        SoHandleEventAction eventAction(viewport);
        eventAction.setEvent(&keyEvent);
        eventAction.apply(root);
        
        // Process any sensor callbacks triggered by event
        SoDB::getSensorManager()->processTimerQueue();
        SoDB::getSensorManager()->processDelayQueue(TRUE);
        
        // Render result
        printf("Processed key: %d\n", keys[i]);
        renderToFile(root, filename);
        
        // Simulate key release
        keyEvent.setState(SoButtonEvent::UP);
        eventAction.setEvent(&keyEvent);
        eventAction.apply(root);
    }
}

// Mouse event simulation
void simulateMouseDrag(SoSeparator *root, 
                       const SbVec2s &start, const SbVec2s &end) {
    SbViewportRegion viewport(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    SoHandleEventAction eventAction(viewport);
    
    // Mouse button down
    SoMouseButtonEvent mouseDown;
    mouseDown.setButton(SoMouseButtonEvent::BUTTON1);
    mouseDown.setState(SoButtonEvent::DOWN);
    mouseDown.setPosition(start);
    eventAction.setEvent(&mouseDown);
    eventAction.apply(root);
    
    // Mouse motion events (interpolate)
    int steps = 10;
    for (int i = 0; i <= steps; i++) {
        float t = float(i) / steps;
        SbVec2s pos(
            start[0] + t * (end[0] - start[0]),
            start[1] + t * (end[1] - start[1])
        );
        
        SoLocation2Event mouseMove;
        mouseMove.setPosition(pos);
        eventAction.setEvent(&mouseMove);
        eventAction.apply(root);
        
        renderToFile(root, filename);
    }
    
    // Mouse button up
    SoMouseButtonEvent mouseUp;
    mouseUp.setButton(SoMouseButtonEvent::BUTTON1);
    mouseUp.setState(SoButtonEvent::UP);
    mouseUp.setPosition(end);
    eventAction.setEvent(&mouseUp);
    eventAction.apply(root);
}
```

### Utilities Needed

```cpp
// headless_event_utils.h
void createKeyEvent(SoKeyboardEvent &event, SoKeyboardEvent::Key key, 
                    SoButtonEvent::State state);
void createMouseButtonEvent(SoMouseButtonEvent &event, 
                           SoMouseButtonEvent::Button button,
                           SoButtonEvent::State state, const SbVec2s &pos);
void createMouseMoveEvent(SoLocation2Event &event, const SbVec2s &pos);
void dispatchEvent(SoSeparator *root, SoEvent *event, 
                   const SbViewportRegion &viewport);
```

### Estimated Effort
- **4-6 hours**
- **Priority: Low** - Complex for limited benefit
- **Alternative:** Document that event handling can't be fully tested headless

---

## 4. Manipulator/Dragger Simulation (Very High Complexity)

### Examples
- 15.1.ConeRadius
- 15.2.SliderBox
- 15.3.AttachManip
- 15.4.Customize

### Strategy

Manipulators are designed for interactive use. Two approaches:

#### Approach A: Simplified (Recommended)
Directly set manipulator values instead of simulating interaction:

```cpp
// Simplified manipulator testing
void testManipulatorValues(SoSeparator *root, SoTransformBoxManip *manip) {
    // Render initial state
    renderToFile(root, "manip_initial.rgb");
    
    // Test different manipulator values
    SoTransform *transform = new SoTransform;
    
    // Scale variations
    for (float scale = 0.5f; scale <= 2.0f; scale += 0.5f) {
        transform->scaleFactor.setValue(scale, scale, scale);
        manip->replaceNode(transform);
        renderToFile(root, filename);
    }
    
    // Rotation variations
    for (float angle = 0; angle < 2*M_PI; angle += M_PI/4) {
        transform->rotation.setValue(SbVec3f(0, 1, 0), angle);
        manip->replaceNode(transform);
        renderToFile(root, filename);
    }
}
```

#### Approach B: Full Simulation (Not Recommended)
Simulate mouse drags on manipulator parts:

```cpp
// Complex manipulator simulation (HIGH EFFORT)
void simulateManipulatorDrag(SoTransformBoxManip *manip) {
    // 1. Identify manipulator parts (corner handles, face handles, etc.)
    // 2. Calculate screen positions of handles
    // 3. Simulate mouse down on handle
    // 4. Simulate drag motion
    // 5. Process manipulator internal logic
    // 6. Simulate mouse up
    // This requires deep understanding of manipulator internals!
}
```

### Recommendation

**Use Approach A (Simplified)**:
- ✅ Tests that manipulators can be attached/detached
- ✅ Tests that manipulator values affect the scene
- ✅ Much simpler implementation
- ❌ Doesn't test interactive drag behavior

### Estimated Effort
- **Approach A: 2-3 hours per example**
- **Approach B: 8-12 hours per example** (not recommended)
- **Priority: Low** - Very high effort for limited benefit

---

## Recommended Implementation Order

Based on complexity and value:

### Phase 1: Complete Easy Example ✅
1. **09.2.Texture** (offscreen rendering) - DONE

### Phase 2: Medium Complexity (High Value)
2. **09.4.PickAction** - Basic pick simulation
3. **02.3.Trackball** - Camera orbital motion
4. **02.4.Examiner** - Camera tumble/dolly/pan

### Phase 3: Medium-High Complexity (Medium Value)
5. **10.5.SelectionCB** - Selection callbacks
6. **10.6.PickFilterTopLevel** - Pick filtering
7. **10.7.PickFilterManip** - Manipulator pick filtering

### Phase 4: Consider If Time Permits
8. **10.1.addEventCB** - Event simulation (complex)
9. **15.1-15.4** - Manipulators (simplified approach only)

### Skip Entirely
- All Xt/Motif/Widget-specific examples (14 examples)

---

## Utility Libraries to Create

### 1. `headless_camera_utils.h`
Camera manipulation utilities for viewer simulation.

### 2. `headless_pick_utils.h`
Pick action utilities for selection simulation.

### 3. `headless_event_utils.h` (optional)
Event creation and dispatch utilities.

---

## Testing Strategy

For each complex example:
1. **Verify functionality** - Does it demonstrate the key concept?
2. **Visual inspection** - Do rendered frames show expected behavior?
3. **Compare with original** - Does it match original example intent?
4. **Document limitations** - What aspects aren't tested?

---

## Conclusion

**Summary:**
- **41 examples complete** (62%)
- **11 more convertible** with varying effort
- **14 examples to skip** (GUI-specific)

**Final achievable target: 52/66 examples (79%)**

The remaining examples require progressively more complex infrastructure. The recommended approach is to:
1. Complete viewer simulation examples (medium effort, good value)
2. Complete pick/selection examples (higher effort, good value)
3. Consider event/manipulator examples only if comprehensive testing is needed

The framework is solid and can be extended as needed for these advanced examples.
