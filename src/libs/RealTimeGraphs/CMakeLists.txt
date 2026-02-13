# =============================================================================
# RealTimeGraphs Library - Custom QPainter-based real-time graph widgets
# =============================================================================

set(LIB_NAME RealTimeGraphs)

file(GLOB LIB_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
file(GLOB LIB_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

add_library(${LIB_NAME} SHARED ${LIB_SOURCES} ${LIB_HEADERS})
add_library(RadioWizard::${LIB_NAME} ALIAS ${LIB_NAME})

# Enable Qt MOC (meta-object compiler) for this target
set_target_properties(${LIB_NAME} PROPERTIES
   AUTOMOC ON
   POSITION_INDEPENDENT_CODE ON
)

target_include_directories(${LIB_NAME}
   PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/>
      $<INSTALL_INTERFACE:include>
)

target_link_libraries(${LIB_NAME}
   PUBLIC
      Qt6::Core
      Qt6::Gui
      Qt6::Widgets
      spdlog::spdlog
      CommonUtils
)

target_compile_definitions(${LIB_NAME}
   PRIVATE
      REALTIMEGRAPHS_LIBRARY
)
