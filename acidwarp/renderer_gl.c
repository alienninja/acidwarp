// Modern OpenGL renderer for Acidwarp (minimal prototype)
// Only included/built if --renderer=opengl is passed
// (c) 2025 Acidwarp Modernization
#include <stdint.h>

// --- Palette constants for intro effect ---
#define PALETTE_SIZE 256
#define COLOR_CHANNELS 3

// Define global intro palette and buffer for intro rendering
#define INTRO_WIDTH 320
#define INTRO_HEIGHT 200
uint8_t intro_palette[PALETTE_SIZE * COLOR_CHANNELS];
uint8_t intro_8bit_buf[INTRO_WIDTH * INTRO_HEIGHT];

#include "handy.h"
#include "acidwarp.h"
#include "effects_rgb.h"
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <stdbool.h>

#define GL_GLEXT_PROTOTYPES 1

static SDL_Window *gl_window = NULL;
static SDL_GLContext gl_context = NULL;
static GLuint gl_texture = 0;
static int gl_width = 640, gl_height = 480;
static uint32_t *gl_rgb_buffer = NULL;
static int use_modern_effect = 0;
static int selected_effect = 0;
static uint32_t effect_cycle_start_time = 0; // For automatic cycling
static const uint32_t EFFECT_CYCLE_INTERVAL_MS = 8000; // 8 seconds per effect

// Add global to track fullscreen mode
static int gl_fullscreen = 0;

// --- Shader support ---
static GLuint mandelbrot_program = 0;
static GLuint julia_program = 0;

static const char *mandelbrot_frag_path = "shaders/mandelbrot.frag";
static const char *julia_frag_path = "shaders/julia.frag";

// Utility to load shader file
static char *load_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char*)malloc(len+1);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, len, f);
    buf[len] = 0;
    fclose(f);
    return buf;
}

static GLuint compile_shader(const char *src, GLenum type) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(sh, 512, NULL, log);
        fprintf(stderr, "Shader compile error: %s\n", log);
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

static GLuint create_program(const char *frag_path) {
    const char *vs_src = "#version 330 core\nlayout(location=0) in vec2 pos;out vec2 uv;void main(){uv=0.5*pos+0.5;gl_Position=vec4(pos,0,1);}";
    GLuint vs = compile_shader(vs_src, GL_VERTEX_SHADER);
    char *fs_src = load_file(frag_path);
    if (!fs_src) { fprintf(stderr, "Failed to load %s\n", frag_path); return 0; }
    GLuint fs = compile_shader(fs_src, GL_FRAGMENT_SHADER);
    free(fs_src);
    if (!vs || !fs) return 0;
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glBindAttribLocation(prog, 0, "pos");
    glLinkProgram(prog);
    glDeleteShader(vs); glDeleteShader(fs);
    GLint ok=0; glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, 512, NULL, log);
        fprintf(stderr, "Program link error: %s\n", log);
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

// Helper: Mandelbrot iteration count for a point
int mandelbrot_iter(double x, double y, int max_iter) {
    double zx = 0, zy = 0;
    int iter = 0;
    while (zx*zx + zy*zy < 4.0 && iter < max_iter) {
        double xt = zx*zx - zy*zy + x;
        zy = 2*zx*zy + y;
        zx = xt;
        iter++;
    }
    return iter;
}

// Initialize OpenGL context and resources
typedef struct {
    int width, height;
} RendererGLConfig;

