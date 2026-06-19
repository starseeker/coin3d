#ifndef OBOL_SCENE_SCENE_H
#define OBOL_SCENE_SCENE_H

/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
\**************************************************************************/

#include <Obol/base/Export.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace obol {

class CadAssembly;

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Color {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

struct Transform {
    Vec3 translation;
    Vec3 rotationAxis = {0.0f, 0.0f, 1.0f};
    float rotationRadians = 0.0f;
    Vec3 scale = {1.0f, 1.0f, 1.0f};
};

struct Matrix4 {
    // Column-major storage: values[column * 4 + row].
    std::array<float, 16> values = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f};
};

enum class ImageFormat {
    Luminance = 1,
    LuminanceAlpha = 2,
    RGB = 3,
    RGBA = 4
};

struct Image2D {
    unsigned int width = 0;
    unsigned int height = 0;
    ImageFormat format = ImageFormat::RGBA;
    std::vector<unsigned char> pixels;
};

enum class TextureWrap {
    Repeat,
    Clamp
};

enum class TextureModel {
    Modulate,
    Decal,
    Blend,
    Replace
};

struct Texture2D {
    Image2D image;
    TextureWrap wrapS = TextureWrap::Repeat;
    TextureWrap wrapT = TextureWrap::Repeat;
    TextureModel model = TextureModel::Modulate;
    Color blendColor = {0.0f, 0.0f, 0.0f, 1.0f};
};

struct Material {
    Color baseColor = {0.8f, 0.8f, 0.8f, 1.0f};
    Color specular = {0.0f, 0.0f, 0.0f, 1.0f};
    Color emissive = {0.0f, 0.0f, 0.0f, 1.0f};
    float shininess = 0.2f;
    std::shared_ptr<Texture2D> baseColorTexture;
    bool unlit = false;
};

enum class MeshTopology {
    Triangles,
    Polygons,
    TriangleStrips,
    QuadGrid
};

struct Mesh {
    MeshTopology topology = MeshTopology::Triangles;
    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<Vec2> texCoords;
    std::vector<uint32_t> indices;
    std::vector<uint32_t> texCoordIndices;
    std::vector<uint32_t> faceVertexCounts;
    std::vector<uint32_t> stripVertexCounts;
    uint32_t gridVertexRows = 0;
    uint32_t gridVertexColumns = 0;
    std::vector<Vec3> faceNormals;
    std::vector<Color> faceColors;
    std::vector<Color> vertexColors;
    std::vector<uint32_t> faceColorIndices;
    std::vector<uint32_t> vertexColorIndices;
};

struct Polyline {
    std::vector<Vec3> points;
    float lineWidth = 1.0f;
};

struct PointCloud {
    std::vector<Vec3> points;
    float pointSize = 1.0f;
};

struct OpenGLCallbackContext {
    void * nativeAction = nullptr;
    void * nativeState = nullptr;
};

using OpenGLDrawCallback = void (*)(void * userData,
                                    const OpenGLCallbackContext & context);

struct OpenGLCallback {
    OpenGLDrawCallback draw = nullptr;
    void * userData = nullptr;
    const char * label = nullptr;
    bool resetInventorLazyState = true;
};

enum class Primitive {
    Cube,
    Sphere,
    Cone,
    Cylinder
};

struct PrimitiveOptions {
    float width = 2.0f;
    float height = 2.0f;
    float depth = 2.0f;
    float radius = 1.0f;
};

OBOL_V2_API Mesh makeSphereMesh(float radius = 1.0f,
                                 uint32_t slices = 32,
                                 uint32_t stacks = 16);

