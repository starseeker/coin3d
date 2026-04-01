/**
 * @file test_cad_ids.cpp
 * @brief Unit tests for obol::CadIdBuilder and obol::CadId128.
 *
 * Tests:
 *  1. Same traversal path produces identical InstanceId
 *  2. Different occurrence index under same parent gives different IDs
 *  3. Order changes change IDs
 *  4. Root sentinel is all-zeros
 *  5. hash128 of same bytes is deterministic
 *  6. hash128 of different bytes produces different IDs
 *  7. std::hash<CadId128> compiles and gives consistent values
 *
 * No BRL-CAD dependency.  No GL context required.
 */

#include "../test_utils.h"
#include <obol/cad/CadIds.h>

#include <unordered_map>
#include <string>
#include <cassert>

using namespace SimpleTest;
using namespace obol;

int main()
{
    TestRunner runner;

    // -----------------------------------------------------------------------
    // 1. Root sentinel is all-zeros
    // -----------------------------------------------------------------------
    runner.startTest("CadIdBuilder::Root() returns zero ID");
    {
        CadId128 root = CadIdBuilder::Root();
        runner.endTest(root.w0 == 0 && root.w1 == 0,
                       "Root ID should be {0,0}");
    }

    // -----------------------------------------------------------------------
    // 2. hash128 is deterministic
    // -----------------------------------------------------------------------
    runner.startTest("CadIdBuilder::hash128 deterministic for same key");
    {
        std::string key = "wheel";
        CadId128 a = CadIdBuilder::hash128(key);
        CadId128 b = CadIdBuilder::hash128(key);
        runner.endTest(a == b, "Same key should produce same ID");
    }

    // -----------------------------------------------------------------------
    // 3. hash128 differs for different keys
    // -----------------------------------------------------------------------
    runner.startTest("CadIdBuilder::hash128 different for different keys");
    {
        CadId128 a = CadIdBuilder::hash128(std::string("wheel"));
        CadId128 b = CadIdBuilder::hash128(std::string("bolt"));
        runner.endTest(a != b, "Different keys should produce different IDs");
    }

    // -----------------------------------------------------------------------
    // 4. Same traversal path produces identical InstanceId
    // -----------------------------------------------------------------------
    runner.startTest("extendNameOccBool: same path gives same ID");
    {
        CadId128 root = CadIdBuilder::Root();
        CadId128 a = CadIdBuilder::extendNameOccBool(root, "arm", 0, 0);
        CadId128 a2 = CadIdBuilder::extendNameOccBool(root, "arm", 0, 0);
        runner.endTest(a == a2, "Same inputs should yield same child ID");
    }

    // -----------------------------------------------------------------------
    // 5. Different occurrence index gives different ID
    // -----------------------------------------------------------------------
    runner.startTest("extendNameOccBool: different occurrence gives different ID");
    {
        CadId128 root = CadIdBuilder::Root();
        CadId128 a = CadIdBuilder::extendNameOccBool(root, "wheel", 0, 0);
        CadId128 b = CadIdBuilder::extendNameOccBool(root, "wheel", 1, 0);
        runner.endTest(a != b,
                       "Different occurrence index should produce different ID");
    }

    // -----------------------------------------------------------------------
    // 6. Different boolOp gives different ID
    // -----------------------------------------------------------------------
    runner.startTest("extendNameOccBool: different boolOp gives different ID");
    {
        CadId128 root = CadIdBuilder::Root();
        CadId128 a = CadIdBuilder::extendNameOccBool(root, "cutout", 0, 0);  // union
        CadId128 b = CadIdBuilder::extendNameOccBool(root, "cutout", 0, 1);  // subtract
        runner.endTest(a != b,
                       "Different boolOp should produce different ID");
    }

    // -----------------------------------------------------------------------
    // 7. Order changes change IDs (documented behaviour)
    // -----------------------------------------------------------------------
    runner.startTest("extendNameOccBool: order changes change IDs");
    {
        CadId128 root = CadIdBuilder::Root();
        // Path A → B
        CadId128 ab_a = CadIdBuilder::extendNameOccBool(root, "A", 0, 0);
        CadId128 ab   = CadIdBuilder::extendNameOccBool(ab_a, "B", 0, 0);
        // Path B → A
        CadId128 ba_b = CadIdBuilder::extendNameOccBool(root, "B", 0, 0);
        CadId128 ba   = CadIdBuilder::extendNameOccBool(ba_b, "A", 0, 0);
        runner.endTest(ab != ba,
                       "Different traversal order should produce different leaf ID");
    }

    // -----------------------------------------------------------------------
    // 8. Deep hierarchy is deterministic
    // -----------------------------------------------------------------------
    runner.startTest("extendNameOccBool: deep hierarchy deterministic");
    {
        auto makePath = [](const std::vector<std::string>& names) -> CadId128 {
            CadId128 id = CadIdBuilder::Root();
            for (uint32_t i = 0; i < static_cast<uint32_t>(names.size()); ++i) {
                id = CadIdBuilder::extendNameOccBool(id, names[i], 0, 0);
            }
            return id;
        };
        std::vector<std::string> path = {"vehicle", "chassis", "axle", "bolt"};
        CadId128 a = makePath(path);
        CadId128 b = makePath(path);
        runner.endTest(a == b, "Deep path should be reproducible");
    }

    // -----------------------------------------------------------------------
    // 9. std::hash<CadId128> is consistent
    // -----------------------------------------------------------------------
    runner.startTest("std::hash<CadId128> consistent for same ID");
    {
        CadId128 id = CadIdBuilder::hash128(std::string("test_key"));
        std::hash<CadId128> hasher;
        size_t h1 = hasher(id);
        size_t h2 = hasher(id);
        runner.endTest(h1 == h2, "Hash should be consistent");
    }

    // -----------------------------------------------------------------------
    // 10. CadId128 usable as unordered_map key
    // -----------------------------------------------------------------------
    runner.startTest("CadId128 usable as unordered_map key");
    {
        std::unordered_map<CadId128, int, std::hash<CadId128>> m;
        CadId128 id1 = CadIdBuilder::hash128(std::string("key1"));
        CadId128 id2 = CadIdBuilder::hash128(std::string("key2"));
        m[id1] = 42;
        m[id2] = 99;
        bool ok = (m[id1] == 42 && m[id2] == 99);
        runner.endTest(ok, "Map operations should work with CadId128 keys");
    }

    // -----------------------------------------------------------------------
    // 11. isValid() reflects non-zero ID
    // -----------------------------------------------------------------------
    runner.startTest("CadId128::isValid returns false for root sentinel");
    {
        bool ok = !CadIdBuilder::Root().isValid();
        runner.endTest(ok, "Root (zero) ID should not be valid");
    }

    runner.startTest("CadId128::isValid returns true for non-zero ID");
    {
        CadId128 id = CadIdBuilder::hash128(std::string("x"));
        runner.endTest(id.isValid(), "Non-zero ID should be valid");
    }

    return runner.getSummary();
}
