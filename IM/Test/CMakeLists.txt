cmake_minimum_required(VERSION 3.10)
project(im_server VERSION 1.0.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 基本优化设置 - 使用更安全的选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")

# 简单并行编译
include(ProcessorCount)
ProcessorCount(CPU_COUNT)
if(CPU_COUNT EQUAL 0)
  set(CPU_COUNT 4)
endif()
message(STATUS "Using ${CPU_COUNT} cores for compilation")

# 查找依赖库
find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread filesystem program_options)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(nlohmann_json REQUIRED)

# MySQL库查找
find_path(MYSQL_INCLUDE_DIR mysql/mysql.h)
find_library(MYSQL_LIBRARY NAMES mysqlclient)

# Redis库查找
find_path(HIREDIS_HEADER hiredis/hiredis.h)
find_library(HIREDIS_LIB hiredis)

# Kafka库查找
find_library(RDKAFKA_LIBRARY rdkafka)
find_library(RDKAFKA_CPP_LIBRARY rdkafka++)

# 生成Protocol Buffers和gRPC代码
set(PROTO_FILES proto/service.proto)
set(GENERATED_PROTO_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_PROTO_DIR})

foreach(PROTO_FILE ${PROTO_FILES})
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
    add_custom_command(
        OUTPUT 
            ${GENERATED_PROTO_DIR}/${PROTO_NAME}.pb.cc
            ${GENERATED_PROTO_DIR}/${PROTO_NAME}.pb.h
            ${GENERATED_PROTO_DIR}/${PROTO_NAME}.grpc.pb.cc
            ${GENERATED_PROTO_DIR}/${PROTO_NAME}.grpc.pb.h
        COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        ARGS
            --proto_path=${CMAKE_SOURCE_DIR}/proto
            --cpp_out=${GENERATED_PROTO_DIR}
            --grpc_out=${GENERATED_PROTO_DIR}
            --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
            ${CMAKE_SOURCE_DIR}/${PROTO_FILE}
        DEPENDS ${CMAKE_SOURCE_DIR}/${PROTO_FILE}
        COMMENT "Generating Protocol Buffers and gRPC code for ${PROTO_FILE}"
        VERBATIM
    )
endforeach()

set(PROTO_SRCS
    ${GENERATED_PROTO_DIR}/service.pb.cc
    ${GENERATED_PROTO_DIR}/service.grpc.pb.cc
)
set(PROTO_HDRS
    ${GENERATED_PROTO_DIR}/service.pb.h
    ${GENERATED_PROTO_DIR}/service.grpc.pb.h
)

# 复制生成的文件到build/proto目录
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/proto
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/proto
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${GENERATED_PROTO_DIR}/service.pb.h
        ${GENERATED_PROTO_DIR}/service.grpc.pb.h
        ${CMAKE_BINARY_DIR}/proto/
    DEPENDS ${PROTO_HDRS}
    COMMENT "Copying protobuf headers to build directory"
)
add_custom_target(proto_copy DEPENDS ${CMAKE_BINARY_DIR}/proto)

# 源文件
file(GLOB_RECURSE SERVER_SOURCES 
    src/server/*.cpp
    src/server/db/*.cpp 
    src/server/utils/*.cpp
    src/server/kafka/*.cpp
)

# 添加可执行文件
add_executable(im_server 
    ${SERVER_SOURCES}
    ${PROTO_SRCS}
)

# 添加依赖关系
add_dependencies(im_server proto_copy)

# 包含路径
target_include_directories(im_server PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}
    ${GENERATED_PROTO_DIR}
    ${PROTOBUF_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
    ${MYSQL_INCLUDE_DIR}
    ${HIREDIS_HEADER}
    ${OPENSSL_INCLUDE_DIR}
)

# 链接库
target_link_libraries(im_server PRIVATE
    # 数据库
    ${MYSQL_LIBRARY}
    ${HIREDIS_LIB}
    
    # Kafka
    ${RDKAFKA_LIBRARY} 
    ${RDKAFKA_CPP_LIBRARY}
    
    # 工具库
    ${Boost_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
    
    # Protobuf 和 gRPC
    gRPC::grpc++
    gRPC::grpc
    protobuf::libprotobuf
    
    # 系统库
    Threads::Threads
    dl
)

# 添加安装规则
install(TARGETS im_server DESTINATION bin)
install(FILES conf/server.json DESTINATION etc/im_server)
install(CODE "file(MAKE_DIRECTORY \${CMAKE_INSTALL_PREFIX}/var/im_server/logs)")
install(CODE "file(MAKE_DIRECTORY \${CMAKE_INSTALL_PREFIX}/var/im_server/uploads)")

# 输出配置信息
message(STATUS "==== IM Server Build Configuration ====")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "CPU cores: ${CPU_COUNT}")
message(STATUS "Simple build mode enabled for reliable compilation")
message(STATUS "======================================")

# 设置一个明确的目标依赖顺序
add_custom_target(proto_base_build DEPENDS ${PROTO_SRCS} ${PROTO_HDRS})
add_dependencies(proto_copy proto_base_build)
add_dependencies(im_server proto_copy) 