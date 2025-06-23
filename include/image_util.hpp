#include "server.hpp"
#include <wlr/render/wlr_renderer.h>

// 加载图片文件并创建纹理
struct wlr_buffer* create_wallpaper_buffer(const char* path);
struct wlr_buffer* create_wallpaper_buffer_scaled(const char* path, int target_width, int target_height);