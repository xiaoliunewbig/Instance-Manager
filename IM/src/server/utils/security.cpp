#include "server/utils/security.h"
#include "server/utils/logger.h"
#include "server/utils/string_util.h"
#include "server/utils/datetime.h"

#include <openssl/md5.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <random>
#include <nlohmann/json.hpp>
#include <regex>
#include <sstream>
#include <iomanip>
#include <unordered_set>
#include <mutex>

namespace im {
namespace utils {

// 存储已使用的nonce，防止重放攻击
static std::unordered_set<std::string> used_nonces;
static std::mutex nonce_mutex;

std::string Security::GenerateJWT(
    const std::map<std::string, std::string>& payload,
    const std::string& secret,
    int expire_seconds
) {
    try {
        // 构建JWT头部，指定算法为HS256
        nlohmann::json header = {
            {"alg", "HS256"},
            {"typ", "JWT"}
        };

        // 构建JWT载荷
        nlohmann::json json_payload;
        for (const auto& pair : payload) {
            json_payload[pair.first] = pair.second;
        }

        // 添加过期时间（Unix时间戳）
        int64_t exp = DateTime::NowSeconds() + expire_seconds;
        json_payload["exp"] = std::to_string(exp);

        // 添加签发时间
        json_payload["iat"] = std::to_string(DateTime::NowSeconds());

        // Base64URL编码头部和载荷
        std::string encoded_header = Base64URLEncode(header.dump());
        std::string encoded_payload = Base64URLEncode(json_payload.dump());

        // 生成JWT签名
        std::string data = encoded_header + "." + encoded_payload;
        std::string signature = HMACSHA256(data, secret);
        std::string encoded_signature = Base64URLEncode(signature);

        // 拼接JWT令牌
        return data + "." + encoded_signature;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to generate JWT: {}", e.what());
        return "";
    }
}

bool Security::VerifyJWT(
    const std::string& token,
    const std::string& secret,
    std::map<std::string, std::string>& payload
) {
    try {
        // 解析JWT令牌
        std::string header_str, payload_str, signature_str;
        if (!ParseJWT(token, header_str, payload_str, signature_str)) {
            return false;
        }

        // 验证签名
        std::string data = header_str + "." + payload_str;
        std::string expected_signature = HMACSHA256(data, secret);
        std::string decoded_signature = Base64URLDecode(signature_str);

        if (expected_signature != decoded_signature) {
            LOG_DEBUG("JWT signature verification failed");
            return false;
        }

        // 解析载荷
        std::string decoded_payload = Base64URLDecode(payload_str);
        nlohmann::json json_payload = nlohmann::json::parse(decoded_payload);

        // 验证过期时间
        if (json_payload.contains("exp")) {
            int64_t exp = StringUtil::ToInt64(json_payload["exp"].get<std::string>());
            if (DateTime::NowSeconds() > exp) {
                LOG_DEBUG("JWT token expired");
                return false;
            }
        }

        // 提取载荷数据
        for (auto it = json_payload.begin(); it != json_payload.end(); ++it) {
            if (it.value().is_string()) {
                payload[it.key()] = it.value().get<std::string>();
            } else {
                payload[it.key()] = it.value().dump();
            }
        }

        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("JWT verification failed: {}", e.what());
        return false;
    }
}

std::string Security::MD5(const std::string& input) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len;
    
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        throw std::runtime_error("Failed to create MD5 context");
    }
    
    if (EVP_DigestInit_ex(ctx, EVP_md5(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx, input.c_str(), input.length()) != 1 ||
        EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("MD5 computation failed");
    }
    
    EVP_MD_CTX_free(ctx);
    
    std::string result;
    result.reserve(digest_len * 2);
    for (unsigned int i = 0; i < digest_len; ++i) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", digest[i]);
        result.append(buf);
    }
    
    return result;
}

std::string Security::SHA256(const std::string& input) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;
    
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        throw std::runtime_error("Failed to create SHA256 context");
    }
    
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx, input.c_str(), input.length()) != 1 ||
        EVP_DigestFinal_ex(ctx, hash, &hash_len) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("SHA256 computation failed");
    }
    
    EVP_MD_CTX_free(ctx);
    
    std::string result;
    result.reserve(hash_len * 2);
    for (unsigned int i = 0; i < hash_len; ++i) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", hash[i]);
        result.append(buf);
    }
    
    return result;
}

