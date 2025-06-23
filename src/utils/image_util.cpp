#include "drm_fourcc.h"

#define STB_IMAGE_IMPLEMENTATION
#include "include/stb_image.h" 
#include "include/server.hpp"


struct wlr_buffer* create_wallpaper_buffer(const char* path) 
{
    int width, height, channels;
    unsigned char* pixel_data = stbi_load(path, &width, &height, &channels, 4);
    if (!pixel_data) {
        return NULL;
    }

    // 创建 shm allocator
    struct wlr_allocator *shm_allocator = wlr_shm_allocator_create();
    if (!shm_allocator) {
        stbi_image_free(pixel_data);
        return NULL;
    }

    // 创建格式描述
    struct wlr_drm_format drm_format = {0};
    drm_format.format = DRM_FORMAT_XRGB8888;
    drm_format.len = 1;
    drm_format.capacity = 1;
    drm_format.modifiers = static_cast<uint64_t*>(malloc(sizeof(uint64_t)));
    drm_format.modifiers[0] = DRM_FORMAT_MOD_LINEAR;

    // 使用 shm allocator 创建 buffer
    struct wlr_buffer* buffer = wlr_allocator_create_buffer(
        shm_allocator, width, height, &drm_format);
    
    free(drm_format.modifiers);
    wlr_allocator_destroy(shm_allocator);

    if (!buffer) {
        wlr_log(WLR_ERROR, "Failed to create SHM buffer");
        stbi_image_free(pixel_data);
        return NULL;
    }

    // SHM buffer 应该支持 CPU 访问
    void* buffer_data;
    uint32_t buffer_format;
    size_t buffer_stride;
    
    if (!wlr_buffer_begin_data_ptr_access(buffer, WLR_BUFFER_DATA_PTR_ACCESS_WRITE, 
                                          &buffer_data, &buffer_format, &buffer_stride)) {
        wlr_log(WLR_ERROR, "SHM buffer doesn't support CPU access");
        wlr_buffer_drop(buffer);
        stbi_image_free(pixel_data);
        return NULL;
    }

    // 复制像素数据
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint8_t* src = pixel_data + y * (width * 4) + x * 4;
            uint32_t* dst = (uint32_t*)((uint8_t*)buffer_data + y * buffer_stride + x * 4);
            
            uint8_t r = src[0], g = src[1], b = src[2], a = src[3];
            *dst = (r << 16) | (g << 8) | b;  // XRGB8888
        }
    }

    wlr_buffer_end_data_ptr_access(buffer);
    stbi_image_free(pixel_data);
    
    wlr_log(WLR_INFO, "Successfully created wallpaper buffer with SHM allocator");
    return buffer;
}


struct wlr_buffer* create_wallpaper_buffer_scaled(const char* path, int target_width, int target_height) 
{
    int original_width, original_height, channels;
    unsigned char* pixel_data = stbi_load(path, &original_width, &original_height, &channels, 4);
    if (!pixel_data) {
        return NULL;
    }

    // 计算缩放比例
    double scale_x = (double)target_width / original_width;
    double scale_y = (double)target_height / original_height;
    
    // 使用最小缩放比例来保持宽高比（letterbox）
    double scale = fmin(scale_x, scale_y);
    
    // 或者使用最大缩放比例来填充整个区域（可能会裁剪）
    // double scale = fmax(scale_x, scale_y);
    
    // 或者拉伸到完全匹配（可能会变形）
    // 这种情况下直接使用 scale_x 和 scale_y
    
    int scaled_width = (int)(original_width * scale);
    int scaled_height = (int)(original_height * scale);

    // 创建 shm allocator
    struct wlr_allocator *shm_allocator = wlr_shm_allocator_create();
    if (!shm_allocator) {
        stbi_image_free(pixel_data);
        return NULL;
    }

    // 创建格式描述
    struct wlr_drm_format drm_format = {0};
    drm_format.format = DRM_FORMAT_XRGB8888;
    drm_format.len = 1;
    drm_format.capacity = 1;
    drm_format.modifiers = static_cast<uint64_t*>(malloc(sizeof(uint64_t)));
    drm_format.modifiers[0] = DRM_FORMAT_MOD_LINEAR;

    // 使用目标尺寸创建 buffer
    struct wlr_buffer* buffer = wlr_allocator_create_buffer(
        shm_allocator, target_width, target_height, &drm_format);
    
    free(drm_format.modifiers);
    wlr_allocator_destroy(shm_allocator);

    if (!buffer) {
        wlr_log(WLR_ERROR, "Failed to create SHM buffer");
        stbi_image_free(pixel_data);
        return NULL;
    }

    void* buffer_data;
    uint32_t buffer_format;
    size_t buffer_stride;
    
    if (!wlr_buffer_begin_data_ptr_access(buffer, WLR_BUFFER_DATA_PTR_ACCESS_WRITE, 
                                          &buffer_data, &buffer_format, &buffer_stride)) {
        wlr_log(WLR_ERROR, "SHM buffer doesn't support CPU access");
        wlr_buffer_drop(buffer);
        stbi_image_free(pixel_data);
        return NULL;
    }

    // 先填充黑色背景
    memset(buffer_data, 0, buffer_stride * target_height);

    // 计算居中位置
    int offset_x = (target_width - scaled_width) / 2;
    int offset_y = (target_height - scaled_height) / 2;

    // 简单的最近邻缩放
    for (int y = 0; y < scaled_height; y++) {
        for (int x = 0; x < scaled_width; x++) {
            // 计算在原图中的对应位置
            int src_x = (int)(x / scale);
            int src_y = (int)(y / scale);
            
            // 边界检查
            if (src_x >= original_width) src_x = original_width - 1;
            if (src_y >= original_height) src_y = original_height - 1;
            
            uint8_t* src = pixel_data + src_y * (original_width * 4) + src_x * 4;
            
            // 计算目标位置（加上偏移量）
            int dst_x = x + offset_x;
            int dst_y = y + offset_y;
            
            if (dst_x >= 0 && dst_x < target_width && dst_y >= 0 && dst_y < target_height) {
                uint32_t* dst = (uint32_t*)((uint8_t*)buffer_data + dst_y * buffer_stride + dst_x * 4);
                
                uint8_t r = src[0], g = src[1], b = src[2], a = src[3];
                *dst = (r << 16) | (g << 8) | b;
            }
        }
    }

    wlr_buffer_end_data_ptr_access(buffer);
    stbi_image_free(pixel_data);
    
    wlr_log(WLR_INFO, "Successfully created scaled wallpaper buffer (%dx%d -> %dx%d)", 
            original_width, original_height, target_width, target_height);
    return buffer;
}