struct DirectionalLight {
    Vec3 direction = {0.0f, 0.0f, -1.0f};
    Color color = {1.0f, 1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
};

struct PointLight {
    Vec3 location = {0.0f, 0.0f, 1.0f};
    Color color = {1.0f, 1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
};

struct SpotLight {
    Vec3 location = {0.0f, 0.0f, 1.0f};
    Vec3 direction = {0.0f, 0.0f, -1.0f};
    Color color = {1.0f, 1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float cutOffAngleRadians = 0.78539816339f;
    float dropOffRate = 0.0f;
};

enum class TextJustification {
    Left,
    Right,
    Center
};

struct Text2D {
    std::string text;
    std::string fontName = "default";
    float fontSize = 1.0f;
    float spacing = 1.0f;
    TextJustification justification = TextJustification::Left;
    bool depthTest = true;
};

enum class Text3DParts : uint32_t {
    Front = 1,
    Sides = 2,
    Back = 4,
    All = 7
};

struct Text3D {
    std::string text;
    std::string fontName = "default";
    float fontSize = 1.0f;
    float spacing = 1.0f;
    TextJustification justification = TextJustification::Left;
    uint32_t parts = static_cast<uint32_t>(Text3DParts::All);
    std::vector<Color> partColors;
    std::vector<Vec2> profile;
};

struct PerspectiveCamera {
    Vec3 position = {0.0f, 0.0f, 5.0f};
    Vec3 target = {0.0f, 0.0f, 0.0f};
    Vec3 up = {0.0f, 1.0f, 0.0f};
    float verticalFieldOfViewRadians = 0.78539816339f;
    float nearDistance = 0.1f;
    float farDistance = 1000.0f;
};

struct OrthographicCamera {
    Vec3 position = {0.0f, 0.0f, 5.0f};
    Vec3 target = {0.0f, 0.0f, 0.0f};
    Vec3 up = {0.0f, 1.0f, 0.0f};
    float height = 6.0f;
    float nearDistance = 0.1f;
    float farDistance = 1000.0f;
};

using SceneObjectId = uint32_t;
inline constexpr SceneObjectId InvalidSceneObjectId = 0;
using SceneGroupId = uint32_t;
inline constexpr SceneGroupId RootSceneGroupId = 0;
inline constexpr SceneGroupId InvalidSceneGroupId = std::numeric_limits<SceneGroupId>::max();

enum class SceneObjectType {
    Any,
    Primitive,
    Mesh,
    Polyline,
    PointCloud,
    DirectionalLight,
    PointLight,
    SpotLight,
    Text2D,
    Text3D,
    CadAssembly,
    OpenGLCallback,
    LegacySceneGraph
};

enum class SceneObjectCategory {
    Any,
    Geometry,
    Light,
    Text,
    Cad,
    BackendNative
};

struct SceneQuery {
    SceneObjectType type = SceneObjectType::Any;
    SceneObjectCategory category = SceneObjectCategory::Any;
};

struct SceneObjectInfo {
    SceneObjectId id = InvalidSceneObjectId;
    SceneObjectType type = SceneObjectType::Any;
    SceneGroupId parent = RootSceneGroupId;
};

enum class SceneCameraKind {
    None,
    Perspective,
    Orthographic
};

struct SceneCameraRecord {
    SceneCameraKind kind = SceneCameraKind::None;
    PerspectiveCamera perspective;
    OrthographicCamera orthographic;
};

struct SceneGroupRecord {
    SceneGroupId id = InvalidSceneGroupId;
    SceneGroupId parent = RootSceneGroupId;
    Transform transform;
    Matrix4 localToWorld;
};

struct SceneObjectRecord {
    SceneObjectId id = InvalidSceneObjectId;
    SceneObjectType type = SceneObjectType::Any;
    SceneObjectCategory category = SceneObjectCategory::Any;
    SceneGroupId parent = RootSceneGroupId;
    Transform transform;
    Matrix4 localToWorld;
    Material material;

    Primitive primitive = Primitive::Cube;
    PrimitiveOptions primitiveOptions;
    Mesh mesh;
    Polyline polyline;
    PointCloud pointCloud;
    DirectionalLight directionalLight;
    PointLight pointLight;
    SpotLight spotLight;
    Text2D text2D;
    Text3D text3D;
    std::shared_ptr<const CadAssembly> cadAssembly;
    OpenGLCallback openGLCallback;
    bool hasLegacySceneGraph = false;
};

struct ScenePacket {
    SceneCameraRecord camera;
    std::vector<SceneGroupRecord> groups;
    std::vector<SceneObjectRecord> objects;
    bool hasLegacyFallbackRoot = false;
};

class OBOL_V2_API Scene {
public:
    Scene();
    Scene(const Scene & other);
    Scene(Scene && other) noexcept;
    ~Scene();

