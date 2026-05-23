#version 330 core
/* Phase 2 skeleton — will be replaced by full hybrid Sobel/Bicubic/Lanczos shader in Phase 8. */
layout(location=0) in vec2 a_pos;   /* NDC -1..1 fullscreen quad */
layout(location=1) in vec2 a_uv;    /* 0..1 */
out vec2 v_uv;
void main() {
    v_uv = a_uv;
    gl_Position = vec4(a_pos, 0.0, 1.0);
}