std::string Security::HMACSHA256(const std::string& input, const std::string& key) {
    unsigned char* digest = HMAC(EVP_sha256(), key.c_str(), key.length(),
                                (unsigned char*)input.c_str(), input.length(),
                                NULL, NULL);
    
    return std::string(reinterpret_cast<char*>(digest), SHA256_DIGEST_LENGTH);
}

std::string Security::GenerateSalt(int length) {
    return StringUtil::RandomString(length);
}

std::string Security::HashPassword(const std::string& password, const std::string& salt) {
    // 使用SHA256和盐值对密码进行哈希
    return SHA256(password + salt);
}

bool Security::VerifyPassword(
    const std::string& password,
    const std::string& hashedPassword,
    const std::string& salt
) {
    // 使用相同的算法计算密码哈希
    std::string calculatedHash = HashPassword(password, salt);
    return calculatedHash == hashedPassword;
}

std::string Security::AESEncrypt(
    const std::string& plaintext,
    const std::string& key,
    const std::string& iv
) {
    try {
        // 创建加密上下文
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            LOG_ERROR("Failed to create AES encryption context");
            return "";
        }

        // 初始化加密操作
        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, 
                              reinterpret_cast<const unsigned char*>(key.c_str()),
                              reinterpret_cast<const unsigned char*>(iv.c_str())) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            LOG_ERROR("Failed to initialize AES encryption");
            return "";
        }

        // 准备输出缓冲区
        int cipher_block_size = EVP_CIPHER_block_size(EVP_aes_256_cbc());
        int max_output_len = plaintext.length() + cipher_block_size;
        unsigned char* ciphertext = new unsigned char[max_output_len];

        // 加密数据
        int out_len1 = 0;
        if (EVP_EncryptUpdate(ctx, ciphertext, &out_len1,
                             reinterpret_cast<const unsigned char*>(plaintext.c_str()),
                             plaintext.length()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            delete[] ciphertext;
            LOG_ERROR("Failed during AES encryption update");
            return "";
        }

        // 加密最后的块
        int out_len2 = 0;
        if (EVP_EncryptFinal_ex(ctx, ciphertext + out_len1, &out_len2) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            delete[] ciphertext;
            LOG_ERROR("Failed during AES encryption final");
            return "";
        }

        // 清理上下文
        EVP_CIPHER_CTX_free(ctx);

        // 将结果转换为Base64编码
        std::string result = Base64Encode(std::string(reinterpret_cast<char*>(ciphertext), out_len1 + out_len2));
        delete[] ciphertext;
        
        return result;
    } catch (const std::exception& e) {
        LOG_ERROR("AES encryption error: {}", e.what());
        return "";
    }
}

std::string Security::AESDecrypt(
    const std::string& ciphertext,
    const std::string& key,
    const std::string& iv
) {
    try {
        // 将Base64编码的密文解码
        std::string decoded_ciphertext = Base64Decode(ciphertext);

        // 创建解密上下文
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            LOG_ERROR("Failed to create AES decryption context");
            return "";
        }

        // 初始化解密操作
        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, 
                              reinterpret_cast<const unsigned char*>(key.c_str()),
                              reinterpret_cast<const unsigned char*>(iv.c_str())) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            LOG_ERROR("Failed to initialize AES decryption");
            return "";
        }

        // 准备输出缓冲区
        int plaintext_len = decoded_ciphertext.length();
        unsigned char* plaintext = new unsigned char[plaintext_len + 16];  // 16字节为一个AES块

        // 解密数据
        int out_len1 = 0;
        if (EVP_DecryptUpdate(ctx, plaintext, &out_len1,
                             reinterpret_cast<const unsigned char*>(decoded_ciphertext.c_str()),
                             decoded_ciphertext.length()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            delete[] plaintext;
            LOG_ERROR("Failed during AES decryption update");
            return "";
        }

        // 解密最后的块
        int out_len2 = 0;
        if (EVP_DecryptFinal_ex(ctx, plaintext + out_len1, &out_len2) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            delete[] plaintext;
            LOG_ERROR("Failed during AES decryption final");
            return "";
        }

        // 清理上下文
        EVP_CIPHER_CTX_free(ctx);

        // 构建结果
        std::string result(reinterpret_cast<char*>(plaintext), out_len1 + out_len2);
        delete[] plaintext;
        
        return result;
    } catch (const std::exception& e) {
        LOG_ERROR("AES decryption error: {}", e.what());
        return "";
    }
}

