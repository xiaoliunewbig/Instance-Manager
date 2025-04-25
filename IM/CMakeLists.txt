cmake_minimum_required(VERSION 3.16)

# 设置CMake策略
if(POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)
endif()

project(im_server VERSION 1.0.0 LANGUAGES CXX)

# =================== 编译优化设置 ===================

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 优化编译选项 - 增强性能优化
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -march=native -pipe")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -flto")

# 启用ccache以加速重复编译
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "Found ccache: ${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    # 配置ccache - 使用更简单的配置方式
    set(CCACHE_MAXSIZE "10G" CACHE STRING "ccache max cache size")
    set(CCACHE_SLOPPINESS "pch_defines,time_macros" CACHE STRING "ccache sloppiness")
    set(CCACHE_COMPRESS "true" CACHE BOOL "ccache compression")
    # 移除可能有问题的compresslevel和dir设置
endif()

# Make特定优化
if(CMAKE_GENERATOR MATCHES "Unix Makefiles")
    # 输出彩色编译信息
    set(CMAKE_COLOR_MAKEFILE ON)
    
    # 减少重新配置时间
    set_property(GLOBAL PROPERTY RULE_MESSAGES OFF)
endif()

# =================== 依赖库配置 ===================

# 查找必要的包
find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread filesystem program_options)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Threads REQUIRED)

# 优化：使用find_package的组件功能，避免多次查找
find_package(absl REQUIRED COMPONENTS 
    base strings synchronization time check log
    str_format log_internal_message log_internal_check_op
)

# 优化：MySQL库查找
find_package(MySQL QUIET)
if(NOT MySQL_FOUND)
    find_path(MYSQL_INCLUDE_DIR mysql/mysql.h
        /usr/include
        /usr/local/include
        /usr/mysql/include
        /usr/local/mysql/include
    )
    find_library(MYSQL_LIBRARY NAMES mysqlclient
        PATHS
        /usr/lib
        /usr/local/lib
        /usr/mysql/lib
        /usr/local/mysql/lib
    )
    if(MYSQL_INCLUDE_DIR AND MYSQL_LIBRARY)
        set(MYSQL_FOUND TRUE)
        message(STATUS "Found MySQL: ${MYSQL_LIBRARY}")
    else()
        message(FATAL_ERROR "MySQL not found. Install libmysqlclient-dev or specify paths manually.")
    endif()
endif()

# 确保hiredis库正确包含
find_path(HIREDIS_INCLUDE_DIR hiredis/hiredis.h)
find_library(HIREDIS_LIBRARY hiredis)

if(NOT HIREDIS_INCLUDE_DIR OR NOT HIREDIS_LIBRARY)
    message(FATAL_ERROR "Hiredis library not found. Please install libhiredis-dev")
endif()

# 优化：Kafka库查找
find_package(RdKafka QUIET)
if(NOT RdKafka_FOUND)
    find_library(RDKAFKA_LIBRARY rdkafka)
    find_library(RDKAFKA_CPP_LIBRARY rdkafka++)
    if(RDKAFKA_LIBRARY AND RDKAFKA_CPP_LIBRARY)
        set(RdKafka_FOUND TRUE)
        message(STATUS "Found RdKafka: ${RDKAFKA_LIBRARY}, ${RDKAFKA_CPP_LIBRARY}")
    else()
        message(WARNING "RdKafka library not found. Will attempt to link during compilation.")
    endif()
endif()

# =================== Protocol Buffers 和 gRPC 生成 ===================

# 优化：使用变量存储生成目录路径
set(PROTO_FILES proto/service.proto)
set(GENERATED_PROTO_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_PROTO_DIR})

# 优化：更细粒度的依赖关系跟踪
set(PROTO_SRCS)
set(PROTO_HDRS)

foreach(PROTO_FILE ${PROTO_FILES})
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
    list(APPEND PROTO_SRCS
        "${GENERATED_PROTO_DIR}/${PROTO_NAME}.pb.cc"
        "${GENERATED_PROTO_DIR}/${PROTO_NAME}.grpc.pb.cc"
    )
    list(APPEND PROTO_HDRS
        "${GENERATED_PROTO_DIR}/${PROTO_NAME}.pb.h"
        "${GENERATED_PROTO_DIR}/${PROTO_NAME}.grpc.pb.h"
    )
    
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

