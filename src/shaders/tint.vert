#version 100

// 这个是 wlroots 自动提供的顶点属性
attribute mediump vec2 pos;      // 顶点位置 (例如：-1.0 到 1.0)
attribute mediump vec2 texcoord; // 纹理坐标 (例如：0.0 到 1.0)

// 这个是 wlroots 自动提供的 uniform 变量
uniform mediump mat3 proj; // 投影矩阵，用于坐标变换

// 这个是我们自己要传递给片段着色器的变量
varying mediump vec2 v_texcoord;

void main() {
    // 将顶点位置通过投影矩阵变换，得到最终在屏幕上的位置
    gl_Position = vec4(proj * vec3(pos, 1.0), 1.0);
    
    // 直接把纹理坐标传递给片段着色器
    v_texcoord = texcoord;
}
