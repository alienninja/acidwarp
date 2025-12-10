/* ACID WARP (c)Copyright 1992, 1993 by Noah Spurrier
 * All Rights reserved. Private Proprietary Source Code by Noah Spurrier
 * Ported to Linux by Steven Wills
 * WebGL/Emscripten port
 */

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <SDL2/SDL.h>
#include <sys/time.h>
#include <math.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#include <GLES3/gl3.h>
#endif

#include "../acidwarp/warp_text.h"
#include "../acidwarp/handy.h"
#include "../acidwarp/acidwarp.h"
#include "../acidwarp/lut.h"
#include "../acidwarp/bit_map.h"
#include "../acidwarp/palinit.h"
#include "../acidwarp/rolnfade.h"

// Forward declarations
void restoreOldVideoMode(void);
void main_loop_iteration(void);

// Modern input state
int paused = 0;
int locked = 0;
int modern_mode = 0;  // 0 = classic (original patterns), 1 = modern (RGB effects)
int current_modern_effect = 0;
#define NUM_MODERN_EFFECTS 6
static Uint32 effect_time_base = 0;  // For animated effects

// Classic mode frame timing - original uses usleep(30000) = 30ms between frames
// At 60fps we get ~16.6ms per frame, so we skip frames to match original speed
static Uint32 last_classic_frame_time = 0;
#define CLASSIC_FRAME_DELAY_MS 30  // Match original 30ms delay

// WebGL shader state
#ifdef __EMSCRIPTEN__
static GLuint gl_vbo = 0;
static int gl_initialized = 0;

// Classic mode texture rendering
static GLuint classic_texture = 0;
static GLuint texture_program = 0;
static GLuint texture_vbo = 0;
static SDL_GLContext sdl_gl_ctx = NULL;

// Vertex shader (WebGL 1.0 / GLSL ES 1.0)
static const char *vertex_shader_v1 = 
    "attribute vec2 position;\n"
    "varying vec2 uv;\n"
    "void main() {\n"
    "    uv = position * 0.5 + 0.5;\n"
    "    gl_Position = vec4(position, 0.0, 1.0);\n"
    "}\n";

static const char *vertex_shader_src;

// Fragment shaders for each effect (WebGL 1.0 / GLSL ES 1.0)
static const char *frag_plasma_v1 = 
    "precision highp float;\n"
    "varying vec2 uv;\n"
    "uniform float time;\n"
    "uniform vec2 resolution;\n"
    "void main() {\n"
    "    vec2 p = uv * resolution / 100.0;\n"
    "    float t = time;\n"
    "    float v = sin(p.x + t) + sin(p.y + t * 0.7) + sin((p.x + p.y) * 0.5 + t * 0.5) + sin(length(p) * 0.8);\n"
    "    v = (v + 4.0) / 8.0;\n"
    "    vec3 col = vec3(sin(v * 6.28318), sin(v * 6.28318 + 2.094), sin(v * 6.28318 + 4.188)) * 0.5 + 0.5;\n"
    "    gl_FragColor = vec4(col, 1.0);\n"
    "}\n";

static const char *frag_tunnel_v1 = 
    "precision highp float;\n"
    "varying vec2 uv;\n"
    "uniform float time;\n"
    "uniform vec2 resolution;\n"
    "void main() {\n"
    "    vec2 p = (uv - 0.5) * 2.0;\n"
    "    p.x *= resolution.x / resolution.y;\n"
    "    float dist = length(p) + 0.001;\n"
    "    float angle = atan(p.y, p.x) + time * 0.5;\n"  /* Add rotation/twirl */
    "    float depth = 1.0 / dist;\n"
    "    float z = depth + time * 3.0;\n"
    "    float twist = angle + depth * 0.3;\n"  /* Spiral twist based on depth */
    "    float rings = sin(z * 2.0) * 0.5 + 0.5;\n"
    "    float checker = step(0.5, mod(floor(z) + floor(twist * 3.0 / 3.14159), 2.0));\n"
    "    float t = time * 0.2;\n"
    "    float r = sin(z * 0.3 + t + twist) * 0.5 + 0.5;\n"
    "    float g = sin(z * 0.3 + t + twist + 2.0) * 0.5 + 0.5;\n"
    "    float b = sin(z * 0.3 + t + twist + 4.0) * 0.5 + 0.5;\n"
    "    vec3 col = mix(vec3(r, g, b), vec3(b, r, g), checker);\n"
    "    col *= rings * 0.5 + 0.5;\n"
    "    col *= 1.0 - smoothstep(0.0, 0.1, dist) * 0.3;\n"
    "    gl_FragColor = vec4(col, 1.0);\n"
    "}\n";

static const char *frag_rings_v1 = 
    "precision highp float;\n"
    "varying vec2 uv;\n"
    "uniform float time;\n"
    "uniform vec2 resolution;\n"
    "void main() {\n"
    "    vec2 p = (uv - 0.5) * 2.0;\n"
    "    p.x *= resolution.x / resolution.y;\n"
    "    float dist = length(p);\n"
    "    float v = sin(dist * 10.0 - time * 3.0) * 0.5 + 0.5;\n"
    "    gl_FragColor = vec4(v, (1.0 - v) * 0.8, sin(v * 3.14159), 1.0);\n"
    "}\n";

static const char *frag_swirl_v1 = 
    "precision highp float;\n"
    "varying vec2 uv;\n"
    "uniform float time;\n"
    "uniform vec2 resolution;\n"
    "void main() {\n"
    "    vec2 p = (uv - 0.5) * 2.0;\n"
    "    p.x *= resolution.x / resolution.y;\n"
    "    float zoom = exp(mod(time * 0.5, 4.0));\n"  /* Endless zoom */
    "    p *= zoom;\n"
    "    float dist = length(p);\n"
    "    float angle = atan(p.y, p.x);\n"
    "    float spiral = angle + dist * 0.8 - time * 2.0;\n"
    "    float arms = sin(spiral * 5.0) * 0.5 + 0.5;\n"
    "    float t = time * 0.3;\n"
    "    float r = sin(arms * 6.28 + t * 2.3 + dist * 0.5) * 0.5 + 0.5;\n"
    "    float g = sin(arms * 6.28 + t * 3.1 + dist * 0.7 + 2.0) * 0.5 + 0.5;\n"
    "    float b = sin(arms * 6.28 + t * 1.7 + dist * 0.3 + 4.0) * 0.5 + 0.5;\n"
    "    float glow = smoothstep(0.4, 0.6, arms);\n"
    "    gl_FragColor = vec4(r * glow + 0.1, g * glow + 0.05, b * glow + 0.15, 1.0);\n"
    "}\n";

