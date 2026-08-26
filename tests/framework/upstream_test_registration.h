#pragma once

#include <gtest/gtest.h>

#include <exception>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace ObolTest {

namespace detail {

struct UpstreamCheckResult {
    std::string name;
    bool passed = false;
    std::string error;
};

struct UpstreamRun {
    std::once_flag once;
    std::string current_name;
    std::vector<UpstreamCheckResult> checks;
    std::exception_ptr exception;
};

using UpstreamEntry = int (*)();

inline thread_local UpstreamRun * active_run = nullptr;

inline void beginUpstreamCheck(const std::string & name)
{
    if (active_run) active_run->current_name = name;
}

inline bool finishUpstreamCheck(bool passed, const std::string & error)
{
    if (!active_run) return false;
    active_run->checks.push_back(
        UpstreamCheckResult{active_run->current_name, passed, error});
    return true;
}

inline std::shared_ptr<UpstreamRun> upstreamRunFor(UpstreamEntry entry)
{
    static std::mutex runs_mutex;
    static std::map<UpstreamEntry, std::shared_ptr<UpstreamRun>> runs;
    std::lock_guard<std::mutex> lock(runs_mutex);
    auto & run = runs[entry];
    if (!run) run = std::make_shared<UpstreamRun>();
    return run;
}

inline void runUpstreamCase(UpstreamEntry entry, int target,
                            const char * expected_name)
{
    const std::shared_ptr<UpstreamRun> run = upstreamRunFor(entry);
    std::call_once(run->once, [&] {
        UpstreamRun * previous = active_run;
        active_run = run.get();
        try {
            (void)entry();
        }
        catch (...) {
            run->exception = std::current_exception();
        }
        active_run = previous;
    });

    if (run->exception) std::rethrow_exception(run->exception);

    if (target < 0 || static_cast<std::size_t>(target) >= run->checks.size()) {
        ADD_FAILURE() << "upstream check index " << target
                      << " was not reached (expected '"
                      << (expected_name ? expected_name : "") << "')";
        return;
    }
    const UpstreamCheckResult & result =
        run->checks[static_cast<std::size_t>(target)];
    const std::string expected = expected_name ? expected_name : "";
    if (result.name != expected) {
        ADD_FAILURE() << "generated upstream check map is stale: expected '"
                      << expected << "' but reached '" << result.name << "'";
        return;
    }
    if (!result.passed) {
        ADD_FAILURE() << result.name
                      << (result.error.empty() ? "" : ": ")
                      << result.error;
    }
}

} // namespace detail

using detail::runUpstreamCase;

} // namespace ObolTest
