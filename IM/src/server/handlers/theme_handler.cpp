// #include "theme_handler.h"
// #include "websocket_manager.h"
// #include "logger.h"

// void ThemeHandler::handleUpdateTheme(const json& data) {
//     std::string theme = data["theme"];
//     if (theme == "light" || theme == "dark") {
//         // 更新用户主题
//         bool success = updateUserTheme(WebSocketManager::getCurrentUserId(), theme);
//         if (success) {
//             Logger::info("用户主题已更新: " + theme);
//             // 通知前端主题更新成功
//             WebSocketManager::sendMessage({
//                 {"type", "themeUpdated"},
//                 {"data", {
//                     {"theme", theme},
//                     {"success", true}
//                 }}
//             });
//         } else {
//             Logger::error("用户主题更新失败: " + theme);
//             // 通知前端主题更新失败
//             WebSocketManager::sendMessage({
//                 {"type", "themeUpdated"},
//                 {"data", {
//                     {"theme", theme},
//                     {"success", false},
//                     {"message", "主题更新失败"}
//                 }}
//             });
//         }
//     } else {
//         Logger::error("无效的主题: " + theme);
//         // 通知前端主题无效
//         WebSocketManager::sendMessage({
//             {"type", "themeUpdated"},
//             {"data", {
//                 {"theme", theme},
//                 {"success", false},
//                 {"message", "无效的主题"}
//             }}
//         });
//     }
// }