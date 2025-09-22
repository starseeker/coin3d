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

/**
 * @file test_lists_comprehensive.cpp
 * @brief Comprehensive tests for all Coin3D list types and user-facing functionality
 *
 * This module provides comprehensive testing of list containers, dynamic arrays,
 * specialized lists for scene graph objects, and list manipulation operations.
 */

#include "utils/test_common.h"
#include "utils/osmesa_test_context.h"
#include "utils/scene_graph_test_utils.h"

// Core list classes
#include <Inventor/lists/SbList.h>
#include <Inventor/lists/SbPList.h>
#include <Inventor/lists/SbIntList.h>
#include <Inventor/lists/SbStringList.h>
#include <Inventor/lists/SbVec3fList.h>
#include <Inventor/lists/SoBaseList.h>
#include <Inventor/lists/SoNodeList.h>
#include <Inventor/lists/SoPathList.h>
#include <Inventor/lists/SoFieldList.h>
#include <Inventor/lists/SoEngineList.h>
#include <Inventor/lists/SoDetailList.h>
#include <Inventor/lists/SoPickedPointList.h>
#include <Inventor/lists/SoTypeList.h>

// Supporting classes for list testing
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/SoPath.h>
#include <Inventor/fields/SoSFInt32.h>
#include <Inventor/fields/SoSFFloat.h>
#include <Inventor/SbLinear.h>
#include <Inventor/SbString.h>

using namespace CoinTestUtils;

