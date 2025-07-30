#version 300 es
precision mediump float;

// 输入：来自 C++ 代码的顶点数据
layout (location = 0) in vec2 aPos;      // 顶点位置
layout (location = 1) in vec2 aTexCoord; // 纹理坐标

// 输入：来自 C++ 代码的全局参数
uniform mat4 u_transform; // 一个矩阵，包含了投影、位置、大小等所有变换

// 输出：传递给片段着色器
out vec2 v_texcoord;

void main() {
    // 将顶点位置应用变换，得到最终在屏幕上的位置
    gl_Position = u_transform * vec4(aPos, 0.0, 1.0);
    
    // 将纹理坐标直接传递过去
    v_texcoord = aTexCoord;
}