int renderer_gl_init(RendererGLConfig *cfg) {
    char cwd[1024];
#ifdef DEBUG
    if (getcwd(cwd, sizeof(cwd))) {
        printf("[acidwarp] Current working directory: %s\n", cwd);
    }
#endif
    gl_width = cfg->width;
    gl_height = cfg->height;
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 0;
    }
    Uint32 win_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
    if (gl_fullscreen) win_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    gl_window = SDL_CreateWindow("Acidwarp Modern (OpenGL)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, gl_width, gl_height, win_flags);
    if (!gl_window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 0;
    }
    // Query actual window size (important for fullscreen)
    SDL_GetWindowSize(gl_window, &gl_width, &gl_height);
    gl_context = SDL_GL_CreateContext(gl_window);
    if (!gl_context) {
        fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return 0;
    }
    if (SDL_GL_MakeCurrent(gl_window, gl_context) != 0) {
        fprintf(stderr, "Failed to make GL context current: %s\n", SDL_GetError());
        exit(1);
    }
    GLenum glew_status = glewInit();
    if (glew_status != GLEW_OK) {
        fprintf(stderr, "GLEW init error: %s\n", glewGetErrorString(glew_status));
        return 0;
    }
    SDL_GL_SetSwapInterval(1); // vsync
    // Create texture
    glGenTextures(1, &gl_texture);
    glBindTexture(GL_TEXTURE_2D, gl_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl_rgb_buffer = (uint32_t*)malloc(gl_width * gl_height * sizeof(uint32_t));
    if (!gl_rgb_buffer) {
        fprintf(stderr, "Failed to allocate RGB buffer\n");
        return 0;
    }
    // Load Mandelbrot shader
    mandelbrot_program = create_program(mandelbrot_frag_path);
    if (!mandelbrot_program) {
        fprintf(stderr, "Failed to load Mandelbrot shader.\n");
    }
    // Load Julia shader
    julia_program = create_program(julia_frag_path);
    if (!julia_program) {
        fprintf(stderr, "Failed to load Julia shader.\n");
    }
    effect_cycle_start_time = SDL_GetTicks();
    return 1;
}

// Parse modern effect flag
void renderer_gl_parse_flags(int argc, char *argv[]) {
    use_modern_effect = 0;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--modern-effect") == 0) use_modern_effect = 1;
        if (strcmp(argv[i], "--fullscreen") == 0) gl_fullscreen = 1;
    }
}

// Fill buffer with a gradient for testing
void renderer_gl_fill_gradient() {
    for (int y = 0; y < gl_height; ++y) {
        for (int x = 0; x < gl_width; ++x) {
            uint8_t r = (x * 255) / (gl_width-1);
            uint8_t g = (y * 255) / (gl_height-1);
            uint8_t b = 128;
            gl_rgb_buffer[y * gl_width + x] = (0xFF << 24) | (r << 16) | (g << 8) | b;
        }
    }
}

// Modern RGB plasma effect
void renderer_gl_fill_modern_effect() {
    for (int y = 0; y < gl_height; ++y) {
        for (int x = 0; x < gl_width; ++x) {
            float fx = (float)x / gl_width;
            float fy = (float)y / gl_height;
            float v = 0.5f + 0.5f * sinf(10.0f * fx + SDL_GetTicks() * 0.001f) * cosf(10.0f * fy + SDL_GetTicks() * 0.001f);
            uint8_t r = (uint8_t)(127 + 127 * sinf(2 * 3.14159f * fx + v));
            uint8_t g = (uint8_t)(127 + 127 * sinf(2 * 3.14159f * fy + v));
            uint8_t b = (uint8_t)(127 + 127 * cosf(2 * 3.14159f * (fx + fy) + v));
            gl_rgb_buffer[y * gl_width + x] = (0xFF << 24) | (r << 16) | (g << 8) | b;
        }
    }
}

// Render current buffer to the window
void renderer_gl_present() {
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, gl_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, gl_width, gl_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, gl_rgb_buffer);
    glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex2f(-1, -1);
        glTexCoord2f(1, 0); glVertex2f( 1, -1);
        glTexCoord2f(1, 1); glVertex2f( 1,  1);
        glTexCoord2f(0, 1); glVertex2f(-1,  1);
    glEnd();
    SDL_GL_SwapWindow(gl_window);
}