static const char *frag_mandelbrot_v1 = 
    "precision highp float;\n"
    "varying vec2 uv;\n"
    "uniform float time;\n"
    "uniform vec2 resolution;\n"
    "void main() {\n"
    "    float t = time * 0.1;\n"
    "    float zoom = 1.5 + sin(t * 0.5) * 1.4;\n"
    "    vec2 center = vec2(-0.745 + sin(t * 0.3) * 0.1, 0.186 + cos(t * 0.3) * 0.1);\n"
    "    vec2 c = (uv - 0.5) * 2.0 / zoom + center;\n"
    "    c.x *= resolution.x / resolution.y;\n"
    "    vec2 z = vec2(0.0);\n"
    "    float iter = 0.0;\n"
    "    for (int i = 0; i < 200; i++) {\n"
    "        if (dot(z, z) > 4.0) break;\n"
    "        z = vec2(z.x * z.x - z.y * z.y, 2.0 * z.x * z.y) + c;\n"
    "        iter += 1.0;\n"
    "    }\n"
    "    if (iter >= 199.0) { gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0); return; }\n"
    "    float smooth_iter = iter + 1.0 - log(log(length(z))) / log(2.0);\n"
    "    float hue = mod(smooth_iter * 0.1 + t, 1.0);\n"
    "    float h6 = hue * 6.0;\n"
    "    float f = fract(h6);\n"
    "    float hi = floor(mod(h6, 6.0));\n"
    "    vec3 col;\n"
    "    if (hi < 1.0) col = vec3(1.0, f, 0.0);\n"
    "    else if (hi < 2.0) col = vec3(1.0 - f, 1.0, 0.0);\n"
    "    else if (hi < 3.0) col = vec3(0.0, 1.0, f);\n"
    "    else if (hi < 4.0) col = vec3(0.0, 1.0 - f, 1.0);\n"
    "    else if (hi < 5.0) col = vec3(f, 0.0, 1.0);\n"
    "    else col = vec3(1.0, 0.0, 1.0 - f);\n"
    "    gl_FragColor = vec4(col, 1.0);\n"
    "}\n";

static const char *frag_burning_ship_v1 = 
    "precision highp float;\n"
    "varying vec2 uv;\n"
    "uniform float time;\n"
    "uniform vec2 resolution;\n"
    "void main() {\n"
    "    float t = time * 0.05;\n"
    "    float zoom = 2.5 + sin(t * 0.3) * 1.5;\n"
    "    vec2 center = vec2(-1.76, -0.028);\n"  /* Centered on the main ship */
    "    vec2 p = (uv - 0.5) * 2.0;\n"
    "    p.x *= resolution.x / resolution.y;\n"
    "    vec2 c = p / zoom + center;\n"
    "    vec2 z = vec2(0.0);\n"
    "    float iter = 0.0;\n"
    "    for (int i = 0; i < 200; i++) {\n"
    "        if (dot(z, z) > 4.0) break;\n"
    "        z = vec2(z.x * z.x - z.y * z.y, abs(2.0 * z.x * z.y)) + c;\n"
    "        iter += 1.0;\n"
    "    }\n"
    "    if (iter >= 199.0) { gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0); return; }\n"
    "    float smooth_iter = iter + 1.0 - log(log(length(z))) / log(2.0);\n"
    "    float hue = mod(smooth_iter * 0.05 + t * 0.3, 1.0);\n"
    "    vec3 col;\n"
    "    if (hue < 0.2) col = vec3(hue * 5.0, 0.0, 0.0);\n"
    "    else if (hue < 0.4) col = vec3(1.0, (hue - 0.2) * 5.0, 0.0);\n"
    "    else if (hue < 0.6) col = vec3(1.0, 1.0, (hue - 0.4) * 5.0);\n"
    "    else if (hue < 0.8) col = vec3(1.0 - (hue - 0.6) * 5.0, 1.0, 1.0);\n"
    "    else col = vec3(0.0, 1.0 - (hue - 0.8) * 5.0, 1.0 - (hue - 0.8) * 2.5);\n"
    "    gl_FragColor = vec4(col, 1.0);\n"
    "}\n";

static const char *fragment_shaders[NUM_MODERN_EFFECTS];

static GLuint compile_shader(GLenum type, const char *src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        char log[512];
        glGetShaderInfoLog(shader, 512, NULL, log);
        printf("Shader compile error: %s\n", log);
    }
    return shader;
}

static GLuint create_program(const char *frag_src) {
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_shader_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, frag_src);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

static GLuint shader_programs[NUM_MODERN_EFFECTS];

/* Initialize GL shaders - called after SDL creates the GL context */
static void init_gl_shaders(void) {
    if (gl_initialized) return;
    
    /* Use WebGL 1.0 / GLSL ES 1.0 shaders for maximum compatibility */
    vertex_shader_src = vertex_shader_v1;
    fragment_shaders[0] = frag_plasma_v1;
    fragment_shaders[1] = frag_tunnel_v1;
    fragment_shaders[2] = frag_rings_v1;
    fragment_shaders[3] = frag_swirl_v1;
    fragment_shaders[4] = frag_mandelbrot_v1;
    fragment_shaders[5] = frag_burning_ship_v1;
    
    for (int i = 0; i < NUM_MODERN_EFFECTS; i++) {
        shader_programs[i] = create_program(fragment_shaders[i]);
    }
    
    /* Create fullscreen quad VBO */
    float vertices[] = { -1, -1, 1, -1, -1, 1, 1, 1 };
    glGenBuffers(1, &gl_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, gl_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    gl_initialized = 1;
}

#endif

/* Modern RGB effects - render directly to 32-bit buffer */
void effect_plasma(Uint32 *buf, int w, int h, int time_ms) {
    float t = time_ms * 0.001f;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float v = sinf(x * 0.05f + t) + sinf(y * 0.05f + t * 0.7f) +
                      sinf((x + y) * 0.03f + t * 0.5f) + sinf(sqrtf(x*x + y*y) * 0.04f);
            v = (v + 4.0f) / 8.0f;  // Normalize to 0-1
            Uint8 r = (Uint8)(sinf(v * 3.14159f * 2.0f) * 127 + 128);
            Uint8 g = (Uint8)(sinf(v * 3.14159f * 2.0f + 2.094f) * 127 + 128);
            Uint8 b = (Uint8)(sinf(v * 3.14159f * 2.0f + 4.188f) * 127 + 128);
            buf[y * w + x] = (0xFFU << 24) | (r << 16) | (g << 8) | b;
        }
    }
}

