#include <gtest/gtest.h>

#include "base/SbImageFormatHandler.h"

#include <atomic>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

class ThreadTestImageHandler final : public SbImageFormatHandler {
public:
    explicit ThreadTestImageHandler(std::string extension)
        : extension_(std::move(extension)) {}

    const char * getFormatName() const override { return "thread-test"; }
    const char * getDescription() const override { return "thread-safety test handler"; }
    std::vector<std::string> getExtensions() const override { return {extension_}; }

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
    std::string extension_;
};

} // namespace

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
