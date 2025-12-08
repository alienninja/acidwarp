#include "effects_rgb.h"
#include <math.h>
#include <stdint.h>
#include <SDL2/SDL.h>

// Helper: HSV to RGB (packed 0xFFRRGGBB)
static uint32_t hsv2rgb(float h, float s, float v) {
    float r, g, b;
    int i = (int)(h * 6.0f);
    float f = h * 6.0f - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - f * s);
    float t = v * (1.0f - (1.0f - f) * s);
    switch (i % 6) {
        case 0: r = v, g = t, b = p; break;
        case 1: r = q, g = v, b = p; break;
        case 2: r = p, g = v, b = t; break;
        case 3: r = p, g = q, b = v; break;
        case 4: r = t, g = p, b = v; break;
        case 5: r = v, g = p, b = q; break;
    }
    return (0xFF << 24) | ((int)(r * 255) << 16) | ((int)(g * 255) << 8) | (int)(b * 255);
}

// Plasma effect
void effect_plasma_rgb(uint32_t *buf, int w, int h, int time_ms) {
    float t = time_ms * 0.0003f;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float fx = (float)x / w, fy = (float)y / h;
            float v = sinf(fx * 10 + t) + sinf((fy * 10 + t) * 1.3f) + sinf((fx + fy + t) * 7.0f);
            float hue = 0.5f + 0.5f * sinf(v + t);
            buf[y * w + x] = hsv2rgb(hue, 1.0f, 1.0f);
        }
    }
}

// Swirl effect
void effect_swirl_rgb(uint32_t *buf, int w, int h, int time_ms) {
    float t = time_ms * 0.0007f;
    float cx = w/2.0f, cy = h/2.0f;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float dx = x - cx, dy = y - cy;
            float dist = sqrtf(dx*dx + dy*dy);
            float angle = atan2f(dy, dx) + t + sinf(dist*0.07f + t);
            float hue = 0.5f + 0.5f * sinf(angle + dist*0.04f);
            buf[y * w + x] = hsv2rgb(hue, 1.0f, 1.0f);
        }
    }
}

// Tunnel effect
void effect_tunnel_rgb(uint32_t *buf, int w, int h, int time_ms) {
    float t = time_ms * 0.0005f;
    float cx = w/2.0f, cy = h/2.0f;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float dx = x - cx, dy = y - cy;
            float dist = sqrtf(dx*dx + dy*dy);
            float angle = atan2f(dy, dx);
            float v = sinf(dist*0.04f - t*3 + angle*4.0f);
            float hue = 0.5f + 0.5f * sinf(v + t + dist*0.01f);
            buf[y * w + x] = hsv2rgb(hue, 1.0f, 1.0f);
        }
    }
}

// Rings effect
void effect_rings_rgb(uint32_t *buf, int w, int h, int time_ms) {
    float t = time_ms * 0.0004f;
    float cx = w/2.0f, cy = h/2.0f;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float dx = x - cx, dy = y - cy;
            float dist = sqrtf(dx*dx + dy*dy);
            float v = sinf(dist*0.07f + t*2);
            float hue = 0.5f + 0.5f * sinf(v + t + dist*0.02f);
            buf[y * w + x] = hsv2rgb(hue, 1.0f, 1.0f);
        }
    }
}

// Checkerboard effect
void effect_checker_rgb(uint32_t *buf, int w, int h, int time_ms) {
    float t = time_ms * 0.001f;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int v = ((int)(x/32 + t*4) ^ (int)(y/32 + t*4)) & 1;
            float hue = v ? 0.6f : 0.1f;
            buf[y * w + x] = hsv2rgb(hue, 0.8f, 1.0f);
        }
    }
}

// Rays plus 2D Waves (case 0)
void effect_rays_waves_rgb(uint32_t *buf, int w, int h, int time_ms) {
    float t = time_ms * 0.0004f;
    float cx = w / 2.0f, cy = h / 2.0f;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float dx = x - cx, dy = y - cy;
            float dist = sqrtf(dx*dx + dy*dy);
            float angle = atan2f(dy, dx);
            float v = angle + sinf(dist * 0.10f + t) + cosf(x * 2 * M_PI / w * 2) + cosf(y * 2 * M_PI / h * 2);
            float hue = 0.5f + 0.5f * sinf(v + t);
            buf[y * w + x] = hsv2rgb(hue, 1.0f, 1.0f);
        }
    }
}

