#version 330 core

in  vec2 v_uv;
out vec4 frag;

uniform sampler2D u_source;
uniform vec2  u_contentXY;   /* px: top-left of image area in window */
uniform vec2  u_contentWH;   /* px: image area size in window */
uniform vec2  u_windowSize;  /* px */
uniform float u_scale;       /* min(windowW/srcW, windowH/srcH) */
uniform vec2  u_texelSize;   /* (1/srcW, 1/srcH) */

/* Sample source with clamp-to-edge (texture wrap already set on CPU side) */
vec3 src(vec2 uv) {
    return texture(u_source, clamp(uv, vec2(0.0), vec2(1.0))).rgb;
}

/* Rec.709 luminance — for Sobel edge classification */
float lum(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }

/* Sobel edge strength: E = min(1.0, sqrt(Gx^2 + Gy^2) / 4.0) */
float edgeStrength(vec2 uv) {
    vec2 d = u_texelSize;
    float l00 = lum(src(uv + vec2(-d.x, -d.y)));
    float l10 = lum(src(uv + vec2( 0.0, -d.y)));
    float l20 = lum(src(uv + vec2( d.x, -d.y)));
    float l01 = lum(src(uv + vec2(-d.x,  0.0)));
    float l21 = lum(src(uv + vec2( d.x,  0.0)));
    float l02 = lum(src(uv + vec2(-d.x,  d.y)));
    float l12 = lum(src(uv + vec2( 0.0,  d.y)));
    float l22 = lum(src(uv + vec2( d.x,  d.y)));
    /* Gx = [-1 0 1; -2 0 2; -1 0 1]  Gy = [-1 -2 -1; 0 0 0; 1 2 1] */
    float gx = -l00 + l20 - 2.0*l01 + 2.0*l21 - l02 + l22;
    float gy = -l00 - 2.0*l10 - l20 + l02 + 2.0*l12 + l22;
    return min(1.0, sqrt(gx*gx + gy*gy) / 4.0);
}

/* Bilinear — manual 4-tap (texture is GL_NEAREST) */
vec3 bilinear(vec2 uv) {
    vec2 px = uv / u_texelSize - 0.5;
    vec2 i  = floor(px);
    vec2 f  = px - i;
    vec2 t00 = (i + vec2(0.5, 0.5)) * u_texelSize;
    vec2 t10 = (i + vec2(1.5, 0.5)) * u_texelSize;
    vec2 t01 = (i + vec2(0.5, 1.5)) * u_texelSize;
    vec2 t11 = (i + vec2(1.5, 1.5)) * u_texelSize;
    return mix(mix(src(t00), src(t10), f.x),
               mix(src(t01), src(t11), f.x), f.y);
}

/* Bicubic Catmull-Rom (a = -0.5), 4x4 tap */
float cubic(float t) {
    float a  = -0.5;
    float at = abs(t);
    if (at < 1.0)
        return (a + 2.0)*at*at*at - (a + 3.0)*at*at + 1.0;
    if (at < 2.0)
        return a*at*at*at - 5.0*a*at*at + 8.0*a*at - 4.0*a;
    return 0.0;
}

vec3 bicubic(vec2 uv) {
    vec2 px = uv / u_texelSize - 0.5;
    vec2 i  = floor(px);
    vec2 f  = px - i;
    vec3 result = vec3(0.0);
    float wsum  = 0.0;
    for (int dy = -1; dy <= 2; dy++) {
        float wy = cubic(f.y - float(dy));
        for (int dx = -1; dx <= 2; dx++) {
            float wx = cubic(f.x - float(dx));
            float w  = wx * wy;
            vec2  s  = (i + vec2(float(dx), float(dy)) + 0.5) * u_texelSize;
            result  += w * src(s);
            wsum    += w;
        }
    }
    return (abs(wsum) > 1e-6) ? result / wsum : src(uv);
}

/* Lanczos sinc */
float sinc_norm(float x) {
    const float PI = 3.14159265358979;
    if (abs(x) < 1e-6) return 1.0;
    float px = PI * x;
    return sin(px) / px;
}

/* Lanczos-3: a=3, 6x6 sample window */
vec3 lanczos3(vec2 uv) {
    const float a = 3.0;
    vec2 px = uv / u_texelSize - 0.5;
    vec2 i  = floor(px);
    vec2 f  = px - i;
    vec3 result = vec3(0.0);
    float wsum  = 0.0;
    for (int dy = -2; dy <= 3; dy++) {
        float ty = f.y - float(dy);
        float wy = sinc_norm(ty) * sinc_norm(ty / a);
        for (int dx = -2; dx <= 3; dx++) {
            float tx = f.x - float(dx);
            float wx = sinc_norm(tx) * sinc_norm(tx / a);
            float w  = wx * wy;
            if (abs(w) < 1e-7) continue;
            vec2 s  = (i + vec2(float(dx), float(dy)) + 0.5) * u_texelSize;
            result += w * src(s);
            wsum   += w;
        }
    }
    return (abs(wsum) > 1e-6) ? result / wsum : src(uv);
}

/* Box filter: area average over the output-pixel footprint in source space. */
vec3 boxFilter(vec2 uv) {
    float footprint = 1.0 / max(u_scale, 1e-4);
    float half_fp = footprint * 0.5;
    vec2 center = uv / u_texelSize - vec2(0.5);
    vec2 boxMin = center - vec2(half_fp);
    vec2 boxMax = center + vec2(half_fp);
    int ix_lo = int(floor(boxMin.x + 0.5));
    int ix_hi = int(floor(boxMax.x + 0.5));
    int iy_lo = int(floor(boxMin.y + 0.5));
    int iy_hi = int(floor(boxMax.y + 0.5));
    vec3 result = vec3(0.0);
    float weightSum = 0.0;

    for (int iy = iy_lo; iy <= iy_hi; iy++) {
        float y0 = max(boxMin.y, float(iy) - 0.5);
        float y1 = min(boxMax.y, float(iy) + 0.5);
        float wy = max(0.0, y1 - y0);
        for (int ix = ix_lo; ix <= ix_hi; ix++) {
            float x0 = max(boxMin.x, float(ix) - 0.5);
            float x1 = min(boxMax.x, float(ix) + 0.5);
            float wx = max(0.0, x1 - x0);
            float weight = wx * wy;
            vec2 s = (vec2(float(ix), float(iy)) + 0.5) * u_texelSize;
            result += weight * src(s);
            weightSum += weight;
        }
    }
    return (weightSum > 0.0) ? result / weightSum : src(uv);
}

void main() {
    vec2 px    = v_uv * u_windowSize;
    vec2 local = px - u_contentXY;

    if (local.x < 0.0 || local.y < 0.0 ||
        local.x > u_contentWH.x || local.y > u_contentWH.y) {
        frag = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    /* Guard: if uniforms not yet initialized by C code, output black safely */
    if (u_texelSize.x < 1e-6 || u_texelSize.y < 1e-6) {
        frag = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec2 srcUV = local / u_contentWH;
    float E    = edgeStrength(srcUV);
    vec3  color;

    if (u_scale > 1.0) {
        color = (E >= 0.20) ? bicubic(srcUV) : bilinear(srcUV);
    } else if (u_scale < 1.0) {
        color = (E >= 0.20) ? lanczos3(srcUV) : boxFilter(srcUV);
    } else {
        color = src(srcUV);
    }

    frag = vec4(color, 1.0);
}
