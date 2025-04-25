#ifndef IM_SECURITY_H
#define IM_SECURITY_H

#include <string>
#include <vector>
#include <map>
#include <chrono>

namespace im {
namespace utils {

/**
 * @brief 安全工具类，提供JWT身份验证、加密解密等功能
 */
class Security {
public:
    /**
     * @brief 生成JWT令牌
     * @param payload 数据载荷
     * @param secret 密钥
     * @param expire_seconds 过期时间（秒），默认为24小时
     * @return JWT令牌
     */
    static std::string GenerateJWT(
        const std::map<std::string, std::string>& payload,
        const std::string& secret,
        int expire_seconds = 86400
    );

    /**
     * @brief 验证JWT令牌
     * @param token JWT令牌
     * @param secret 密钥
     * @param payload 输出参数，成功时返回解析后的数据载荷
     * @return 有效返回true，无效返回false
     */
    static bool VerifyJWT(
        const std::string& token,
        const std::string& secret,
        std::map<std::string, std::string>& payload
    );

    /**
     * @brief MD5加密
     * @param input 输入字符串
     * @return MD5哈希值
     */
    static std::string MD5(const std::string& input);

    /**
     * @brief SHA256加密
     * @param input 输入字符串
     * @return SHA256哈希值
     */
    static std::string SHA256(const std::string& input);

    /**
     * @brief HMAC-SHA256加密
     * @param input 输入字符串
     * @param key 密钥
     * @return HMAC-SHA256哈希值
     */
    static std::string HMACSHA256(const std::string& input, const std::string& key);

    /**
     * @brief 生成随机盐值
     * @param length 盐值长度
     * @return 随机盐值
     */
    static std::string GenerateSalt(int length = 16);

    /**
     * @brief 使用盐值对密码进行哈希处理
     * @param password 密码
     * @param salt 盐值
     * @return 哈希后的密码
     */
    static std::string HashPassword(const std::string& password, const std::string& salt);

    /**
     * @brief 验证密码
     * @param password 输入密码
     * @param hashedPassword 哈希后的密码
     * @param salt 盐值
     * @return 验证成功返回true，失败返回false
     */
    static bool VerifyPassword(
        const std::string& password,
        const std::string& hashedPassword,
        const std::string& salt
    );

    /**
     * @brief AES加密
     * @param plaintext 明文
     * @param key 密钥
     * @param iv 初始化向量
     * @return 加密后的密文（Base64编码）
     */
    static std::string AESEncrypt(
        const std::string& plaintext,
        const std::string& key,
        const std::string& iv
    );

    /**
     * @brief AES解密
     * @param ciphertext 密文（Base64编码）
     * @param key 密钥
     * @param iv 初始化向量
     * @return 解密后的明文
     */
    static std::string AESDecrypt(
        const std::string& ciphertext,
        const std::string& key,
        const std::string& iv
    );

    /**
     * @brief Base64编码
     * @param input 输入数据
     * @return Base64编码后的字符串
     */
    static std::string Base64Encode(const std::string& input);

    /**
     * @brief Base64解码
     * @param input Base64编码的字符串
     * @return 解码后的数据
     */
    static std::string Base64Decode(const std::string& input);

    /**
     * @brief 生成随机数
     * @param min 最小值
     * @param max 最大值
     * @return 随机数
     */
    static int RandomInt(int min, int max);

    /**
     * @brief 生成验证码
     * @param length 验证码长度
     * @return 验证码
     */
    static std::string GenerateVerificationCode(int length = 6);

    /**
     * @brief 对请求参数进行签名
     * @param params 请求参数
     * @param secret 密钥
     * @return 签名
     */
    static std::string SignRequest(
        const std::map<std::string, std::string>& params,
        const std::string& secret
    );

    /**
     * @brief 验证请求签名
     * @param params 请求参数
     * @param signature 签名
     * @param secret 密钥
     * @return 验证成功返回true，失败返回false
     */
    static bool VerifyRequestSignature(
        const std::map<std::string, std::string>& params,
        const std::string& signature,
        const std::string& secret
    );

    /**
     * @brief 判断IP地址是否在白名单中
     * @param ip IP地址
     * @param whitelist IP地址白名单
     * @return 在白名单中返回true，否则返回false
     */
    static bool IsIpInWhitelist(
        const std::string& ip,
        const std::vector<std::string>& whitelist
    );

    /**
     * @brief 判断IP地址是否在黑名单中
     * @param ip IP地址
     * @param blacklist IP地址黑名单
     * @return 在黑名单中返回true，否则返回false
     */
    static bool IsIpInBlacklist(
        const std::string& ip,
        const std::vector<std::string>& blacklist
    );

    /**
     * @brief 生成防重放攻击的nonce
     * @return nonce字符串
     */
    static std::string GenerateNonce();

    /**
     * @brief 验证nonce是否有效
     * @param nonce nonce字符串
     * @param expireTime 过期时间
     * @return 有效返回true，无效返回false
     */
    static bool VerifyNonce(const std::string& nonce, int expireTime = 300);

    /**
     * @brief 获取JWT令牌的过期时间
     * @param token JWT令牌
     * @return 过期时间戳（Unix时间戳，秒）
     */
    static int64_t GetJWTExpireTime(const std::string& token);

    /**
     * @brief 检查JWT令牌是否过期
     * @param token JWT令牌
     * @return 已过期返回true，未过期返回false
     */
    static bool IsJWTExpired(const std::string& token);

private:
    /**
     * @brief Base64URL编码
     * @param input 输入数据
     * @return Base64URL编码后的字符串
     */
    static std::string Base64URLEncode(const std::string& input);

    /**
     * @brief Base64URL解码
     * @param input Base64URL编码的字符串
     * @return 解码后的数据
     */
    static std::string Base64URLDecode(const std::string& input);

    /**
     * @brief 解析JWT令牌
     * @param token JWT令牌
     * @param header 输出参数，JWT头部
     * @param payload 输出参数，JWT载荷
     * @param signature 输出参数，JWT签名
     * @return 解析成功返回true，失败返回false
     */
    static bool ParseJWT(
        const std::string& token,
        std::string& header,
        std::string& payload,
        std::string& signature
    );

    /**
     * @brief 解析JWT的JSON数据
     * @param json JSON字符串
     * @return 解析后的键值对
     */
    static std::map<std::string, std::string> ParseJWTJson(const std::string& json);
};

} // namespace utils
} // namespace im

#endif // IM_SECURITY_H 