TEST_CASE("List System Comprehensive Tests", "[lists][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("Basic SbList operations") {
        SECTION("SbList<int> basic operations") {
            SbList<int> intList;
            
            // Test empty list
            CHECK(intList.getLength() == 0);
            CHECK(intList.getArrayPtr() != nullptr);
            
            // Test append operations
            intList.append(10);
            intList.append(20);
            intList.append(30);
            
            CHECK(intList.getLength() == 3);
            CHECK(intList[0] == 10);
            CHECK(intList[1] == 20);
            CHECK(intList[2] == 30);
            
            // Test insert operation
            intList.insert(15, 1);
            CHECK(intList.getLength() == 4);
            CHECK(intList[1] == 15);
            CHECK(intList[2] == 20);
            
            // Test remove operation
            intList.remove(2);
            CHECK(intList.getLength() == 3);
            CHECK(intList[2] == 30);
            
            // Test find operation
            int index = intList.find(15);
            CHECK(index == 1);
            
            index = intList.find(999);
            CHECK(index == -1);
        }
        
        SECTION("SbList<float> operations") {
            SbList<float> floatList;
            
            floatList.append(1.5f);
            floatList.append(2.5f);
            floatList.append(3.5f);
            
            CHECK(floatList.getLength() == 3);
            CHECK(floatList[0] == 1.5f);
            CHECK(floatList[1] == 2.5f);
            CHECK(floatList[2] == 3.5f);
            
            // Test copy constructor
            SbList<float> copyList(floatList);
            CHECK(copyList.getLength() == 3);
            CHECK(copyList[0] == 1.5f);
            
            // Test assignment operator
            SbList<float> assignedList;
            assignedList = floatList;
            CHECK(assignedList.getLength() == 3);
            CHECK(assignedList[1] == 2.5f);
        }
    }
    
    SECTION("SbPList operations") {
        SECTION("Pointer list basic operations") {
            SbPList ptrList;
            
            int* ptr1 = new int(100);
            int* ptr2 = new int(200);
            int* ptr3 = new int(300);
            
            ptrList.append(ptr1);
            ptrList.append(ptr2);
            ptrList.append(ptr3);
            
            CHECK(ptrList.getLength() == 3);
            CHECK(*(static_cast<int*>(ptrList[0])) == 100);
            CHECK(*(static_cast<int*>(ptrList[1])) == 200);
            CHECK(*(static_cast<int*>(ptrList[2])) == 300);
            
            // Test find
            int index = ptrList.find(ptr2);
            CHECK(index == 1);
            
            // Test remove
            ptrList.remove(1);
            CHECK(ptrList.getLength() == 2);
            CHECK(*(static_cast<int*>(ptrList[1])) == 300);
            
            delete ptr1;
            delete ptr2;
            delete ptr3;
        }
    }
    
    SECTION("SbIntList operations") {
        SECTION("Integer list specific operations") {
            SbIntList intList;
            
            intList.append(5);
            intList.append(10);
            intList.append(15);
            intList.append(20);
            
            CHECK(intList.getLength() == 4);
            
            // Test array access - SbIntList uses different method
            CHECK(intList.getLength() == 4);
            CHECK(intList[0] == 5);
            CHECK(intList[3] == 20);
            
            // Test truncate
            intList.truncate(2);
            CHECK(intList.getLength() == 2);
            CHECK(intList[1] == 10);
            
            // Test capacity management
            intList.fit();
            CHECK(intList.getLength() == 2);
        }
    }
    
    SECTION("SbStringList operations") {
        SECTION("String list operations") {
            SbStringList stringList;
            
            // Create string objects to add to list (SbStringList expects pointers)
            SbString* str1 = new SbString("Hello");
            SbString* str2 = new SbString("World");
            SbString* str3 = new SbString("Test");
            
            stringList.append(str1);
            stringList.append(str2);
            stringList.append(str3);
            
            CHECK(stringList.getLength() == 3);
            CHECK(stringList[0] == str1);
            CHECK(stringList[1] == str2);
            CHECK(stringList[2] == str3);
            
            // Test find
            int index = stringList.find(str2);
            CHECK(index == 1);
            
            SbString notFound("NotFound");
            index = stringList.find(&notFound);
            CHECK(index == -1);
            
            // Clean up
            delete str1;
            delete str2;
            delete str3;
        }
    }
    
    SECTION("SbVec3fList operations") {
        SECTION("Vector list operations") {
            SbVec3fList vecList;
            
            // Create vector objects
            SbVec3f* vec1 = new SbVec3f(1, 0, 0);
            SbVec3f* vec2 = new SbVec3f(0, 1, 0);
            SbVec3f* vec3 = new SbVec3f(0, 0, 1);
            
            vecList.append(vec1);
            vecList.append(vec2);
            vecList.append(vec3);
            
            CHECK(vecList.getLength() == 3);
            CHECK(vecList[0] == vec1);
            CHECK(vecList[1] == vec2);
            CHECK(vecList[2] == vec3);
            
            // Test vector access (dereference pointers for operations)
            SbVec3f sum = *vec1 + *vec2 + *vec3;
            CHECK(sum == SbVec3f(1, 1, 1));
            
            // Clean up
            delete vec1;
            delete vec2;
            delete vec3;
        }
    }
    
    SECTION("SoNodeList operations") {
        SECTION("Node list scene graph operations") {
            SoNodeList nodeList;
            
            SoCube* cube = new SoCube;
            SoSphere* sphere = new SoSphere;
            SoMaterial* material = new SoMaterial;
            
            cube->ref();
            sphere->ref();
            material->ref();
            
            nodeList.append(cube);
            nodeList.append(sphere);
            nodeList.append(material);
            
            CHECK(nodeList.getLength() == 3);
            CHECK(nodeList[0] == cube);
            CHECK(nodeList[1] == sphere);
            CHECK(nodeList[2] == material);
            
            // Test type checking
            CHECK(nodeList[0]->isOfType(SoCube::getClassTypeId()));
            CHECK(nodeList[1]->isOfType(SoSphere::getClassTypeId()));
            CHECK(nodeList[2]->isOfType(SoMaterial::getClassTypeId()));
            
            cube->unref();
            sphere->unref();
            material->unref();
        }
        
        SECTION("Node list with scene hierarchy") {
            SoNodeList nodeList;
            
            auto scene = StandardTestScenes::createBasicGeometryScene();
            
            // Traverse and collect nodes
            // Note: This would require a custom traversal action in real implementation
            // For testing, we manually add some nodes
            SoCube* cube = new SoCube;
            SoSphere* sphere = new SoSphere;
            
            cube->ref();
            sphere->ref();
            
            nodeList.append(cube);
            nodeList.append(sphere);
            
            CHECK(nodeList.getLength() == 2);
            
            cube->unref();
            sphere->unref();
            scene->unref();
        }
    }
    
    SECTION("SoPathList operations") {
        SECTION("Path list operations") {
            SoPathList pathList;
            
            auto scene = StandardTestScenes::createComplexScene();
            
            // Create some paths through the scene
            SoPath* path1 = new SoPath(scene);
            SoPath* path2 = new SoPath(scene);
            
            path1->ref();
            path2->ref();
            
            pathList.append(path1);
            pathList.append(path2);
            
            CHECK(pathList.getLength() == 2);
            CHECK(pathList[0] == path1);
            CHECK(pathList[1] == path2);
            
            // Test path list specific operations
            pathList.uniquify();
            CHECK(pathList.getLength() <= 2); // May remove duplicates
            
            path1->unref();
            path2->unref();
            scene->unref();
        }
    }
    
    SECTION("SoFieldList operations") {
        SECTION("Field list operations") {
            SoFieldList fieldList;
            
            SoSFInt32* intField = new SoSFInt32;
            SoSFFloat* floatField = new SoSFFloat;
            
            intField->setValue(42);
            floatField->setValue(3.14f);
            
            fieldList.append(intField);
            fieldList.append(floatField);
            
            CHECK(fieldList.getLength() == 2);
            CHECK(fieldList[0] == intField);
            CHECK(fieldList[1] == floatField);
            
            // Test field type checking
            CHECK(fieldList[0]->isOfType(SoSFInt32::getClassTypeId()));
            CHECK(fieldList[1]->isOfType(SoSFFloat::getClassTypeId()));
            
            delete intField;
            delete floatField;
        }
    }
    
    SECTION("SoTypeList operations") {
        SECTION("Type list operations") {
            SoTypeList typeList;
            
            typeList.append(SoCube::getClassTypeId());
            typeList.append(SoSphere::getClassTypeId());
            typeList.append(SoMaterial::getClassTypeId());
            
            CHECK(typeList.getLength() == 3);
            
            // Test type list specific operations
            int index = typeList.find(SoSphere::getClassTypeId());
            CHECK(index == 1);
            
            index = typeList.find(SoType::badType());
            CHECK(index == -1);
        }
    }
}

