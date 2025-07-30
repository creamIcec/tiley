#version 100 

precision mediump float;

attribute vec2 aPos;      // GLES 2.0 使用 'attribute' 接收顶点数据
attribute vec2 aTexCoord;

uniform mat4 u_transform;

varying vec2 v_texcoord; // GLES 2.0 使用 'varying' 向片段着色器传递数据

void main() {
    gl_Position = u_transform * vec4(aPos, 0.0, 1.0);
    v_texcoord = aTexCoord;
}