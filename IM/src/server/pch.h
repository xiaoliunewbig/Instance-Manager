#ifndef IM_PCH_H
#define IM_PCH_H

// 标准库
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <functional>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <stdexcept>

// 第三方库
#include <grpcpp/grpcpp.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/server_builder.h>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <mysql/mysql.h>

// 工程公共头文件
#include "include/server/utils/logger.h"
#include "include/server/utils/config.h"

#endif // IM_PCH_H