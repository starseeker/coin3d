#include <Obol/scene/Scene.h>

#include <Obol/cad/CadAssembly.h>
#include <Obol/compat/cad/SoCADAssembly.h>

#include <Inventor/SbName.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbRotation.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/elements/SoGLLazyElement.h>
#include <Inventor/elements/SoLazyElement.h>
#include <Inventor/nodes/SoCallback.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/nodes/SoFont.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <Inventor/nodes/SoIndexedTriangleStripSet.h>
#include <Inventor/nodes/SoLineSet.h>
#include <Inventor/nodes/SoLightModel.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoMaterialBinding.h>
#include <Inventor/nodes/SoNormal.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoPointLight.h>
#include <Inventor/nodes/SoPointSet.h>
#include <Inventor/nodes/SoQuadMesh.h>
#include <Inventor/nodes/SoLinearProfile.h>
#include <Inventor/nodes/SoProfileCoordinate2.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoShapeHints.h>
#include <Inventor/nodes/SoSpotLight.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoText2.h>
#include <Inventor/nodes/SoText3.h>
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/nodes/SoTextureCoordinate2.h>
#include <Inventor/nodes/SoTriangleStripSet.h>
#include <Inventor/nodes/SoTransform.h>

#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace obol {
namespace {

constexpr float kPi = 3.14159265358979323846f;

SbVec3f toSbVec3f(const Vec3 & v)
{
    return SbVec3f(v.x, v.y, v.z);
}

std::string legacyObjectName(SceneObjectId id)
{
    return std::string("ObolSceneObject_") + std::to_string(id);
}

std::string legacyGroupName(SceneGroupId id)
{
    return std::string("ObolSceneGroup_") + std::to_string(id);
}

SceneObjectCategory categoryForType(SceneObjectType type)
{
    switch (type) {
    case SceneObjectType::Primitive:
    case SceneObjectType::Mesh:
    case SceneObjectType::Polyline:
    case SceneObjectType::PointCloud:
        return SceneObjectCategory::Geometry;
    case SceneObjectType::DirectionalLight:
    case SceneObjectType::PointLight:
    case SceneObjectType::SpotLight:
        return SceneObjectCategory::Light;
    case SceneObjectType::Text2D:
    case SceneObjectType::Text3D:
        return SceneObjectCategory::Text;
    case SceneObjectType::CadAssembly:
        return SceneObjectCategory::Cad;
    case SceneObjectType::OpenGLCallback:
    case SceneObjectType::LegacySceneGraph:
        return SceneObjectCategory::BackendNative;
    case SceneObjectType::Any:
        break;
    }
    return SceneObjectCategory::Any;
}

void invokeOpenGLCallback(void * userdata, SoAction * action)
{
    const OpenGLCallback * callback =
        static_cast<const OpenGLCallback *>(userdata);
    if (!callback || !callback->draw ||
        !action->isOfType(SoGLRenderAction::getClassTypeId())) {
        return;
    }

    OpenGLCallbackContext context;
    context.nativeAction = action;
    context.nativeState = action->getState();
    callback->draw(callback->userData, context);

    if (callback->resetInventorLazyState) {
        SoState * state = action->getState();
        SoGLLazyElement * lazyElement =
            static_cast<SoGLLazyElement *>(SoLazyElement::getInstance(state));
        lazyElement->reset(state,
            SoLazyElement::DIFFUSE_MASK | SoLazyElement::LIGHT_MODEL_MASK);
    }
}

std::shared_ptr<SoSeparator> copyLegacySeparator(const SoSeparator & root)
{
    SoNode * copy = root.copy(FALSE);
    SoSeparator * separator =
        copy && copy->isOfType(SoSeparator::getClassTypeId())
            ? static_cast<SoSeparator *>(copy)
            : nullptr;
    if (!separator) {
        return {};
    }
    separator->ref();
    return std::shared_ptr<SoSeparator>(
        separator,
        [](SoSeparator * node) {
            if (node) node->unref();
        });
}

SoMaterial * createMaterial(const Material & material)
{
    SoMaterial * node = new SoMaterial;
    node->diffuseColor.setValue(material.baseColor.r,
                                material.baseColor.g,
                                material.baseColor.b);
    node->specularColor.setValue(material.specular.r,
                                 material.specular.g,
                                 material.specular.b);
    node->emissiveColor.setValue(material.emissive.r,
                                 material.emissive.g,
                                 material.emissive.b);
    node->shininess.setValue(material.shininess);
    node->transparency.setValue(1.0f - material.baseColor.a);
    return node;
}

SoTexture2::Wrap toLegacyWrap(TextureWrap wrap)
{
    switch (wrap) {
    case TextureWrap::Repeat:
        return SoTexture2::REPEAT;
    case TextureWrap::Clamp:
        return SoTexture2::CLAMP;
    }
    return SoTexture2::REPEAT;
}

SoTexture2::Model toLegacyTextureModel(TextureModel model)
{
    switch (model) {
    case TextureModel::Modulate:
        return SoTexture2::MODULATE;
    case TextureModel::Decal:
        return SoTexture2::DECAL;
    case TextureModel::Blend:
        return SoTexture2::BLEND;
    case TextureModel::Replace:
        return SoTexture2::REPLACE;
    }
    return SoTexture2::MODULATE;
}

int imageComponentCount(ImageFormat format)
{
    return static_cast<int>(format);
}

bool isValidImage(const Image2D & image)
{
    const int components = imageComponentCount(image.format);
    return image.width > 0 &&
           image.height > 0 &&
           components > 0 &&
           image.pixels.size() >=
               static_cast<size_t>(image.width) *
               static_cast<size_t>(image.height) *
               static_cast<size_t>(components);
}

SoTexture2 * createTexture(const Texture2D & texture)
{
    if (!isValidImage(texture.image)) {
        return nullptr;
    }

    SoTexture2 * node = new SoTexture2;
    node->setImageData(static_cast<int>(texture.image.width),
                       static_cast<int>(texture.image.height),
                       imageComponentCount(texture.image.format),
                       texture.image.pixels.data());
    node->wrapS.setValue(toLegacyWrap(texture.wrapS));
    node->wrapT.setValue(toLegacyWrap(texture.wrapT));
    node->model.setValue(toLegacyTextureModel(texture.model));
    node->blendColor.setValue(texture.blendColor.r,
                              texture.blendColor.g,
                              texture.blendColor.b);
    return node;
}

SoTransform * createTransform(const Transform & transform)
{
    SoTransform * node = new SoTransform;
    node->translation.setValue(toSbVec3f(transform.translation));
    node->scaleFactor.setValue(toSbVec3f(transform.scale));

    const Vec3 axis = transform.rotationAxis;
    const float axisLen = std::sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
    if (axisLen > 0.0f && transform.rotationRadians != 0.0f) {
        node->rotation.setValue(SbRotation(SbVec3f(axis.x / axisLen,
                                                   axis.y / axisLen,
                                                   axis.z / axisLen),
                                          transform.rotationRadians));
    }
    return node;
}

SbMatrix transformMatrix(const Transform & transform)
{
    const Vec3 axis = transform.rotationAxis;
    const float axisLen = std::sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
    SbRotation rotation;
    if (axisLen > 0.0f && transform.rotationRadians != 0.0f) {
        rotation.setValue(SbVec3f(axis.x / axisLen,
                                  axis.y / axisLen,
                                  axis.z / axisLen),
                          transform.rotationRadians);
    }

    SbMatrix matrix;
    matrix.setTransform(toSbVec3f(transform.translation),
                        rotation,
                        toSbVec3f(transform.scale));
    return matrix;
}

Matrix4 toMatrix4(const SbMatrix & matrix)
{
    Matrix4 result;
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            result.values[static_cast<size_t>(column) * 4 +
                          static_cast<size_t>(row)] =
                matrix[column][row];
        }
    }
    return result;
}

SoNode * createPrimitiveNode(Primitive primitive, const PrimitiveOptions & options)
{
    switch (primitive) {
    case Primitive::Cube: {
        SoCube * cube = new SoCube;
        cube->width.setValue(options.width);
        cube->height.setValue(options.height);
        cube->depth.setValue(options.depth);
        return cube;
    }
    case Primitive::Sphere: {
        SoSphere * sphere = new SoSphere;
        sphere->radius.setValue(options.radius);
        return sphere;
    }
    case Primitive::Cone: {
        SoCone * cone = new SoCone;
        cone->bottomRadius.setValue(options.radius);
        cone->height.setValue(options.height);
        return cone;
    }
    case Primitive::Cylinder: {
        SoCylinder * cylinder = new SoCylinder;
        cylinder->radius.setValue(options.radius);
        cylinder->height.setValue(options.height);
        return cylinder;
    }
    }
    return new SoCube;
}

Vec3 fromSbVec3f(const SbVec3f & v)
{
    return {v[0], v[1], v[2]};
}

Color fromSbColor(const SbColor & color, float alpha = 1.0f)
{
    return {color[0], color[1], color[2], alpha};
}

bool nearlyEqual(float lhs, float rhs)
{
    return std::fabs(lhs - rhs) <= 1.0e-5f;
}

bool isZeroVector(const SbVec3f & v)
{
    return nearlyEqual(v[0], 0.0f) &&
           nearlyEqual(v[1], 0.0f) &&
           nearlyEqual(v[2], 0.0f);
}

bool isIdentityRotation(const SbRotation & rotation)
{
    return rotation.equals(SbRotation::identity(), 1.0e-5f);
}

bool transformFromMatrix(const SbMatrix & matrix, Transform & transform)
{
    SbVec3f translation;
    SbRotation rotation;
    SbVec3f scale;
    SbRotation scaleOrientation;
    matrix.getTransform(translation, rotation, scale, scaleOrientation);

    if (!isIdentityRotation(scaleOrientation)) {
        return false;
    }

    transform.translation = fromSbVec3f(translation);
    transform.scale = fromSbVec3f(scale);

    SbVec3f axis;
    float radians = 0.0f;
    rotation.getValue(axis, radians);
    if (nearlyEqual(radians, 0.0f)) {
        transform.rotationAxis = {0.0f, 0.0f, 1.0f};
        transform.rotationRadians = 0.0f;
    } else {
        transform.rotationAxis = fromSbVec3f(axis);
        transform.rotationRadians = radians;
    }
    return true;
}

SbVec3f transformPoint(const SbMatrix & matrix, const SbVec3f & point)
{
    SbVec3f result;
    matrix.multVecMatrix(point, result);
    return result;
}

SbVec3f transformDirection(const SbMatrix & matrix, const SbVec3f & direction)
{
    SbVec3f result;
    matrix.multDirMatrix(direction, result);
    if (result.length() > 0.0f) {
        result.normalize();
    }
    return result;
}

struct LegacyExtractionState {
    Material material;
    std::vector<Material> materials;
    std::vector<Vec3> coordinates;
    std::vector<Vec3> normals;
    std::vector<Vec2> texCoords;
    SoMaterialBinding::Binding materialBinding = SoMaterialBinding::OVERALL;
    SoNormalBinding::Binding normalBinding = SoNormalBinding::PER_VERTEX_INDEXED;
    float lineWidth = 1.0f;
    float pointSize = 1.0f;
    SbMatrix transform = SbMatrix::identity();
};

struct LegacyExtractionResult {
    bool fullySupported = true;
    bool extractedAny = false;
};

bool applyLegacyMaterial(const SoMaterial & node,
                         LegacyExtractionState & state)
{
    if (node.ambientColor.getNum() > 1) {
        return false;
    }

    const int diffuseCount = node.diffuseColor.getNum();
    const bool hasMaterialPalette = diffuseCount > 1;

    if (hasMaterialPalette) {
        if (node.specularColor.getNum() > 1 ||
            node.emissiveColor.getNum() > 1 ||
            node.shininess.getNum() > 1 ||
            (node.transparency.getNum() > 1 &&
             node.transparency.getNum() != diffuseCount)) {
            return false;
        }

        std::vector<Material> materials;
        materials.reserve(static_cast<size_t>(diffuseCount));
        for (int i = 0; i < diffuseCount; ++i) {
            Material material = state.material;
            material.baseColor = fromSbColor(node.diffuseColor[i],
                                            material.baseColor.a);
            if (node.specularColor.getNum() == 1) {
                material.specular =
                    fromSbColor(node.specularColor[0], material.specular.a);
            }
            if (node.emissiveColor.getNum() == 1) {
                material.emissive =
                    fromSbColor(node.emissiveColor[0], material.emissive.a);
            }
            if (node.shininess.getNum() == 1) {
                material.shininess = node.shininess[0];
            }
            if (node.transparency.getNum() == 1) {
                material.baseColor.a = 1.0f - node.transparency[0];
            } else if (node.transparency.getNum() == diffuseCount) {
                material.baseColor.a = 1.0f - node.transparency[i];
            }
            materials.push_back(material);
        }
        state.materials = materials;
        state.material = materials.front();
        return true;
    }

    if (node.specularColor.getNum() > 1 ||
        node.emissiveColor.getNum() > 1 ||
        node.shininess.getNum() > 1 ||
        node.transparency.getNum() > 1) {
        return false;
    }

    Material material = state.material;
    if (node.diffuseColor.getNum() == 1) {
        material.baseColor = fromSbColor(node.diffuseColor[0], material.baseColor.a);
    }
    if (node.specularColor.getNum() == 1) {
        material.specular = fromSbColor(node.specularColor[0], material.specular.a);
    }
    if (node.emissiveColor.getNum() == 1) {
        material.emissive = fromSbColor(node.emissiveColor[0], material.emissive.a);
    }
    if (node.shininess.getNum() == 1) {
        material.shininess = node.shininess[0];
    }
    if (node.transparency.getNum() == 1) {
        material.baseColor.a = 1.0f - node.transparency[0];
    }

    state.material = material;
    state.materials.clear();
    return true;
}