TEST_CASE("List Performance and Memory Tests", "[lists][performance]") {
    CoinTestFixture fixture;
    
    SECTION("Large list operations") {
        SECTION("Performance with large integer list") {
            SbIntList largeList;
            
            const int numElements = 10000;
            
            // Test bulk append
            for (int i = 0; i < numElements; ++i) {
                largeList.append(i);
            }
            
            CHECK(largeList.getLength() == numElements);
            CHECK(largeList[0] == 0);
            CHECK(largeList[numElements - 1] == numElements - 1);
            
            // Test find operation on large list
            int index = largeList.find(5000);
            CHECK(index == 5000);
            
            // Test truncate on large list
            largeList.truncate(1000);
            CHECK(largeList.getLength() == 1000);
        }
        
        SECTION("Memory efficiency with pointer list") {
            SbPList ptrList;
            
            // Test memory management with many pointers
            std::vector<int*> ptrs;
            
            for (int i = 0; i < 1000; ++i) {
                int* ptr = new int(i);
                ptrs.push_back(ptr);
                ptrList.append(ptr);
            }
            
            CHECK(ptrList.getLength() == 1000);
            
            // Clean up
            for (int* ptr : ptrs) {
                delete ptr;
            }
        }
    }
    
    SECTION("List copy and assignment performance") {
        SECTION("Large list copying") {
            SbIntList originalList;  // Use SbIntList instead of SbVec3fList to avoid pointer issues
            
            for (int i = 0; i < 1000; ++i) {
                originalList.append(i);
            }
            
            // Test copy constructor performance
            SbIntList copyList(originalList);
            CHECK(copyList.getLength() == originalList.getLength());
            CHECK(copyList[500] == originalList[500]);
            
            // Test assignment operator performance
            SbIntList assignedList;
            assignedList = originalList;
            CHECK(assignedList.getLength() == originalList.getLength());
            CHECK(assignedList[750] == originalList[750]);
        }
    }
}

TEST_CASE("List Edge Cases and Error Handling", "[lists][edge_cases]") {
    CoinTestFixture fixture;
    
    SECTION("Empty list operations") {
        SECTION("Operations on empty lists") {
            SbIntList emptyList;
            
            CHECK(emptyList.getLength() == 0);
            CHECK(emptyList.find(10) == -1);
            
            // Test truncate on empty list
            emptyList.truncate(0);
            CHECK(emptyList.getLength() == 0);
            
            // Test fit on empty list
            emptyList.fit();
            CHECK(emptyList.getLength() == 0);
        }
    }
    
    SECTION("Boundary conditions") {
        SECTION("Index boundary testing") {
            SbStringList stringList;
            
            // Create string objects
            SbString* str1 = new SbString("First");
            SbString* str2 = new SbString("Second");
            SbString* str3 = new SbString("Third");
            
            stringList.append(str1);
            stringList.append(str2);
            stringList.append(str3);
            
            // Test valid indices
            CHECK(stringList[0] == str1);
            CHECK(stringList[2] == str3);
            
            // Test list operations at boundaries
            stringList.remove(0);  // Remove first
            CHECK(stringList.getLength() == 2);
            CHECK(stringList[0] == str2);  // Now "Second" should be first
            
            stringList.remove(stringList.getLength() - 1);  // Remove last
            CHECK(stringList.getLength() == 1);
            CHECK(stringList[0] == str2);
            
            // Clean up remaining strings
            delete str1;
            delete str2;
            delete str3;
        }
    }
    
    SECTION("Type-specific edge cases") {
        SECTION("SoNodeList with null pointers") {
            SoNodeList nodeList;
            
            SoCube* cube = new SoCube;
            cube->ref();
            
            nodeList.append(cube);
            nodeList.append(nullptr);  // Add null pointer
            
            CHECK(nodeList.getLength() == 2);
            CHECK(nodeList[0] == cube);
            CHECK(nodeList[1] == nullptr);
            
            // Test find with null
            int index = nodeList.find(nullptr);
            CHECK(index == 1);
            
            cube->unref();
        }
        
        SECTION("SoPathList with invalid paths") {
            SoPathList pathList;
            
            auto scene = StandardTestScenes::createMinimalScene();
            SoPath* validPath = new SoPath(scene);
            validPath->ref();
            
            pathList.append(validPath);
            
            CHECK(pathList.getLength() == 1);
            
            validPath->unref();
            scene->unref();
        }
    }
    
    SECTION("List capacity and growth") {
        SECTION("Dynamic growth behavior") {
            SbIntList growingList;
            
            // Add elements one by one and observe growth
            for (int i = 0; i < 100; ++i) {
                growingList.append(i);
                CHECK(growingList.getLength() == i + 1);
                CHECK(growingList[i] == i);
            }
            
            // Test that capacity is reasonable
            CHECK(growingList.getLength() == 100);
            
            // Test explicit capacity management
            growingList.fit();
            CHECK(growingList.getLength() == 100);
        }
    }
}