// Render Mandelbrot using shader
static void renderer_gl_present_mandelbrot_shader(float time_s) {
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(mandelbrot_program);
    int loc_time = glGetUniformLocation(mandelbrot_program, "u_time");
    int loc_res = glGetUniformLocation(mandelbrot_program, "u_resolution");
    int loc_zoom = glGetUniformLocation(mandelbrot_program, "u_zoom");
    int loc_swirl = glGetUniformLocation(mandelbrot_program, "u_swirl");
    int loc_center = glGetUniformLocation(mandelbrot_program, "u_center");
    // Stable Mandelbrot: Fixed Zoom, Fixed Center, Color Cycling
    const double center_x_ref = -0.743643887037158704752191506114774;
    const double center_y_ref = 0.131825904205311970493132056385139;
    double zoom = 80000.0; // Adjust for desired detail
    float center_x = (float)center_x_ref;
    float center_y = (float)center_y_ref;
    float swirl = 0.15f * sinf(0.3f * time_s); // Smooth oscillation
    glUniform1f(loc_time, time_s);
    glUniform2f(loc_res, (float)gl_width, (float)gl_height);
    glUniform1f(loc_zoom, (float)zoom);
    glUniform2f(loc_center, center_x, center_y);
    glUniform1f(loc_swirl, swirl);
    // Fullscreen quad
    float verts[8] = {-1,-1, 1,-1, 1,1, -1,1};
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao); glBindVertexArray(vao);
    glGenBuffers(1, &vbo); glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glUseProgram(0);
    SDL_GL_SwapWindow(gl_window);
}

// Render Julia using shader
static void renderer_gl_present_julia_shader(float time_s) {
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(julia_program);
    int loc_time = glGetUniformLocation(julia_program, "u_time");
    int loc_res = glGetUniformLocation(julia_program, "u_resolution");
    int loc_zoom = glGetUniformLocation(julia_program, "u_zoom");
    int loc_swirl = glGetUniformLocation(julia_program, "u_swirl");
    int loc_center = glGetUniformLocation(julia_program, "u_center");
    // Reverse (breathing) zoom logic for Julia (reduced range)
    float tmod = fmodf(time_s, 32.0f);
    float tphase = (tmod < 16.0f) ? tmod : (32.0f - tmod);
    float zoom = powf(1.015f, tphase * 250.0f); // reduced zoom range for visual appeal
    float swirl = 0.12f * time_s;
    float center_x = 0.0f;
    float center_y = 0.0f;
    glUniform1f(loc_time, time_s);
    glUniform2f(loc_res, (float)gl_width, (float)gl_height);
    glUniform1f(loc_zoom, zoom);
    glUniform1f(loc_swirl, swirl);
    glUniform2f(loc_center, center_x, center_y);
    // Fullscreen quad
    float verts[8] = {-1,-1, 1,-1, 1,1, -1,1};
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao); glBindVertexArray(vao);
    glGenBuffers(1, &vbo); glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glUseProgram(0);
    SDL_GL_SwapWindow(gl_window);
}

// Display an animated RGBA buffer (classic intro) as a fullscreen OpenGL texture for a given duration (ms)
void renderer_gl_show_intro_texture(const uint32_t *rgba_buffer, int width, int height, int display_ms) {
    // We'll need to animate the palette, so accept the 8-bit buffer and palette
    extern void cycle_intro_palette(uint8_t *palette, int frame); // We'll define this below
    extern void convert_8bit_to_32bit(const uint8_t *buffer, uint32_t *out_rgba, int width, int height, const uint8_t *palette);
    uint32_t *frame_rgba = (uint32_t*)malloc(width * height * sizeof(uint32_t));
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    Uint32 start = SDL_GetTicks();
    SDL_Event event;
    int running = 1;
    int frame = 0;
    while (running && (SDL_GetTicks() - start < (Uint32)display_ms)) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            if (event.type == SDL_KEYDOWN) running = 0;
        }
        // Animate palette
        cycle_intro_palette(intro_palette, frame);
        convert_8bit_to_32bit(intro_8bit_buf, frame_rgba, width, height, intro_palette);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, frame_rgba);

        glViewport(0, 0, gl_width, gl_height);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, tex);
        // Compute aspect-correct quad
        float window_aspect = (float)gl_width / (float)gl_height;
        float image_aspect = (float)width / (float)height;
        float x0, x1, y0, y1;
        if (window_aspect > image_aspect) {
            float scale = (float)gl_height / height;
            float w = width * scale / gl_width;
            x0 = -w; x1 = w; y0 = -1; y1 = 1;
        } else {
            float scale = (float)gl_width / width;
            float h = height * scale / gl_height;
            x0 = -1; x1 = 1; y0 = -h; y1 = h;
        }
        glBegin(GL_QUADS);
            glTexCoord2f(0, 1); glVertex2f(x0, y0);
            glTexCoord2f(1, 1); glVertex2f(x1, y0);
            glTexCoord2f(1, 0); glVertex2f(x1, y1);
            glTexCoord2f(0, 0); glVertex2f(x0, y1);
        glEnd();
        SDL_GL_SwapWindow(gl_window);
        SDL_Delay(16);
        frame++;
    }
    glDeleteTextures(1, &tex);
    free(frame_rgba);
}