bool applyLegacyCoordinates(const SoCoordinate3 & node,
                            LegacyExtractionState & state)
{
    state.coordinates.clear();
    state.coordinates.reserve(static_cast<size_t>(node.point.getNum()));
    for (int i = 0; i < node.point.getNum(); ++i) {
        state.coordinates.push_back(fromSbVec3f(node.point[i]));
    }
    return true;
}

bool applyLegacyNormals(const SoNormal & node,
                        LegacyExtractionState & state)
{
    state.normals.clear();
    state.normals.reserve(static_cast<size_t>(node.vector.getNum()));
    for (int i = 0; i < node.vector.getNum(); ++i) {
        state.normals.push_back(fromSbVec3f(node.vector[i]));
    }
    return true;
}

bool applyLegacyTextureCoordinates(const SoTextureCoordinate2 & node,
                                   LegacyExtractionState & state)
{
    state.texCoords.clear();
    state.texCoords.reserve(static_cast<size_t>(node.point.getNum()));
    for (int i = 0; i < node.point.getNum(); ++i) {
        const SbVec2f point = node.point[i];
        state.texCoords.push_back({point[0], point[1]});
    }
    return true;
}

bool applyLegacyMaterialBinding(const SoMaterialBinding & node,
                                LegacyExtractionState & state)
{
    state.materialBinding =
        static_cast<SoMaterialBinding::Binding>(node.value.getValue());
    return true;
}

bool applyLegacyNormalBinding(const SoNormalBinding & node,
                              LegacyExtractionState & state)
{
    state.normalBinding =
        static_cast<SoNormalBinding::Binding>(node.value.getValue());
    return true;
}

bool applyLegacyDrawStyle(const SoDrawStyle & node,
                          LegacyExtractionState & state)
{
    if (node.style.getValue() != SoDrawStyle::FILLED) {
        return false;
    }
    state.lineWidth = node.lineWidth.getValue();
    state.pointSize = node.pointSize.getValue();
    return true;
}

bool appendLegacyTransform(const SoTransform & node,
                           LegacyExtractionState & state)
{
    if (!isZeroVector(node.center.getValue()) ||
        !isIdentityRotation(node.scaleOrientation.getValue())) {
        return false;
    }

    SbMatrix matrix;
    matrix.setTransform(node.translation.getValue(),
                        node.rotation.getValue(),
                        node.scaleFactor.getValue());
    state.transform.multRight(matrix);
    return true;
}

bool addLegacyPrimitive(Scene & scene,
                        Primitive primitive,
                        const PrimitiveOptions & options,
                        const LegacyExtractionState & state)
{
    if (state.materialBinding != SoMaterialBinding::OVERALL) {
        return false;
    }
    Transform transform;
    if (!transformFromMatrix(state.transform, transform)) {
        return false;
    }
    scene.addPrimitive(primitive, state.material, transform, options);
    return true;
}

std::vector<Color> legacyMaterialColors(const LegacyExtractionState & state)
{
    std::vector<Color> colors;
    if (!state.materials.empty()) {
        colors.reserve(state.materials.size());
        for (const Material & material : state.materials) {
            colors.push_back(material.baseColor);
        }
    } else {
        colors.push_back(state.material.baseColor);
    }
    return colors;
}

size_t triangleStripFaceCount(const Mesh & mesh);

bool addLegacyIndexedFaceSet(Scene & scene,
                             const SoIndexedFaceSet & node,
                             const LegacyExtractionState & state)
{
    if (state.coordinates.empty() || node.coordIndex.getNum() <= 0) {
        return false;
    }

    Transform transform;
    if (!transformFromMatrix(state.transform, transform)) {
        return false;
    }

    Mesh mesh;
    mesh.topology = MeshTopology::Polygons;
    mesh.positions = state.coordinates;

    size_t currentFaceVertexCount = 0;
    std::vector<size_t> faceMaterialIndices;
    std::vector<size_t> faceNormalIndices;
    const int coordCount = node.coordIndex.getNum();
    const int materialIndexCount = node.materialIndex.getNum();
    const int normalIndexCount = node.normalIndex.getNum();
    const int textureCoordIndexCount = node.textureCoordIndex.getNum();

    for (int i = 0; i < coordCount; ++i) {
        const int coordIndex = node.coordIndex[i];
        const int materialIndex =
            i < materialIndexCount ? node.materialIndex[i] : -1;
        const int normalIndex =
            i < normalIndexCount ? node.normalIndex[i] : -1;
        const int textureCoordIndex =
            i < textureCoordIndexCount ? node.textureCoordIndex[i] : -1;

        if (coordIndex == SO_END_FACE_INDEX) {
            if (currentFaceVertexCount < 3) {
                return false;
            }
            mesh.faceVertexCounts.push_back(
                static_cast<uint32_t>(currentFaceVertexCount));
            currentFaceVertexCount = 0;
            continue;
        }
        if (coordIndex < 0 ||
            static_cast<size_t>(coordIndex) >= state.coordinates.size()) {
            return false;
        }

        if (!state.texCoords.empty()) {
            size_t texCoordIndex = static_cast<size_t>(coordIndex);
            if (textureCoordIndex >= 0) {
                texCoordIndex = static_cast<size_t>(textureCoordIndex);
            }
            if (texCoordIndex >= state.texCoords.size()) {
                return false;
            }
            mesh.texCoordIndices.push_back(static_cast<uint32_t>(texCoordIndex));
        }

        if (currentFaceVertexCount == 0 &&
            (state.materialBinding == SoMaterialBinding::PER_FACE ||
             state.materialBinding == SoMaterialBinding::PER_PART ||
             state.materialBinding == SoMaterialBinding::PER_FACE_INDEXED ||
             state.materialBinding == SoMaterialBinding::PER_PART_INDEXED)) {
            size_t faceMaterialIndex = mesh.faceVertexCounts.size();
            if (state.materialBinding == SoMaterialBinding::PER_FACE_INDEXED ||
                state.materialBinding == SoMaterialBinding::PER_PART_INDEXED) {
                if (materialIndex >= 0) {
                    faceMaterialIndex = static_cast<size_t>(materialIndex);
                } else if (static_cast<int>(mesh.faceVertexCounts.size()) <
                           materialIndexCount) {
                    const int indexed =
                        node.materialIndex[static_cast<int>(mesh.faceVertexCounts.size())];
                    if (indexed < 0) {
                        return false;
                    }
                    faceMaterialIndex = static_cast<size_t>(indexed);
                }
            }
            faceMaterialIndices.push_back(faceMaterialIndex);
        }
        if (currentFaceVertexCount == 0 &&
            !state.normals.empty() &&
            (state.normalBinding == SoNormalBinding::PER_FACE ||
             state.normalBinding == SoNormalBinding::PER_PART ||
             state.normalBinding == SoNormalBinding::PER_FACE_INDEXED ||
             state.normalBinding == SoNormalBinding::PER_PART_INDEXED)) {
            size_t faceNormalIndex = mesh.faceVertexCounts.size();
            if (state.normalBinding == SoNormalBinding::PER_FACE_INDEXED ||
                state.normalBinding == SoNormalBinding::PER_PART_INDEXED) {
                if (normalIndex >= 0) {
                    faceNormalIndex = static_cast<size_t>(normalIndex);
                } else if (static_cast<int>(mesh.faceVertexCounts.size()) <
                           normalIndexCount) {
                    const int indexed =
                        node.normalIndex[static_cast<int>(mesh.faceVertexCounts.size())];
                    if (indexed < 0) {
                        return false;
                    }
                    faceNormalIndex = static_cast<size_t>(indexed);
                }
            }
            faceNormalIndices.push_back(faceNormalIndex);
        }

        if (state.materialBinding == SoMaterialBinding::PER_VERTEX_INDEXED) {
            size_t vertexMaterialIndex = static_cast<size_t>(coordIndex);
            if (materialIndex >= 0) {
                vertexMaterialIndex = static_cast<size_t>(materialIndex);
            }
            mesh.vertexColorIndices.push_back(
                static_cast<uint32_t>(vertexMaterialIndex));
        }
        if (!state.normals.empty() &&
            (state.normalBinding == SoNormalBinding::PER_VERTEX ||
             state.normalBinding == SoNormalBinding::PER_VERTEX_INDEXED)) {
            size_t vertexNormalIndex = static_cast<size_t>(coordIndex);
            if (normalIndex >= 0) {
                vertexNormalIndex = static_cast<size_t>(normalIndex);
            }
            if (vertexNormalIndex != static_cast<size_t>(coordIndex) ||
                vertexNormalIndex >= state.normals.size()) {
                return false;
            }
        }

        mesh.indices.push_back(static_cast<uint32_t>(coordIndex));
        ++currentFaceVertexCount;
    }

    if (currentFaceVertexCount != 0) {
        if (currentFaceVertexCount < 3) {
            return false;
        }
        mesh.faceVertexCounts.push_back(
            static_cast<uint32_t>(currentFaceVertexCount));
    }
    if (mesh.faceVertexCounts.empty()) {
        return false;
    }
    if (!state.texCoords.empty()) {
        mesh.texCoords = state.texCoords;
        if (textureCoordIndexCount == 0) {
            mesh.texCoordIndices.clear();
        } else if (mesh.texCoordIndices.size() != mesh.indices.size()) {
            return false;
        }
    }
    if (!state.normals.empty()) {
        switch (state.normalBinding) {
        case SoNormalBinding::OVERALL:
            if (state.normals.empty()) {
                return false;
            }
            mesh.faceNormals.assign(mesh.faceVertexCounts.size(),
                                    state.normals.front());
            break;
        case SoNormalBinding::PER_FACE:
        case SoNormalBinding::PER_PART:
            if (state.normals.size() < mesh.faceVertexCounts.size()) {
                return false;
            }
            mesh.faceNormals.assign(
                state.normals.begin(),
                state.normals.begin() +
                    static_cast<std::ptrdiff_t>(mesh.faceVertexCounts.size()));
            break;
        case SoNormalBinding::PER_FACE_INDEXED:
        case SoNormalBinding::PER_PART_INDEXED:
            if (faceNormalIndices.size() != mesh.faceVertexCounts.size()) {
                return false;
            }
            for (size_t index : faceNormalIndices) {
                if (index >= state.normals.size()) {
                    return false;
                }
                mesh.faceNormals.push_back(state.normals[index]);
            }
            break;
        case SoNormalBinding::PER_VERTEX:
        case SoNormalBinding::PER_VERTEX_INDEXED:
            if (state.normals.size() < mesh.positions.size()) {
                return false;
            }
            mesh.normals.assign(
                state.normals.begin(),
                state.normals.begin() +
                    static_cast<std::ptrdiff_t>(mesh.positions.size()));
            break;
        }
    }

    const std::vector<Color> colors = legacyMaterialColors(state);
    switch (state.materialBinding) {
    case SoMaterialBinding::OVERALL:
        break;
    case SoMaterialBinding::PER_FACE:
    case SoMaterialBinding::PER_PART:
        if (colors.size() < mesh.faceVertexCounts.size()) {
            return false;
        }
        mesh.faceColors.assign(colors.begin(),
                               colors.begin() +
                                   static_cast<std::ptrdiff_t>(mesh.faceVertexCounts.size()));
        break;
    case SoMaterialBinding::PER_FACE_INDEXED:
    case SoMaterialBinding::PER_PART_INDEXED:
        if (faceMaterialIndices.size() != mesh.faceVertexCounts.size()) {
            return false;
        }
        for (size_t index : faceMaterialIndices) {
            if (index >= colors.size()) {
                return false;
            }
            mesh.faceColorIndices.push_back(static_cast<uint32_t>(index));
        }
        mesh.faceColors = colors;
        break;
    case SoMaterialBinding::PER_VERTEX:
        if (colors.size() < mesh.positions.size()) {
            return false;
        }
        mesh.vertexColors.assign(colors.begin(),
                                 colors.begin() +
                                     static_cast<std::ptrdiff_t>(mesh.positions.size()));
        break;
    case SoMaterialBinding::PER_VERTEX_INDEXED:
        if (mesh.vertexColorIndices.size() != mesh.indices.size()) {
            return false;
        }
        for (uint32_t index : mesh.vertexColorIndices) {
            if (index >= colors.size()) {
                return false;
            }
        }
        mesh.vertexColors = colors;
        break;
    }

    scene.addMesh(mesh, state.material, transform);
    return true;
}

