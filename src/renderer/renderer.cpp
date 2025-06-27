#include "server.hpp"
#include "src/wrap/c99_unsafe_defs_wrap.h"

#include <wlr/render/gles2.h>
#include <wlr/types/wlr_output.h>


void render_surface_and_subsurfaces(
    struct wlr_surface *surface,
    double base_x, double base_y,
    struct wlr_render_pass *render_pass
) {

    // --- 1. 渲染所有在“下面”的子表面 ---
    struct wlr_subsurface *subsurface;
    wl_list_for_each(subsurface, &surface->current.subsurfaces_below, current.link) {
        struct wlr_surface *sub_surface = subsurface->surface;
        double sx = base_x + subsurface->current.x;
        double sy = base_y + subsurface->current.y;
        render_surface_and_subsurfaces(sub_surface, sx, sy, render_pass);
    }

    // 渲染主表面
    struct wlr_texture *texture = wlr_surface_get_texture(surface);
    if (texture) {
        struct wlr_render_texture_options texture_options = {
            texture,
            { 0, 0, (double)surface->current.width, (double)surface->current.height },
            { static_cast<int>(base_x), static_cast<int>(base_y), surface->current.width, surface->current.height },
            nullptr,
            nullptr,
            surface->current.transform,
            WLR_SCALE_FILTER_BILINEAR,
            WLR_RENDER_BLEND_MODE_PREMULTIPLIED
        };
        wlr_render_pass_add_texture(render_pass, &texture_options);
    }

    // 遍历并渲染所有子表面
    wl_list_for_each(subsurface, &surface->current.subsurfaces_above, current.link) {
        struct wlr_surface *subsurface_surface = subsurface->surface;
        
        // 子表面有自己的位置偏移
        double sx = subsurface->current.x + base_x;
        double sy = subsurface->current.y + base_y;
        
        render_surface_and_subsurfaces(
            subsurface_surface, sx, sy, render_pass);
    }
}

void render_toplevel_with_tint(
    struct surface_toplevel* toplevel,
    double render_x, double render_y, // 接收已经转换好的输出坐标
    struct wlr_render_pass *render_pass 
) {
    struct wlr_surface* surface = toplevel->xdg_toplevel->base->surface;
    if (!surface) return;

    // 递归渲染所有表面
    render_surface_and_subsurfaces(surface, render_x, render_y, render_pass);
    

    float alpha = 0.0f;

    // 2. 创建一个预乘后的颜色数组
    float premultiplied_color[4] = {
        1.0f * alpha,  // R * A
        0.0f * alpha,  // G * A
        0.0f * alpha,  // B * A
        alpha          // A
    };

    struct wlr_box tint_box = {
        static_cast<int>(render_x),
        static_cast<int>(render_y),
        surface->current.width,
        surface->current.height
    };
    
    // 在所有窗口内容上方添加红色覆盖层
    struct wlr_render_rect_options tint_options = {
        tint_box,
        { premultiplied_color[0], premultiplied_color[1], premultiplied_color[2], premultiplied_color[3] },
        nullptr,
        WLR_RENDER_BLEND_MODE_PREMULTIPLIED
    };
    wlr_render_pass_add_rect(render_pass, &tint_options);
    
}

/*
void render_toplevel_with_tint(
    struct surface_toplevel* toplevel,
    struct wlr_output* output,
    struct wlr_render_pass *render_pass 
) {
    // 获取 surface 并做基本检查
    struct wlr_surface* surface = toplevel->xdg_toplevel->base->surface;
    if (!surface || !surface->current.width || !surface->current.height) {
        wlr_log(WLR_DEBUG, "跳过无效表面的渲染");
        return;
    }
    
    // 日志输出调试信息
    wlr_log(WLR_DEBUG, "尝试渲染窗口：宽=%d, 高=%d", 
            surface->current.width, surface->current.height);
    
    struct wlr_texture *texture = wlr_surface_get_texture(surface);
    if (!texture) {
        wlr_log(WLR_DEBUG, "无法获取窗口纹理");
        return;
    }
    
    // 获取窗口在屏幕上的绝对位置（确保这些函数返回正确的值）
    double x = get_scene_tree_node_x(toplevel) + 50;
    double y = get_scene_tree_node_y(toplevel) + 50;
    int width = surface->current.width;
    int height = surface->current.height;
    
    wlr_log(WLR_DEBUG, "窗口位置: x=%.1f, y=%.1f", x, y);
    
    // 先只渲染窗口纹理，不使用半透明覆盖层，确认基本渲染正常
    struct wlr_render_texture_options texture_options = {
        texture,
        { 0, 0, (double)width, (double)height },
        { static_cast<int>(x), static_cast<int>(y), width, height },
        nullptr,
        nullptr,
        WL_OUTPUT_TRANSFORM_NORMAL,
        WLR_SCALE_FILTER_BILINEAR,
        WLR_RENDER_BLEND_MODE_PREMULTIPLIED
    };
    
    wlr_render_pass_add_texture(render_pass, &texture_options);
    
    // 如果窗口正常渲染，再添加这部分
    /*
    // 添加半透明红色矩形作为覆盖层
    struct wlr_render_rect_options tint_options = {
        .box = { x, y, (double)width, (double)height },
        .color = { 1.0f, 0.0f, 0.0f, 0.3f },
        .blend_mode = WLR_RENDER_BLEND_MODE_PREMULTIPLIED
    };
    
    wlr_render_pass_add_rect(render_pass, &tint_options);
}
*/


