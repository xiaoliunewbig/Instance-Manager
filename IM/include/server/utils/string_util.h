#ifndef IM_STRING_UTIL_H
#define IM_STRING_UTIL_H

#include <string>
#include <vector>
#include <sstream>
#include <memory>

namespace im {
namespace utils {

/**
 * @brief 字符串工具类，提供常用的字符串操作
 */
class StringUtil {
public:
    /**
     * @brief 转换字符串为小写
     * @param str 输入字符串
     * @return 转换后的字符串
     */
    static std::string ToLower(const std::string& str);

    /**
     * @brief 转换字符串为大写
     * @param str 输入字符串
     * @return 转换后的字符串
     */
    static std::string ToUpper(const std::string& str);

    /**
     * @brief 去除字符串两端的空白字符
     * @param str 输入字符串
     * @return 去除空白后的字符串
     */
    static std::string Trim(const std::string& str);

    /**
     * @brief 去除字符串左侧的空白字符
     * @param str 输入字符串
     * @return 去除空白后的字符串
     */
    static std::string LTrim(const std::string& str);

    /**
     * @brief 去除字符串右侧的空白字符
     * @param str 输入字符串
     * @return 去除空白后的字符串
     */
    static std::string RTrim(const std::string& str);

    /**
     * @brief 分割字符串
     * @param str 输入字符串
     * @param delimiter 分隔符
     * @return 分割后的字符串数组
     */
    static std::vector<std::string> Split(const std::string& str, char delimiter);

    /**
     * @brief 分割字符串
     * @param str 输入字符串
     * @param delimiter 分隔符字符串
     * @return 分割后的字符串数组
     */
    static std::vector<std::string> Split(const std::string& str, const std::string& delimiter);

    /**
     * @brief 连接字符串数组
     * @param strs 字符串数组
     * @param delimiter 分隔符
     * @return 连接后的字符串
     */
    static std::string Join(const std::vector<std::string>& strs, const std::string& delimiter);

    /**
     * @brief 替换字符串中的子串
     * @param str 输入字符串
     * @param oldStr 要替换的子串
     * @param newStr 替换为的子串
     * @return 替换后的字符串
     */
    static std::string Replace(const std::string& str, const std::string& oldStr, const std::string& newStr);

    /**
     * @brief 检查字符串是否以指定子串开头
     * @param str 输入字符串
     * @param prefix 前缀字符串
     * @return 如果以指定子串开头，返回true；否则返回false
     */
    static bool StartsWith(const std::string& str, const std::string& prefix);

    /**
     * @brief 检查字符串是否以指定子串结尾
     * @param str 输入字符串
     * @param suffix 后缀字符串
     * @return 如果以指定子串结尾，返回true；否则返回false
     */
    static bool EndsWith(const std::string& str, const std::string& suffix);

    /**
     * @brief 检查字符串是否包含指定子串
     * @param str 输入字符串
     * @param substr 子串
     * @return 如果包含指定子串，返回true；否则返回false
     */
    static bool Contains(const std::string& str, const std::string& substr);

    /**
     * @brief 转换字符串为整数
     * @param str 输入字符串
     * @param defaultValue 转换失败时返回的默认值
     * @return 转换后的整数
     */
    static int ToInt(const std::string& str, int defaultValue = 0);

    /**
     * @brief 转换字符串为长整数
     * @param str 输入字符串
     * @param defaultValue 转换失败时返回的默认值
     * @return 转换后的长整数
     */
    static int64_t ToInt64(const std::string& str, int64_t defaultValue = 0);

    /**
     * @brief 转换字符串为双精度浮点数
     * @param str 输入字符串
     * @param defaultValue 转换失败时返回的默认值
     * @return 转换后的双精度浮点数
     */
    static double ToDouble(const std::string& str, double defaultValue = 0.0);

    /**
     * @brief 转换字符串为布尔值
     * @param str 输入字符串
     * @param defaultValue 转换失败时返回的默认值
     * @return 转换后的布尔值
     */
    static bool ToBool(const std::string& str, bool defaultValue = false);

    /**
     * @brief 格式化字符串
     * @param format 格式字符串
     * @param args 参数列表
     * @return 格式化后的字符串
     */
    template<typename... Args>
    static std::string Format(const std::string& format, Args... args) {
        // 计算格式化后字符串所需的长度
        int size = snprintf(nullptr, 0, format.c_str(), args...) + 1;
        if (size <= 0) {
            return "";
        }

        // 分配足够的缓冲区
        std::unique_ptr<char[]> buf(new char[size]);
        // 格式化字符串
        snprintf(buf.get(), size, format.c_str(), args...);
        
        // 返回格式化后的字符串，不包含末尾的\0
        return std::string(buf.get(), buf.get() + size - 1);
    }

    /**
     * @brief 将二进制数据转换为十六进制字符串
     * @param data 二进制数据
     * @param length 数据长度
     * @return 十六进制字符串
     */
    static std::string BinToHex(const unsigned char* data, size_t length);

    /**
     * @brief 将十六进制字符串转换为二进制数据
     * @param hex 十六进制字符串
     * @return 二进制数据
     */
    static std::vector<unsigned char> HexToBin(const std::string& hex);

    /**
     * @brief 生成随机字符串
     * @param length 字符串长度
     * @return 随机字符串
     */
    static std::string RandomString(size_t length);

    /**
     * @brief 生成UUID字符串
     * @return UUID字符串
     */
    static std::string GenUUID();

    /**
     * @brief 对URL进行编码
     * @param url 原始URL
     * @return 编码后的URL
     */
    static std::string UrlEncode(const std::string& url);

    /**
     * @brief 对URL进行解码
     * @param url 编码后的URL
     * @return 解码后的URL
     */
    static std::string UrlDecode(const std::string& url);
};

} // namespace utils
} // namespace im

#endif // IM_STRING_UTIL_H 