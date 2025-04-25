#ifndef IM_JWT_VERIFIER_H
#define IM_JWT_VERIFIER_H

#include <string>
#include <nlohmann/json.hpp>

namespace im {
namespace jwt {

/**
 * @brief JWT令牌验证器
 */
class JWTVerifier {
public:
    /**
     * @brief 验证并解析JWT令牌
     * @param token JWT令牌
     * @return 解析后的payload
     * @throws 验证失败时抛出std::runtime_error
     */
    static nlohmann::json Verify(const std::string& token);

private:
    // 实现细节
};

} // namespace jwt
} // namespace im

#endif // IM_JWT_VERIFIER_H 