bool addLegacyIndexedTriangleStripSet(Scene & scene,
                                      const SoIndexedTriangleStripSet & node,
                                      const LegacyExtractionState & state)
{
    if (state.coordinates.empty() || node.coordIndex.getNum() <= 0) {
        return false;
    }
    if (state.materialBinding == SoMaterialBinding::PER_VERTEX_INDEXED ||
        state.materialBinding == SoMaterialBinding::PER_FACE_INDEXED ||
        state.materialBinding == SoMaterialBinding::PER_PART_INDEXED ||
        state.normalBinding == SoNormalBinding::PER_FACE ||
        state.normalBinding == SoNormalBinding::PER_PART ||
        state.normalBinding == SoNormalBinding::PER_FACE_INDEXED ||
        state.normalBinding == SoNormalBinding::PER_PART_INDEXED) {
        return false;
    }

    Transform transform;
    if (!transformFromMatrix(state.transform, transform)) {
        return false;
    }

    Mesh mesh;
    mesh.topology = MeshTopology::TriangleStrips;
    mesh.positions = state.coordinates;

    size_t currentStripVertexCount = 0;
    const int coordCount = node.coordIndex.getNum();
    const int normalIndexCount = node.normalIndex.getNum();
    const int textureCoordIndexCount = node.textureCoordIndex.getNum();

    for (int i = 0; i < coordCount; ++i) {
        const int coordIndex = node.coordIndex[i];
        const int normalIndex =
            i < normalIndexCount ? node.normalIndex[i] : -1;
        const int textureCoordIndex =
            i < textureCoordIndexCount ? node.textureCoordIndex[i] : -1;

        if (coordIndex == SO_END_STRIP_INDEX) {
            if (currentStripVertexCount < 3) {
                return false;
            }
            mesh.stripVertexCounts.push_back(
                static_cast<uint32_t>(currentStripVertexCount));
            currentStripVertexCount = 0;
            continue;
        }
        if (coordIndex < 0 ||
            static_cast<size_t>(coordIndex) >= state.coordinates.size()) {
            return false;
        }

        if (!state.texCoords.empty()) {
            size_t texCoordIndex = static_cast<size_t>(coordIndex);
            if (textureCoordIndex >= 0) {
                texCoordIndex = static_cast<size_t>(textureCoordIndex);
            }
            if (texCoordIndex >= state.texCoords.size()) {
                return false;
            }
            mesh.texCoordIndices.push_back(static_cast<uint32_t>(texCoordIndex));
        }

        if (!state.normals.empty() &&
            (state.normalBinding == SoNormalBinding::PER_VERTEX ||
             state.normalBinding == SoNormalBinding::PER_VERTEX_INDEXED)) {
            size_t vertexNormalIndex = static_cast<size_t>(coordIndex);
            if (normalIndex >= 0) {
                vertexNormalIndex = static_cast<size_t>(normalIndex);
            }
            if (vertexNormalIndex != static_cast<size_t>(coordIndex) ||
                vertexNormalIndex >= state.normals.size()) {
                return false;
            }
        }

        mesh.indices.push_back(static_cast<uint32_t>(coordIndex));
        ++currentStripVertexCount;
    }

    if (currentStripVertexCount != 0) {
        if (currentStripVertexCount < 3) {
            return false;
        }
        mesh.stripVertexCounts.push_back(
            static_cast<uint32_t>(currentStripVertexCount));
    }
    if (mesh.stripVertexCounts.empty()) {
        return false;
    }

    if (!state.texCoords.empty()) {
        mesh.texCoords = state.texCoords;
        if (textureCoordIndexCount == 0) {
            mesh.texCoordIndices.clear();
        } else if (mesh.texCoordIndices.size() != mesh.indices.size()) {
            return false;
        }
    }
    if (!state.normals.empty()) {
        switch (state.normalBinding) {
        case SoNormalBinding::OVERALL:
            if (state.normals.empty()) {
                return false;
            }
            mesh.normals.assign(mesh.positions.size(), state.normals.front());
            break;
        case SoNormalBinding::PER_VERTEX:
        case SoNormalBinding::PER_VERTEX_INDEXED:
            if (state.normals.size() < mesh.positions.size()) {
                return false;
            }
            mesh.normals.assign(
                state.normals.begin(),
                state.normals.begin() +
                    static_cast<std::ptrdiff_t>(mesh.positions.size()));
            break;
        case SoNormalBinding::PER_FACE:
        case SoNormalBinding::PER_PART:
        case SoNormalBinding::PER_FACE_INDEXED:
        case SoNormalBinding::PER_PART_INDEXED:
            return false;
        }
    }

    const std::vector<Color> colors = legacyMaterialColors(state);
    switch (state.materialBinding) {
    case SoMaterialBinding::OVERALL:
        break;
    case SoMaterialBinding::PER_PART:
        if (colors.size() < mesh.stripVertexCounts.size()) {
            return false;
        }
        mesh.faceColors.assign(
            colors.begin(),
            colors.begin() +
                static_cast<std::ptrdiff_t>(mesh.stripVertexCounts.size()));
        break;
    case SoMaterialBinding::PER_FACE: {
        const size_t faceCount = triangleStripFaceCount(mesh);
        if (colors.size() < faceCount) {
            return false;
        }
        mesh.faceColors.assign(colors.begin(),
                               colors.begin() +
                                   static_cast<std::ptrdiff_t>(faceCount));
        break;
    }
    case SoMaterialBinding::PER_FACE_INDEXED:
    case SoMaterialBinding::PER_PART_INDEXED:
    case SoMaterialBinding::PER_VERTEX:
    case SoMaterialBinding::PER_VERTEX_INDEXED:
        return false;
    }

    scene.addMesh(mesh, state.material, transform);
    return true;
}

bool addLegacyLineSet(Scene & scene,
                      const SoLineSet & node,
                      const LegacyExtractionState & state)
{
    if (state.coordinates.empty() ||
        (state.materialBinding != SoMaterialBinding::OVERALL &&
         state.materialBinding != SoMaterialBinding::PER_PART)) {
        return false;
    }

    Transform transform;
    if (!transformFromMatrix(state.transform, transform)) {
        return false;
    }

    size_t cursor = 0;
    std::vector<uint32_t> lineCounts;
    if (node.numVertices.getNum() == 0 ||
        (node.numVertices.getNum() == 1 && node.numVertices[0] < 0)) {
        lineCounts.push_back(static_cast<uint32_t>(state.coordinates.size()));
    } else {
        for (int i = 0; i < node.numVertices.getNum(); ++i) {
            const int count = node.numVertices[i];
            if (count < 0) {
                if (cursor >= state.coordinates.size()) {
                    return false;
                }
                lineCounts.push_back(
                    static_cast<uint32_t>(state.coordinates.size() - cursor));
                cursor = state.coordinates.size();
                break;
            }
            lineCounts.push_back(static_cast<uint32_t>(count));
            cursor += static_cast<size_t>(count);
        }
        if (cursor > state.coordinates.size()) {
            return false;
        }
    }

    if (state.materialBinding == SoMaterialBinding::PER_PART &&
        state.materials.size() < lineCounts.size()) {
        return false;
    }

    cursor = 0;
    for (size_t lineIndex = 0; lineIndex < lineCounts.size(); ++lineIndex) {
        const uint32_t count = lineCounts[lineIndex];
        if (count < 2 || cursor + count > state.coordinates.size()) {
            return false;
        }
        Polyline polyline;
        polyline.lineWidth = state.lineWidth;
        polyline.points.assign(state.coordinates.begin() +
                                   static_cast<std::ptrdiff_t>(cursor),
                               state.coordinates.begin() +
                                   static_cast<std::ptrdiff_t>(cursor + count));
        const Material & material =
            state.materialBinding == SoMaterialBinding::PER_PART
                ? state.materials[lineIndex]
                : state.material;
        scene.addPolyline(polyline, material, transform);
        cursor += count;
    }

    return !lineCounts.empty();
}

bool addLegacyPointSet(Scene & scene,
                       const SoPointSet & node,
                       const LegacyExtractionState & state)
{
    if (state.coordinates.empty() ||
        state.materialBinding != SoMaterialBinding::OVERALL) {
        return false;
    }

    Transform transform;
    if (!transformFromMatrix(state.transform, transform)) {
        return false;
    }

    const int requested = node.numPoints.getValue();
    const size_t count = requested < 0
        ? state.coordinates.size()
        : static_cast<size_t>(requested);
    if (count == 0 || count > state.coordinates.size()) {
        return false;
    }

    PointCloud pointCloud;
    pointCloud.pointSize = state.pointSize;
    pointCloud.points.assign(state.coordinates.begin(),
                             state.coordinates.begin() +
                                 static_cast<std::ptrdiff_t>(count));
    scene.addPointCloud(pointCloud, state.material, transform);
    return true;
}

bool addLegacyLight(Scene & scene,
                    const SoDirectionalLight & node,
                    const LegacyExtractionState & state)
{
    if (!node.on.getValue()) {
        return true;
    }
    DirectionalLight light;
    light.direction = fromSbVec3f(transformDirection(state.transform,
                                                     node.direction.getValue()));
    light.color = fromSbColor(node.color.getValue());
    light.intensity = node.intensity.getValue();
    scene.addDirectionalLight(light);
    return true;
}

bool addLegacyLight(Scene & scene,
                    const SoPointLight & node,
                    const LegacyExtractionState & state)
{
    if (!node.on.getValue()) {
        return true;
    }
    PointLight light;
    light.location = fromSbVec3f(transformPoint(state.transform,
                                                node.location.getValue()));
    light.color = fromSbColor(node.color.getValue());
    light.intensity = node.intensity.getValue();
    scene.addPointLight(light);
    return true;
}

bool addLegacyLight(Scene & scene,
                    const SoSpotLight & node,
                    const LegacyExtractionState & state)
{
    if (!node.on.getValue()) {
        return true;
    }
    SpotLight light;
    light.location = fromSbVec3f(transformPoint(state.transform,
                                                node.location.getValue()));
    light.direction = fromSbVec3f(transformDirection(state.transform,
                                                     node.direction.getValue()));
    light.color = fromSbColor(node.color.getValue());
    light.intensity = node.intensity.getValue();
    light.cutOffAngleRadians = node.cutOffAngle.getValue();
    light.dropOffRate = node.dropOffRate.getValue();
    scene.addSpotLight(light);
    return true;
}

LegacyExtractionResult extractLegacyNode(const SoNode & node,
                                         LegacyExtractionState & state,
                                         Scene & scene);

LegacyExtractionResult extractLegacyChildren(const SoGroup & group,
                                             LegacyExtractionState & state,
                                             Scene & scene,
                                             bool isolateState)
{
    LegacyExtractionState childState = state;
    LegacyExtractionState & traversalState = isolateState ? childState : state;
    LegacyExtractionResult result;

    for (int i = 0; i < group.getNumChildren(); ++i) {
        SoNode * child = group.getChild(i);
        if (!child) {
            continue;
        }
        LegacyExtractionResult childResult =
            extractLegacyNode(*child, traversalState, scene);
        result.fullySupported = result.fullySupported && childResult.fullySupported;
        result.extractedAny = result.extractedAny || childResult.extractedAny;
        if (!result.fullySupported) {
            break;
        }
    }
    return result;
}