void effect_tunnel(Uint32 *buf, int w, int h, int time_ms) {
    float t = time_ms * 0.002f;
    float cx = w / 2.0f, cy = h / 2.0f;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float dx = x - cx, dy = y - cy;
            float dist = sqrtf(dx*dx + dy*dy) + 0.1f;
            float angle = atan2f(dy, dx);
            /* Use sin/cos of angle directly to avoid discontinuity */
            float depth = 1.0f / dist * 50.0f + t;
            /* Create tunnel texture using angle in a continuous way */
            float u1 = sinf(angle * 8.0f + depth);
            float u2 = cosf(angle * 8.0f + depth);
            float v = (u1 + u2) * 0.25f + 0.5f;
            Uint8 c = (Uint8)(v * 255);
            Uint8 r = c, g = (Uint8)(c * 0.7f), b = (Uint8)(c * 0.5f);
            buf[y * w + x] = (0xFFU << 24) | (r << 16) | (g << 8) | b;
        }
    }
}

void effect_rings(Uint32 *buf, int w, int h, int time_ms) {
    float t = time_ms * 0.003f;
    int cx = w / 2, cy = h / 2;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float dx = x - cx, dy = y - cy;
            float dist = sqrtf(dx*dx + dy*dy);
            float v = sinf(dist * 0.1f - t) * 0.5f + 0.5f;
            Uint8 r = (Uint8)(v * 255);
            Uint8 g = (Uint8)((1.0f - v) * 200);
            Uint8 b = (Uint8)(sinf(v * 3.14159f) * 255);
            buf[y * w + x] = (0xFFU << 24) | (r << 16) | (g << 8) | b;
        }
    }
}

void effect_swirl(Uint32 *buf, int w, int h, int time_ms) {
    float t = time_ms * 0.001f;
    float cx = w / 2.0f, cy = h / 2.0f;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float dx = x - cx, dy = y - cy;
            float dist = sqrtf(dx*dx + dy*dy);
            /* Use continuous angle with swirl distortion */
            float angle = atan2f(dy, dx) + dist * 0.02f + t;
            /* Use sin of angle*2 to avoid discontinuity at PI boundary */
            float v1 = sinf(angle * 4.0f) * 0.5f + 0.5f;
            float v2 = cosf(angle * 4.0f) * 0.5f + 0.5f;
            float v3 = sinf(dist * 0.05f - t * 2.0f) * 0.5f + 0.5f;
            Uint8 r = (Uint8)(v1 * 255);
            Uint8 g = (Uint8)(v2 * 255);
            Uint8 b = (Uint8)(v3 * 255);
            buf[y * w + x] = (0xFFU << 24) | (r << 16) | (g << 8) | b;
        }
    }
}

void effect_burning_ship(Uint32 *buf, int w, int h, int time_ms) {
    /* Animated zoom into the Burning Ship fractal */
    float t = time_ms * 0.00008f;
    float zoom = 0.8f + sinf(t * 0.4f) * 0.6f;  /* Zoom oscillates */
    /* Center on the "ship" area */
    float cx = -1.755f + sinf(t * 0.2f) * 0.05f;
    float cy = -0.04f + cosf(t * 0.2f) * 0.02f;
    
    int max_iter = 80 + (int)(sinf(t) * 40 + 40);
    
    for (int py = 0; py < h; py++) {
        for (int px = 0; px < w; px++) {
            /* Map pixel to complex plane (flipped Y for proper orientation) */
            float x0 = (px - w/2.0f) / (w/3.0f) / zoom + cx;
            float y0 = (py - h/2.0f) / (h/3.0f) / zoom + cy;
            
            float x = 0, y = 0;
            int iter = 0;
            
            /* Burning Ship: use absolute values */
            while (x*x + y*y <= 4.0f && iter < max_iter) {
                float xtemp = x*x - y*y + x0;
                y = fabsf(2*x*y) + y0;  /* Key difference: absolute value */
                x = xtemp;
                iter++;
            }
            
            if (iter == max_iter) {
                buf[py * w + px] = 0xFF000000;  /* Black for inside */
            } else {
                /* Smooth coloring */
                float smooth = iter + 1 - logf(logf(sqrtf(x*x + y*y))) / logf(2.0f);
                float hue = fmodf(smooth * 0.08f + t * 0.5f, 1.0f);
                
                /* Fire-like palette: black -> red -> orange -> yellow -> white */
                float r, g, b;
                if (hue < 0.25f) {
                    r = hue * 4.0f; g = 0; b = 0;
                } else if (hue < 0.5f) {
                    r = 1.0f; g = (hue - 0.25f) * 2.0f; b = 0;
                } else if (hue < 0.75f) {
                    r = 1.0f; g = 0.5f + (hue - 0.5f) * 2.0f; b = (hue - 0.5f) * 2.0f;
                } else {
                    r = 1.0f; g = 1.0f; b = 0.5f + (hue - 0.75f) * 2.0f;
                }
                buf[py * w + px] = (0xFFU << 24) | ((Uint8)(r*255) << 16) | ((Uint8)(g*255) << 8) | (Uint8)(b*255);
            }
        }
    }
}

