#version 300 es
// rounded_corners.frag 用于渲染圆角窗口
precision mediump float;

// 传入归一化纹理坐标
in vec2 v_textcoord;

uniform sampler2D u_texture;     // 窗口内容纹理
uniform vec2 u_resolution;       // 窗口buffer尺寸
uniform float u_radius;          // 圆角半径

// 传出某个像素点最终的颜色
out vec4 FragColor;

// SDF: 符号距离场: 计算一个点到目标裁剪后的圆角矩形的距离, 用于判断是否在内部(保留像素)
// 转换成两个圆的半径比较问题。
// 返回值: <0 在内部, =0 在边界上, >0 在外部
float sdRoundedBox(vec2 p, vec2 b, float r){
    vec2 q = abs(p) - b + r;
    return length(max(q, 0.0)) - r;
}

void main(){
    // 从原始窗口纹理中获取颜色
    vec4 texColor = texture(u_texture, v_textcoord);

    // 1. 计算当前像素相对于窗口中心的坐标
    // u_resolution / 2.0 : 圆角矩形的半长轴和半短轴
    vec2 center_pos = v_textcoord * u_resolution - u_resolution / 2.0;

    // 2. 计算是否在内部
    float distance = sdRoundedBox(center_pos, u_resolution / 2.0, u_radius);

    // 使用 smoothstep 创建一个 1.5 像素宽的平滑过渡带，实现抗锯齿
    float alpha = 1.0 - smoothstep(0.0, 1.5, distance);

    // 4. 最终输出的颜色是原始颜色，但透明度要乘以我们计算出的 alpha 值(实现裁剪)
    // 这样既保留了窗口本身的透明度，又叠加上了我们的圆角裁剪
    FragColor = vec4(texColor.rgb, texColor.a * alpha);

}