# 确保proto编译是单线程的，避免竞争
add_custom_target(proto_copy
    COMMAND ${CMAKE_COMMAND} -E echo "Copying proto files to build directory"
    DEPENDS ${PROTO_SRCS} ${PROTO_HDRS}
)

# 设置proto目标的job pool
add_custom_target(proto_base_build DEPENDS ${PROTO_SRCS} ${PROTO_HDRS})
set_property(TARGET proto_base_build PROPERTY JOB_POOL proto_pool)
# 修正：先确保proto_copy目标存在，然后再添加依赖
add_dependencies(proto_copy proto_base_build)
set_property(TARGET proto_copy PROPERTY JOB_POOL proto_pool)

# =================== 源文件管理 ===================

# 优化：分组源文件以实现更好的并行编译
set(SERVER_CORE_SOURCES
    src/server/admin_service.cpp
    src/server/file_service.cpp
    src/server/main.cpp
    src/server/message_service.cpp
    src/server/notification_service.cpp
    src/server/push_notification_service.cpp
    src/server/relation_service.cpp
    src/server/server.cpp
    src/server/user_service.cpp
    src/server/websocket_handler.cpp
)

set(SERVER_DB_SOURCES)
file(GLOB SERVER_DB_SOURCES src/server/db/*.cpp)

set(SERVER_UTILS_SOURCES)
file(GLOB SERVER_UTILS_SOURCES src/server/utils/*.cpp)

set(SERVER_KAFKA_SOURCES)
file(GLOB SERVER_KAFKA_SOURCES src/server/kafka/*.cpp)

# =================== 构建主目标 ===================

# 服务器编译可以多线程
add_executable(im_server ${SERVER_CORE_SOURCES} ${SERVER_DB_SOURCES} ${SERVER_UTILS_SOURCES} ${SERVER_KAFKA_SOURCES} ${PROTO_SRCS})
add_dependencies(im_server proto_copy)

# 设置头文件搜索路径
target_include_directories(im_server PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
    ${GENERATED_PROTO_DIR}
    ${PROTOBUF_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
    ${MYSQL_INCLUDE_DIR}
    ${HIREDIS_INCLUDE_DIR}
    ${OPENSSL_INCLUDE_DIR}
)

# 使用target_link_libraries的PRIVATE关键字并整理顺序
target_link_libraries(im_server PRIVATE
    # 数据库和消息队列
    ${MYSQL_LIBRARY}
    ${HIREDIS_LIBRARY}
    
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

# 添加Abseil库
if(absl_FOUND)
    target_link_libraries(im_server PRIVATE
        absl::base
        absl::strings
        absl::synchronization
        absl::time
        absl::check
        absl::log
        absl::str_format
    )
endif()

# 添加Kafka库
if(RdKafka_FOUND)
    target_link_libraries(im_server PRIVATE 
        ${RDKAFKA_LIBRARY} 
        ${RDKAFKA_CPP_LIBRARY}
    )
endif()

# =================== 安装配置 ===================

# 安装目标
install(TARGETS im_server DESTINATION bin)
install(FILES conf/server.json DESTINATION etc/im_server)
install(CODE "file(MAKE_DIRECTORY \${CMAKE_INSTALL_PREFIX}/var/im_server/logs)")
install(CODE "file(MAKE_DIRECTORY \${CMAKE_INSTALL_PREFIX}/var/im_server/uploads)")

# 设置 CPack 配置
set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

# =================== 编译信息输出 ===================

# 添加编译信息输出
message(STATUS "======== IM Server Build Configuration ========")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
message(STATUS "============================================")

# 启用详细的日志输出
set(CMAKE_VERBOSE_MAKEFILE ON)

# 在项目定义之后
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # 使用CMake内置的预编译头支持
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.16)
        # 确认PCH头文件和源文件存在
        if(EXISTS "${CMAKE_SOURCE_DIR}/src/server/pch.h")
            target_precompile_headers(im_server PRIVATE 
                "${CMAKE_SOURCE_DIR}/src/server/pch.h")
            message(STATUS "Using precompiled headers")
        else()
            message(WARNING "PCH header not found, skipping precompiled headers")
        endif()
    endif()
endif()

# 启用并行编译
include(ProcessorCount)
ProcessorCount(CPU_COUNT)
if(CPU_COUNT)
    set(CMAKE_BUILD_PARALLEL_LEVEL ${CPU_COUNT})
endif()