void effect_mandelbrot(Uint32 *buf, int w, int h, int time_ms) {
    /* Animated zoom into the Mandelbrot set */
    float t = time_ms * 0.0001f;
    float zoom = 1.5f + sinf(t * 0.5f) * 1.4f;  /* Zoom oscillates */
    float cx = -0.745f + sinf(t * 0.3f) * 0.1f; /* Pan around interesting area */
    float cy = 0.186f + cosf(t * 0.3f) * 0.1f;
    
    int max_iter = 200 + (int)(sinf(t) * 50 + 50);  /* Higher iterations for detail */
    
    for (int py = 0; py < h; py++) {
        for (int px = 0; px < w; px++) {
            /* Map pixel to complex plane */
            float x0 = (px - w/2.0f) / (w/4.0f) / zoom + cx;
            float y0 = (py - h/2.0f) / (h/4.0f) / zoom + cy;
            
            float x = 0, y = 0;
            int iter = 0;
            
            while (x*x + y*y <= 4.0f && iter < max_iter) {
                float xtemp = x*x - y*y + x0;
                y = 2*x*y + y0;
                x = xtemp;
                iter++;
            }
            
            /* Color based on iteration count with smooth coloring */
            if (iter == max_iter) {
                buf[py * w + px] = 0xFF000000;  /* Black for inside */
            } else {
                /* Smooth coloring using escape time */
                float smooth = iter + 1 - logf(logf(sqrtf(x*x + y*y))) / logf(2.0f);
                float hue = fmodf(smooth * 0.1f + t, 1.0f);
                
                /* HSV to RGB (simplified) */
                float r, g, b;
                float h6 = hue * 6.0f;
                float f = h6 - floorf(h6);
                int hi = (int)h6 % 6;
                switch (hi) {
                    case 0: r = 1; g = f; b = 0; break;
                    case 1: r = 1-f; g = 1; b = 0; break;
                    case 2: r = 0; g = 1; b = f; break;
                    case 3: r = 0; g = 1-f; b = 1; break;
                    case 4: r = f; g = 0; b = 1; break;
                    default: r = 1; g = 0; b = 1-f; break;
                }
                buf[py * w + px] = (0xFFU << 24) | ((Uint8)(r*255) << 16) | ((Uint8)(g*255) << 8) | (Uint8)(b*255);
            }
        }
    }
}

/* Render modern effect directly to pixel buffer */
void render_modern_effect(Uint32 *buf, int w, int h) {
    Uint32 time_ms = SDL_GetTicks() - effect_time_base;
    switch (current_modern_effect) {
        case 0: effect_plasma(buf, w, h, time_ms); break;
        case 1: effect_tunnel(buf, w, h, time_ms); break;
        case 2: effect_rings(buf, w, h, time_ms); break;
        case 3: effect_swirl(buf, w, h, time_ms); break;
        case 4: effect_mandelbrot(buf, w, h, time_ms); break;
        case 5: effect_burning_ship(buf, w, h, time_ms); break;
    }
}

#define NUM_IMAGE_FUNCTIONS 40
#define NOAHS_FACE   0

/* Global state */
int ROTATION_DELAY = 30000;
int logo_time = 4, image_time = 20;
int XMax = 0, YMax = 0;
UCHAR *buf_graf;
int GO = TRUE;
int SKIP = FALSE;
int NP = FALSE;
int LOCK = FALSE;
UCHAR MainPalArray [256 * 3];
UCHAR TargetPalArray [256 * 3];
int FadeCompleteFlag = 0;

// SDL2 globals
SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *texture = NULL;

// Main loop state machine
typedef enum {
    STATE_LOGO_DISPLAY,
    STATE_LOGO_FADE,
    STATE_NEW_IMAGE,
    STATE_FADE_IN,
    STATE_ROTATE,
    STATE_FADE_OUT
} AppState;

AppState current_state = STATE_LOGO_DISPLAY;
int imageFuncList[NUM_IMAGE_FUNCTIONS];
int imageFuncListIndex = 0;
int paletteTypeNum = 0;
time_t ltime, mtime;
Uint32 *pixel_buffer = NULL;
int fade_dir = TRUE;
int logo_initialized = 0;

// Window defaults
int window_width = 640;
int window_height = 480;
int fullscreen = 0;
int userOptionImageFuncNum = -1;

#ifdef __EMSCRIPTEN__
EM_JS(int, is_mobile_device, (), {
    return (('ontouchstart' in window) || (navigator.maxTouchPoints > 0)) && (window.innerWidth <= 768);
});
EM_JS(int, get_canvas_width, (), {
    var isMobile = (('ontouchstart' in window) || (navigator.maxTouchPoints > 0)) && (window.innerWidth <= 768);
    return isMobile ? Math.min(window.innerWidth, screen.width) : 640;
});
EM_JS(int, get_canvas_height, (), {
    var isMobile = (('ontouchstart' in window) || (navigator.maxTouchPoints > 0)) && (window.innerWidth <= 768);
    if (isMobile) {
        // Leave room for touch controls on mobile (70px)
        return Math.min(window.innerHeight, screen.height) - 70;
    }
    return 480;
});
#endif

// Helper to convert 8-bit framebuffer to 32-bit RGBA
void convert_8bit_to_32bit(const UCHAR *src, Uint32 *dst, int width, int height, const UCHAR *palette) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = src[y * width + x];
            /* Scale 6-bit VGA palette (0-63) to 8-bit (0-255) */
            Uint8 r = (palette[idx * 3 + 0] << 2) | (palette[idx * 3 + 0] >> 4);
            Uint8 g = (palette[idx * 3 + 1] << 2) | (palette[idx * 3 + 1] >> 4);
            Uint8 b = (palette[idx * 3 + 2] << 2) | (palette[idx * 3 + 2] >> 4);
            dst[y * width + x] = (0xFFU << 24) | (r << 16) | (g << 8) | b;
        }
    }
}

