cmake_minimum_required(VERSION 3.20)

project(PubSubLib CXX)

find_package(czmq REQUIRED)
find_package(zyre REQUIRED)
find_package(ZeroMQ REQUIRED)

file(GLOB SOURCES     "*.cpp")
file(GLOB HEADERS     "*.h")

add_library(${PROJECT_NAME} SHARED ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    czmq-static
    zyre-static
    libzmq-static
    CommonUtils
    protobuf::libprotobuf
)
