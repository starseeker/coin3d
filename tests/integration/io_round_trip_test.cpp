#include <gtest/gtest.h>

#include <Inventor/SoDB.h>
#include <Inventor/SoInput.h>
#include <Inventor/SoOutput.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/actions/SoWriteAction.h>
#include <Inventor/fields/SoField.h>
#include <Inventor/fields/SoSFFloat.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/sensors/SoSensorManager.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <utility>

namespace {

char * active_buffer = nullptr;

void * growBuffer(void * pointer, const size_t size)
{
    active_buffer = static_cast<char *>(std::realloc(pointer, size));
    return active_buffer;
}

class OwnedBuffer final {
public:
    OwnedBuffer() = default;
    OwnedBuffer(char * data, const size_t size) : data_(data), size_(size) {}
    ~OwnedBuffer() { std::free(data_); }

    OwnedBuffer(const OwnedBuffer &) = delete;
    OwnedBuffer & operator=(const OwnedBuffer &) = delete;
    OwnedBuffer(OwnedBuffer && other) noexcept
        : data_(std::exchange(other.data_, nullptr)),
          size_(std::exchange(other.size_, 0)) {}
    OwnedBuffer & operator=(OwnedBuffer && other) noexcept
    {
        if (this != &other) {
            std::free(data_);
            data_ = std::exchange(other.data_, nullptr);
            size_ = std::exchange(other.size_, 0);
        }
        return *this;
    }

    void * data() const { return data_; }
    size_t size() const { return size_; }

private:
    char * data_ = nullptr;
    size_t size_ = 0;
};

class TemporaryDirectory final {
public:
    TemporaryDirectory()
    {
        const auto root = std::filesystem::temp_directory_path();
        const auto timestamp = std::chrono::high_resolution_clock::now()
                               .time_since_epoch()
                               .count();
        for (int attempt = 0; attempt < 16; ++attempt) {
            const auto candidate = root / ("obol-gtest-" + std::to_string(timestamp) +
                                           "-" + std::to_string(attempt));
            if (std::filesystem::create_directory(candidate)) {
                path_ = candidate;
                break;
            }
        }
    }

    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    const std::filesystem::path & path() const { return path_; }

private:
    std::filesystem::path path_;
};

OwnedBuffer writeScene(SoSeparator * root)
{
    active_buffer = nullptr;
    void * buffer = nullptr;
    size_t size = 0;
    {
        SoOutput output;
        output.setBuffer(nullptr, 1, growBuffer);
        SoWriteAction action(&output);
        action.apply(root);
        EXPECT_TRUE(output.getBuffer(buffer, size));
    }
    active_buffer = nullptr;
    return OwnedBuffer(static_cast<char *>(buffer), size);
}

} // namespace

TEST(SceneIo, WriteActionRoundTripsNamedNodeAndFieldValuesInMemory)
{
    auto * original = new SoSeparator;
    original->ref();
    auto * cube = new SoCube;
    cube->width = 3.0f;
    cube->setName("modern-io-cube");
    original->addChild(cube);

    const OwnedBuffer serialized = writeScene(original);
    original->unref();
    ASSERT_NE(serialized.data(), nullptr);
    ASSERT_GT(serialized.size(), 0u);

    SoInput input;
    input.setBuffer(serialized.data(), serialized.size());
    SoSeparator * loaded = SoDB::readAll(&input);
    ASSERT_NE(loaded, nullptr);
    loaded->ref();
    ASSERT_EQ(loaded->getNumChildren(), 1);

    SoSearchAction search;
    search.setName("modern-io-cube");
    search.setFind(SoSearchAction::NAME);
    search.apply(loaded);
    ASSERT_NE(search.getPath(), nullptr);
    auto * loaded_cube = static_cast<SoCube *>(search.getPath()->getTail());
    EXPECT_FLOAT_EQ(loaded_cube->width.getValue(), 3.0f);
    loaded->unref();
}

TEST(SceneIo, DatabaseHeaderValidationAndVersionAreAvailable)
{
    EXPECT_TRUE(SoDB::isValidHeader("#Inventor V2.1 ascii"));
    EXPECT_FALSE(SoDB::isValidHeader("not a valid header"));
    const char * version = SoDB::getVersion();
    ASSERT_NE(version, nullptr);
    EXPECT_NE(version[0], '\0');

    EXPECT_NE(SoDB::getGlobalField("realTime"), nullptr);
    auto * global_float = static_cast<SoSFFloat *>(
        SoDB::createGlobalField("modernSceneIoGlobalFloat", SoSFFloat::getClassTypeId()));
    ASSERT_NE(global_float, nullptr);
    global_float->setValue(99.0f);
    EXPECT_FLOAT_EQ(global_float->getValue(), 99.0f);
    EXPECT_EQ(SoDB::getGlobalField("modernSceneIoGlobalFloat"), global_float);
    EXPECT_NE(SoDB::getSensorManager(), nullptr);
}