// Render current frame to screen
void render_frame(void) {
#ifdef __EMSCRIPTEN__
    /* Check if we have GL context or fell back to SDL renderer */
    if (sdl_gl_ctx && gl_initialized) {
        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        if (modern_mode) {
            /* Modern mode: GPU-rendered effects via shaders */
            float time_sec = (SDL_GetTicks() - effect_time_base) / 1000.0f;
            
            GLuint prog = shader_programs[current_modern_effect];
            glUseProgram(prog);
            
            glUniform1f(glGetUniformLocation(prog, "time"), time_sec);
            glUniform2f(glGetUniformLocation(prog, "resolution"), (float)w, (float)h);
            
            glBindBuffer(GL_ARRAY_BUFFER, gl_vbo);
            GLint posLoc = glGetAttribLocation(prog, "position");
            glEnableVertexAttribArray(posLoc);
            glVertexAttribPointer(posLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glDisableVertexAttribArray(posLoc);
        } else {
            /* Classic mode: render pixel buffer as texture */
            convert_8bit_to_32bit(buf_graf, pixel_buffer, XMax, YMax, MainPalArray);
            
            glBindTexture(GL_TEXTURE_2D, classic_texture);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, XMax, YMax, GL_RGBA, GL_UNSIGNED_BYTE, pixel_buffer);
            
            glUseProgram(texture_program);
            glBindBuffer(GL_ARRAY_BUFFER, texture_vbo);
            GLint posLoc = glGetAttribLocation(texture_program, "position");
            glEnableVertexAttribArray(posLoc);
            glVertexAttribPointer(posLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);
            
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, classic_texture);
            glUniform1i(glGetUniformLocation(texture_program, "tex"), 0);
            
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glDisableVertexAttribArray(posLoc);
        }
        
        SDL_GL_SwapWindow(window);
        return;
    }
    
    /* Fallback: SDL renderer path */
    if (renderer && texture) {
        if (modern_mode) {
            render_modern_effect(pixel_buffer, XMax, YMax);
        } else {
            convert_8bit_to_32bit(buf_graf, pixel_buffer, XMax, YMax, MainPalArray);
        }
        SDL_UpdateTexture(texture, NULL, pixel_buffer, XMax * sizeof(Uint32));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }
#else
    if (modern_mode) {
        render_modern_effect(pixel_buffer, XMax, YMax);
    } else {
        convert_8bit_to_32bit(buf_graf, pixel_buffer, XMax, YMax, MainPalArray);
    }
    SDL_UpdateTexture(texture, NULL, pixel_buffer, XMax * sizeof(Uint32));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
#endif
}

// Handle SDL events
void handle_sdl_events(void) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
#ifdef __EMSCRIPTEN__
            emscripten_cancel_main_loop();
#endif
            restoreOldVideoMode();
            exit(0);
        } else if (event.type == SDL_KEYDOWN) {
            SDL_Keycode key = event.key.keysym.sym;
            if (key == SDLK_q || key == SDLK_ESCAPE) {
#ifdef __EMSCRIPTEN__
                emscripten_cancel_main_loop();
#endif
                restoreOldVideoMode();
                exit(0);
            } else if (key == SDLK_SPACE || key == SDLK_p) {
                paused = !paused;
            } else if (key == SDLK_l) {
                locked = !locked;
                LOCK = locked;
            } else if (key == SDLK_f) {
                /* Toggle fullscreen */
                Uint32 flags = SDL_GetWindowFlags(window);
                if (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) {
                    SDL_SetWindowFullscreen(window, 0);
                } else {
                    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                }
            } else if (key == SDLK_m) {
                /* Toggle modern/classic mode */
                modern_mode = !modern_mode;
                SKIP = 1; /* Force new image */
            } else if (key == SDLK_LEFT) {
                if (modern_mode) {
                    current_modern_effect = (current_modern_effect - 1 + NUM_MODERN_EFFECTS) % NUM_MODERN_EFFECTS;
                } else {
                    /* Classic mode: go to previous pattern */
                    if (--imageFuncListIndex < 0) imageFuncListIndex = NUM_IMAGE_FUNCTIONS - 1;
                    generate_image(
                        (userOptionImageFuncNum < 0) ? imageFuncList[imageFuncListIndex] : userOptionImageFuncNum,
                        buf_graf, XMax/2, YMax/2, XMax, YMax, 255);
                }
                SKIP = 0; /* Don't skip, we already changed */
            } else if (key == SDLK_RIGHT) {
                if (modern_mode) {
                    current_modern_effect = (current_modern_effect + 1) % NUM_MODERN_EFFECTS;
                } else {
                    /* Classic mode: go to next pattern */
                    if (++imageFuncListIndex >= NUM_IMAGE_FUNCTIONS) imageFuncListIndex = 0;
                    generate_image(
                        (userOptionImageFuncNum < 0) ? imageFuncList[imageFuncListIndex] : userOptionImageFuncNum,
                        buf_graf, XMax/2, YMax/2, XMax, YMax, 255);
                }
                SKIP = 0; /* Don't skip, we already changed */
            }
        }
    }
}

void writePixel(int x, int y, int color) {
    if (x >= 0 && x <= XMax && y >= 0 && y <= YMax) {
        buf_graf[y * (XMax+1) + x] = (UCHAR)color;
    }
}

void printStrArray(char *strArray[]) {
    char **strPtr;
    for (strPtr = strArray; **strPtr; ++strPtr)
        printf("%s", *strPtr);
}

void setNewVideoMode(void) {}

void makeShuffledList(int *list, int listSize) {
    int entryNum, r;
    for (entryNum = 0; entryNum < listSize; ++entryNum)
        list[entryNum] = -1;
    for (entryNum = 0; entryNum < listSize; ++entryNum) {
        do
            r = RANDOM(listSize);
        while (list[r] != -1);
        list[r] = entryNum;
    }
}