    Scene & operator=(const Scene & other);
    Scene & operator=(Scene && other) noexcept;

    SceneGroupId addGroup(const Transform & transform = Transform{},
                          SceneGroupId parent = RootSceneGroupId);
    bool setGroupTransform(SceneGroupId group, const Transform & transform);
    bool getGroupTransform(SceneGroupId group, Transform & transform) const;
    bool setGroupVisible(SceneGroupId group, bool visible);
    bool isGroupVisible(SceneGroupId group) const;
    bool removeGroup(SceneGroupId group);

    SceneObjectId addPrimitive(Primitive primitive,
                               const Material & material = Material{},
                               const Transform & transform = Transform{},
                               const PrimitiveOptions & options = PrimitiveOptions{},
                               SceneGroupId parent = RootSceneGroupId);

    SceneObjectId addMesh(const Mesh & mesh,
                          const Material & material = Material{},
                          const Transform & transform = Transform{},
                          SceneGroupId parent = RootSceneGroupId);
    SceneObjectId addPolyline(const Polyline & polyline,
                              const Material & material = Material{},
                              const Transform & transform = Transform{},
                              SceneGroupId parent = RootSceneGroupId);
    SceneObjectId addPointCloud(const PointCloud & pointCloud,
                                const Material & material = Material{},
                                const Transform & transform = Transform{},
                                SceneGroupId parent = RootSceneGroupId);

    SceneObjectId addDirectionalLight(const DirectionalLight & light,
                                      SceneGroupId parent = RootSceneGroupId);
    SceneObjectId addPointLight(const PointLight & light,
                                SceneGroupId parent = RootSceneGroupId);
    SceneObjectId addSpotLight(const SpotLight & light,
                               SceneGroupId parent = RootSceneGroupId);
    SceneObjectId addText2D(const Text2D & text,
                            const Material & material = Material{},
                            const Transform & transform = Transform{},
                            SceneGroupId parent = RootSceneGroupId);
    SceneObjectId addText3D(const Text3D & text,
                            const Material & material = Material{},
                            const Transform & transform = Transform{},
                            SceneGroupId parent = RootSceneGroupId);
    SceneObjectId addCadAssembly(const CadAssembly & assembly,
                                 const Transform & transform = Transform{},
                                 SceneGroupId parent = RootSceneGroupId);
    SceneObjectId addOpenGLCallback(const OpenGLCallback & callback,
                                    SceneGroupId parent = RootSceneGroupId);
    SceneObjectId addLegacySceneGraph(NativeSceneGraphHandle root,
                                      const Transform & transform = Transform{},
                                      SceneGroupId parent = RootSceneGroupId);
    bool setObjectTransform(SceneObjectId object, const Transform & transform);
    bool getObjectTransform(SceneObjectId object, Transform & transform) const;
    bool setObjectMaterial(SceneObjectId object, const Material & material);
    bool setObjectPrimitiveOptions(SceneObjectId object, const PrimitiveOptions & options);
    bool setObjectPointCloud(SceneObjectId object, const PointCloud & pointCloud);
    bool setObjectVisible(SceneObjectId object, bool visible);
    bool isObjectVisible(SceneObjectId object) const;
    bool removeObject(SceneObjectId object);

    void setCamera(const PerspectiveCamera & camera);
    void setCamera(const OrthographicCamera & camera);
    void clearCamera();
    bool hasCamera() const;

    bool empty() const;
    size_t objectCount() const;
    size_t groupCount() const;
    std::vector<SceneObjectInfo> findObjects(const SceneQuery & query = SceneQuery{}) const;
    SceneObjectId findFirstObject(const SceneQuery & query = SceneQuery{}) const;
    bool hasObjects(const SceneQuery & query = SceneQuery{}) const;
    ScenePacket capturePacket() const;
    void clear();

    // Legacy bridge for the v2 rollout.  New application code should submit
    // obol::FrameRequest values to obol::Renderer instead of depending on
    // native scene graph handles.
    NativeSceneGraphHandle createLegacySceneGraph() const;
    static void releaseLegacySceneGraph(NativeSceneGraphHandle root);

    static Scene fromLegacySceneGraph(NativeSceneGraphHandle root);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace obol

#endif // OBOL_SCENE_SCENE_H