// Rays plus 2D Waves (case 1)
void effect_rays_waves2_rgb(uint32_t *buf, int w, int h, int time_ms) {
    float t = time_ms * 0.0005f;
    float cx = w / 2.0f, cy = h / 2.0f;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float dx = x - cx, dy = y - cy;
            float dist = sqrtf(dx*dx + dy*dy);
            float angle = atan2f(dy, dx);
            float v = angle + sinf(dist * 0.10f + t) * 0.7f + cosf(x * 2 * M_PI / w * 2) * 0.5f + cosf(y * 2 * M_PI / h * 2) * 0.5f;
            float hue = 0.5f + 0.5f * sinf(v + t);
            buf[y * w + x] = hsv2rgb(hue, 1.0f, 1.0f);
        }
    }
}

// Multi-frequency radial waves (case 2)
void effect_multi_radial_rgb(uint32_t *buf, int w, int h, int time_ms) {
    float t = time_ms * 0.00035f;
    float cx = w / 2.0f, cy = h / 2.0f;
    float x1 = 20 * sinf(t), y1 = 20 * cosf(t);
    float x2 = 20 * cosf(t*1.1f), y2 = 20 * sinf(t*1.2f);
    float x3 = 20 * sinf(t*1.3f), y3 = 20 * cosf(t*1.4f);
    float x4 = 20 * cosf(t*1.5f), y4 = 20 * sinf(t*1.6f);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float dx = x - cx, dy = y - cy;
            float v = sinf(sqrtf((dx + x1)*(dx + x1) + (dy + y1)*(dy + y1)) * 0.04f)
                    + sinf(sqrtf((dx + x2)*(dx + x2) + (dy + y2)*(dy + y2)) * 0.08f)
                    + sinf(sqrtf((dx + x3)*(dx + x3) + (dy + y3)*(dy + y3)) * 0.16f)
                    + sinf(sqrtf((dx + x4)*(dx + x4) + (dy + y4)*(dy + y4)) * 0.32f);
            float hue = 0.5f + 0.5f * sinf(v + t);
            buf[y * w + x] = hsv2rgb(hue, 1.0f, 1.0f);
        }
    }
}

// Peacock (case 3)
void effect_peacock_rgb(uint32_t *buf, int w, int h, int time_ms) {
    float t = time_ms * 0.0004f;
    float cx = w / 2.0f, cy = h / 2.0f;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float dx = x - cx, dy = y - cy;
            float angle = atan2f(dy, dx);
            float v = angle + sinf(sqrtf((dx + 20)*(dx + 20) + dy*dy) * 0.10f)
                      + angle + sinf(sqrtf((dx - 20)*(dx - 20) + dy*dy) * 0.10f);
            float hue = 0.5f + 0.5f * sinf(v + t);
            buf[y * w + x] = hsv2rgb(hue, 1.0f, 1.0f);
        }
    }
}

// Simple concentric rings (case 4)
void effect_rings_simple_rgb(uint32_t *buf, int w, int h, int time_ms) {
    float t = time_ms * 0.0006f;
    float cx = w / 2.0f, cy = h / 2.0f;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float dx = x - cx, dy = y - cy;
            float dist = sqrtf(dx*dx + dy*dy);
            float hue = fmodf(dist * 0.04f + t, 1.0f);
            buf[y * w + x] = hsv2rgb(hue, 1.0f, 1.0f);
        }
    }
}

// 2D Wave + Spiral (case 5)
void effect_wave_spiral_rgb(uint32_t *buf, int w, int h, int time_ms) {
    float t = time_ms * 0.00045f;
    float cx = w / 2.0f, cy = h / 2.0f;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float dx = x - cx, dy = y - cy;
            float dist = sqrtf(dx*dx + dy*dy);
            float angle = atan2f(dy, dx);
            float v = cosf(x * M_PI / w) + cosf(y * M_PI / h) + angle + sinf(dist + t);
            float hue = 0.5f + 0.5f * sinf(v + t);
            buf[y * w + x] = hsv2rgb(hue, 1.0f, 1.0f);
        }
    }
}