std::string Security::Base64Encode(const std::string& input) {
    BIO* bio, *b64;
    BUF_MEM* bufferPtr;

    b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL); // 不添加换行符
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_write(bio, input.c_str(), input.length());
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);

    std::string result(bufferPtr->data, bufferPtr->length);
    BIO_free_all(bio);

    return result;
}

std::string Security::Base64Decode(const std::string& input) {
    BIO* bio, *b64;
    std::vector<char> buffer(input.size());

    b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL); // 不添加换行符
    bio = BIO_new_mem_buf(input.c_str(), input.length());
    bio = BIO_push(b64, bio);

    int decodedLength = BIO_read(bio, buffer.data(), input.size());
    BIO_free_all(bio);

    return std::string(buffer.data(), decodedLength);
}

int Security::RandomInt(int min, int max) {
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<> distribution(min, max);
    return distribution(generator);
}

std::string Security::GenerateVerificationCode(int length) {
    std::string result;
    for (int i = 0; i < length; ++i) {
        result += std::to_string(RandomInt(0, 9));
    }
    return result;
}

std::string Security::SignRequest(
    const std::map<std::string, std::string>& params,
    const std::string& secret
) {
    // 按键名字典序排序
    std::map<std::string, std::string> sorted_params(params.begin(), params.end());

    // 构建待签名字符串
    std::string sign_str;
    for (const auto& pair : sorted_params) {
        if (pair.first != "sign" && !pair.second.empty()) {
            sign_str += pair.first + "=" + pair.second + "&";
        }
    }

    // 添加密钥
    sign_str += "key=" + secret;

    // 使用SHA256生成签名
    return SHA256(sign_str);
}

bool Security::VerifyRequestSignature(
    const std::map<std::string, std::string>& params,
    const std::string& signature,
    const std::string& secret
) {
    // 提取不包含签名的参数
    std::map<std::string, std::string> params_without_sign;
    for (const auto& pair : params) {
        if (pair.first != "sign") {
            params_without_sign[pair.first] = pair.second;
        }
    }

    // 生成预期的签名
    std::string expected_signature = SignRequest(params_without_sign, secret);

    // 验证签名是否匹配
    return expected_signature == signature;
}

bool Security::IsIpInWhitelist(
    const std::string& ip,
    const std::vector<std::string>& whitelist
) {
    // 如果白名单为空，则允许所有IP
    if (whitelist.empty()) {
        return true;
    }

    // 检查IP是否在白名单中
    for (const auto& allowed_ip : whitelist) {
        // 精确匹配
        if (allowed_ip == ip) {
            return true;
        }

        // CIDR匹配 (简化实现，仅支持/8, /16, /24)
        if (allowed_ip.find('/') != std::string::npos) {
            std::vector<std::string> cidr_parts = StringUtil::Split(allowed_ip, '/');
            if (cidr_parts.size() == 2) {
                std::string network = cidr_parts[0];
                int mask_bits = StringUtil::ToInt(cidr_parts[1]);

                // 简单的CIDR匹配
                std::vector<std::string> ip_parts = StringUtil::Split(ip, '.');
                std::vector<std::string> network_parts = StringUtil::Split(network, '.');

                if (ip_parts.size() == 4 && network_parts.size() == 4) {
                    bool match = true;
                    
                    // 根据子网掩码长度进行比较
                    if (mask_bits >= 24) { // /24 或更精确
                        match = (ip_parts[0] == network_parts[0] && 
                                ip_parts[1] == network_parts[1] && 
                                ip_parts[2] == network_parts[2]);
                    } else if (mask_bits >= 16) { // /16
                        match = (ip_parts[0] == network_parts[0] && 
                                ip_parts[1] == network_parts[1]);
                    } else if (mask_bits >= 8) { // /8
                        match = (ip_parts[0] == network_parts[0]);
                    }

                    if (match) {
                        return true;
                    }
                }
            }
        }

        // 通配符匹配
        if (allowed_ip.find('*') != std::string::npos) {
            std::string pattern = StringUtil::Replace(allowed_ip, ".", "\\.");
            pattern = StringUtil::Replace(pattern, "*", ".*");
            std::regex ip_regex(pattern);
            if (std::regex_match(ip, ip_regex)) {
                return true;
            }
        }
    }

    return false;
}

