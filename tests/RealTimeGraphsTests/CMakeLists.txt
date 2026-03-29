project(UnitTests_RealTimeGraphs)

include(GoogleTest)

# Enable CTest
enable_testing()

file(GLOB UNIT_TEST_SOURCE ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

# Add the source files
add_executable(${PROJECT_NAME} ${UNIT_TEST_SOURCE})

# Enable Qt MOC for test target (needed for QWidget-derived classes)
set_target_properties(${PROJECT_NAME} PROPERTIES
   AUTOMOC ON
)

target_link_libraries(${PROJECT_NAME}
   PRIVATE
      RealTimeGraphs
      GTest::gtest
      GTest::gmock
      Qt6::Core
      Qt6::Gui
      Qt6::Widgets
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
