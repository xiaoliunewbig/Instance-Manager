#include "server/utils/jwt_verifier.h"
#include "server/utils/security.h"
#include "server/utils/config.h"
#include "server/utils/logger.h"
#include <stdexcept>

namespace im {
namespace jwt {

using json = nlohmann::json;

json JWTVerifier::Verify(const std::string& token) {
    if (token.empty()) {
        throw std::runtime_error("Token is empty");
    }
    
    std::string secret = utils::Config::GetInstance().GetString("security.jwt_secret", "your_jwt_secret");
    std::map<std::string, std::string> payload;
    
    if (!utils::Security::VerifyJWT(token, secret, payload)) {
        throw std::runtime_error("Invalid token");
    }
    
    // 将字符串映射转换为JSON对象
    json result;
    for (const auto& [key, value] : payload) {
        // 尝试将值解析为JSON，如果失败则作为字符串处理
        try {
            result[key] = json::parse(value);
        } catch (...) {
            result[key] = value;
        }
    }
    
    return result;
}

} // namespace jwt
} // namespace im 