// Dummy palette cycling for now (replace with real logic)
void cycle_intro_palette(uint8_t *palette, int frame) {
    // Example: simple flash
    int flash = (frame / 15) % 2 ? 32 : 0;
    for (int i = 0; i < PALETTE_SIZE * COLOR_CHANNELS; ++i) {
        int v = palette[i] + flash;
        if (v > 255) v = 255;
        palette[i] = (uint8_t)v;
    }
}

void renderer_gl_cleanup() {
    if (gl_rgb_buffer) free(gl_rgb_buffer);
    if (gl_texture) glDeleteTextures(1, &gl_texture);
    if (gl_context) SDL_GL_DeleteContext(gl_context);
    if (gl_window) SDL_DestroyWindow(gl_window);
    SDL_Quit();
}

// Main event loop for OpenGL mode
typedef struct {
    int running;
} RendererGLEventState;

void renderer_gl_mainloop() {
    RendererGLEventState state = {1};
    SDL_Event event;
    uint32_t last_effect = (uint32_t)-1;
    effect_cycle_start_time = SDL_GetTicks();
    while (state.running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) state.running = 0;
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) state.running = 0;
                if (event.key.keysym.sym == SDLK_n) {
                    selected_effect = (selected_effect + 1) % rgb_effect_count;
#ifdef DEBUG
                    printf("[OpenGL] Effect: %s\n", rgb_effect_names[selected_effect]);
#endif
                    effect_cycle_start_time = SDL_GetTicks();
                }
                if (event.key.keysym.sym == SDLK_p) {
                    selected_effect = (selected_effect - 1 + rgb_effect_count) % rgb_effect_count;
#ifdef DEBUG
                    printf("[OpenGL] Effect: %s\n", rgb_effect_names[selected_effect]);
#endif
                    effect_cycle_start_time = SDL_GetTicks();
                }
            }
        }
        int time_ms = SDL_GetTicks();
        // Automatic cycling
        if ((uint32_t)(time_ms - effect_cycle_start_time) > EFFECT_CYCLE_INTERVAL_MS) {
            int prev_effect = selected_effect;
            // Pick a new random effect, not the same as current
            int next_effect;
            do {
                next_effect = rand() % rgb_effect_count;
            } while (next_effect == prev_effect && rgb_effect_count > 1);
            selected_effect = next_effect;
#ifdef DEBUG
            printf("[OpenGL] [AutoCycle] Effect: %s\n", rgb_effect_names[selected_effect]);
#endif
            effect_cycle_start_time = time_ms;
        }
        float time_s = time_ms * 0.001f;
        // Use GPU Mandelbrot/Julia if selected (only at correct indices)
        if (selected_effect == EFFECT_IDX_MANDELBROT && mandelbrot_program) {
            renderer_gl_present_mandelbrot_shader(time_s);
        } else if (selected_effect == EFFECT_IDX_JULIA && julia_program) {
            renderer_gl_present_julia_shader(time_s);
        } else {
            rgb_effects[selected_effect](gl_rgb_buffer, gl_width, gl_height, time_ms);
            renderer_gl_present();
        }
        SDL_Delay(16); // ~60 FPS
    }
    renderer_gl_cleanup();
}