LegacyExtractionResult extractLegacyNode(const SoNode & node,
                                         LegacyExtractionState & state,
                                         Scene & scene)
{
    LegacyExtractionResult result;

    if (node.isOfType(SoSeparator::getClassTypeId())) {
        return extractLegacyChildren(static_cast<const SoSeparator &>(node),
                                     state,
                                     scene,
                                     true);
    }
    if (node.isOfType(SoGroup::getClassTypeId())) {
        return extractLegacyChildren(static_cast<const SoGroup &>(node),
                                     state,
                                     scene,
                                     false);
    }
    if (node.isOfType(SoMaterial::getClassTypeId())) {
        result.fullySupported =
            applyLegacyMaterial(static_cast<const SoMaterial &>(node), state);
        return result;
    }
    if (node.isOfType(SoMaterialBinding::getClassTypeId())) {
        result.fullySupported =
            applyLegacyMaterialBinding(static_cast<const SoMaterialBinding &>(node),
                                       state);
        return result;
    }
    if (node.isOfType(SoCoordinate3::getClassTypeId())) {
        result.fullySupported =
            applyLegacyCoordinates(static_cast<const SoCoordinate3 &>(node), state);
        return result;
    }
    if (node.isOfType(SoNormal::getClassTypeId())) {
        result.fullySupported =
            applyLegacyNormals(static_cast<const SoNormal &>(node), state);
        return result;
    }
    if (node.isOfType(SoNormalBinding::getClassTypeId())) {
        result.fullySupported =
            applyLegacyNormalBinding(static_cast<const SoNormalBinding &>(node),
                                     state);
        return result;
    }
    if (node.isOfType(SoTextureCoordinate2::getClassTypeId())) {
        result.fullySupported =
            applyLegacyTextureCoordinates(
                static_cast<const SoTextureCoordinate2 &>(node),
                state);
        return result;
    }
    if (node.isOfType(SoDrawStyle::getClassTypeId())) {
        result.fullySupported =
            applyLegacyDrawStyle(static_cast<const SoDrawStyle &>(node), state);
        return result;
    }
    if (node.isOfType(SoTransform::getClassTypeId())) {
        result.fullySupported =
            appendLegacyTransform(static_cast<const SoTransform &>(node), state);
        return result;
    }
    if (node.isOfType(SoDirectionalLight::getClassTypeId())) {
        result.fullySupported =
            addLegacyLight(scene, static_cast<const SoDirectionalLight &>(node), state);
        result.extractedAny = result.fullySupported;
        return result;
    }
    if (node.isOfType(SoPointLight::getClassTypeId())) {
        result.fullySupported =
            addLegacyLight(scene, static_cast<const SoPointLight &>(node), state);
        result.extractedAny = result.fullySupported;
        return result;
    }
    if (node.isOfType(SoSpotLight::getClassTypeId())) {
        result.fullySupported =
            addLegacyLight(scene, static_cast<const SoSpotLight &>(node), state);
        result.extractedAny = result.fullySupported;
        return result;
    }
    if (node.isOfType(SoCube::getClassTypeId())) {
        const SoCube & cube = static_cast<const SoCube &>(node);
        PrimitiveOptions options;
        options.width = cube.width.getValue();
        options.height = cube.height.getValue();
        options.depth = cube.depth.getValue();
        result.fullySupported =
            addLegacyPrimitive(scene, Primitive::Cube, options, state);
        result.extractedAny = result.fullySupported;
        return result;
    }
    if (node.isOfType(SoSphere::getClassTypeId())) {
        const SoSphere & sphere = static_cast<const SoSphere &>(node);
        PrimitiveOptions options;
        options.radius = sphere.radius.getValue();
        result.fullySupported =
            addLegacyPrimitive(scene, Primitive::Sphere, options, state);
        result.extractedAny = result.fullySupported;
        return result;
    }
    if (node.isOfType(SoCone::getClassTypeId())) {
        const SoCone & cone = static_cast<const SoCone &>(node);
        if (cone.parts.getValue() != SoCone::ALL) {
            result.fullySupported = false;
            return result;
        }
        PrimitiveOptions options;
        options.radius = cone.bottomRadius.getValue();
        options.height = cone.height.getValue();
        result.fullySupported =
            addLegacyPrimitive(scene, Primitive::Cone, options, state);
        result.extractedAny = result.fullySupported;
        return result;
    }
    if (node.isOfType(SoCylinder::getClassTypeId())) {
        const SoCylinder & cylinder = static_cast<const SoCylinder &>(node);
        if (cylinder.parts.getValue() != SoCylinder::ALL) {
            result.fullySupported = false;
            return result;
        }
        PrimitiveOptions options;
        options.radius = cylinder.radius.getValue();
        options.height = cylinder.height.getValue();
        result.fullySupported =
            addLegacyPrimitive(scene, Primitive::Cylinder, options, state);
        result.extractedAny = result.fullySupported;
        return result;
    }
    if (node.isOfType(SoIndexedFaceSet::getClassTypeId())) {
        result.fullySupported =
            addLegacyIndexedFaceSet(scene,
                                    static_cast<const SoIndexedFaceSet &>(node),
                                    state);
        result.extractedAny = result.fullySupported;
        return result;
    }
    if (node.isOfType(SoIndexedTriangleStripSet::getClassTypeId())) {
        result.fullySupported =
            addLegacyIndexedTriangleStripSet(
                scene,
                static_cast<const SoIndexedTriangleStripSet &>(node),
                state);
        result.extractedAny = result.fullySupported;
        return result;
    }
    if (node.isOfType(SoLineSet::getClassTypeId())) {
        result.fullySupported =
            addLegacyLineSet(scene, static_cast<const SoLineSet &>(node), state);
        result.extractedAny = result.fullySupported;
        return result;
    }
    if (node.isOfType(SoPointSet::getClassTypeId())) {
        result.fullySupported =
            addLegacyPointSet(scene, static_cast<const SoPointSet &>(node), state);
        result.extractedAny = result.fullySupported;
        return result;
    }

    result.fullySupported = false;
    return result;
}

bool tryExtractLegacyScene(const SoSeparator & root,
                           Scene & scene)
{
    Scene candidate;
    LegacyExtractionState state;
    LegacyExtractionResult result = extractLegacyNode(root, state, candidate);
    if (!result.fullySupported || !result.extractedAny) {
        return false;
    }

    scene = std::move(candidate);
    return true;
}

bool hasPolygonFaces(const Mesh & mesh)
{
    return mesh.topology == MeshTopology::Polygons ||
           !mesh.faceVertexCounts.empty();
}

bool hasTriangleStrips(const Mesh & mesh)
{
    return mesh.topology == MeshTopology::TriangleStrips ||
           !mesh.stripVertexCounts.empty();
}

bool hasQuadGrid(const Mesh & mesh)
{
    return mesh.topology == MeshTopology::QuadGrid ||
           (mesh.gridVertexRows > 0 && mesh.gridVertexColumns > 0);
}

std::vector<uint32_t> meshIndexStream(const Mesh & mesh)
{
    if (!mesh.indices.empty()) {
        return mesh.indices;
    }

    std::vector<uint32_t> stream;
    stream.reserve(mesh.positions.size());
    for (size_t i = 0; i < mesh.positions.size(); ++i) {
        stream.push_back(static_cast<uint32_t>(i));
    }
    return stream;
}

size_t triangleStripFaceCount(const Mesh & mesh)
{
    const std::vector<uint32_t> stream = meshIndexStream(mesh);
    if (mesh.stripVertexCounts.empty()) {
        return stream.size() > 2 ? stream.size() - 2 : 0;
    }

    size_t count = 0;
    size_t index = 0;
    for (uint32_t vertices : mesh.stripVertexCounts) {
        if (vertices < 3 || index + vertices > stream.size()) {
            break;
        }
        count += vertices - 2;
        index += vertices;
    }
    return count;
}

size_t quadGridFaceCount(const Mesh & mesh)
{
    if (mesh.gridVertexRows < 2 || mesh.gridVertexColumns < 2) {
        return 0;
    }
    return static_cast<size_t>(mesh.gridVertexRows - 1) *
           static_cast<size_t>(mesh.gridVertexColumns - 1);
}

std::vector<Color> effectiveFaceColors(const Mesh & mesh)
{
    if (mesh.faceColors.empty()) {
        return {};
    }

    if (hasQuadGrid(mesh)) {
        const size_t faceCount = quadGridFaceCount(mesh);
        if (mesh.faceColors.size() == faceCount) {
            return mesh.faceColors;
        }
        if (mesh.faceColors.size() == 1 && faceCount > 0) {
            return std::vector<Color>(faceCount, mesh.faceColors[0]);
        }
        return mesh.faceColors;
    }

    if (!hasTriangleStrips(mesh)) {
        return mesh.faceColors;
    }

    const size_t triangleCount = triangleStripFaceCount(mesh);
    if (mesh.faceColors.size() == triangleCount) {
        return mesh.faceColors;
    }

    std::vector<Color> colors;
    if (mesh.faceColors.size() == 1 && triangleCount > 0) {
        colors.assign(triangleCount, mesh.faceColors[0]);
        return colors;
    }

    const size_t stripCount = mesh.stripVertexCounts.empty()
        ? 1
        : mesh.stripVertexCounts.size();
    if (mesh.faceColors.size() == stripCount) {
        colors.reserve(triangleCount);
        if (mesh.stripVertexCounts.empty()) {
            colors.assign(triangleCount, mesh.faceColors[0]);
            return colors;
        }

        for (size_t strip = 0; strip < mesh.stripVertexCounts.size(); ++strip) {
            const uint32_t vertices = mesh.stripVertexCounts[strip];
            if (vertices < 3) {
                continue;
            }
            for (uint32_t triangle = 0; triangle < vertices - 2; ++triangle) {
                colors.push_back(mesh.faceColors[strip]);
            }
        }
    }
    return colors;
}

SoMaterial * createFaceColorMaterial(const std::vector<Color> & colors)
{
    if (colors.empty()) {
        return nullptr;
    }

    SoMaterial * node = new SoMaterial;
    for (size_t i = 0; i < colors.size(); ++i) {
        const Color & color = colors[i];
        node->diffuseColor.set1Value(static_cast<int>(i),
                                     color.r, color.g, color.b);
        node->transparency.set1Value(static_cast<int>(i), 1.0f - color.a);
    }
    return node;
}

enum class LegacyMeshMaterialBinding {
    None,
    PerPart,
    PerFace,
    PerFaceIndexed,
    PerVertexIndexed
};

LegacyMeshMaterialBinding meshMaterialBinding(const Mesh & mesh)
{
    if (!mesh.vertexColors.empty()) {
        return LegacyMeshMaterialBinding::PerVertexIndexed;
    }
    if (!mesh.faceColorIndices.empty() && !mesh.faceColors.empty()) {
        return LegacyMeshMaterialBinding::PerFaceIndexed;
    }
    if (!mesh.faceColors.empty()) {
        const size_t stripCount = mesh.stripVertexCounts.empty()
            ? 1
            : mesh.stripVertexCounts.size();
        if (hasTriangleStrips(mesh) && mesh.faceColors.size() == stripCount) {
            return LegacyMeshMaterialBinding::PerPart;
        }
        return LegacyMeshMaterialBinding::PerFace;
    }
    return LegacyMeshMaterialBinding::None;
}

std::vector<Color> meshMaterialColors(const Mesh & mesh)
{
    const LegacyMeshMaterialBinding binding = meshMaterialBinding(mesh);
    if (binding == LegacyMeshMaterialBinding::PerVertexIndexed) {
        return mesh.vertexColors;
    }
    if (binding == LegacyMeshMaterialBinding::PerPart ||
        binding == LegacyMeshMaterialBinding::PerFaceIndexed) {
        return mesh.faceColors;
    }
    return effectiveFaceColors(mesh);
}

SoMaterialBinding::Binding toLegacyMaterialBinding(LegacyMeshMaterialBinding binding)
{
    switch (binding) {
    case LegacyMeshMaterialBinding::PerPart:
        return SoMaterialBinding::PER_PART;
    case LegacyMeshMaterialBinding::PerFace:
        return SoMaterialBinding::PER_FACE;
    case LegacyMeshMaterialBinding::PerFaceIndexed:
        return SoMaterialBinding::PER_FACE_INDEXED;
    case LegacyMeshMaterialBinding::PerVertexIndexed:
        return SoMaterialBinding::PER_VERTEX_INDEXED;
    case LegacyMeshMaterialBinding::None:
        break;
    }
    return SoMaterialBinding::OVERALL;
}

