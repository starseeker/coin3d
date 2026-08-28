#include <gtest/gtest.h>

#include "base/SbImageFormatHandler.h"

#include <Inventor/SbPList.h>
#include <Inventor/SbString.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoOffscreenRenderer.h>

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

class ThreadTestImageHandler final : public SbImageFormatHandler {
public:
    explicit ThreadTestImageHandler(std::string extension)
        : extensions_{std::move(extension)} {}

    const char * getFormatName() const override { return "thread-test"; }
    const char * getDescription() const override { return "thread-safety test handler"; }
    const std::vector<std::string> & getExtensions() const override
    {
        return extensions_;
    }

    unsigned char * readImage(const char * filename, int *, int *, int *) override
    {
        setError(filename ? filename : "null");
        std::this_thread::yield();
        return nullptr;
    }

    bool saveImage(const char *, const unsigned char *, int, int, int) override
    {
        return false;
    }

private:
    std::vector<std::string> extensions_;
};

class MallocImageHandler final : public SbImageFormatHandler {
public:
    explicit MallocImageHandler(std::string extension)
        : extensions_{std::move(extension)} {}

    const char * getFormatName() const override { return "malloc-test"; }
    const char * getDescription() const override { return "allocator ownership test"; }
    const std::vector<std::string> & getExtensions() const override
    {
        return extensions_;
    }

    unsigned char * readImage(const char *, int * width, int * height,
                              int * components) override
    {
        if (width) *width = 1;
        if (height) *height = 1;
        if (components) *components = 1;
        auto * data = static_cast<unsigned char *>(std::malloc(1));
        if (data) data[0] = 42;
        return data;
    }

    bool saveImage(const char *, const unsigned char *, int, int, int) override
    {
        return false;
    }

    void freeImageData(unsigned char * data) override
    {
        frees_.fetch_add(1, std::memory_order_relaxed);
        std::free(data);
    }

    int freeCount() const { return frees_.load(std::memory_order_relaxed); }

private:
    std::vector<std::string> extensions_;
    std::atomic<int> frees_{0};
};

class TemporaryImageFile {
public:
    explicit TemporaryImageFile(const char * extension)
    {
        static std::atomic<unsigned long> serial{0};
        path_ = std::filesystem::temp_directory_path() /
            ("obol-image-registry-" +
             std::to_string(serial.fetch_add(1, std::memory_order_relaxed)) +
             extension);
    }

    ~TemporaryImageFile()
    {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }

    const char * c_str() const { return path_string().c_str(); }

private:
    const std::string & path_string() const
    {
        path_cache_ = path_.string();
        return path_cache_;
    }

    std::filesystem::path path_;
    mutable std::string path_cache_;
};

} // namespace

TEST(ImageFormatRegistry, BuiltInPngRoundTripPreservesPixels)
{
    SbImageFormatRegistry & registry = SbImageFormatRegistry::getInstance();
    TemporaryImageFile file(".png");
    const unsigned char source[] = {
        255, 0, 0, 255, 0, 255, 0, 128,
        0, 0, 255, 64, 255, 255, 255, 0
    };

    ASSERT_TRUE(registry.saveImage(file.c_str(), source, 2, 2, 4))
        << registry.getLastError();

    int width = 0;
    int height = 0;
    int components = 0;
    unsigned char * decoded =
        registry.readImage(file.c_str(), &width, &height, &components);
    ASSERT_NE(decoded, nullptr) << registry.getLastError();
    EXPECT_EQ(width, 2);
    EXPECT_EQ(height, 2);
    EXPECT_EQ(components, 4);
    EXPECT_EQ(std::memcmp(decoded, source, sizeof(source)), 0);
    registry.freeImageData(decoded);
}

TEST(ImageFormatRegistry, BuiltInJpegCanReadItsOwnOutput)
{
    SbImageFormatRegistry & registry = SbImageFormatRegistry::getInstance();
    TemporaryImageFile file(".jpg");
    const unsigned char source[] = {
        255, 0, 0, 0, 255, 0,
        0, 0, 255, 255, 255, 255
    };

    ASSERT_TRUE(registry.saveImage(file.c_str(), source, 2, 2, 3))
        << registry.getLastError();

    int width = 0;
    int height = 0;
    int components = 0;
    unsigned char * decoded =
        registry.readImage(file.c_str(), &width, &height, &components);
    ASSERT_NE(decoded, nullptr) << registry.getLastError();
    EXPECT_EQ(width, 2);
    EXPECT_EQ(height, 2);
    EXPECT_EQ(components, 3);
    registry.freeImageData(decoded);
}

TEST(ImageFormatRegistry, FreesDataWithTheHandlerThatAllocatedIt)
{
    SbImageFormatRegistry & registry = SbImageFormatRegistry::getInstance();
    static std::atomic<unsigned long> serial{0};
    const std::string extension =
        "malloc-owner-" +
        std::to_string(serial.fetch_add(1, std::memory_order_relaxed));
    auto handler = std::make_unique<MallocImageHandler>(extension);
    MallocImageHandler * handler_ptr = handler.get();
    registry.registerHandler(std::move(handler));

    const std::string filename = "unused." + extension;
    unsigned char * data =
        registry.readImage(filename.c_str(), nullptr, nullptr, nullptr);
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data[0], 42);
    registry.freeImageData(data);
    EXPECT_EQ(handler_ptr->freeCount(), 1);
}

