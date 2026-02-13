# =============================================================================
# Protocol Buffer Library - CMakeLists.txt
# =============================================================================

project(ProtoLib CXX)

# Find protobuf compiler from Conan
find_package(protobuf REQUIRED CONFIG)

# Get protoc from the Conan package folder
# Conan sets either protobuf_PACKAGE_FOLDER_RELEASE or protobuf_PACKAGE_FOLDER_DEBUG
if(DEFINED protobuf_PACKAGE_FOLDER_RELEASE)
   set(CONAN_PROTOC_PATH "${protobuf_PACKAGE_FOLDER_RELEASE}/bin/protoc")
elseif(DEFINED protobuf_PACKAGE_FOLDER_DEBUG)
   set(CONAN_PROTOC_PATH "${protobuf_PACKAGE_FOLDER_DEBUG}/bin/protoc")
endif()

if(EXISTS "${CONAN_PROTOC_PATH}")
   set(PROTOC_EXECUTABLE "${CONAN_PROTOC_PATH}")
else()
   # Fallback: use Protobuf_PROTOC_EXECUTABLE if it's from a Conan package
   if(DEFINED Protobuf_PROTOC_EXECUTABLE AND "${Protobuf_PROTOC_EXECUTABLE}" MATCHES "conan2")
      set(PROTOC_EXECUTABLE "${Protobuf_PROTOC_EXECUTABLE}")
   elseif(TARGET protobuf::protoc)
      # Get from target property
      get_target_property(_protoc_loc protobuf::protoc IMPORTED_LOCATION)
      if(_protoc_loc AND "${_protoc_loc}" MATCHES "conan2")
         set(PROTOC_EXECUTABLE "${_protoc_loc}")
      endif()
   endif()
endif()

if(NOT PROTOC_EXECUTABLE)
   message(FATAL_ERROR "Could not find protoc executable from Conan package. "
                       "protobuf_PACKAGE_FOLDER_RELEASE=${protobuf_PACKAGE_FOLDER_RELEASE}, "
                       "protobuf_PACKAGE_FOLDER_DEBUG=${protobuf_PACKAGE_FOLDER_DEBUG}, "
                       "Protobuf_PROTOC_EXECUTABLE=${Protobuf_PROTOC_EXECUTABLE}")
endif()

message(STATUS "Using protoc: ${PROTOC_EXECUTABLE}")

# Proto source files
file (GLOB PROTO_FILES ${CMAKE_CURRENT_LIST_DIR}/proto-messages/*.proto)

# Output directories for generated files
set(PROTO_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${PROTO_GENERATED_DIR})

# Generate C++ files from proto files
set(PROTO_SRCS)
set(PROTO_HDRS)

foreach(PROTO_FILE ${PROTO_FILES})
   get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)

   set(PROTO_SRC ${PROTO_GENERATED_DIR}/${PROTO_NAME}.pb.cc)
   set(PROTO_HDR ${PROTO_GENERATED_DIR}/${PROTO_NAME}.pb.h)

   list(APPEND PROTO_SRCS ${PROTO_SRC})
   list(APPEND PROTO_HDRS ${PROTO_HDR})

   add_custom_command(
      OUTPUT ${PROTO_SRC} ${PROTO_HDR}
      COMMAND ${PROTOC_EXECUTABLE}
         --proto_path=${CMAKE_CURRENT_LIST_DIR}/proto-messages
         --cpp_out=${PROTO_GENERATED_DIR}
         ${PROTO_FILE}
      DEPENDS ${PROTO_FILE}
      COMMENT "Generating C++ files from ${PROTO_NAME}.proto"
      VERBATIM
   )
endforeach()

# Create the proto library
add_library(${PROJECT_NAME} STATIC
   ${PROTO_SRCS}
   ${PROTO_HDRS}
)

add_library(RadioWizard::proto ALIAS ${PROJECT_NAME})

# Include directories (SYSTEM suppresses warnings in generated headers)
target_include_directories(${PROJECT_NAME}
   SYSTEM PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
      $<INSTALL_INTERFACE:include>
)

# Link protobuf
target_link_libraries(${PROJECT_NAME}
   PUBLIC
      protobuf::libprotobuf
)

# Suppress warnings in generated code
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
   target_compile_options(${PROJECT_NAME} PRIVATE -w)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
   target_compile_options(${PROJECT_NAME} PRIVATE /w)
endif()

# Set properties
set_target_properties(${PROJECT_NAME} PROPERTIES
   OUTPUT_NAME "proto"
   POSITION_INDEPENDENT_CODE ON
   CXX_CLANG_TIDY ""
)