TEST(SceneIo, ParsesAsciiScenesAndRoundTripsThroughAnIsolatedFile)
{
    constexpr char kScene[] =
        "#Inventor V2.1 ascii\n"
        "Separator {\n"
        "  Sphere { radius 2.5 }\n"
        "  Cube { width 4 height 3 depth 2 }\n"
        "}\n";
    SoInput scene_input;
    scene_input.setBuffer(kScene, sizeof(kScene) - 1);
    auto * parsed = SoDB::readAll(&scene_input);
    ASSERT_NE(parsed, nullptr);
    parsed->ref();
    EXPECT_EQ(parsed->getNumChildren(), 2);
    SoGetBoundingBoxAction parsed_bounds(SbViewportRegion(512, 512));
    parsed_bounds.apply(parsed);
    EXPECT_FALSE(parsed_bounds.getBoundingBox().isEmpty());
    parsed->unref();

    constexpr char kTranslatedScene[] =
        "#Inventor V2.1 ascii\n"
        "Separator { Transform { translation 1 2 3 } Sphere { } }\n";
    SoInput translated_input;
    translated_input.setBuffer(kTranslatedScene, sizeof(kTranslatedScene) - 1);
    auto * translated = SoDB::readAll(&translated_input);
    ASSERT_NE(translated, nullptr);
    translated->ref();
    SoGetBoundingBoxAction translated_bounds(SbViewportRegion(512, 512));
    translated_bounds.apply(translated);
    ASSERT_FALSE(translated_bounds.getBoundingBox().isEmpty());
    EXPECT_NEAR(translated_bounds.getBoundingBox().getCenter()[0], 1.0f, 0.1f);
    EXPECT_NEAR(translated_bounds.getBoundingBox().getCenter()[1], 2.0f, 0.1f);
    translated->unref();

    auto * original = new SoSeparator;
    original->ref();
    auto * cube = new SoCube;
    cube->width.setValue(3.0f);
    original->addChild(cube);
    TemporaryDirectory temporary_directory;
    ASSERT_FALSE(temporary_directory.path().empty());
    const auto file_path = temporary_directory.path() / "round-trip.iv";
    SoOutput output;
    ASSERT_TRUE(output.openFile(file_path.string().c_str()));
    SoWriteAction write_action(&output);
    write_action.apply(original);
    output.closeFile();
    original->unref();
    ASSERT_TRUE(std::filesystem::exists(file_path));
    EXPECT_GT(std::filesystem::file_size(file_path), 0u);

    SoInput file_input;
    ASSERT_TRUE(file_input.openFile(file_path.string().c_str()));
    auto * loaded = SoDB::readAll(&file_input);
    ASSERT_NE(loaded, nullptr);
    loaded->ref();
    ASSERT_EQ(loaded->getNumChildren(), 1);
    auto * loaded_cube = static_cast<SoCube *>(loaded->getChild(0));
    EXPECT_FLOAT_EQ(loaded_cube->width.getValue(), 3.0f);
    loaded->unref();
}

TEST(SceneIo, OutputModesAndBinaryRoundTripRetainSceneStructure)
{
    SoOutput output;
    EXPECT_FALSE(output.isBinary());
    output.setBinary(TRUE);
    EXPECT_TRUE(output.isBinary());
    output.setBinary(FALSE);
    EXPECT_FALSE(output.isBinary());
    output.setHeaderString("#VRML V2.0 utf8");
    output.resetHeaderString();

    auto * original = new SoSeparator;
    original->ref();
    auto * cube = new SoCube;
    cube->height.setValue(4.0f);
    original->addChild(cube);
    TemporaryDirectory temporary_directory;
    ASSERT_FALSE(temporary_directory.path().empty());
    const auto file_path = temporary_directory.path() / "binary-round-trip.iv";
    output.setBinary(TRUE);
    ASSERT_TRUE(output.openFile(file_path.string().c_str()));
    SoWriteAction write_action(&output);
    write_action.apply(original);
    output.closeFile();
    original->unref();

    ASSERT_TRUE(std::filesystem::exists(file_path));
    EXPECT_GT(std::filesystem::file_size(file_path), 0u);
    SoInput input;
    ASSERT_TRUE(input.openFile(file_path.string().c_str()));
    auto * loaded = SoDB::readAll(&input);
    ASSERT_NE(loaded, nullptr);
    loaded->ref();
    ASSERT_EQ(loaded->getNumChildren(), 1);
    auto * loaded_cube = static_cast<SoCube *>(loaded->getChild(0));
    EXPECT_FLOAT_EQ(loaded_cube->height.getValue(), 4.0f);
    loaded->unref();
}

