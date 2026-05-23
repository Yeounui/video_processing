#version 330 core
in vec2 v_uv;
out vec4 frag;

uniform sampler2D u_source;
uniform int u_alg_id;
uniform vec2 u_texSize;
uniform float u_param0;
uniform float u_param1;
uniform float u_param2;

float clampByte(float v) {
    return clamp(round(v), 0.0, 255.0);
}

vec3 byteRgb(vec2 uv) {
    return round(texture(u_source, uv).rgb * 255.0);
}

vec3 sampleByte(vec2 base, int dx, int dy) {
    return byteRgb(base + vec2(float(dx), float(dy)) * u_texSize);
}

float lum(vec3 rgb) {
    return 0.299 * rgb.r + 0.587 * rgb.g + 0.114 * rgb.b;
}

int normOddDistance(float value) {
    int d = int(round(value));
    d = clamp(d, 3, 31);
    if ((d % 2) == 0) {
        d += (d < 31) ? 1 : -1;
    }
    return d;
}

int normKernel(float value) {
    int k = int(round(value));
    if (k <= 3) return 3;
    if (k >= 7) return 7;
    return 5;
}

float gaussianWeight(int k, float sigma) {
    float x = float(k);
    return exp(-(x * x) / (2.0 * sigma * sigma));
}

vec3 gaussianBlur(vec2 uv, int kernelSize, float sigma) {
    int halfK = kernelSize / 2;
    sigma = clamp(sigma, 0.1, 5.0);
    vec3 acc = vec3(0.0);
    float sum = 0.0;
    for (int y = -3; y <= 3; y++) {
        if (abs(y) > halfK) continue;
        float wy = gaussianWeight(y, sigma);
        for (int x = -3; x <= 3; x++) {
            if (abs(x) > halfK) continue;
            float w = wy * gaussianWeight(x, sigma);
            acc += w * sampleByte(uv, x, y);
            sum += w;
        }
    }
    return clamp(round(acc / max(sum, 1e-6)), vec3(0.0), vec3(255.0));
}

float medianChannel(inout float vals[9]) {
    for (int i = 1; i < 9; i++) {
        float key = vals[i];
        int j = i - 1;
        while (j >= 0 && vals[j] > key) {
            vals[j + 1] = vals[j];
            j--;
        }
        vals[j + 1] = key;
    }
    return vals[4];
}

vec3 median3x3(vec2 uv) {
    float r[9];
    float g[9];
    float b[9];
    int n = 0;
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            vec3 c = sampleByte(uv, x, y);
            r[n] = c.r;
            g[n] = c.g;
            b[n] = c.b;
            n++;
        }
    }
    return vec3(medianChannel(r), medianChannel(g), medianChannel(b));
}

