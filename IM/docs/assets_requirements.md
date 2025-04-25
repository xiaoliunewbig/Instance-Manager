# 图片资源需求文档

## 1. 图片列表
| 图片路径 | 功能描述 |
|----------|----------|
| `qrc:/assets/login_logo.png` | 登录页面的应用 Logo |
| `qrc:/assets/register_logo.png` | 注册页面的应用 Logo |
| `qrc:/assets/default_avatar.png` | 用户默认头像，用于用户列表和聊天界面 |
| `qrc:/assets/group_avatar.png` | 群组默认头像，用于群组列表 |
| `qrc:/assets/emoji_icon.png` | 表情选择器的图标 |
| `qrc:/assets/file_icon.png` | 文件上传和下载的图标 |
| `qrc:/assets/theme_icon.png` | 主题切换页面的图标 |
| `qrc:/assets/status_online.png` | 用户在线状态的图标 |
| `qrc:/assets/status_offline.png` | 用户离线状态的图标 |
| `qrc:/assets/status_away.png` | 用户离开状态的图标 |
| `qrc:/assets/status_busy.png` | 用户忙碌状态的图标 |

## 2. 功能描述
1. **登录页面**：
   - 应用 Logo：用于展示品牌标识，提升用户体验。

2. **注册页面**：
   - 应用 Logo：用于展示品牌标识，提升用户体验。

3. **用户列表和聊天界面**：
   - 默认头像：用于展示用户和群组头像，增强辨识度。
   - 状态图标：用于展示用户在线状态，方便用户了解对方状态。

4. **表情选择器**：
   - 表情图标：用于快速选择表情，提升聊天趣味性。

5. **文件上传和下载**：
   - 文件图标：用于标识文件类型，方便用户识别。

6. **主题切换页面**：
   - 主题图标：用于标识主题选项，方便用户选择。

## 3. 使用说明
所有图片资源均放置在 `qrc:/assets/` 路径下，QML 代码中通过 `source` 属性引用。例如：
```qml
Image {
    source: "qrc:/assets/login_logo.png"
}
```