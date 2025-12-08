#ifndef RENDERER_GL_H
#define RENDERER_GL_H

typedef struct {
    int width, height;
} RendererGLConfig;

int renderer_gl_init(RendererGLConfig *cfg);
void renderer_gl_parse_flags(int argc, char *argv[]);
void renderer_gl_fill_gradient();
void renderer_gl_fill_modern_effect();
void renderer_gl_present();
void renderer_gl_mainloop();
void renderer_gl_cleanup();
void renderer_gl_show_intro_texture(const uint32_t *rgba_buffer, int width, int height, int display_ms);

#endif // RENDERER_GL_H
