#version 330 core
in vec2 v_uv;
out vec4 frag;
uniform sampler2D u_source;
uniform vec2 u_texSize;
void main() {
    vec3 sum = vec3(0.0);
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            sum += texture(u_source, v_uv + vec2(float(dx), float(dy)) * u_texSize).rgb;
        }
    }
    frag = vec4(clamp(round(sum / 9.0 * 255.0) / 255.0, 0.0, 1.0), 1.0);
}