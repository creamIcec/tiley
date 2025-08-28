#version 100 

precision mediump float;

attribute vec2 aPos;
attribute vec2 aTexCoord;

uniform mat4 u_transform;

varying vec2 v_texcoord;

void main() {
    gl_Position = u_transform * vec4(aPos, 0.0, 1.0);
    v_texcoord = aTexCoord;
}