void main() {
    vec3 c = byteRgb(v_uv);
    vec3 outRgb = c;

    if (u_alg_id == 1) {
        outRgb = clamp(round(c + u_param0), vec3(0.0), vec3(255.0));
    } else if (u_alg_id == 2) {
        outRgb = clamp(round(c * u_param0), vec3(0.0), vec3(255.0));
    } else if (u_alg_id == 3) {
        float gamma = clamp((u_param0 <= 0.0) ? 1.0 : u_param0, 0.1, 5.0);
        outRgb = clamp(round(255.0 * pow(c / 255.0, vec3(1.0 / gamma))), vec3(0.0), vec3(255.0));
    } else if (u_alg_id == 4) {
        float y = lum(c);
        float threshold = clamp(round(u_param0), 0.0, 255.0);
        float v = (y >= threshold) ? 255.0 : 0.0;
        outRgb = vec3(v);
    } else if (u_alg_id == 5) {
        float y = lum(c);
        float v = (y >= u_param0) ? 255.0 : 0.0;
        outRgb = vec3(v);
    } else if (u_alg_id == 6) {
        int mask = int(round(u_param0)) & 255;
        ivec3 bits = ivec3(c) & ivec3(mask);
        outRgb = vec3(bits);
    } else if (u_alg_id == 7) {
        int mode = clamp(int(round(u_param0)), 0, 2);
        vec2 uv = v_uv;
        if (mode == 0 || mode == 2) uv.x = 1.0 - uv.x;
        if (mode == 1 || mode == 2) uv.y = 1.0 - uv.y;
        outRgb = byteRgb(uv);
    } else if (u_alg_id == 8) {
        vec2 size = 1.0 / u_texSize;
        vec2 pos = v_uv * size - vec2(0.5);
        vec2 center = (size - vec2(1.0)) * 0.5;
        float theta = -u_param0 * 3.14159265358979323846 / 180.0;
        float ct = cos(theta);
        float st = sin(theta);
        vec2 d = pos - center;
        vec2 src = vec2(ct * d.x - st * d.y, st * d.x + ct * d.y) + center;
        vec2 roundedSrc = floor(src + vec2(0.5));
        if (roundedSrc.x < 0.0 || roundedSrc.x >= size.x ||
            roundedSrc.y < 0.0 || roundedSrc.y >= size.y) {
            outRgb = vec3(0.0);
        } else {
            outRgb = byteRgb((roundedSrc + vec2(0.5)) * u_texSize);
        }
    } else if (u_alg_id == 9) {
        outRgb = clamp(round(vec3(128.0) - sampleByte(v_uv, -1, -1) +
                             sampleByte(v_uv, 1, 1)), vec3(0.0), vec3(255.0));
    } else if (u_alg_id == 10) {
        float low = u_param0;
        float high = u_param1;
        if (high == low) {
            outRgb = c;
        } else {
            outRgb = clamp(round((c - vec3(low)) * (255.0 / (high - low))), vec3(0.0), vec3(255.0));
        }
    } else if (u_alg_id == 11) {
        vec3 sum = vec3(0.0);
        for (int y = -1; y <= 1; y++)
            for (int x = -1; x <= 1; x++)
                sum += sampleByte(v_uv, x, y);
        outRgb = clamp(round(sum / 9.0), vec3(0.0), vec3(255.0));
    } else if (u_alg_id == 12) {
        vec3 sum = vec3(0.0);
        for (int y = -2; y <= 2; y++)
            for (int x = -2; x <= 2; x++)
                sum += sampleByte(v_uv, x, y);
        outRgb = clamp(round(sum / 25.0), vec3(0.0), vec3(255.0));
    } else if (u_alg_id == 13) {
        outRgb = gaussianBlur(v_uv, normKernel(u_param0), u_param1);
    } else if (u_alg_id == 14 || u_alg_id == 16) {
        float strength = clamp(u_param0, 0.0, 3.0);
        vec3 blur = gaussianBlur(v_uv, 5, 1.0);
        outRgb = clamp(round(c + strength * (c - blur)), vec3(0.0), vec3(255.0));
    } else if (u_alg_id == 15) {
        float strength = clamp(u_param0, 0.0, 3.0);
        vec3 hval = 8.0 * c
                  - sampleByte(v_uv, -1, -1) - sampleByte(v_uv, 0, -1) - sampleByte(v_uv, 1, -1)
                  - sampleByte(v_uv, -1,  0)                         - sampleByte(v_uv, 1,  0)
                  - sampleByte(v_uv, -1,  1) - sampleByte(v_uv, 0,  1) - sampleByte(v_uv, 1,  1);
        outRgb = clamp(round(c + strength * hval), vec3(0.0), vec3(255.0));
    } else if (u_alg_id == 17 || u_alg_id == 18) {
        int d = normOddDistance(u_param0);
        int halfD = d / 2;
        vec3 sum = vec3(0.0);
        for (int k = -15; k <= 15; k++) {
            if (abs(k) > halfD) continue;
            sum += (u_alg_id == 17) ? sampleByte(v_uv, k, k) : sampleByte(v_uv, k, 0);
        }
        outRgb = clamp(round(sum / float(d)), vec3(0.0), vec3(255.0));
    } else if (u_alg_id == 19) {
        float gy = -lum(sampleByte(v_uv, -1, -1)) - 2.0 * lum(sampleByte(v_uv, 0, -1)) - lum(sampleByte(v_uv, 1, -1))
                   + lum(sampleByte(v_uv, -1, 1)) + 2.0 * lum(sampleByte(v_uv, 0, 1)) + lum(sampleByte(v_uv, 1, 1));
        outRgb = vec3(clampByte(abs(gy) * clamp(u_param0, 0.1, 4.0)));
    } else if (u_alg_id == 20) {
        float gx = -lum(sampleByte(v_uv, -1, -1)) + lum(sampleByte(v_uv, 1, -1))
                   - 2.0 * lum(sampleByte(v_uv, -1, 0)) + 2.0 * lum(sampleByte(v_uv, 1, 0))
                   - lum(sampleByte(v_uv, -1, 1)) + lum(sampleByte(v_uv, 1, 1));
        outRgb = vec3(clampByte(abs(gx) * clamp(u_param0, 0.1, 4.0)));
    } else if (u_alg_id == 21) {
        float l = 4.0 * lum(c)
                - lum(sampleByte(v_uv, 0, -1))
                - lum(sampleByte(v_uv, -1, 0))
                - lum(sampleByte(v_uv, 1, 0))
                - lum(sampleByte(v_uv, 0, 1));
        outRgb = vec3(clampByte(l + 128.0));
    } else if (u_alg_id == 22) {
        float sigmaSmall = clamp(u_param0, 0.1, 5.0);
        float sigmaLarge = clamp(u_param1, 0.1, 5.0);
        if (sigmaLarge <= sigmaSmall) sigmaLarge = clamp(sigmaSmall + 0.1, 0.1, 5.0);
        float gain = clamp(u_param2, 0.1, 4.0);
        vec3 small = gaussianBlur(v_uv, 5, sigmaSmall);
        vec3 large = gaussianBlur(v_uv, 5, sigmaLarge);
        outRgb = clamp(round(vec3(128.0) + gain * (small - large)), vec3(0.0), vec3(255.0));
    } else if (u_alg_id == 23) {
        float low = u_param0;
        float high = u_param1;
        if (high <= low) {
            outRgb = vec3(0.0);
        } else {
            float v = clampByte((lum(c) - low) * (255.0 / (high - low)));
            outRgb = vec3(v);
        }
    } else if (u_alg_id == 24) {
        float threshold = clamp(round(u_param0), 0.0, 255.0);
        bool fg = lum(c) >= threshold;
        int neighbors = 0;
        vec2 size = 1.0 / u_texSize;
        vec2 pos = v_uv * size - vec2(0.5);
        if (fg) {
            for (int y = -1; y <= 1; y++) {
                for (int x = -1; x <= 1; x++) {
                    if (x == 0 && y == 0) continue;
                    vec2 p = pos + vec2(float(x), float(y));
                    if (p.x < 0.0 || p.x >= size.x || p.y < 0.0 || p.y >= size.y) continue;
                    if (lum(sampleByte(v_uv, x, y)) >= threshold) neighbors++;
                }
            }
        }
        outRgb = (fg && neighbors == 1) ? vec3(255.0) : vec3(0.0);
    } else if (u_alg_id == 25) {
        outRgb = median3x3(v_uv);
    } else if (u_alg_id == 26) {
        outRgb = vec3(clampByte((c.r + c.g + c.b) / 3.0));
    } else if (u_alg_id == 27) {
        outRgb = vec3(clampByte(lum(c)));
    } else if (u_alg_id == 28) {
        outRgb = vec3(clampByte((min(min(c.r, c.g), c.b) + max(max(c.r, c.g), c.b)) * 0.5));
    }

    frag = vec4(outRgb / 255.0, 1.0);
}