int generate_image(int imageFuncNum, UCHAR *buf_graf, int xcenter, int ycenter, int xmax, int ymax, int colormax)
{
  long x, y, dx, dy, dist, angle;
  long color;
  long a1, a2, a3, a4, x1,x2,x3,x4,y1,y2,y3,y4;
  
  x1 = RANDOM(40)-20;  x2 = RANDOM(40)-20;  x3 = RANDOM(40)-20;  x4 = RANDOM(40)-20;
  y1 = RANDOM(40)-20;  y2 = RANDOM(40)-20;  y3 = RANDOM(40)-20;  y4 = RANDOM(40)-20;
  
  a1 = RANDOM(ANGLE_UNIT);  a2 = RANDOM(ANGLE_UNIT);  a3 = RANDOM(ANGLE_UNIT);  a4 = RANDOM(ANGLE_UNIT);
  (void)a1; (void)a2; (void)a3; (void)a4; /* suppress unused warnings */
  
  for (y = 0; y < ymax; ++y) {
    for (x = 0; x < xmax; ++x) {
      dx = x - xcenter;
      dy = y - ycenter;
      dist  = lut_dist(dx, dy);
      angle = lut_angle(dx, dy);
      
      switch (imageFuncNum) {
        case 0: color = angle + lut_sin(dist * 10) / 64 +
                  lut_cos(x * ANGLE_UNIT / xmax * 2) / 32 +
                  lut_cos(y * ANGLE_UNIT / ymax * 2) / 32; break;
        case 1: color = angle + lut_sin(dist * 10) / 16 +
                  lut_cos(x * ANGLE_UNIT / xmax * 2) / 8 +
                  lut_cos(y * ANGLE_UNIT / ymax * 2) / 8; break;
        case 2: color = lut_sin(lut_dist(dx + x1, dy + y1) * 4) / 32 +
                  lut_sin(lut_dist(dx + x2, dy + y2) * 8) / 32 +
                  lut_sin(lut_dist(dx + x3, dy + y3) * 16) / 32 +
                  lut_sin(lut_dist(dx + x4, dy + y4) * 32) / 32; break;
        case 3: color = angle + lut_sin(lut_dist(dx + 20, dy) * 10) / 32 +
                  angle + lut_sin(lut_dist(dx - 20, dy) * 10) / 32; break;
        case 4: color = lut_sin(dist) / 16; break;
        case 5: color = lut_cos(x * ANGLE_UNIT / xmax) / 8 +
                  lut_cos(y * ANGLE_UNIT / ymax) / 8 +
                  angle + lut_sin(dist) / 32; break;
        case 6: color = lut_sin(lut_dist(dx, dy - 20) * 4) / 32 +
                  lut_sin(lut_dist(dx + 20, dy + 20) * 4) / 32 +
                  lut_sin(lut_dist(dx - 20, dy + 20) * 4) / 32; break;
        case 7: color = angle +
                  lut_sin(lut_dist(dx, dy - 20) * 8) / 32 +
                  lut_sin(lut_dist(dx + 20, dy + 20) * 8) / 32 +
                  lut_sin(lut_dist(dx - 20, dy + 20) * 8) / 32; break;
        case 8: color = lut_sin(lut_dist(dx, dy - 20) * 12) / 32 +
                  lut_sin(lut_dist(dx + 20, dy + 20) * 12) / 32 +
                  lut_sin(lut_dist(dx - 20, dy + 20) * 12) / 32; break;
        case 9: color = dist + lut_sin(5 * angle) / 64; break;
        case 10: color = lut_cos(x * ANGLE_UNIT / xmax * 2) / 4 +
                   lut_cos(y * ANGLE_UNIT / ymax * 2) / 4; break;
        case 11: color = lut_cos(x * ANGLE_UNIT / xmax) / 8 +
                   lut_cos(y * ANGLE_UNIT / ymax) / 8; break;
        case 12: color = dist; break;
        case 13: color = angle; break;
        case 14: color = angle + lut_sin(dist * 8) / 32; break;
        case 15: color = lut_sin(dist * 4) / 32; break;
        case 16: color = dist + lut_sin(dist * 4) / 32; break;
        case 17: color = lut_sin(lut_cos(2 * x * ANGLE_UNIT / xmax)) / (20 + dist)
                   + lut_sin(lut_cos(2 * y * ANGLE_UNIT / ymax)) / (20 + dist); break;
        case 18: color = lut_cos(7 * x * ANGLE_UNIT / xmax) / (20 + dist) +
                   lut_cos(7 * y * ANGLE_UNIT / ymax) / (20 + dist); break;
        case 19: color = lut_cos(17 * x * ANGLE_UNIT / xmax) / (20 + dist) +
                   lut_cos(17 * y * ANGLE_UNIT / ymax) / (20 + dist); break;
        case 20: color = lut_cos(17 * x * ANGLE_UNIT / xmax) / 32 +
                   lut_cos(17 * y * ANGLE_UNIT / ymax) / 32 + dist + angle; break;
        case 21: color = lut_cos(7 * x * ANGLE_UNIT / xmax) / 32 +
                   lut_cos(7 * y * ANGLE_UNIT / ymax) / 32 + dist; break;
        case 22: color = lut_cos(7 * x * ANGLE_UNIT / xmax) / 32 +
                   lut_cos(7 * y * ANGLE_UNIT / ymax) / 32 +
                   lut_cos(11 * x * ANGLE_UNIT / xmax) / 32 +
                   lut_cos(11 * y * ANGLE_UNIT / ymax) / 32; break;
        case 23: color = lut_sin(angle * 7) / 32; break;
        case 24: color = lut_sin(lut_dist(dx + x1, dy + y1) * 2) / 12 +
                   lut_sin(lut_dist(dx + x2, dy + y2) * 4) / 12 +
                   lut_sin(lut_dist(dx + x3, dy + y3) * 6) / 12 +
                   lut_sin(lut_dist(dx + x4, dy + y4) * 8) / 12; break;
        case 25: color = angle + lut_sin(lut_dist(dx + x1, dy + y1) * 2) / 16 +
                   angle + lut_sin(lut_dist(dx + x2, dy + y2) * 4) / 16 +
                   lut_sin(lut_dist(dx + x3, dy + y3) * 6) / 8 +
                   lut_sin(lut_dist(dx + x4, dy + y4) * 8) / 8; break;
        case 26: color = angle + lut_sin(lut_dist(dx + x1, dy + y1) * 2) / 12 +
                   angle + lut_sin(lut_dist(dx + x2, dy + y2) * 4) / 12 +
                   angle + lut_sin(lut_dist(dx + x3, dy + y3) * 6) / 12 +
                   angle + lut_sin(lut_dist(dx + x4, dy + y4) * 8) / 12; break;
        case 27: color = lut_sin(lut_dist(dx + x1, dy + y1) * 2) / 32 +
                   lut_sin(lut_dist(dx + x2, dy + y2) * 4) / 32 +
                   lut_sin(lut_dist(dx + x3, dy + y3) * 6) / 32 +
                   lut_sin(lut_dist(dx + x4, dy + y4) * 8) / 32; break;
        case 28:
          if (y == 0 || x == 0) color = RANDOM(16);
          else color = (*(buf_graf + (xmax * y) + (x-1)) +
                        *(buf_graf + (xmax * (y-1)) + x)) / 2 + RANDOM(16) - 8;
          break;
        case 29:
          if (y == 0 || x == 0) color = RANDOM(1024);
          else color = dist/6 + (*(buf_graf + (xmax * y) + (x-1)) +
                        *(buf_graf + (xmax * (y-1)) + x)) / 2 + RANDOM(16) - 8;
          break;
        case 30: color = lut_sin(lut_dist(dx, dy - 20) * 4) / 32 ^
                   lut_sin(lut_dist(dx + 20, dy + 20) * 4) / 32 ^
                   lut_sin(lut_dist(dx - 20, dy + 20) * 4) / 32; break;
        case 31: color = (angle % (ANGLE_UNIT/4)) ^ dist; break;
        case 32: color = dy ^ dx; break;
        case 33:
          if (y == 0 || x == 0) color = RANDOM(16);
          else color = (*(buf_graf + (xmax * y) + (x-1)) +
                        *(buf_graf + (xmax * (y-1)) + x)) / 2;
          color += RANDOM(2) - 1;
          if (color < 64) color += RANDOM(16) - 8;
          break;
        case 34:
          if (y == 0 || x == 0) color = RANDOM(16);
          else color = (*(buf_graf + (xmax * y) + (x-1)) +
                        *(buf_graf + (xmax * (y-1)) + x)) / 2;
          if (color < 100) color += RANDOM(16) - 8;
          break;
        case 35:
          color = angle + lut_sin(dist * 8) / 32;
          dx = x - xcenter; dy = (y - ycenter) * 2;
          dist = lut_dist(dx, dy); angle = lut_angle(dx, dy);
          color = (color + angle + lut_sin(dist * 8) / 32) / 2;
          break;
        case 36:
          color = angle + lut_sin(dist * 10) / 16 +
            lut_cos(x * ANGLE_UNIT / xmax * 2) / 8 +
            lut_cos(y * ANGLE_UNIT / ymax * 2) / 8;
          dx = x - xcenter; dy = (y - ycenter) * 2;
          dist = lut_dist(dx, dy); angle = lut_angle(dx, dy);
          color = (color + angle + lut_sin(dist * 8) / 32) / 2;
          break;
        case 37:
          color = angle + lut_sin(dist * 10) / 16 +
            lut_cos(x * ANGLE_UNIT / xmax * 2) / 8 +
            lut_cos(y * ANGLE_UNIT / ymax * 2) / 8;
          dx = x - xcenter; dy = (y - ycenter) * 2;
          dist = lut_dist(dx, dy); angle = lut_angle(dx, dy);
          color = (color + angle + lut_sin(dist * 10) / 16 +
            lut_cos(x * ANGLE_UNIT / xmax * 2) / 8 +
            lut_cos(y * ANGLE_UNIT / ymax * 2) / 8) / 2;
          break;
        case 38:
          if (dy % 2) { dy *= 2; dist = lut_dist(dx, dy); angle = lut_angle(dx, dy); }
          color = angle + lut_sin(dist * 8) / 32;
          break;
        case 39:
          color = (angle % (ANGLE_UNIT/4)) ^ dist;
          dx = x - xcenter; dy = (y - ycenter) * 2;
          dist = lut_dist(dx, dy); angle = lut_angle(dx, dy);
          color = (color + ((angle % (ANGLE_UNIT/4)) ^ dist)) / 2;
          break;
        case 40:
          color = dy ^ dx;
          dx = x - xcenter; dy = (y - ycenter) * 2;
          color = (color + (dy ^ dx)) / 2;
          break;
        default:
          color = RANDOM(colormax - 1) + 1;
          break;
      }
      
      color = color % (colormax - 1);
      if (color < 0) color += (colormax - 1);
      ++color;
      *(buf_graf + (xmax * y) + x) = (UCHAR)color;
    }
  }
  return 0;
}

