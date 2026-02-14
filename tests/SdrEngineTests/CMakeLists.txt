project(UnitTests_SdrEngine)

include(GoogleTest)

enable_testing()

file(GLOB UNIT_TEST_SOURCE ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

add_executable(${PROJECT_NAME} ${UNIT_TEST_SOURCE})

target_link_libraries(${PROJECT_NAME}
   PRIVATE
      SdrEngine
      CommonUtils
      GTest::gtest
      GTest::gmock
)

# Suppress -Wunused-result in test code
target_compile_options(${PROJECT_NAME} PRIVATE -Wno-unused-result)

# Discover tests for CTest
gtest_discover_tests(${PROJECT_NAME}
   WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
   PROPERTIES
      LABELS "unit"
)

# Set output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