/*

void render_toplevel_with_tint(
    struct surface_toplevel* toplevel,
    struct wlr_output* output,
    // render_pass 参数虽然没有被直接使用，但保留它可以让函数签名
    // 在上下文中保持清晰，表明它是一个渲染阶段的一部分。
    const struct wlr_render_pass *render_pass 
) {
    tiley::TileyServer& server = tiley::TileyServer::getInstance();
    struct wlr_surface* surface = toplevel->xdg_toplevel->base->surface;
    tiley::TileyShader& shader = server.tint_shader;

    // 按需初始化着色器 (懒加载)
    if (shader.program == 0) {
        if (!init_shader(shader, "src/shaders/tint.vert", "src/shaders/tint.frag")) {
            wlr_log(WLR_ERROR, "Failed to init shader on-demand!");
            return;
        }
    }

    struct wlr_texture *texture = wlr_surface_get_texture(surface);
    if (!texture) {
        return;
    }

    struct wlr_gles2_texture_attribs tex_attribs;
    if (!wlr_texture_is_gles2(texture)) {
        return;
    }
    wlr_gles2_texture_get_attribs(texture, &tex_attribs);

    // 1. 获取窗口的几何位置和尺寸
    struct wlr_box geometry = {
        (int)(get_scene_tree_node_x(toplevel)), 
        (int)(get_scene_tree_node_y(toplevel)),
        surface->current.width, 
        surface->current.height
    };

    // 2. 计算最终的模型-视图-投影 (MVP) 矩阵
    float output_projection[9];
    int output_width, output_height;
    wlr_output_effective_resolution(output, &output_width, &output_height);
    matrix_projection_(output_projection, output_width, output_height, output->transform);

    float final_matrix[9];
    wlr_matrix_project_box_(
        final_matrix,
        &geometry,
        surface->current.transform,
        output_projection
    );

    // 3. 准备顶点和纹理坐标
    // 顶点坐标定义了一个从 (0,0) 到 (1,1) 的单位矩形，
    // final_matrix 会负责将它缩放和平移到正确的位置。
    static const GLfloat verts[] = {
        0, 0, // 左上
        1, 0, // 右上
        0, 1, // 左下
        1, 1, // 右下
    };

    // **修正**: 使用标准的、不翻转的纹理坐标
    // (0,0) 是纹理的左上角
    static const GLfloat texcoords[] = {
        0, 0, // 左上
        1, 0, // 右上
        0, 1, // 左下
        1, 1, // 右下
    };

    // 4. 执行 OpenGL 渲染命令
    glUseProgram(shader.program);
    glUniformMatrix3fv(shader.proj, 1, GL_FALSE, final_matrix);
    glUniform1f(shader.alpha, 1.0f); // 假设您的着色器需要 alpha

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(tex_attribs.target, tex_attribs.tex);
    
    // 设置纹理过滤模式
    glTexParameteri(tex_attribs.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(tex_attribs.target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glUniform1i(shader.tex, 0);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, texcoords);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // 注意：我们使用 GL_TRIANGLE_STRIP，所以顶点顺序很重要
    // 左上 -> 右上 -> 左下 -> 右下
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // 5. 清理 OpenGL 状态
    glDisable(GL_BLEND);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glBindTexture(tex_attribs.target, 0);
    glUseProgram(0);
}

*/