SoSeparator * createTriangleStripNode(const Mesh & mesh)
{
    const std::vector<uint32_t> stream = meshIndexStream(mesh);

    SoSeparator * sep = new SoSeparator;

    SoShapeHints * hints = new SoShapeHints;
    hints->vertexOrdering.setValue(SoShapeHints::COUNTERCLOCKWISE);
    sep->addChild(hints);

    SoCoordinate3 * coords = new SoCoordinate3;
    for (size_t i = 0; i < stream.size(); ++i) {
        const uint32_t index = stream[i];
        if (index >= mesh.positions.size()) {
            continue;
        }
        coords->point.set1Value(static_cast<int>(i), toSbVec3f(mesh.positions[index]));
    }
    sep->addChild(coords);

    if (!mesh.normals.empty()) {
        SoNormal * normals = new SoNormal;
        for (size_t i = 0; i < stream.size(); ++i) {
            const uint32_t index = stream[i];
            if (index < mesh.normals.size()) {
                normals->vector.set1Value(static_cast<int>(i),
                                          toSbVec3f(mesh.normals[index]));
            }
        }
        sep->addChild(normals);
    }

    if (!mesh.texCoords.empty()) {
        SoTextureCoordinate2 * texCoords = new SoTextureCoordinate2;
        for (size_t i = 0; i < stream.size(); ++i) {
            uint32_t index = stream[i];
            if (!mesh.texCoordIndices.empty() && i < mesh.texCoordIndices.size()) {
                index = mesh.texCoordIndices[i];
            }
            if (index < mesh.texCoords.size()) {
                texCoords->point.set1Value(static_cast<int>(i),
                                           mesh.texCoords[index].x,
                                           mesh.texCoords[index].y);
            }
        }
        sep->addChild(texCoords);
    }

    if (!mesh.vertexColors.empty()) {
        SoMaterial * material = new SoMaterial;
        for (size_t i = 0; i < stream.size(); ++i) {
            const uint32_t index = stream[i];
            if (index < mesh.vertexColors.size()) {
                const Color & color = mesh.vertexColors[index];
                material->diffuseColor.set1Value(static_cast<int>(i),
                                                 color.r, color.g, color.b);
                material->transparency.set1Value(static_cast<int>(i),
                                                 1.0f - color.a);
            }
        }
        sep->addChild(material);
        SoMaterialBinding * binding = new SoMaterialBinding;
        binding->value.setValue(SoMaterialBinding::PER_VERTEX);
        sep->addChild(binding);
    } else if (!mesh.faceColors.empty()) {
        const size_t stripCount = mesh.stripVertexCounts.empty()
            ? 1
            : mesh.stripVertexCounts.size();
        SoMaterialBinding::Binding bindingValue = SoMaterialBinding::OVERALL;
        std::vector<Color> colors;
        if (mesh.faceColors.size() == 1) {
            colors = mesh.faceColors;
            bindingValue = SoMaterialBinding::OVERALL;
        } else if (mesh.faceColors.size() == stripCount) {
            colors = mesh.faceColors;
            bindingValue = SoMaterialBinding::PER_PART;
        } else {
            colors = effectiveFaceColors(mesh);
            bindingValue = SoMaterialBinding::PER_FACE;
        }

        SoMaterial * material = createFaceColorMaterial(colors);
        if (material) {
            sep->addChild(material);
            if (bindingValue != SoMaterialBinding::OVERALL) {
                SoMaterialBinding * binding = new SoMaterialBinding;
                binding->value.setValue(bindingValue);
                sep->addChild(binding);
            }
        }
    }

    SoTriangleStripSet * strips = new SoTriangleStripSet;
    if (mesh.stripVertexCounts.empty()) {
        strips->numVertices.set1Value(0, static_cast<int32_t>(stream.size()));
    } else {
        for (size_t i = 0; i < mesh.stripVertexCounts.size(); ++i) {
            strips->numVertices.set1Value(static_cast<int>(i),
                static_cast<int32_t>(mesh.stripVertexCounts[i]));
        }
    }
    sep->addChild(strips);

    return sep;
}

SoSeparator * createQuadGridNode(const Mesh & mesh)
{
    const std::vector<uint32_t> stream = meshIndexStream(mesh);

    SoSeparator * sep = new SoSeparator;

    SoCoordinate3 * coords = new SoCoordinate3;
    for (size_t i = 0; i < stream.size(); ++i) {
        const uint32_t index = stream[i];
        if (index >= mesh.positions.size()) {
            continue;
        }
        coords->point.set1Value(static_cast<int>(i), toSbVec3f(mesh.positions[index]));
    }
    sep->addChild(coords);

    if (!mesh.normals.empty()) {
        SoNormal * normals = new SoNormal;
        for (size_t i = 0; i < stream.size(); ++i) {
            const uint32_t index = stream[i];
            if (index < mesh.normals.size()) {
                normals->vector.set1Value(static_cast<int>(i),
                                          toSbVec3f(mesh.normals[index]));
            }
        }
        sep->addChild(normals);
    }

    if (!mesh.texCoords.empty()) {
        SoTextureCoordinate2 * texCoords = new SoTextureCoordinate2;
        for (size_t i = 0; i < stream.size(); ++i) {
            uint32_t index = stream[i];
            if (!mesh.texCoordIndices.empty() && i < mesh.texCoordIndices.size()) {
                index = mesh.texCoordIndices[i];
            }
            if (index < mesh.texCoords.size()) {
                texCoords->point.set1Value(static_cast<int>(i),
                                           mesh.texCoords[index].x,
                                           mesh.texCoords[index].y);
            }
        }
        sep->addChild(texCoords);
    }

    if (!mesh.vertexColors.empty()) {
        SoMaterial * material = new SoMaterial;
        for (size_t i = 0; i < stream.size(); ++i) {
            const uint32_t index = stream[i];
            if (index < mesh.vertexColors.size()) {
                const Color & color = mesh.vertexColors[index];
                material->diffuseColor.set1Value(static_cast<int>(i),
                                                 color.r, color.g, color.b);
                material->transparency.set1Value(static_cast<int>(i),
                                                 1.0f - color.a);
            }
        }
        sep->addChild(material);
        SoMaterialBinding * binding = new SoMaterialBinding;
        binding->value.setValue(SoMaterialBinding::PER_VERTEX);
        sep->addChild(binding);
    } else if (!mesh.faceColors.empty()) {
        const std::vector<Color> colors = effectiveFaceColors(mesh);
        SoMaterial * material = createFaceColorMaterial(colors);
        if (material) {
            sep->addChild(material);
            SoMaterialBinding * binding = new SoMaterialBinding;
            binding->value.setValue(colors.size() == 1
                ? SoMaterialBinding::OVERALL
                : SoMaterialBinding::PER_FACE);
            sep->addChild(binding);
        }
    }

    SoQuadMesh * quadMesh = new SoQuadMesh;
    quadMesh->verticesPerRow.setValue(static_cast<int32_t>(mesh.gridVertexColumns));
    quadMesh->verticesPerColumn.setValue(static_cast<int32_t>(mesh.gridVertexRows));
    sep->addChild(quadMesh);

    return sep;
}

SoSeparator * createMeshNode(const Mesh & mesh)
{
    if (hasTriangleStrips(mesh) &&
        mesh.faceColorIndices.empty() &&
        mesh.vertexColorIndices.empty()) {
        return createTriangleStripNode(mesh);
    }

    if (hasQuadGrid(mesh) &&
        mesh.faceColorIndices.empty() &&
        mesh.vertexColorIndices.empty()) {
        const std::vector<uint32_t> stream = meshIndexStream(mesh);
        const size_t required =
            static_cast<size_t>(mesh.gridVertexRows) *
            static_cast<size_t>(mesh.gridVertexColumns);
        if (mesh.gridVertexRows > 0 &&
            mesh.gridVertexColumns > 0 &&
            required <= stream.size()) {
            return createQuadGridNode(mesh);
        }
    }

    SoSeparator * sep = new SoSeparator;

    SoCoordinate3 * coords = new SoCoordinate3;
    for (size_t i = 0; i < mesh.positions.size(); ++i) {
        coords->point.set1Value(static_cast<int>(i), toSbVec3f(mesh.positions[i]));
    }
    sep->addChild(coords);

    if (!mesh.faceNormals.empty()) {
        SoNormal * normals = new SoNormal;
        for (size_t i = 0; i < mesh.faceNormals.size(); ++i) {
            normals->vector.set1Value(static_cast<int>(i),
                                      toSbVec3f(mesh.faceNormals[i]));
        }
        sep->addChild(normals);

        SoNormalBinding * binding = new SoNormalBinding;
        binding->value.setValue(SoNormalBinding::PER_FACE);
        sep->addChild(binding);
    } else if (!mesh.normals.empty()) {
        SoNormal * normals = new SoNormal;
        for (size_t i = 0; i < mesh.normals.size(); ++i) {
            normals->vector.set1Value(static_cast<int>(i), toSbVec3f(mesh.normals[i]));
        }
        sep->addChild(normals);
    }

    if (!mesh.texCoords.empty()) {
        SoTextureCoordinate2 * texCoords = new SoTextureCoordinate2;
        for (size_t i = 0; i < mesh.texCoords.size(); ++i) {
            texCoords->point.set1Value(static_cast<int>(i),
                                       mesh.texCoords[i].x,
                                       mesh.texCoords[i].y);
        }
        sep->addChild(texCoords);
    }

    SoIndexedFaceSet * faces = new SoIndexedFaceSet;
    int out = 0;
    int materialOut = 0;
    int textureOut = 0;
    size_t faceIndex = 0;
    size_t vertexMaterialIndex = 0;
    size_t textureIndex = 0;
    const LegacyMeshMaterialBinding materialBinding = meshMaterialBinding(mesh);
    auto appendFace = [&](const std::vector<uint32_t> & faceIndices) {
        for (uint32_t coordIndex : faceIndices) {
            faces->coordIndex.set1Value(out++, static_cast<int32_t>(coordIndex));
            if (!mesh.texCoordIndices.empty()) {
                uint32_t texCoordIndex = coordIndex;
                if (textureIndex < mesh.texCoordIndices.size()) {
                    texCoordIndex = mesh.texCoordIndices[textureIndex];
                }
                faces->textureCoordIndex.set1Value(textureOut++,
                    static_cast<int32_t>(texCoordIndex));
                ++textureIndex;
            }
            if (materialBinding == LegacyMeshMaterialBinding::PerVertexIndexed) {
                uint32_t materialIndex = coordIndex;
                if (!mesh.vertexColorIndices.empty() &&
                    vertexMaterialIndex < mesh.vertexColorIndices.size()) {
                    materialIndex = mesh.vertexColorIndices[vertexMaterialIndex];
                }
                faces->materialIndex.set1Value(materialOut++,
                    static_cast<int32_t>(materialIndex));
                ++vertexMaterialIndex;
            }
        }
        faces->coordIndex.set1Value(out++, SO_END_FACE_INDEX);
        if (!mesh.texCoordIndices.empty()) {
            faces->textureCoordIndex.set1Value(textureOut++, SO_END_FACE_INDEX);
        }
        if (materialBinding == LegacyMeshMaterialBinding::PerVertexIndexed) {
            faces->materialIndex.set1Value(materialOut++, SO_END_FACE_INDEX);
        } else if (materialBinding == LegacyMeshMaterialBinding::PerFaceIndexed) {
            uint32_t materialIndex = static_cast<uint32_t>(faceIndex);
            if (faceIndex < mesh.faceColorIndices.size()) {
                materialIndex = mesh.faceColorIndices[faceIndex];
            }
            faces->materialIndex.set1Value(static_cast<int>(faceIndex),
                static_cast<int32_t>(materialIndex));
        }
        ++faceIndex;
    };

    if (hasQuadGrid(mesh)) {
        const std::vector<uint32_t> stream = meshIndexStream(mesh);
        const uint32_t rows = mesh.gridVertexRows;
        const uint32_t columns = mesh.gridVertexColumns;
        if (rows >= 2 &&
            columns >= 2 &&
            static_cast<size_t>(rows) * static_cast<size_t>(columns) <= stream.size()) {
            for (uint32_t row = 0; row + 1 < rows; ++row) {
                for (uint32_t column = 0; column + 1 < columns; ++column) {
                    const uint32_t upperLeft = row * columns + column;
                    const uint32_t lowerLeft = (row + 1) * columns + column;
                    const uint32_t lowerRight = lowerLeft + 1;
                    const uint32_t upperRight = upperLeft + 1;
                    appendFace({stream[upperLeft],
                                stream[lowerLeft],
                                stream[lowerRight],
                                stream[upperRight]});
                }
            }
        }
    } else if (hasTriangleStrips(mesh)) {
        const std::vector<uint32_t> stream = meshIndexStream(mesh);
        auto addStrip = [&](size_t start, uint32_t count) {
            if (count < 3 || start + count > stream.size()) {
                return;
            }
            for (uint32_t i = 0; i + 2 < count; ++i) {
                const uint32_t a = stream[start + i];
                const uint32_t b = stream[start + i + 1];
                const uint32_t c = stream[start + i + 2];
                if (i % 2 == 0) {
                    appendFace({b, a, c});
                } else {
                    appendFace({a, b, c});
                }
            }
        };

        if (mesh.stripVertexCounts.empty()) {
            addStrip(0, static_cast<uint32_t>(stream.size()));
        } else {
            size_t index = 0;
            for (uint32_t count : mesh.stripVertexCounts) {
                addStrip(index, count);
                index += count;
                if (index > stream.size()) {
                    break;
                }
            }
        }
    } else if (hasPolygonFaces(mesh)) {
        size_t index = 0;
        for (uint32_t count : mesh.faceVertexCounts) {
            if (count < 3 || index + count > mesh.indices.size()) {
                break;
            }
            std::vector<uint32_t> faceIndices;
            faceIndices.reserve(count);
            for (uint32_t i = 0; i < count; ++i) {
                faceIndices.push_back(mesh.indices[index++]);
            }
            appendFace(faceIndices);
        }
    } else {
        for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
            appendFace({mesh.indices[i], mesh.indices[i + 1], mesh.indices[i + 2]});
        }
    }
    sep->addChild(faces);

    return sep;
}

