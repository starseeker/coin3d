# Detail class tests (SoFaceDetail, SoLineDetail, SoPointDetail)
add_executable(test_details_suite test_details_suite.cpp)
target_link_libraries(test_details_suite simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_details_suite PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_details_suite pthread)
endif()
add_test(NAME test_details_suite COMMAND test_details_suite)

# Deeper detail coverage: SoCubeDetail, SoCylinderDetail, SoConeDetail, SoTextDetail
add_executable(test_details_deep test_details_deep.cpp)
target_link_libraries(test_details_deep simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_details_deep PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_details_deep pthread)
endif()
add_test(NAME test_details_deep COMMAND test_details_deep)
