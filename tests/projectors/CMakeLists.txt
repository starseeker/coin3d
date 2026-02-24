# Projector tests
add_executable(test_projectors_suite test_projectors_suite.cpp)
target_link_libraries(test_projectors_suite simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_projectors_suite PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_projectors_suite pthread)
endif()
add_test(NAME test_projectors_suite COMMAND test_projectors_suite)

# Deeper projector coverage (SbSphereSheetProjector, SbSpherePlaneProjector,
# SbCylinderSheetProjector, SbCylinderPlaneProjector)
add_executable(test_projectors_deep test_projectors_deep.cpp)
target_link_libraries(test_projectors_deep simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_projectors_deep PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_projectors_deep pthread)
endif()
add_test(NAME test_projectors_deep COMMAND test_projectors_deep)