TEST(ImageFormatRegistry, ConcurrentRegistrationQueriesAndErrorsAreIsolated)
{
    SbImageFormatRegistry & registry = SbImageFormatRegistry::getInstance();
    const int initial_count = registry.getNumHandlers();

    auto shared_owner = std::make_unique<ThreadTestImageHandler>("thread-shared");
    ThreadTestImageHandler * shared = shared_owner.get();
    registry.registerHandler(std::move(shared_owner));

    constexpr int thread_count = 8;
    constexpr int iterations = 200;
    constexpr int registration_interval = 25;
    constexpr int registrations_per_thread =
        iterations / registration_interval;
    std::atomic<int> ready{0};
    std::atomic<bool> failed{false};
    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    for (int thread_index = 0; thread_index < thread_count; ++thread_index) {
        threads.emplace_back([&, thread_index] {
            const std::string extension = "thread-ext-" + std::to_string(thread_index);
            registry.registerHandler(
                std::make_unique<ThreadTestImageHandler>(extension));

            ready.fetch_add(1, std::memory_order_release);
            while (ready.load(std::memory_order_acquire) != thread_count) {}

            const std::string error = "thread-error-" + std::to_string(thread_index);
            for (int iteration = 0; iteration < iterations && !failed; ++iteration) {
                if (iteration % registration_interval == 0) {
                    registry.registerHandler(std::make_unique<ThreadTestImageHandler>(
                        extension + "-" + std::to_string(iteration)));
                }
                shared->readImage(error.c_str(), nullptr, nullptr, nullptr);
                const std::string filename = error + "." + extension;
                registry.readImage(filename.c_str(), nullptr, nullptr, nullptr);
                const std::string registry_error =
                    "Failed to read image: " + filename;
                if (std::strcmp(shared->getLastError(), error.c_str()) != 0 ||
                    std::strcmp(registry.getLastError(), registry_error.c_str()) != 0 ||
                    !registry.isExtensionSupported(extension) ||
                    registry.getSupportedExtensions().empty()) {
                    failed.store(true, std::memory_order_relaxed);
                }
            }
        });
    }
    for (std::thread & thread : threads) thread.join();

    EXPECT_FALSE(failed.load());
    EXPECT_EQ(registry.getNumHandlers(),
              initial_count + thread_count +
                  thread_count * registrations_per_thread + 1);
}

TEST(ImageFormatRegistry, OffscreenCompatibilityExtensionsHaveStableLifetime)
{
    SoOffscreenRenderer renderer(SbViewportRegion(1, 1));
    const int format_count = renderer.getNumWriteFiletypes();
    ASSERT_GT(format_count, 0);

    struct ExtensionPointer {
        const char * pointer;
        std::string value;
    };
    std::vector<ExtensionPointer> original_extensions;
    for (int format = 0; format < format_count; ++format) {
        SbPList extensions;
        SbString name;
        SbString description;
        renderer.getWriteFiletypeInfo(format, extensions, name, description);
        ASSERT_GT(extensions.getLength(), 0) << "format " << format;
        for (int extension = 0; extension < extensions.getLength(); ++extension) {
            const char * pointer = static_cast<const char *>(extensions[extension]);
            ASSERT_NE(pointer, nullptr);
            original_extensions.push_back({pointer, pointer});
        }
    }

    for (int iteration = 0; iteration < 1000; ++iteration) {
        for (int format = 0; format < format_count; ++format) {
            SbPList extensions;
            SbString name;
            SbString description;
            renderer.getWriteFiletypeInfo(format, extensions, name, description);
        }
    }

    for (const ExtensionPointer & extension : original_extensions) {
        EXPECT_STREQ(extension.pointer, extension.value.c_str());
    }
}

TEST(ImageFormatRegistry, OffscreenCompatibilityEnumerationIsConcurrent)
{
    constexpr int thread_count = 8;
    constexpr int iterations = 250;
    std::atomic<bool> failed{false};
    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    for (int thread_index = 0; thread_index < thread_count; ++thread_index) {
        threads.emplace_back([&] {
            SoOffscreenRenderer renderer(SbViewportRegion(1, 1));
            for (int iteration = 0; iteration < iterations && !failed; ++iteration) {
                const int format_count = renderer.getNumWriteFiletypes();
                for (int format = 0; format < format_count; ++format) {
                    SbPList extensions;
                    SbString name;
                    SbString description;
                    renderer.getWriteFiletypeInfo(format, extensions, name, description);
                    if (extensions.getLength() == 0 || name.getLength() == 0) {
                        failed.store(true, std::memory_order_relaxed);
                    }
                }
            }
        });
    }
    for (std::thread & thread : threads) thread.join();

    EXPECT_FALSE(failed.load());
}