TEST(SceneIo, FixedOutputBuffersCanBeResetAndReused)
{
    SoOutput output;
    char buffer[256] = {};
    output.setBuffer(buffer, sizeof(buffer), nullptr);
    output.write("first payload");

    void * first_data = nullptr;
    size_t first_size = 0;
    ASSERT_TRUE(output.getBuffer(first_data, first_size));
    EXPECT_EQ(first_data, static_cast<void *>(buffer));
    EXPECT_GT(first_size, 0u);

    output.reset();
    output.write("second payload");
    void * second_data = nullptr;
    size_t second_size = 0;
    ASSERT_TRUE(output.getBuffer(second_data, second_size));
    EXPECT_EQ(second_data, static_cast<void *>(buffer));
    EXPECT_GT(second_size, 0u);
}

TEST(SceneIo, CopiedOutputsPreserveReferenceIdsWithoutSharingNewEntries)
{
    auto * first = new SoCube;
    auto * second = new SoCube;
    auto * added_to_copy = new SoCube;
    first->ref();
    second->ref();
    added_to_copy->ref();

    {
        SoOutput original;
        const int first_id = original.addReference(first);
        const int second_id = original.addReference(second);
        ASSERT_NE(first_id, second_id);

        SoOutput copy(&original);
        EXPECT_EQ(copy.findReference(first), first_id);
        EXPECT_EQ(copy.findReference(second), second_id);

        const int copied_id = copy.addReference(added_to_copy);
        EXPECT_GT(copied_id, std::max(first_id, second_id));
        EXPECT_EQ(original.findReference(added_to_copy), -1);
    }

    first->unref();
    second->unref();
    added_to_copy->unref();
}

TEST(SceneIo, FileRoundTripRetainsStructuredMaterialTransformAndShapeScenes)
{
    auto * original = new SoSeparator;
    original->ref();
    auto * material = new SoMaterial;
    material->diffuseColor.setValue(SbColor(0.0f, 0.0f, 1.0f));
    material->shininess.setValue(0.8f);
    auto * transform = new SoTransform;
    transform->translation.setValue(1.0f, 2.0f, 3.0f);
    transform->scaleFactor.setValue(2.0f, 2.0f, 2.0f);
    auto * sphere = new SoSphere;
    sphere->radius.setValue(1.5f);
    original->addChild(material);
    original->addChild(transform);
    original->addChild(sphere);

    TemporaryDirectory temporary_directory;
    ASSERT_FALSE(temporary_directory.path().empty());
    const auto file_path = temporary_directory.path() / "structured-scene.iv";
    SoOutput output;
    ASSERT_TRUE(output.openFile(file_path.string().c_str()));
    SoWriteAction write_action(&output);
    write_action.apply(original);
    output.closeFile();
    original->unref();

    SoInput input;
    ASSERT_TRUE(input.openFile(file_path.string().c_str()));
    auto * loaded = SoDB::readAll(&input);
    ASSERT_NE(loaded, nullptr);
    loaded->ref();
    ASSERT_EQ(loaded->getNumChildren(), 3);
    auto * loaded_material = static_cast<SoMaterial *>(loaded->getChild(0));
    auto * loaded_transform = static_cast<SoTransform *>(loaded->getChild(1));
    auto * loaded_sphere = static_cast<SoSphere *>(loaded->getChild(2));
    EXPECT_EQ(loaded_material->diffuseColor[0], SbColor(0.0f, 0.0f, 1.0f));
    ASSERT_EQ(loaded_material->shininess.getNum(), 1);
    EXPECT_FLOAT_EQ(loaded_material->shininess[0], 0.8f);
    EXPECT_EQ(loaded_transform->translation.getValue(), SbVec3f(1.0f, 2.0f, 3.0f));
    EXPECT_EQ(loaded_transform->scaleFactor.getValue(), SbVec3f(2.0f, 2.0f, 2.0f));
    EXPECT_FLOAT_EQ(loaded_sphere->radius.getValue(), 1.5f);

    SoGetBoundingBoxAction bounds(SbViewportRegion(512, 512));
    bounds.apply(loaded);
    ASSERT_FALSE(bounds.getBoundingBox().isEmpty());
    EXPECT_NEAR(bounds.getBoundingBox().getCenter()[0], 1.0f, 0.1f);
    EXPECT_NEAR(bounds.getBoundingBox().getCenter()[1], 2.0f, 0.1f);
    EXPECT_NEAR(bounds.getBoundingBox().getCenter()[2], 3.0f, 0.1f);
    loaded->unref();
}
