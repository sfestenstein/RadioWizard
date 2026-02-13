project(UnitTests_Vita49_2)

include(GoogleTest)

# Enable CTest
enable_testing()

file(GLOB UNIT_TEST_SOURCE ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

# Add the source files
add_executable(${PROJECT_NAME} ${UNIT_TEST_SOURCE})

target_link_libraries(${PROJECT_NAME}
   PRIVATE
      Vita49_2
      GTest::gtest
      GTest::gmock
)

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


# =============================================================================
# Coverage Target
# =============================================================================

if(ENABLE_COVERAGE)
   # Find coverage tools
   find_program(LCOV_EXECUTABLE lcov)
   find_program(GENHTML_EXECUTABLE genhtml)
   find_program(GCOV_EXECUTABLE gcov)

   set(COVERAGE_TARGET "Vita49_2Coverage")

   if(LCOV_EXECUTABLE AND GENHTML_EXECUTABLE)
      # lcov 2.0+ with newer GCC requires ignoring some inconsistencies
      set(LCOV_IGNORE_ERRORS --ignore-errors inconsistent,mismatch,unused,negative)

      # Create coverage target
      add_custom_target(${COVERAGE_TARGET}
         COMMAND ${LCOV_EXECUTABLE} --zerocounters --directory ${CMAKE_BINARY_DIR} ${LCOV_IGNORE_ERRORS}
         COMMAND ${CMAKE_COMMAND} -E echo "Running tests..."
         COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L unit
         COMMAND ${LCOV_EXECUTABLE}
            --capture
            --directory ${CMAKE_BINARY_DIR}
            --output-file ${CMAKE_BINARY_DIR}/${COVERAGE_TARGET}.info
            --gcov-tool ${GCOV_EXECUTABLE}
            ${LCOV_IGNORE_ERRORS}
         COMMAND ${LCOV_EXECUTABLE}
            --remove ${CMAKE_BINARY_DIR}/${COVERAGE_TARGET}.info
            /usr/*
            ${CMAKE_BINARY_DIR}/*
            */test/*
            */tests/*
            --output-file ${CMAKE_BINARY_DIR}/${COVERAGE_TARGET}.info
            ${LCOV_IGNORE_ERRORS}
         COMMAND ${GENHTML_EXECUTABLE}
            ${CMAKE_BINARY_DIR}/${COVERAGE_TARGET}.info
            --output-directory ${CMAKE_BINARY_DIR}/${COVERAGE_TARGET}
            --title "RadioWizard Coverage Report"
            --legend
            --demangle-cpp
            ${LCOV_IGNORE_ERRORS}
         COMMAND ${CMAKE_COMMAND} -E echo "Coverage report: ${CMAKE_BINARY_DIR}/${COVERAGE_TARGET}/index.html"
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
         DEPENDS ${PROJECT_NAME}
         COMMENT "Generating code coverage report"
      )
   else()
      message(WARNING "lcov/genhtml not found. Coverage report generation disabled.")

      # Simplified coverage target without HTML generation
      add_custom_target(${COVERAGE_TARGET}
         COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
         DEPENDS ${PROJECT_NAME}
         COMMENT "Running tests (${COVERAGE_TARGET} HTML disabled - lcov/genhtml not found)"
      )
   endif()
endif()