SoSeparator * createPolylineNode(const Polyline & polyline)
{
    SoSeparator * sep = new SoSeparator;

    if (polyline.lineWidth != 1.0f) {
        SoDrawStyle * drawStyle = new SoDrawStyle;
        drawStyle->lineWidth.setValue(polyline.lineWidth);
        sep->addChild(drawStyle);
    }

    SoCoordinate3 * coords = new SoCoordinate3;
    for (size_t i = 0; i < polyline.points.size(); ++i) {
        coords->point.set1Value(static_cast<int>(i),
                                toSbVec3f(polyline.points[i]));
    }
    sep->addChild(coords);

    SoLineSet * line = new SoLineSet;
    line->numVertices.set1Value(0, static_cast<int32_t>(polyline.points.size()));
    sep->addChild(line);

    return sep;
}

SoSeparator * createPointCloudNode(const PointCloud & pointCloud)
{
    SoSeparator * sep = new SoSeparator;

    SoDrawStyle * style = new SoDrawStyle;
    style->pointSize.setValue(pointCloud.pointSize);
    sep->addChild(style);

    SoCoordinate3 * coords = new SoCoordinate3;
    for (size_t i = 0; i < pointCloud.points.size(); ++i) {
        coords->point.set1Value(static_cast<int>(i),
                                toSbVec3f(pointCloud.points[i]));
    }
    sep->addChild(coords);

    SoPointSet * points = new SoPointSet;
    points->numPoints.setValue(static_cast<int32_t>(pointCloud.points.size()));
    sep->addChild(points);

    return sep;
}

SoSeparator * createTextNode(const Text2D & text)
{
    SoSeparator * sep = new SoSeparator;

    SoFont * font = new SoFont;
    font->name.setValue(text.fontName.c_str());
    font->size.setValue(text.fontSize);
    sep->addChild(font);

    SoText2 * node = new SoText2;
    node->string.setValue(text.text.c_str());
    node->spacing.setValue(text.spacing);
    node->depthTest.setValue(text.depthTest ? TRUE : FALSE);
    switch (text.justification) {
    case TextJustification::Left:
        node->justification.setValue(SoText2::LEFT);
        break;
    case TextJustification::Right:
        node->justification.setValue(SoText2::RIGHT);
        break;
    case TextJustification::Center:
        node->justification.setValue(SoText2::CENTER);
        break;
    }
    sep->addChild(node);

    return sep;
}

SoText3::Justification toLegacyText3Justification(TextJustification justification)
{
    switch (justification) {
    case TextJustification::Left:
        return SoText3::LEFT;
    case TextJustification::Right:
        return SoText3::RIGHT;
    case TextJustification::Center:
        return SoText3::CENTER;
    }
    return SoText3::LEFT;
}

SoSeparator * createText3DNode(const Text3D & text,
                               const Material & baseMaterial)
{
    SoSeparator * sep = new SoSeparator;

    SoFont * font = new SoFont;
    font->name.setValue(text.fontName.c_str());
    font->size.setValue(text.fontSize);
    sep->addChild(font);

    if (!text.partColors.empty()) {
        SoMaterial * material = createFaceColorMaterial(text.partColors);
        if (material) {
            material->specularColor.setValue(baseMaterial.specular.r,
                                             baseMaterial.specular.g,
                                             baseMaterial.specular.b);
            material->shininess.setValue(baseMaterial.shininess);
            sep->addChild(material);
            SoMaterialBinding * binding = new SoMaterialBinding;
            binding->value.setValue(SoMaterialBinding::PER_PART);
            sep->addChild(binding);
        }
    }

    if (!text.profile.empty()) {
        SoProfileCoordinate2 * coords = new SoProfileCoordinate2;
        for (size_t i = 0; i < text.profile.size(); ++i) {
            coords->point.set1Value(static_cast<int>(i),
                                    text.profile[i].x,
                                    text.profile[i].y);
        }
        sep->addChild(coords);

        SoLinearProfile * profile = new SoLinearProfile;
        for (size_t i = 0; i < text.profile.size(); ++i) {
            profile->index.set1Value(static_cast<int>(i),
                                     static_cast<int32_t>(i));
        }
        sep->addChild(profile);
    }

    SoText3 * node = new SoText3;
    node->string.setValue(text.text.c_str());
    node->spacing.setValue(text.spacing);
    node->justification.setValue(toLegacyText3Justification(text.justification));
    node->parts.setValue(static_cast<int>(text.parts));
    sep->addChild(node);

    return sep;
}

} // namespace

Mesh
makeSphereMesh(float radius,
               uint32_t slices,
               uint32_t stacks)
{
    if (radius <= 0.0f) radius = 1.0f;
    if (slices < 3) slices = 3;
    if (stacks < 2) stacks = 2;

    Mesh mesh;
    mesh.topology = MeshTopology::Triangles;
    mesh.positions.reserve((stacks + 1) * (slices + 1));
    mesh.normals.reserve((stacks + 1) * (slices + 1));
    mesh.indices.reserve(static_cast<size_t>(slices) *
                         static_cast<size_t>(stacks - 1) * 6);

    for (uint32_t stack = 0; stack <= stacks; ++stack) {
        const float phi = -0.5f * kPi +
            kPi * static_cast<float>(stack) / static_cast<float>(stacks);
        const float y = std::sin(phi);
        const float ringRadius = std::cos(phi);
        for (uint32_t slice = 0; slice <= slices; ++slice) {
            const float theta = 2.0f * kPi *
                static_cast<float>(slice) / static_cast<float>(slices);
            const float x = ringRadius * std::cos(theta);
            const float z = ringRadius * std::sin(theta);
            mesh.normals.push_back({x, y, z});
            mesh.positions.push_back({radius * x, radius * y, radius * z});
        }
    }

    const uint32_t row = slices + 1;
    for (uint32_t stack = 0; stack < stacks; ++stack) {
        for (uint32_t slice = 0; slice < slices; ++slice) {
            const uint32_t a = stack * row + slice;
            const uint32_t b = a + 1;
            const uint32_t c = (stack + 1) * row + slice;
            const uint32_t d = c + 1;
            if (stack != 0) {
                mesh.indices.push_back(a);
                mesh.indices.push_back(c);
                mesh.indices.push_back(b);
            }
            if (stack + 1 != stacks) {
                mesh.indices.push_back(b);
                mesh.indices.push_back(c);
                mesh.indices.push_back(d);
            }
        }
    }

    return mesh;
}

struct Scene::Impl {
    struct Group {
        Transform transform;
        SceneGroupId parent = RootSceneGroupId;
    };

    enum class CameraKind {
        Perspective,
        Orthographic
    };

    struct Object {
        enum class Kind {
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

        Kind kind = Kind::Primitive;
        Primitive primitive = Primitive::Cube;
        PrimitiveOptions primitiveOptions;
        Mesh mesh;
        Polyline polyline;
        PointCloud pointCloud;
        Material material;
        Transform transform;
        DirectionalLight directionalLight;
        PointLight pointLight;
        SpotLight spotLight;
        Text2D text2D;
        Text3D text3D;
        std::shared_ptr<CadAssembly> cadAssembly;
        OpenGLCallback openGLCallback;
        std::shared_ptr<SoSeparator> legacySceneGraph;
        SceneGroupId parent = RootSceneGroupId;
        bool active = true;

        SceneObjectType publicType() const
        {
            switch (kind) {
            case Kind::Primitive:
                return SceneObjectType::Primitive;
            case Kind::Mesh:
                return SceneObjectType::Mesh;
            case Kind::Polyline:
                return SceneObjectType::Polyline;
            case Kind::PointCloud:
                return SceneObjectType::PointCloud;
            case Kind::DirectionalLight:
                return SceneObjectType::DirectionalLight;
            case Kind::PointLight:
                return SceneObjectType::PointLight;
            case Kind::SpotLight:
                return SceneObjectType::SpotLight;
            case Kind::Text2D:
                return SceneObjectType::Text2D;
            case Kind::Text3D:
                return SceneObjectType::Text3D;
            case Kind::CadAssembly:
                return SceneObjectType::CadAssembly;
            case Kind::OpenGLCallback:
                return SceneObjectType::OpenGLCallback;
            case Kind::LegacySceneGraph:
                return SceneObjectType::LegacySceneGraph;
            }
            return SceneObjectType::Any;
        }
    };

    Impl() = default;

    Impl(const Impl & other)
        : objects(other.objects)
        , groups(other.groups)
        , hasCamera(other.hasCamera)
        , cameraKind(other.cameraKind)
        , camera(other.camera)
        , orthographicCamera(other.orthographicCamera)
    {
        if (other.legacyRoot) {
            SoNode * copy = other.legacyRoot->copy(FALSE);
            legacyRoot = copy && copy->isOfType(SoSeparator::getClassTypeId())
                ? static_cast<SoSeparator *>(copy)
                : nullptr;
            if (legacyRoot) legacyRoot->ref();
        }
    }

    Impl & operator=(const Impl & other)
    {
        if (this == &other) return *this;
        clearLegacyRoot();
        objects = other.objects;
        groups = other.groups;
        hasCamera = other.hasCamera;
        cameraKind = other.cameraKind;
        camera = other.camera;
        orthographicCamera = other.orthographicCamera;
        if (other.legacyRoot) {
            SoNode * copy = other.legacyRoot->copy(FALSE);
            legacyRoot = copy && copy->isOfType(SoSeparator::getClassTypeId())
                ? static_cast<SoSeparator *>(copy)
                : nullptr;
            if (legacyRoot) legacyRoot->ref();
        }
        return *this;
    }

    ~Impl()
    {
        clearLegacyRoot();
    }

    void setLegacyRoot(const SoSeparator & root)
    {
        clearLegacyRoot();
        SoNode * copy = root.copy(FALSE);
        legacyRoot = copy && copy->isOfType(SoSeparator::getClassTypeId())
            ? static_cast<SoSeparator *>(copy)
            : nullptr;
        if (legacyRoot) legacyRoot->ref();
    }

    void clearLegacyRoot()
    {
        if (legacyRoot) {
            legacyRoot->unref();
            legacyRoot = nullptr;
        }
    }

