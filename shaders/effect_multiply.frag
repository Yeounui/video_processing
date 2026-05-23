#version 330 core
in vec2 v_uv;
out vec4 frag;
uniform sampler2D u_source;
uniform float u_factor;
void main() {
    vec3 c = texture(u_source, v_uv).rgb;
    vec3 rgb = clamp(round(c * 255.0 * u_factor) / 255.0, 0.0, 1.0);
    frag = vec4(rgb, 1.0);
}