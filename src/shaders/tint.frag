#version 100

// 精度声明
precision mediump float;

// 从顶点着色器接收的纹理坐标
varying mediump vec2 v_texcoord;

// 这个是 wlroots 自动提供的 uniform 变量，代表窗口的纹理
uniform sampler2D tex;

// 这个是我们自己定义的 uniform 变量，用于控制 tint 的颜色和程度
uniform vec4 tint_color; // (R, G, B, A)，A 代表混合程度

void main() {
    // 1. 从窗口纹理中获取原始的像素颜色
    vec4 original_color = texture2D(tex, v_texcoord);

    // 2. 使用 alpha 混合公式进行颜色混合
    //    final_color = top_layer_color * top_layer_alpha + bottom_layer_color * (1.0 - top_layer_alpha)
    //    在这里，tint_color 是顶层，original_color 是底层
    float tint_alpha = tint_color.a;
    vec3 mixed_rgb = tint_color.rgb * tint_alpha + original_color.rgb * (1.0 - tint_alpha);

    // 3. 最终的 alpha 值保持窗口原始的 alpha 值，以支持窗口本身的透明度
    float final_alpha = original_color.a;

    // 4. 输出最终计算出的像素颜色
    gl_FragColor = vec4(mixed_rgb, final_alpha);
}