    std::vector<Object> objects;
    std::vector<Group> groups;
    SoSeparator * legacyRoot = nullptr;
    bool hasCamera = false;
    CameraKind cameraKind = CameraKind::Perspective;
    PerspectiveCamera camera;
    OrthographicCamera orthographicCamera;
};

bool matchesQuery(SceneObjectType type, const SceneQuery & query)
{
    if (query.type != SceneObjectType::Any && query.type != type) {
        return false;
    }
    if (query.category != SceneObjectCategory::Any &&
        query.category != categoryForType(type)) {
        return false;
    }
    return true;
}

Scene::Scene()
    : impl_(new Impl)
{
}

Scene::Scene(const Scene & other)
    : impl_(new Impl(*other.impl_))
{
}

Scene::Scene(Scene && other) noexcept = default;

Scene::~Scene() = default;

Scene &
Scene::operator=(const Scene & other)
{
    if (this != &other) {
        *impl_ = *other.impl_;
    }
    return *this;
}

Scene &
Scene::operator=(Scene && other) noexcept = default;

SceneGroupId
Scene::addGroup(const Transform & transform,
                SceneGroupId parent)
{
    Impl::Group group;
    group.transform = transform;
    group.parent = parent <= impl_->groups.size() ? parent : RootSceneGroupId;
    impl_->groups.push_back(group);
    return static_cast<SceneGroupId>(impl_->groups.size());
}

bool
Scene::setGroupTransform(SceneGroupId group,
                         const Transform & transform)
{
    if (group == RootSceneGroupId ||
        group == InvalidSceneGroupId ||
        group > impl_->groups.size()) {
        return false;
    }
    impl_->groups[group - 1].transform = transform;
    return true;
}

SceneObjectId
Scene::addPrimitive(Primitive primitive,
                    const Material & material,
                    const Transform & transform,
                    const PrimitiveOptions & options,
                    SceneGroupId parent)
{
    Impl::Object object;
    object.kind = Impl::Object::Kind::Primitive;
    object.primitive = primitive;
    object.primitiveOptions = options;
    object.material = material;
    object.transform = transform;
    object.parent = parent <= impl_->groups.size() ? parent : RootSceneGroupId;
    impl_->objects.push_back(object);
    return static_cast<SceneObjectId>(impl_->objects.size());
}

SceneObjectId
Scene::addMesh(const Mesh & mesh,
               const Material & material,
               const Transform & transform,
               SceneGroupId parent)
{
    Impl::Object object;
    object.kind = Impl::Object::Kind::Mesh;
    object.mesh = mesh;
    object.material = material;
    object.transform = transform;
    object.parent = parent <= impl_->groups.size() ? parent : RootSceneGroupId;
    impl_->objects.push_back(object);
    return static_cast<SceneObjectId>(impl_->objects.size());
}

SceneObjectId
Scene::addPolyline(const Polyline & polyline,
                   const Material & material,
                   const Transform & transform,
                   SceneGroupId parent)
{
    Impl::Object object;
    object.kind = Impl::Object::Kind::Polyline;
    object.polyline = polyline;
    object.material = material;
    object.transform = transform;
    object.parent = parent <= impl_->groups.size() ? parent : RootSceneGroupId;
    impl_->objects.push_back(object);
    return static_cast<SceneObjectId>(impl_->objects.size());
}

SceneObjectId
Scene::addPointCloud(const PointCloud & pointCloud,
                     const Material & material,
                     const Transform & transform,
                     SceneGroupId parent)
{
    Impl::Object object;
    object.kind = Impl::Object::Kind::PointCloud;
    object.pointCloud = pointCloud;
    object.material = material;
    object.transform = transform;
    object.parent = parent <= impl_->groups.size() ? parent : RootSceneGroupId;
    impl_->objects.push_back(object);
    return static_cast<SceneObjectId>(impl_->objects.size());
}

SceneObjectId
Scene::addDirectionalLight(const DirectionalLight & light,
                           SceneGroupId parent)
{
    Impl::Object object;
    object.kind = Impl::Object::Kind::DirectionalLight;
    object.directionalLight = light;
    object.parent = parent <= impl_->groups.size() ? parent : RootSceneGroupId;
    impl_->objects.push_back(object);
    return static_cast<SceneObjectId>(impl_->objects.size());
}

SceneObjectId
Scene::addPointLight(const PointLight & light,
                     SceneGroupId parent)
{
    Impl::Object object;
    object.kind = Impl::Object::Kind::PointLight;
    object.pointLight = light;
    object.parent = parent <= impl_->groups.size() ? parent : RootSceneGroupId;
    impl_->objects.push_back(object);
    return static_cast<SceneObjectId>(impl_->objects.size());
}

SceneObjectId
Scene::addSpotLight(const SpotLight & light,
                    SceneGroupId parent)
{
    Impl::Object object;
    object.kind = Impl::Object::Kind::SpotLight;
    object.spotLight = light;
    object.parent = parent <= impl_->groups.size() ? parent : RootSceneGroupId;
    impl_->objects.push_back(object);
    return static_cast<SceneObjectId>(impl_->objects.size());
}

SceneObjectId
Scene::addText2D(const Text2D & text,
                 const Material & material,
                 const Transform & transform,
                 SceneGroupId parent)
{
    Impl::Object object;
    object.kind = Impl::Object::Kind::Text2D;
    object.text2D = text;
    object.material = material;
    object.transform = transform;
    object.parent = parent <= impl_->groups.size() ? parent : RootSceneGroupId;
    impl_->objects.push_back(object);
    return static_cast<SceneObjectId>(impl_->objects.size());
}

SceneObjectId
Scene::addText3D(const Text3D & text,
                 const Material & material,
                 const Transform & transform,
                 SceneGroupId parent)
{
    Impl::Object object;
    object.kind = Impl::Object::Kind::Text3D;
    object.text3D = text;
    object.material = material;
    object.transform = transform;
    object.parent = parent <= impl_->groups.size() ? parent : RootSceneGroupId;
    impl_->objects.push_back(object);
    return static_cast<SceneObjectId>(impl_->objects.size());
}

SceneObjectId
Scene::addCadAssembly(const CadAssembly & assembly,
                      const Transform & transform,
                      SceneGroupId parent)
{
    Impl::Object object;
    object.kind = Impl::Object::Kind::CadAssembly;
    object.cadAssembly = std::make_shared<CadAssembly>(assembly);
    object.transform = transform;
    object.parent = parent <= impl_->groups.size() ? parent : RootSceneGroupId;
    impl_->objects.push_back(object);
    return static_cast<SceneObjectId>(impl_->objects.size());
}

SceneObjectId
Scene::addOpenGLCallback(const OpenGLCallback & callback,
                         SceneGroupId parent)
{
    if (!callback.draw) {
        return InvalidSceneObjectId;
    }

    Impl::Object object;
    object.kind = Impl::Object::Kind::OpenGLCallback;
    object.openGLCallback = callback;
    object.parent = parent <= impl_->groups.size() ? parent : RootSceneGroupId;
    impl_->objects.push_back(object);
    return static_cast<SceneObjectId>(impl_->objects.size());
}

SceneObjectId
Scene::addLegacySceneGraph(NativeSceneGraphHandle rootHandle,
                           const Transform & transform,
                           SceneGroupId parent)
{
    SoSeparator * root = static_cast<SoSeparator *>(rootHandle);
    if (!root) {
        return InvalidSceneObjectId;
    }

    std::shared_ptr<SoSeparator> copy = copyLegacySeparator(*root);
    if (!copy) {
        return InvalidSceneObjectId;
    }

    Impl::Object object;
    object.kind = Impl::Object::Kind::LegacySceneGraph;
    object.legacySceneGraph = copy;
    object.transform = transform;
    object.parent = parent <= impl_->groups.size() ? parent : RootSceneGroupId;
    impl_->objects.push_back(object);
    return static_cast<SceneObjectId>(impl_->objects.size());
}

bool
Scene::setObjectTransform(SceneObjectId object,
                          const Transform & transform)
{
    if (object == InvalidSceneObjectId ||
        object > impl_->objects.size()) {
        return false;
    }
    Impl::Object & target = impl_->objects[object - 1];
    if (!target.active) {
        return false;
    }
    target.transform = transform;
    return true;
}

bool
Scene::setObjectMaterial(SceneObjectId object,
                         const Material & material)
{
    if (object == InvalidSceneObjectId ||
        object > impl_->objects.size()) {
        return false;
    }
    Impl::Object & target = impl_->objects[object - 1];
    if (!target.active) {
        return false;
    }
    switch (target.kind) {
    case Impl::Object::Kind::Primitive:
    case Impl::Object::Kind::Mesh:
    case Impl::Object::Kind::Polyline:
    case Impl::Object::Kind::PointCloud:
    case Impl::Object::Kind::Text2D:
    case Impl::Object::Kind::Text3D:
    case Impl::Object::Kind::LegacySceneGraph:
        target.material = material;
        return true;
    case Impl::Object::Kind::DirectionalLight:
    case Impl::Object::Kind::PointLight:
    case Impl::Object::Kind::SpotLight:
    case Impl::Object::Kind::CadAssembly:
    case Impl::Object::Kind::OpenGLCallback:
        break;
    }
    return false;
}

bool
Scene::setObjectPrimitiveOptions(SceneObjectId object,
                                 const PrimitiveOptions & options)
{
    if (object == InvalidSceneObjectId ||
        object > impl_->objects.size()) {
        return false;
    }
    Impl::Object & target = impl_->objects[object - 1];
    if (!target.active) {
        return false;
    }
    if (target.kind != Impl::Object::Kind::Primitive) {
        return false;
    }
    target.primitiveOptions = options;
    return true;
}

bool
Scene::setObjectPointCloud(SceneObjectId object,
                           const PointCloud & pointCloud)
{
    if (object == InvalidSceneObjectId ||
        object > impl_->objects.size()) {
        return false;
    }
    Impl::Object & target = impl_->objects[object - 1];
    if (!target.active) {
        return false;
    }
    if (target.kind != Impl::Object::Kind::PointCloud) {
        return false;
    }
    target.pointCloud = pointCloud;
    return true;
}

bool
Scene::removeObject(SceneObjectId object)
{
    if (object == InvalidSceneObjectId ||
        object > impl_->objects.size()) {
        return false;
    }
    Impl::Object & target = impl_->objects[object - 1];
    if (!target.active) {
        return false;
    }
    target.active = false;
    return true;
}

void
Scene::setCamera(const PerspectiveCamera & camera)
{
    impl_->camera = camera;
    impl_->cameraKind = Impl::CameraKind::Perspective;
    impl_->hasCamera = true;
}

void
Scene::setCamera(const OrthographicCamera & camera)
{
    impl_->orthographicCamera = camera;
    impl_->cameraKind = Impl::CameraKind::Orthographic;
    impl_->hasCamera = true;
}

void
Scene::clearCamera()
{
    impl_->hasCamera = false;
    impl_->cameraKind = Impl::CameraKind::Perspective;
}

bool
Scene::hasCamera() const
{
    return impl_->hasCamera;
}

bool
Scene::empty() const
{
    for (const Impl::Object & object : impl_->objects) {
        if (object.active) {
            return false;
        }
    }
    return impl_->groups.empty() && !impl_->legacyRoot;
}

size_t
Scene::objectCount() const
{
    size_t count = 0;
    for (const Impl::Object & object : impl_->objects) {
        if (object.active) {
            ++count;
        }
    }
    return count;
}

size_t
Scene::groupCount() const
{
    return impl_->groups.size();
}

std::vector<SceneObjectInfo>
Scene::findObjects(const SceneQuery & query) const
{
    std::vector<SceneObjectInfo> results;
    for (size_t i = 0; i < impl_->objects.size(); ++i) {
        const Impl::Object & object = impl_->objects[i];
        if (!object.active) {
            continue;
        }
        const SceneObjectType type = object.publicType();
        if (!matchesQuery(type, query)) {
            continue;
        }
        SceneObjectInfo info;
        info.id = static_cast<SceneObjectId>(i + 1);
        info.type = type;
        info.parent = object.parent;
        results.push_back(info);
    }
    return results;
}

SceneObjectId
Scene::findFirstObject(const SceneQuery & query) const
{
    for (size_t i = 0; i < impl_->objects.size(); ++i) {
        if (!impl_->objects[i].active) {
            continue;
        }
        const SceneObjectType type = impl_->objects[i].publicType();
        if (matchesQuery(type, query)) {
            return static_cast<SceneObjectId>(i + 1);
        }
    }
    return InvalidSceneObjectId;
}

bool
Scene::hasObjects(const SceneQuery & query) const
{
    return findFirstObject(query) != InvalidSceneObjectId;
}

ScenePacket
Scene::capturePacket() const
{
    ScenePacket packet;
    std::vector<SbMatrix> groupWorldMatrices;
    groupWorldMatrices.reserve(impl_->groups.size());
    packet.groups.reserve(impl_->groups.size());
    for (size_t i = 0; i < impl_->groups.size(); ++i) {
        const Impl::Group & group = impl_->groups[i];
        SbMatrix worldMatrix = transformMatrix(group.transform);
        if (group.parent != RootSceneGroupId &&
            group.parent <= groupWorldMatrices.size()) {
            worldMatrix.multLeft(groupWorldMatrices[group.parent - 1]);
        }
        groupWorldMatrices.push_back(worldMatrix);

        SceneGroupRecord record;
        record.id = static_cast<SceneGroupId>(i + 1);
        record.parent = group.parent;
        record.transform = group.transform;
        record.localToWorld = toMatrix4(worldMatrix);
        packet.groups.push_back(record);
    }

    const auto parentWorldMatrix = [&](SceneGroupId parent) -> SbMatrix {
        if (parent != RootSceneGroupId && parent <= groupWorldMatrices.size()) {
            return groupWorldMatrices[parent - 1];
        }
        return SbMatrix::identity();
    };

    if (impl_->hasCamera && impl_->cameraKind == Impl::CameraKind::Perspective) {
        packet.camera.kind = SceneCameraKind::Perspective;
        packet.camera.perspective = impl_->camera;
    } else if (impl_->hasCamera &&
               impl_->cameraKind == Impl::CameraKind::Orthographic) {
        packet.camera.kind = SceneCameraKind::Orthographic;
        packet.camera.orthographic = impl_->orthographicCamera;
    }

    packet.objects.reserve(objectCount());
    for (size_t i = 0; i < impl_->objects.size(); ++i) {
        const Impl::Object & object = impl_->objects[i];
        if (!object.active) {
            continue;
        }

        SceneObjectRecord record;
        record.id = static_cast<SceneObjectId>(i + 1);
        record.type = object.publicType();
        record.category = categoryForType(record.type);
        record.parent = object.parent;
        record.transform = object.transform;
        SbMatrix objectWorldMatrix = transformMatrix(object.transform);
        objectWorldMatrix.multLeft(parentWorldMatrix(object.parent));
        record.localToWorld = toMatrix4(objectWorldMatrix);
        record.material = object.material;
        record.primitive = object.primitive;
        record.primitiveOptions = object.primitiveOptions;
        record.mesh = object.mesh;
        record.polyline = object.polyline;
        record.pointCloud = object.pointCloud;
        record.directionalLight = object.directionalLight;
        record.pointLight = object.pointLight;
        record.spotLight = object.spotLight;
        record.text2D = object.text2D;
        record.text3D = object.text3D;
        record.cadAssembly = object.cadAssembly;
        record.openGLCallback = object.openGLCallback;
        record.hasLegacySceneGraph =
            object.kind == Impl::Object::Kind::LegacySceneGraph &&
            object.legacySceneGraph != nullptr;
        packet.objects.push_back(record);
    }
    packet.hasLegacyFallbackRoot = impl_->legacyRoot != nullptr;
    return packet;
}

void
Scene::clear()
{
    impl_->objects.clear();
    impl_->groups.clear();
    impl_->clearLegacyRoot();
    impl_->hasCamera = false;
    impl_->cameraKind = Impl::CameraKind::Perspective;
}

NativeSceneGraphHandle
Scene::createLegacySceneGraph() const
{
    SoSeparator * legacyRoot = nullptr;
    if (impl_->legacyRoot) {
        SoNode * copy = impl_->legacyRoot->copy(FALSE);
        legacyRoot = copy && copy->isOfType(SoSeparator::getClassTypeId())
            ? static_cast<SoSeparator *>(copy)
            : nullptr;
    }
    SoSeparator * root = new SoSeparator;
    root->ref();

    if (impl_->hasCamera && impl_->cameraKind == Impl::CameraKind::Perspective) {
        SoPerspectiveCamera * camera = new SoPerspectiveCamera;
        camera->position.setValue(toSbVec3f(impl_->camera.position));
        camera->pointAt(toSbVec3f(impl_->camera.target), toSbVec3f(impl_->camera.up));
        camera->heightAngle.setValue(impl_->camera.verticalFieldOfViewRadians);
        camera->nearDistance.setValue(impl_->camera.nearDistance);
        camera->farDistance.setValue(impl_->camera.farDistance);
        root->addChild(camera);
    } else if (impl_->hasCamera && impl_->cameraKind == Impl::CameraKind::Orthographic) {
        SoOrthographicCamera * camera = new SoOrthographicCamera;
        camera->position.setValue(toSbVec3f(impl_->orthographicCamera.position));
        camera->pointAt(toSbVec3f(impl_->orthographicCamera.target),
                        toSbVec3f(impl_->orthographicCamera.up));
        camera->height.setValue(impl_->orthographicCamera.height);
        camera->nearDistance.setValue(impl_->orthographicCamera.nearDistance);
        camera->farDistance.setValue(impl_->orthographicCamera.farDistance);
        root->addChild(camera);
    }

    std::vector<SbMatrix> groupWorldMatrices;
    groupWorldMatrices.reserve(impl_->groups.size());
    for (size_t i = 0; i < impl_->groups.size(); ++i) {
        const Impl::Group & group = impl_->groups[i];
        SbMatrix matrix = transformMatrix(group.transform);
        if (group.parent != RootSceneGroupId &&
            group.parent <= groupWorldMatrices.size()) {
            matrix.multLeft(groupWorldMatrices[group.parent - 1]);
        }
        groupWorldMatrices.push_back(matrix);
    }

    const auto parentWorldMatrix = [&](SceneGroupId parent) -> SbMatrix {
        if (parent != RootSceneGroupId && parent <= groupWorldMatrices.size()) {
            return groupWorldMatrices[parent - 1];
        }
        return SbMatrix::identity();
    };

    const auto transformPoint = [](const SbMatrix & matrix, const Vec3 & point) -> SbVec3f {
        SbVec3f result;
        matrix.multVecMatrix(toSbVec3f(point), result);
        return result;
    };

    const auto transformDirection = [](const SbMatrix & matrix, const Vec3 & direction) -> SbVec3f {
        SbVec3f result;
        matrix.multDirMatrix(toSbVec3f(direction), result);
        if (result.length() > 0.0f) {
            result.normalize();
        }
        return result;
    };

    for (size_t i = 0; i < impl_->objects.size(); ++i) {
        const Impl::Object & object = impl_->objects[i];
        if (!object.active) {
            continue;
        }
        const SceneObjectId objectId = static_cast<SceneObjectId>(i + 1);
        const SbMatrix parentMatrix = parentWorldMatrix(object.parent);
        if (object.kind == Impl::Object::Kind::DirectionalLight) {
            SoDirectionalLight * light = new SoDirectionalLight;
            const std::string name = legacyObjectName(objectId);
            light->setName(SbName(name.c_str()));
            light->direction.setValue(transformDirection(parentMatrix,
                                                        object.directionalLight.direction));
            light->color.setValue(object.directionalLight.color.r,
                                  object.directionalLight.color.g,
                                  object.directionalLight.color.b);
            light->intensity.setValue(object.directionalLight.intensity);
            root->addChild(light);
        } else if (object.kind == Impl::Object::Kind::PointLight) {
            SoPointLight * light = new SoPointLight;
            const std::string name = legacyObjectName(objectId);
            light->setName(SbName(name.c_str()));
            light->location.setValue(transformPoint(parentMatrix,
                                                   object.pointLight.location));
            light->color.setValue(object.pointLight.color.r,
                                  object.pointLight.color.g,
                                  object.pointLight.color.b);
            light->intensity.setValue(object.pointLight.intensity);
            root->addChild(light);
        } else if (object.kind == Impl::Object::Kind::SpotLight) {
            SoSpotLight * light = new SoSpotLight;
            const std::string name = legacyObjectName(objectId);
            light->setName(SbName(name.c_str()));
            light->location.setValue(transformPoint(parentMatrix,
                                                   object.spotLight.location));
            light->direction.setValue(transformDirection(parentMatrix,
                                                        object.spotLight.direction));
            light->color.setValue(object.spotLight.color.r,
                                  object.spotLight.color.g,
                                  object.spotLight.color.b);
            light->intensity.setValue(object.spotLight.intensity);
            light->cutOffAngle.setValue(object.spotLight.cutOffAngleRadians);
            light->dropOffRate.setValue(object.spotLight.dropOffRate);
            root->addChild(light);
        }
    }

    if (legacyRoot) {
        root->addChild(legacyRoot);
    }

    std::vector<SoSeparator *> groupNodes;
    groupNodes.reserve(impl_->groups.size());
    for (size_t i = 0; i < impl_->groups.size(); ++i) {
        const Impl::Group & group = impl_->groups[i];
        const SceneGroupId groupId = static_cast<SceneGroupId>(i + 1);
        SoSeparator * sep = new SoSeparator;
        const std::string name = legacyGroupName(groupId);
        sep->setName(SbName(name.c_str()));
        sep->addChild(createTransform(group.transform));

        SoSeparator * parent = root;
        if (group.parent != RootSceneGroupId &&
            group.parent <= groupNodes.size()) {
            parent = groupNodes[group.parent - 1];
        }
        parent->addChild(sep);
        groupNodes.push_back(sep);
    }

    for (size_t i = 0; i < impl_->objects.size(); ++i) {
        const Impl::Object & object = impl_->objects[i];
        if (!object.active) {
            continue;
        }
        const SceneObjectId objectId = static_cast<SceneObjectId>(i + 1);
        SoSeparator * parent = root;
        if (object.parent != RootSceneGroupId &&
            object.parent <= groupNodes.size()) {
            parent = groupNodes[object.parent - 1];
        }
        if (object.kind == Impl::Object::Kind::DirectionalLight ||
            object.kind == Impl::Object::Kind::PointLight ||
            object.kind == Impl::Object::Kind::SpotLight) {
            continue;
        }

        if (object.kind == Impl::Object::Kind::OpenGLCallback) {
            SoCallback * callback = new SoCallback;
            callback->setCallback(invokeOpenGLCallback,
                                  const_cast<OpenGLCallback *>(&object.openGLCallback));
            parent->addChild(callback);
            continue;
        }

        SoSeparator * sep = new SoSeparator;
        const std::string name = legacyObjectName(objectId);
        sep->setName(SbName(name.c_str()));
        sep->addChild(createTransform(object.transform));
        if (object.kind == Impl::Object::Kind::CadAssembly) {
            if (object.cadAssembly) {
                sep->addChild(static_cast<SoNode *>(
                    object.cadAssembly->createLegacyNode()));
            }
            parent->addChild(sep);
            continue;
        }
        if (object.kind == Impl::Object::Kind::LegacySceneGraph) {
            sep->addChild(createMaterial(object.material));
            if (object.legacySceneGraph) {
                SoNode * copy = object.legacySceneGraph->copy(FALSE);
                SoSeparator * legacy =
                    copy && copy->isOfType(SoSeparator::getClassTypeId())
                        ? static_cast<SoSeparator *>(copy)
                        : nullptr;
                if (legacy) {
                    sep->addChild(legacy);
                }
            }
            parent->addChild(sep);
            continue;
        }
        if (object.material.baseColorTexture) {
            SoTexture2 * texture = createTexture(*object.material.baseColorTexture);
            if (texture) sep->addChild(texture);
        }
        if (object.material.unlit) {
            SoLightModel * lightModel = new SoLightModel;
            lightModel->model.setValue(SoLightModel::BASE_COLOR);
            sep->addChild(lightModel);
        }
        if (object.kind == Impl::Object::Kind::Mesh) {
            const LegacyMeshMaterialBinding meshBinding =
                meshMaterialBinding(object.mesh);
            SoMaterial * meshMaterial =
                createFaceColorMaterial(meshMaterialColors(object.mesh));
            if (meshBinding != LegacyMeshMaterialBinding::None && meshMaterial) {
                sep->addChild(meshMaterial);
                SoMaterialBinding * binding = new SoMaterialBinding;
                binding->value.setValue(toLegacyMaterialBinding(meshBinding));
                sep->addChild(binding);
            } else {
                sep->addChild(createMaterial(object.material));
            }
        } else {
            sep->addChild(createMaterial(object.material));
        }
        if (object.kind == Impl::Object::Kind::Primitive) {
            sep->addChild(createPrimitiveNode(object.primitive, object.primitiveOptions));
        } else if (object.kind == Impl::Object::Kind::Mesh) {
            sep->addChild(createMeshNode(object.mesh));
        } else if (object.kind == Impl::Object::Kind::Polyline) {
            sep->addChild(createPolylineNode(object.polyline));
        } else if (object.kind == Impl::Object::Kind::PointCloud) {
            sep->addChild(createPointCloudNode(object.pointCloud));
        } else if (object.kind == Impl::Object::Kind::Text2D) {
            sep->addChild(createTextNode(object.text2D));
        } else if (object.kind == Impl::Object::Kind::Text3D) {
            sep->addChild(createText3DNode(object.text3D, object.material));
        }
        parent->addChild(sep);
    }

    return root;
}

void
Scene::releaseLegacySceneGraph(NativeSceneGraphHandle rootHandle)
{
    SoSeparator * root = static_cast<SoSeparator *>(rootHandle);
    if (root) {
        root->unref();
    }
}

Scene
Scene::fromLegacySceneGraph(NativeSceneGraphHandle rootHandle)
{
    Scene scene;
    SoSeparator * root = static_cast<SoSeparator *>(rootHandle);
    if (!root) {
        return scene;
    }
    if (tryExtractLegacyScene(*root, scene)) {
        return scene;
    }

    scene.impl_->setLegacyRoot(*root);
    return scene;
}

} // namespace obol
