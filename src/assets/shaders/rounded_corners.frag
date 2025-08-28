#version 100
// Tiley compositor: rounded_corners.frag
// feature: rounded corner window

precision mediump float;
varying vec2 v_texcoord;

uniform sampler2D u_texture;      // Window content texture
uniform vec2 u_resolution;        // Window physical(scaled) size
uniform float u_radius;           // border radius(px)
uniform float u_border_width;     // border width(px)
uniform vec3 u_border_color;      // border color(RGB)

// SDF
float sdRoundedBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + r;
    return length(max(q, 0.0)) - r;
}

void main() {

    // Texture coordinates to local coordinates
    vec2 center_pos = v_texcoord * u_resolution - u_resolution / 2.0;

    // SDF: calculate distance to center of final rounded rect for every point
    float distance = sdRoundedBox(center_pos, u_resolution / 2.0 - u_radius, u_radius);

    // get raw color of points
    vec4 texColor = texture2D(u_texture, v_texcoord);

    // smoothed border
    float border_mix_factor = smoothstep(-u_border_width, 0.0, distance);

    // mix border and content color
    vec3 final_color = mix(texColor.rgb, u_border_color, border_mix_factor);

    // anti-alias
    float shape_alpha = 1.0 - smoothstep(0.0, 1.5, distance);

    gl_FragColor = vec4(final_color, texColor.a * shape_alpha);
}