bool Security::IsIpInBlacklist(
    const std::string& ip,
    const std::vector<std::string>& blacklist
) {
    // 黑名单为空，则所有IP都不在黑名单中
    if (blacklist.empty()) {
        return false;
    }

    // 使用与白名单相同的匹配逻辑
    return IsIpInWhitelist(ip, blacklist);
}

std::string Security::GenerateNonce() {
    // 生成随机字符串作为nonce
    std::string nonce = StringUtil::GenUUID();

    // 存储nonce，防止重放攻击
    std::lock_guard<std::mutex> lock(nonce_mutex);
    used_nonces.insert(nonce);

    return nonce;
}

bool Security::VerifyNonce(const std::string& nonce, int expireTime) {
    std::lock_guard<std::mutex> lock(nonce_mutex);

    // 检查nonce是否已被使用
    if (used_nonces.find(nonce) != used_nonces.end()) {
        return false;
    }

    // 将nonce添加到已使用集合中
    used_nonces.insert(nonce);

    // 注意：实际应用中，应该有一个定时任务清理过期的nonce
    // 这里简化处理，仅做示例

    return true;
}

int64_t Security::GetJWTExpireTime(const std::string& token) {
    std::string header_str, payload_str, signature_str;
    if (!ParseJWT(token, header_str, payload_str, signature_str)) {
        return 0;
    }

    try {
        std::string decoded_payload = Base64URLDecode(payload_str);
        nlohmann::json json_payload = nlohmann::json::parse(decoded_payload);

        if (json_payload.contains("exp")) {
            return StringUtil::ToInt64(json_payload["exp"].get<std::string>());
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to get JWT expire time: {}", e.what());
    }

    return 0;
}

bool Security::IsJWTExpired(const std::string& token) {
    int64_t expire_time = GetJWTExpireTime(token);
    if (expire_time == 0) {
        // 解析失败或没有过期时间，视为已过期
        return true;
    }

    // 检查是否过期
    return DateTime::NowSeconds() > expire_time;
}

std::string Security::Base64URLEncode(const std::string& input) {
    std::string base64 = Base64Encode(input);
    
    // 将标准Base64转换为Base64URL
    base64 = StringUtil::Replace(base64, "+", "-");
    base64 = StringUtil::Replace(base64, "/", "_");
    
    // 去除填充字符
    base64 = StringUtil::Replace(base64, "=", "");
    
    return base64;
}

std::string Security::Base64URLDecode(const std::string& input) {
    std::string base64 = input;
    
    // 将Base64URL转换为标准Base64
    base64 = StringUtil::Replace(base64, "-", "+");
    base64 = StringUtil::Replace(base64, "_", "/");
    
    // 添加填充字符
    switch (base64.length() % 4) {
        case 0: break;
        case 2: base64 += "=="; break;
        case 3: base64 += "="; break;
    }
    
    return Base64Decode(base64);
}

bool Security::ParseJWT(
    const std::string& token,
    std::string& header,
    std::string& payload,
    std::string& signature
) {
    try {
        std::vector<std::string> parts = StringUtil::Split(token, '.');
        if (parts.size() != 3) {
            LOG_DEBUG("Invalid JWT token format");
            return false;
        }

        header = parts[0];
        payload = parts[1];
        signature = parts[2];
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("JWT parsing error: {}", e.what());
        return false;
    }
}

std::map<std::string, std::string> Security::ParseJWTJson(const std::string& json) {
    std::map<std::string, std::string> result;
    
    try {
        nlohmann::json parsed = nlohmann::json::parse(json);
        
        for (auto it = parsed.begin(); it != parsed.end(); ++it) {
            if (it.value().is_string()) {
                result[it.key()] = it.value().get<std::string>();
            } else {
                result[it.key()] = it.value().dump();
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("JWT JSON parsing error: {}", e.what());
    }
    
    return result;
}

} // namespace utils
} // namespace im