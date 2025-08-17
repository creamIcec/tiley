#version 100
// Tiley compositor: rounded_corners.frag
// 功能: 为窗口渲染圆角,并附带可配置的边框。

// GLSL ES 1.0 (OpenGL ES 2.0) 需要为浮点数指定精度
precision mediump float;

// 输入
// 从顶点着色器传入的纹理坐标 (范围 0.0 到 1.0)
varying vec2 v_texcoord;

// 全局参数 (由 C++ 代码传入)
uniform sampler2D u_texture;      // 窗口内容的纹理
uniform vec2 u_resolution;        // 窗口的物理像素尺寸 (e.g., 1920x1080 * scale)
uniform float u_radius;           // 圆角的半径 (物理像素)
uniform float u_border_width;     // 边框的宽度 (物理像素)
uniform vec3 u_border_color;      // 边框的颜色 (RGB)

// 核心函数: 符号距离场 (SDF)
// 计算一个点 p 到一个尺寸为 b、圆角半径为 r 的矩形的有符号距离。
// 返回值: <0 在内部, =0 在边界上, >0 在外部。
float sdRoundedBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + r;
    return length(max(q, 0.0)) - r;
}

void main() {
    // 步骤 1: 计算当前像素到形状中心的距离
    // 将纹理坐标(0-1)转换到以窗口中心为原点(0,0)的坐标系
    vec2 center_pos = v_texcoord * u_resolution - u_resolution / 2.0;

    // 使用 SDF 函数计算该像素到圆角矩形边缘的精确距离
    float distance = sdRoundedBox(center_pos, u_resolution / 2.0 - u_radius, u_radius);

    // 步骤 2: 计算最终颜色(俄罗斯套娃逻辑)
    // 首先,获取窗口纹理在当前位置的原始颜色
    vec4 texColor = texture2D(u_texture, v_texcoord);

    // 使用 smoothstep 创建一个从“内部”到“边框”的平滑过渡
    // 当 distance 从 -u_border_width (边框内侧) 变为 0 (边框外侧) 时,
    // border_mix_factor 会平滑地从 0.0 变为 1.0。
    float border_mix_factor = smoothstep(-u_border_width, 0.0, distance);

    // mix(A, B, factor) 函数:
    // 当 factor=0 时, 结果是 A (窗口内容); 当 factor=1 时, 结果是 B (边框颜色)。
    // 这行代码优雅地在窗口内容和边框颜色之间进行了混合。
    vec3 final_color = mix(texColor.rgb, u_border_color, border_mix_factor);

    // 步骤 3: 计算最终的 Alpha 透明度 (用于抗锯齿和裁剪)
    // 使用 smoothstep 创建一个在形状最外边缘的、宽度为 1.5 像素的平滑过渡带。
    // 这能有效地实现抗锯齿,让边缘看起来更柔和。
    // 当 distance > 0 时,像素在形状外部,alpha 会平滑地变为 0。
    float shape_alpha = 1.0 - smoothstep(0.0, 1.5, distance);

    // 步骤 4: 输出最终的像素颜色
    // 使用我们混合好的 final_color 作为 RGB 值,
    // 并将它与窗口原始的 alpha 值和我们计算出的形状 alpha 值相乘,得到最终的透明度。
    gl_FragColor = vec4(final_color, texColor.a * shape_alpha);
}