// Peacock, three centers (case 6)
void effect_peacock3a_rgb(uint32_t *buf, int w, int h, int time_ms) {
    float t = time_ms * 0.0005f;
    float cx = w / 2.0f, cy = h / 2.0f;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float dx = x - cx, dy = y - cy;
            float v = sinf(sqrtf(dx*dx + (dy - 20)*(dy - 20)) * 0.04f)
                    + sinf(sqrtf((dx + 20)*(dx + 20) + (dy + 20)*(dy + 20)) * 0.04f)
                    + sinf(sqrtf((dx - 20)*(dx - 20) + (dy + 20)*(dy + 20)) * 0.04f);
            float hue = 0.5f + 0.5f * sinf(v + t);
            buf[y * w + x] = hsv2rgb(hue, 1.0f, 1.0f);
        }
    }
}

// Peacock, three centers with angle (case 7)
void effect_peacock3b_rgb(uint32_t *buf, int w, int h, int time_ms) {
    float t = time_ms * 0.00045f;
    float cx = w / 2.0f, cy = h / 2.0f;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float dx = x - cx, dy = y - cy;
            float angle = atan2f(dy, dx);
            float v = angle
                    + sinf(sqrtf(dx*dx + (dy - 20)*(dy - 20)) * 0.08f)
                    + sinf(sqrtf((dx + 20)*(dx + 20) + (dy + 20)*(dy + 20)) * 0.08f)
                    + sinf(sqrtf((dx - 20)*(dx - 20) + (dy + 20)*(dy + 20)) * 0.08f);
            float hue = 0.5f + 0.5f * sinf(v + t);
            buf[y * w + x] = hsv2rgb(hue, 1.0f, 1.0f);
        }
    }
}

// Peacock, three centers variant (case 8)
void effect_peacock3c_rgb(uint32_t *buf, int w, int h, int time_ms) {
    float t = time_ms * 0.0005f;
    float cx = w / 2.0f, cy = h / 2.0f;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float dx = x - cx, dy = y - cy;
            float v = sinf(sqrtf(dx*dx + (dy - 20)*(dy - 20)) * 0.12f)
                    + sinf(sqrtf((dx + 20)*(dx + 20) + (dy + 20)*(dy + 20)) * 0.12f)
                    + sinf(sqrtf((dx - 20)*(dx - 20) + (dy + 20)*(dy + 20)) * 0.12f);
            float hue = 0.5f + 0.5f * sinf(v + t);
            buf[y * w + x] = hsv2rgb(hue, 1.0f, 1.0f);
        }
    }
}

// Five Arm Star (case 9)
void effect_five_arm_star_rgb(uint32_t *buf, int w, int h, int time_ms) {
    float t = time_ms * 0.0004f;
    float cx = w / 2.0f, cy = h / 2.0f;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float dx = x - cx, dy = y - cy;
            float dist = sqrtf(dx*dx + dy*dy);
            float angle = atan2f(dy, dx);
            float v = dist + sinf(5 * angle + t);
            float hue = 0.5f + 0.5f * sinf(v * 0.15f + t);
            buf[y * w + x] = hsv2rgb(hue, 1.0f, 1.0f);
        }
    }
}

// 2D Wave (case 10)
void effect_2d_wave1_rgb(uint32_t *buf, int w, int h, int time_ms) {
    float t = time_ms * 0.0004f;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float v = cosf(x * 2 * M_PI / w * 2) * 0.25f + cosf(y * 2 * M_PI / h * 2) * 0.25f;
            float hue = 0.5f + 0.5f * sinf(v + t);
            buf[y * w + x] = hsv2rgb(hue, 1.0f, 1.0f);
        }
    }
}

// 2D Wave (case 11)
void effect_2d_wave2_rgb(uint32_t *buf, int w, int h, int time_ms) {
    float t = time_ms * 0.0005f;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float v = cosf(x * 2 * M_PI / w) * 0.125f + cosf(y * 2 * M_PI / h) * 0.125f;
            float hue = 0.5f + 0.5f * sinf(v + t);
            buf[y * w + x] = hsv2rgb(hue, 1.0f, 1.0f);
        }
    }
}

