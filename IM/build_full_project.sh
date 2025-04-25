#!/bin/bash
set -e

# 创建构建目录
rm -rf build_full
mkdir -p build_full
cd build_full

# 配置CMake
cat > CMakeLists.txt << 'EOT'
cmake_minimum_required(VERSION 3.10)
project(im_server LANGUAGES CXX)

# 基本设置
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g")

# 查找依赖
find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS system program_options thread filesystem)
find_package(OpenSSL REQUIRED)

# 设置protobuf生成目录
set(GENERATED_PROTO_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_PROTO_DIR})

# 生成protobuf和grpc文件
add_custom_command(
    OUTPUT 
        ${GENERATED_PROTO_DIR}/service.pb.cc
        ${GENERATED_PROTO_DIR}/service.pb.h
        ${GENERATED_PROTO_DIR}/service.grpc.pb.cc
        ${GENERATED_PROTO_DIR}/service.grpc.pb.h
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
    ARGS
        --proto_path=${CMAKE_SOURCE_DIR}/proto
        --cpp_out=${GENERATED_PROTO_DIR}
        --grpc_out=${GENERATED_PROTO_DIR}
        --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
        ${CMAKE_SOURCE_DIR}/proto/service.proto
    DEPENDS ${CMAKE_SOURCE_DIR}/proto/service.proto
    COMMENT "生成Protocol Buffers和gRPC代码"
    VERBATIM
)

# 创建proto生成目标
add_custom_target(gen_proto_files DEPENDS 
    ${GENERATED_PROTO_DIR}/service.pb.h 
    ${GENERATED_PROTO_DIR}/service.grpc.pb.h
)

# Proto库
add_library(proto_gen
    ${GENERATED_PROTO_DIR}/service.pb.cc
    ${GENERATED_PROTO_DIR}/service.grpc.pb.cc
)

target_include_directories(proto_gen PUBLIC
    ${CMAKE_BINARY_DIR}
    ${Protobuf_INCLUDE_DIRS}
)

target_link_libraries(proto_gen PUBLIC
    gRPC::grpc++
    protobuf::libprotobuf
)

# 收集所有源文件
file(GLOB SERVER_SOURCES
    ${CMAKE_SOURCE_DIR}/src/server/*.cpp
)

file(GLOB UTILS_SOURCES
    ${CMAKE_SOURCE_DIR}/src/server/utils/*.cpp
)

file(GLOB DB_SOURCES
    ${CMAKE_SOURCE_DIR}/src/server/db/*.cpp
)

file(GLOB KAFKA_SOURCES
    ${CMAKE_SOURCE_DIR}/src/server/kafka/*.cpp
)

# 两个目标：测试版和完整版
add_executable(im_server_test src/server/main.cpp)
add_executable(im_server src/server/main.cpp ${SERVER_SOURCES} ${UTILS_SOURCES} ${DB_SOURCES} ${KAFKA_SOURCES})

# 设置包含路径
target_include_directories(im_server PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
    ${GENERATED_PROTO_DIR}
    ${Boost_INCLUDE_DIRS}
)

target_include_directories(im_server_test PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
    ${GENERATED_PROTO_DIR}
    ${Boost_INCLUDE_DIRS}
)

# 链接库
target_link_libraries(im_server PRIVATE
    proto_gen
    Threads::Threads
    ${Boost_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
)

target_link_libraries(im_server_test PRIVATE
    proto_gen
    Threads::Threads
    ${Boost_LIBRARIES}
)

# 添加依赖关系
add_dependencies(proto_gen gen_proto_files)
add_dependencies(im_server proto_gen)
add_dependencies(im_server_test proto_gen)
EOT

# 编译项目
cmake ..
make -j$(nproc)
