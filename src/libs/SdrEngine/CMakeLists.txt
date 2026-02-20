# =============================================================================
# SdrEngine Library — SDR device abstraction + FFT processing pipeline
#
# No Qt dependency — uses DataHandler (CommonUtils) for data distribution.
# =============================================================================

set(LIB_NAME SdrEngine)

file(GLOB LIB_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
file(GLOB LIB_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

add_library(${LIB_NAME} SHARED ${LIB_SOURCES} ${LIB_HEADERS})

set_target_properties(${LIB_NAME} PROPERTIES
   POSITION_INDEPENDENT_CODE ON
)

target_include_directories(${LIB_NAME}
   PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/>
      $<INSTALL_INTERFACE:include>
)

target_link_libraries(${LIB_NAME}
   PUBLIC
      CommonUtils
   PRIVATE
      FFTW3::fftw3f
      liquid-dsp::liquid-dsp
      PkgConfig::RTLSDR
)