void newpal(void) {
    int pt = RANDOM(NUM_PALETTE_TYPES + 1);
    initPalArray(MainPalArray, pt);
}

int checkinput(void) { return 0; }

void processinput(void) {
    switch(checkinput()) {
        case 1: GO = !GO; break;
        case 2: SKIP = TRUE; break;
        case 3: exit(0); break;
        case 4: NP = TRUE; break;
        case 5: LOCK = !LOCK; break;
        case 6: ROTATION_DELAY = (ROTATION_DELAY > 5000) ? ROTATION_DELAY - 5000 : 0; break;
        case 7: ROTATION_DELAY += 5000; break;
    }
}

// Texture shader sources for classic mode (WebGL 1.0 / GLSL ES 1.0)
#ifdef __EMSCRIPTEN__
static const char *texture_vert_src = 
    "attribute vec2 position;\n"
    "varying vec2 uv;\n"
    "void main() {\n"
    "    uv = position * 0.5 + 0.5;\n"
    "    uv.y = 1.0 - uv.y;\n"
    "    gl_Position = vec4(position, 0.0, 1.0);\n"
    "}\n";

static const char *texture_frag_src = 
    "precision highp float;\n"
    "varying vec2 uv;\n"
    "uniform sampler2D tex;\n"
    "void main() {\n"
    "    gl_FragColor = texture2D(tex, uv);\n"
    "}\n";
#endif

