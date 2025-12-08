#ifndef EFFECTS_RGB_H
#define EFFECTS_RGB_H
#include <stdint.h>

// Signature for all modern RGB effects
typedef void (*rgb_effect_fn)(uint32_t *buf, int w, int h, int time_ms);

// Effect table and count
extern rgb_effect_fn rgb_effects[];
extern int rgb_effect_count;
extern const char *rgb_effect_names[];

// Individual effect prototypes
void effect_plasma_rgb(uint32_t *buf, int w, int h, int time_ms);
void effect_swirl_rgb(uint32_t *buf, int w, int h, int time_ms);
void effect_tunnel_rgb(uint32_t *buf, int w, int h, int time_ms);
void effect_rings_rgb(uint32_t *buf, int w, int h, int time_ms);
void effect_checker_rgb(uint32_t *buf, int w, int h, int time_ms);
// ...add more as ported

// Effect indices (must match rgb_effects[] order in effects_rgb.c)
#define EFFECT_IDX_MANDELBROT 25 // "Mandelbrot Fractal"
#define EFFECT_IDX_JULIA 26      // "Julia Set"

#endif // EFFECTS_RGB_H