// Simple concentric rings (case 12)
void effect_rings_concentric_rgb(uint32_t *buf, int w, int h, int time_ms) {
    float t = time_ms * 0.0003f;
    float cx = w / 2.0f, cy = h / 2.0f;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float dx = x - cx, dy = y - cy;
            float dist = sqrtf(dx*dx + dy*dy);
            float hue = fmodf(dist * 0.04f + t, 1.0f);
            buf[y * w + x] = hsv2rgb(hue, 1.0f, 1.0f);
        }
    }
}

// Simple rays (case 13)
void effect_rays_simple_rgb(uint32_t *buf, int w, int h, int time_ms) {
    float t = time_ms * 0.0004f;
    float cx = w / 2.0f, cy = h / 2.0f;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float dx = x - cx, dy = y - cy;
            float angle = atan2f(dy, dx);
            float hue = fmodf(angle / (2 * M_PI) + t, 1.0f);
            buf[y * w + x] = hsv2rgb(hue, 1.0f, 1.0f);
        }
    }
}

// Improved Toothed Spiral Sharp (case 14, revised)
void effect_spiral_sharp_rgb(uint32_t *buf, int w, int h, int time_ms) {
    float t = time_ms * 0.0003f;
    float cx = w / 2.0f, cy = h / 2.0f;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float dx = x - cx, dy = y - cy;
            float angle = atan2f(dy, dx);
            float dist = sqrtf(dx*dx + dy*dy);
            // Sharper teeth by quantizing the sine
            float teeth = sinf(dist * 0.15f + t);
            teeth = (teeth > 0.0f) ? 1.0f : -1.0f;
            float hue = fmodf(angle / (2 * M_PI) + 0.5f * teeth + t, 1.0f);
            buf[y * w + x] = hsv2rgb(hue, 1.0f, 1.0f);
        }
    }
}

// Rings with sine (case 15)
void effect_rings_sine_rgb(uint32_t *buf, int w, int h, int time_ms) {
    float t = time_ms * 0.0005f;
    float cx = w / 2.0f, cy = h / 2.0f;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float dx = x - cx, dy = y - cy;
            float dist = sqrtf(dx*dx + dy*dy);
            float hue = 0.5f + 0.5f * sinf(dist * 0.16f + t);
            buf[y * w + x] = hsv2rgb(hue, 1.0f, 1.0f);
        }
    }
}

// Rings with sine, sliding inner rings (case 16)
void effect_rings_sine_slide_rgb(uint32_t *buf, int w, int h, int time_ms) {
    float t = time_ms * 0.0005f;
    float cx = w / 2.0f, cy = h / 2.0f;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float dx = x - cx, dy = y - cy;
            float dist = sqrtf(dx*dx + dy*dy);
            float hue = 0.5f + 0.5f * sinf(dist * 0.16f + t + dist * 0.04f);
            buf[y * w + x] = hsv2rgb(hue, 1.0f, 1.0f);
        }
    }
}

// Nested cos/sin (case 17)
void effect_nested_trig_rgb(uint32_t *buf, int w, int h, int time_ms) {
    float t = time_ms * 0.0004f;
    float cx = w / 2.0f, cy = h / 2.0f;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float dx = x - cx, dy = y - cy;
            float dist = sqrtf(dx*dx + dy*dy);
            float v = sinf(cosf(2 * x * M_PI / w)) / (20.0f + dist)
                    + sinf(cosf(2 * y * M_PI / h)) / (20.0f + dist);
            float hue = 0.5f + 0.5f * sinf(v * 2.0f + t);
            buf[y * w + x] = hsv2rgb(hue, 1.0f, 1.0f);
        }
    }
}

// 2D Wave (case 18)
void effect_2d_wave3_rgb(uint32_t *buf, int w, int h, int time_ms) {
    float t = time_ms * 0.00045f;
    float cx = w / 2.0f, cy = h / 2.0f;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float dx = x - cx, dy = y - cy;
            float dist = sqrtf(dx*dx + dy*dy);
            float v = cosf(7 * x * M_PI / w) / (20.0f + dist) + cosf(7 * y * M_PI / h) / (20.0f + dist);
            float hue = 0.5f + 0.5f * sinf(v * 2.0f + t);
            buf[y * w + x] = hsv2rgb(hue, 1.0f, 1.0f);
        }
    }
}