void graphicsinit(void) {
#ifdef __EMSCRIPTEN__
    // Get actual screen dimensions on web
    window_width = get_canvas_width();
    window_height = get_canvas_height();
    // Ensure minimum size
    if (window_width < 320) window_width = 320;
    if (window_height < 240) window_height = 240;
#endif
    XMax = window_width;
    YMax = window_height;
    buf_graf = (UCHAR *)malloc(XMax * YMax);
    pixel_buffer = (Uint32 *)malloc(XMax * YMax * sizeof(Uint32));

    SDL_Init(SDL_INIT_VIDEO);
    
#ifdef __EMSCRIPTEN__
    /* Try WebGL 2.0 first (OpenGL ES 3.0), fallback to WebGL 1.0 */
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    
    Uint32 win_flags = SDL_WINDOW_OPENGL;
    if (fullscreen) win_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    window = SDL_CreateWindow("Acidwarp", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, XMax, YMax, win_flags);
    
    if (!window) {
        printf("Failed to create window: %s\n", SDL_GetError());
        return;
    }
    
    sdl_gl_ctx = SDL_GL_CreateContext(window);
    if (!sdl_gl_ctx) {
        printf("Failed to create GL context: %s, falling back to SDL renderer\n", SDL_GetError());
        /* Fallback to SDL renderer */
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, XMax, YMax);
        return;
    }
    SDL_GL_MakeCurrent(window, sdl_gl_ctx);
    SDL_GL_SetSwapInterval(1);  // VSync
    
    const char *gl_version = (const char *)glGetString(GL_VERSION);
    const char *glsl_version = (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
    (void)gl_version;
    (void)glsl_version;
    
    /* Initialize modern effect shaders */
    init_gl_shaders();
    
    /* Create texture shader for classic mode */
    GLuint vs = compile_shader(GL_VERTEX_SHADER, texture_vert_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, texture_frag_src);
    texture_program = glCreateProgram();
    glAttachShader(texture_program, vs);
    glAttachShader(texture_program, fs);
    glLinkProgram(texture_program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    
    /* Create VBO for texture quad */
    float vertices[] = { -1, -1, 1, -1, -1, 1, 1, 1 };
    glGenBuffers(1, &texture_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, texture_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    /* Create texture for classic mode pixel buffer */
    glGenTextures(1, &classic_texture);
    glBindTexture(GL_TEXTURE_2D, classic_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, XMax, YMax, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    
#else
    Uint32 win_flags = 0;
    if (fullscreen) win_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    window = SDL_CreateWindow("Acidwarp", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, XMax, YMax, win_flags);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, XMax, YMax);
#endif
}

void restoreOldVideoMode(void) {
    if (texture) SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}

/* ============================================================================
 * MAIN LOOP - State Machine (Emscripten compatible)
 * ============================================================================
 * Instead of nested for(;;) loops, we use a state machine that processes
 * one frame at a time. This allows the browser to remain responsive.
 */

void main_loop_iteration(void) {
    handle_sdl_events();
    
    if (paused) {
        SDL_Delay(10);
        return;
    }

    /* Classic mode: throttle to match original speed (~30ms per frame) */
    if (!modern_mode) {
        Uint32 now = SDL_GetTicks();
        if (now - last_classic_frame_time < CLASSIC_FRAME_DELAY_MS) {
            return;  /* Skip this frame to slow down */
        }
        last_classic_frame_time = now;
    }

    switch (current_state) {
        case STATE_LOGO_DISPLAY:
            if (!logo_initialized) {
                initPalArray(MainPalArray, RGBW_LIGHTNING_PAL);
                initPalArray(TargetPalArray, RGBW_LIGHTNING_PAL);
                writeBitmapImageToArray(buf_graf, NOAHS_FACE, XMax, YMax);
                ltime = time(NULL);
                mtime = ltime + logo_time;
                logo_initialized = 1;
            }
            
            if (logo_time == 0 || SKIP || time(NULL) > mtime) {
                current_state = STATE_LOGO_FADE;
                SKIP = FALSE;
                break;
            }
            
            processinput();
            if (GO) rollMainPalArrayAndLoadDACRegs(MainPalArray);
            render_frame();
            break;

        case STATE_LOGO_FADE:
            if (SKIP || FadeCompleteFlag) {
                FadeCompleteFlag = 0;
                SKIP = FALSE;
                makeShuffledList(imageFuncList, NUM_IMAGE_FUNCTIONS);
                imageFuncListIndex = -1;  // Will be incremented to 0
                current_state = STATE_NEW_IMAGE;
                break;
            }
            
            processinput();
            if (GO) rolNFadeBlkMainPalArrayNLoadDAC(MainPalArray);
            render_frame();
            break;

        case STATE_NEW_IMAGE:
            /* Move to next image */
            if (++imageFuncListIndex >= NUM_IMAGE_FUNCTIONS) {
                imageFuncListIndex = 0;
                makeShuffledList(imageFuncList, NUM_IMAGE_FUNCTIONS);
            }

            /* Generate new image */
            generate_image(
                (userOptionImageFuncNum < 0) ? 
                    imageFuncList[imageFuncListIndex] : 
                    userOptionImageFuncNum,
                buf_graf, XMax/2, YMax/2, XMax, YMax, 255);

            /* Create new target palette */
            paletteTypeNum = RANDOM(NUM_PALETTE_TYPES + 1);
            initPalArray(TargetPalArray, paletteTypeNum);
            
            FadeCompleteFlag = 0;
            current_state = STATE_FADE_IN;
            render_frame();
            break;

        case STATE_FADE_IN:
            if (SKIP || FadeCompleteFlag) {
                FadeCompleteFlag = 0;
                SKIP = FALSE;
                ltime = time(NULL);
                mtime = ltime + image_time;
                current_state = STATE_ROTATE;
                break;
            }
            
            processinput();
            if (GO) rolNFadeMainPalAryToTargNLodDAC(MainPalArray, TargetPalArray);
            render_frame();
            break;

        case STATE_ROTATE:
            if (SKIP || (!LOCK && time(NULL) > mtime)) {
                SKIP = FALSE;
                FadeCompleteFlag = 0;
                current_state = STATE_FADE_OUT;
                break;
            }
            
            processinput();
            if (GO) rollMainPalArrayAndLoadDACRegs(MainPalArray);
            if (NP) { newpal(); NP = FALSE; }
            render_frame();
            break;

        case STATE_FADE_OUT:
            if (FadeCompleteFlag) {
                FadeCompleteFlag = 0;
                current_state = STATE_NEW_IMAGE;
                break;
            }
            
            processinput();
            if (GO) {
                if (fade_dir)
                    rolNFadeBlkMainPalArrayNLoadDAC(MainPalArray);
                else
                    rolNFadeWhtMainPalArrayNLoadDAC(MainPalArray);
            }
            render_frame();
            break;
    }
}

/* ============================================================================
 * MAIN
 * ============================================================================ */

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    RANDOMIZE();
    
    printf("Acidwarp WebGL Edition (2025)\n");
    printf("Hotkeys: [SPACE]=Pause [LEFT/RIGHT]=Prev/Next [L]=Lock [M]=Mode [F]=Fullscreen [ESC]=Quit\n");
    
    graphicsinit();

#ifdef __EMSCRIPTEN__
    /* Let browser control the frame rate (typically 60fps via requestAnimationFrame) */
    emscripten_set_main_loop(main_loop_iteration, 0, 1);
#else
    /* Native: run our own loop */
    for (;;) {
        main_loop_iteration();
        SDL_Delay(16);  /* ~60fps */
    }
#endif

    restoreOldVideoMode();
    return 0;
}
