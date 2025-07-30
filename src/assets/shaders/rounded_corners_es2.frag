#version 100 
// rounded_corners.frag 用于渲染圆角窗口

precision mediump float;

varying vec2 v_texcoord;

uniform sampler2D u_texture;
uniform vec2 u_resolution;
uniform float u_radius;

// SDF: 符号距离场: 计算一个点到目标裁剪后的圆角矩形的距离, 用于判断是否在内部(保留像素)
// 转换成两个圆的半径比较问题。
// 返回值: <0 在内部, =0 在边界上, >0 在外部
float sdRoundedBox(vec2 p, vec2 b, float r){
    vec2 q = abs(p) - b + r;
    return length(max(q, 0.0)) - r;
}

void main() {
    
    vec4 texColor = texture2D(u_texture, v_texcoord);

    // 1. 计算当前像素相对于窗口中心的坐标
    vec2 center_pos = v_texcoord * u_resolution - u_resolution / 2.0;
    // 2. 计算是否在内部
    float distance = sdRoundedBox(center_pos, u_resolution / 2.0 - u_radius, u_radius);
    // 3. smoothstep: 创建一个 1.5 像素宽的平滑过渡带, 用来实现抗锯齿
    float alpha = 1.0 - smoothstep(0.0, 1.5, distance);
    
    // 4. 最终输出的颜色是原始颜色, 透明度乘以计算出的 alpha 值实现裁剪
    // 既保留窗口本身的透明度, 又叠加上了圆角裁剪
    gl_FragColor = vec4(texColor.rgb, texColor.a * alpha);
}
