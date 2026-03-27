cmake_minimum_required(VERSION 3.20)

project(PubSubLib CXX)

file(GLOB SOURCES     "*.cpp")
file(GLOB HEADERS     "*.h")

add_library(${PROJECT_NAME} SHARED ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    CommonUtils
    protobuf::libprotobuf
)