// 2D Wave (case 19)
void effect_2d_wave4_rgb(uint32_t *buf, int w, int h, int time_ms) {
    float t = time_ms * 0.00045f;
    float cx = w / 2.0f, cy = h / 2.0f;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float dx = x - cx, dy = y - cy;
            float dist = sqrtf(dx*dx + dy*dy);
            float v = cosf(17 * x * M_PI / w) / (20.0f + dist) + cosf(17 * y * M_PI / h) / (20.0f + dist);
            float hue = 0.5f + 0.5f * sinf(v * 2.0f + t);
            buf[y * w + x] = hsv2rgb(hue, 1.0f, 1.0f);
        }
    }
}

// Julia Set (animated c, swirl, zoom, dynamic palette)
void effect_julia_rgb(uint32_t *buf, int w, int h, int time_ms) {
    double t = time_ms * 0.00004;
    double zoom = pow(1.008, t * 60.0);
    double swirl = 0.10 * t;
    double c_re = -0.70176 + 0.25 * cos(t * 1.1);
    double c_im = -0.3842 + 0.25 * sin(t * 0.9);
    double scale = 1.5 / zoom;
    int max_iter = 350;
    float color_cycle = fmodf(time_ms * 0.00011f, 1.0f);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            double dx = (x - w/2.0) * scale / (w/2.0);
            double dy = (y - h/2.0) * scale / (h/2.0);
            double sx = cos(swirl) * dx - sin(swirl) * dy;
            double sy = sin(swirl) * dx + cos(swirl) * dy;
            double zx = sx;
            double zy = sy;
            int iter = 0;
            while (zx*zx + zy*zy < 4.0 && iter < max_iter) {
                double tmp = zx*zx - zy*zy + c_re;
                zy = 2.0 * zx * zy + c_im;
                zx = tmp;
                iter++;
            }
            float mu = (iter < max_iter) ? iter - log2(log2(zx*zx + zy*zy)) : iter;
            float norm = mu / max_iter;
            float hue = fmodf(0.4f + 0.5f * norm + color_cycle, 1.0f);
            float sat = 0.85f + 0.15f * cosf(time_ms * 0.0002f);
            float val = (iter < max_iter) ? 1.0f : 0.15f;
            buf[y * w + x] = hsv2rgb(hue, sat, val);
        }
    }
}

// Effect table and metadata
rgb_effect_fn rgb_effects[] = {
    effect_plasma_rgb,
    effect_swirl_rgb,
    effect_tunnel_rgb,
    effect_rings_rgb,
    effect_checker_rgb,
    effect_rays_waves_rgb,
    effect_rays_waves2_rgb,
    effect_multi_radial_rgb,
    effect_peacock_rgb,
    effect_rings_simple_rgb,
    effect_wave_spiral_rgb,
    effect_peacock3a_rgb,
    effect_peacock3b_rgb,
    effect_peacock3c_rgb,
    effect_five_arm_star_rgb,
    effect_2d_wave1_rgb,
    effect_2d_wave2_rgb,
    effect_rings_concentric_rgb,
    effect_rays_simple_rgb,
    effect_spiral_sharp_rgb,
    effect_rings_sine_rgb,
    effect_rings_sine_slide_rgb,
    effect_nested_trig_rgb,
    effect_2d_wave3_rgb,
    effect_2d_wave4_rgb,
    effect_julia_rgb
};

const char *rgb_effect_names[] = {
    "Plasma",
    "Swirl",
    "Tunnel",
    "Rings",
    "Checker",
    "Rays plus 2D Waves",
    "Rays plus 2D Waves 2",
    "Multi-frequency radial waves",
    "Peacock",
    "Simple concentric rings",
    "2D Wave + Spiral",
    "Peacock (three centers)",
    "Peacock (three centers, angle)",
    "Peacock (three centers, variant)",
    "Five Arm Star",
    "2D Wave",
    "2D Wave 2",
    "Concentric Rings",
    "Simple Rays",
    "Toothed Spiral Sharp",
    "Rings with Sine",
    "Rings with Sine (slide)",
    "Nested Trig",
    "2D Wave 3",
    "2D Wave 4",
    "Mandelbrot Fractal", // restore name for menu
    "Julia Set"
};

int rgb_effect_count = sizeof(rgb_effects)/sizeof(rgb_effects[0]);

void effect_2d_wave3_rgb(uint32_t *buf, int w, int h, int time_ms);
void effect_2d_wave4_rgb(uint32_t *buf, int w, int h, int time_ms);
