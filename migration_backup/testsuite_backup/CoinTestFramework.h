#ifndef COIN_TEST_FRAMEWORK_H
#define COIN_TEST_FRAMEWORK_H

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

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <sstream>

namespace CoinTestFramework {

struct TestCase {
    std::string name;
    std::function<void()> func;
    std::string suite;
};

struct TestSuite {
    std::string name;
    std::vector<TestCase> tests;
};

class TestRegistry {
public:
    static TestRegistry& instance() {
        static TestRegistry registry;
        return registry;
    }

    void registerTest(const std::string& suite_name, const std::string& test_name, std::function<void()> test_func) {
        TestCase test;
        test.name = test_name;
        test.func = test_func;
        test.suite = suite_name;
        
        // Find or create suite
        TestSuite* suite = nullptr;
        for (auto& s : suites) {
            if (s.name == suite_name) {
                suite = &s;
                break;
            }
        }
        if (!suite) {
            suites.emplace_back();
            suite = &suites.back();
            suite->name = suite_name;
        }
        
        suite->tests.push_back(test);
    }

    int runAllTests(int argc, char* argv[]) {
        int failed = 0;
        int total = 0;
        
        for (const auto& suite : suites) {
            std::cout << "Running test suite: " << suite.name << std::endl;
            for (const auto& test : suite.tests) {
                total++;
                std::cout << "  Running " << test.name << "... ";
                try {
                    test.func();
                    std::cout << "PASSED" << std::endl;
                } catch (const std::exception& e) {
                    std::cout << "FAILED: " << e.what() << std::endl;
                    failed++;
                } catch (...) {
                    std::cout << "FAILED: Unknown exception" << std::endl;
                    failed++;
                }
            }
        }
        
        std::cout << std::endl << "Test Results: " << (total - failed) << "/" << total << " tests passed" << std::endl;
        return failed;
    }

private:
    std::vector<TestSuite> suites;
};

class TestAssertion : public std::exception {
public:
    TestAssertion(const std::string& message) : message_(message) {}
    const char* what() const noexcept override { return message_.c_str(); }
private:
    std::string message_;
};

template<typename T>
std::string to_string(const T& value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

// Specialization for string types
inline std::string to_string(const std::string& value) {
    return value;
}

inline std::string to_string(const char* value) {
    return std::string(value);
}

} // namespace CoinTestFramework

// Helper class for test registration
class TestRegistrar {
public:
    TestRegistrar(const std::string& suite_name, const std::string& test_name, std::function<void()> test_func) {
        CoinTestFramework::TestRegistry::instance().registerTest(suite_name, test_name, test_func);
    }
};

// Macros to replace Boost.Test equivalents
#define BOOST_AUTO_TEST_SUITE(suite_name) \
    namespace coin_test_suite_##suite_name { \
        const char* current_test_suite = #suite_name;

#define BOOST_AUTO_TEST_SUITE_END() \
    } // end namespace

#define BOOST_AUTO_TEST_CASE(test_name) \
    void test_##test_name(); \
    static TestRegistrar reg_##test_name(current_test_suite, #test_name, test_##test_name); \
    void test_##test_name()

#define BOOST_CHECK_MESSAGE(condition, message) \
    do { \
        if (!(condition)) { \
            std::ostringstream oss; \
            oss << message; \
            throw CoinTestFramework::TestAssertion(oss.str()); \
        } \
    } while (0)

#define BOOST_CHECK(condition) \
    BOOST_CHECK_MESSAGE(condition, "Check failed: " #condition)

#define BOOST_CHECK_EQUAL(left, right) \
    BOOST_CHECK_MESSAGE((left) == (right), "Check failed: " #left " == " #right " [" + CoinTestFramework::to_string(left) + " != " + CoinTestFramework::to_string(right) + "]")

#define BOOST_REQUIRE(condition) \
    BOOST_CHECK_MESSAGE(condition, "Required condition failed: " #condition)

#define BOOST_REQUIRE_MESSAGE(condition, message) \
    BOOST_CHECK_MESSAGE(condition, message)

#define BOOST_ASSERT(condition) \
    BOOST_CHECK_MESSAGE(condition, "Assertion failed: " #condition)

#define BOOST_STATIC_ASSERT(condition) \
    static_assert(condition, "Static assertion failed: " #condition)

// Test framework main function
inline int unit_test_main(int argc, char* argv[]) {
    return CoinTestFramework::TestRegistry::instance().runAllTests(argc, argv);
}

// For compatibility with the existing init function signature
inline bool init_unit_test() {
    return true;
}

#endif // COIN_TEST_FRAMEWORK_H