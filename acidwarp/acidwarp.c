/* ACID WARP (c)Copyright 1992, 1993 by Noah Spurrier
 * All Rights reserved. Private Proprietary Source Code by Noah Spurrier
 * Ported to Linux by Steven Wills
 */
#ifndef DBG_PRINT
#define DBG_PRINT(...)
#endif

// --- Acidwarp Modernization Constants ---
#define PALETTE_SIZE 256
#define COLOR_CHANNELS 3
#define MAX_UNIQUE_INDICES 10
#define MAX_COLOR_VALUE 255
#define BUFFER_DEBUG_PRINT_COUNT 32
#define ANGLE_UNIT 256
#define NUM_IMAGE_FUNCTIONS 40
#define NOAHS_FACE   0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include <sys/time.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>

#include "warp_text.h"
#include "handy.h"
#include "acidwarp.h"
#include "lut.h"
#include "bit_map.h"
#include "palinit.h"
#include "rolnfade.h"
#include "renderer_gl.h"

// Renderer selection enum
typedef enum { RENDERER_SDL, RENDERER_OPENGL } RendererType;
static RendererType renderer_type = RENDERER_SDL;

// Parse renderer flag
void parse_renderer_flag(int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--renderer=opengl") == 0) {
            renderer_type = RENDERER_OPENGL;
        } else if (strcmp(argv[i], "--renderer=sdl") == 0) {
            renderer_type = RENDERER_SDL;
        }
    }
}

// Forward declarations
void restoreOldVideoMode(void);

// Modern input state (must be before any function that uses them)
bool is_paused = false;
bool palette_locked = false;
bool is_running = true;
bool skip_image = false;
bool new_palette_requested = false;
bool fade_dir = true;

static const int ROTATION_DELAY_DEFAULT = 30000;
static const int LOGO_TIME_DEFAULT = 4;
static const int IMAGE_TIME_DEFAULT = 20;

int ROTATION_DELAY = ROTATION_DELAY_DEFAULT;
int logo_time = LOGO_TIME_DEFAULT, image_time = IMAGE_TIME_DEFAULT;
int XMax = 0, YMax = 0;
uint8_t *buf_graf = NULL;
bool FadeCompleteFlag = false;

// SDL2 globals
SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *texture = NULL;

// Helper to convert 8-bit framebuffer to 32-bit RGBA
void convert_8bit_to_32bit(const uint8_t *src, Uint32 *dst, int width, int height, const uint8_t *palette) {
    DBG_PRINT("[DEBUG] Enter convert_8bit_to_32bit\n");
    DBG_PRINT("[DEBUG] palette[0-8]: %d %d %d %d %d %d %d %d %d\n", palette[0], palette[1], palette[2], palette[3], palette[4], palette[5], palette[6], palette[7], palette[8]);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = src[y * width + x];
            /* Scale 6-bit VGA palette (0-63) to 8-bit (0-255) */
            Uint8 r = (palette[idx * COLOR_CHANNELS + 0] << 2) | (palette[idx * COLOR_CHANNELS + 0] >> 4);
            Uint8 g = (palette[idx * COLOR_CHANNELS + 1] << 2) | (palette[idx * COLOR_CHANNELS + 1] >> 4);
            Uint8 b = (palette[idx * COLOR_CHANNELS + 2] << 2) | (palette[idx * COLOR_CHANNELS + 2] >> 4);
            dst[y * width + x] = (0xFFU << 24) | (r << 16) | (g << 8) | b;
        }
    }
    DBG_PRINT("[DEBUG] Exit convert_8bit_to_32bit\n");
    DBG_PRINT("[DEBUG] palette[0-8]: %d %d %d %d %d %d %d %d %d\n", palette[0], palette[1], palette[2], palette[3], palette[4], palette[5], palette[6], palette[7], palette[8]);
}

// Helper to process SDL events and handle quit
void handle_sdl_events(void) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            restoreOldVideoMode();
            exit(0);
        } else if (event.type == SDL_KEYDOWN) {
            SDL_Keycode key = event.key.keysym.sym;
            if (key == SDLK_q || key == SDLK_ESCAPE) {
                restoreOldVideoMode();
                exit(0);
            } else if (key == SDLK_SPACE || key == SDLK_p) {
                is_paused = !is_paused;
            } else if (key == SDLK_n) {
                skip_image = true;
            } else if (key == SDLK_l) {
                palette_locked = !palette_locked;
            } else if (key == SDLK_f) {
                /* Toggle fullscreen */
                Uint32 flags = SDL_GetWindowFlags(window);
                if (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) {
                    SDL_SetWindowFullscreen(window, 0);
                } else {
                    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                }
            } else if (key == SDLK_h) {
                printf("Hotkeys: [SPACE]=Pause [N]=Next [L]=Lock [F]=Fullscreen [ESC]=Quit\n");
            }
        }
    }
}

#ifdef DEBUG
void debug_print_palette(const uint8_t *palette) {
    printf("MainPalArray (first 5 colors):\n");
    for (int i = 0; i < 5; ++i) {
        printf("%3d: R=%3d G=%3d B=%3d\n", i, palette[i*COLOR_CHANNELS+0], palette[i*COLOR_CHANNELS+1], palette[i*COLOR_CHANNELS+2]);
    }
}

void debug_print_palette_range(const uint8_t *palette, int start, int end) {
    printf("Palette entries [%d..%d]:\n", start, end);
    for (int i = start; i <= end; ++i) {
        printf("  idx=%3d: R=%3d G=%3d B=%3d\n", i, palette[i*COLOR_CHANNELS+0], palette[i*COLOR_CHANNELS+1], palette[i*COLOR_CHANNELS+2]);
    }
}

void debug_print_buffer(const uint8_t *buf, int width, int height) {
    printf("buf_graf (first %d values): ", BUFFER_DEBUG_PRINT_COUNT);
    for (int i = 0; i < BUFFER_DEBUG_PRINT_COUNT && i < width * height; ++i) {
        printf("%3d ", buf[i]);
    }
    printf("\n");
}

void debug_print_palette_indices(const uint8_t *palette, const uint8_t *indices, int num_indices) {
    printf("Palette values for buf_graf indices (first %d):\n", num_indices);
    for (int i = 0; i < num_indices; ++i) {
        int idx = indices[i];
        printf("  idx=%3d: R=%3d G=%3d B=%3d\n", idx, palette[idx*COLOR_CHANNELS+0], palette[idx*COLOR_CHANNELS+1], palette[idx*COLOR_CHANNELS+2]);
    }
}
#endif // DEBUG

#ifdef DEBUG
#define CALL_DEBUG_PALETTE_RANGE(pal, start, end) debug_print_palette_range((pal), (start), (end))
#define CALL_DEBUG_PALETTE(pal) debug_print_palette((pal))
#define CALL_DEBUG_BUFFER(buf, w, h) debug_print_buffer((buf), (w), (h))
#define CALL_DEBUG_PALETTE_INDICES(pal, idx, n) debug_print_palette_indices((pal), (idx), (n))
#else
#define CALL_DEBUG_PALETTE_RANGE(pal, start, end)
#define CALL_DEBUG_PALETTE(pal)
#define CALL_DEBUG_BUFFER(buf, w, h)
#define CALL_DEBUG_PALETTE_INDICES(pal, idx, n)
#endif

void writePixel(int x, int y, int color) {
  if (x >= 0 && x <= XMax && y >= 0 && y <= YMax) {
    buf_graf[y * (XMax+1) + x] = (uint8_t)color;
  }
}

void printStrArray(char *strArray[])
{
  /* Prints an array of strings.  The array is terminated with a null string.     */
  char **strPtr;
  
  for (strPtr = strArray; **strPtr; ++strPtr)
    printf ("%s", *strPtr);
}

void setNewVideoMode(void) {}

void makeShuffledList(int *list, int listSize)
{
  int entryNum, r;
  
  for (entryNum = 0; entryNum < listSize; ++entryNum)
    list[entryNum] = -1;
  
  for (entryNum = 0; entryNum < listSize; ++entryNum)
    {
      do
	r = RANDOM(listSize);
      while (list[r] != -1);
      
      list[r] = entryNum;
    }
}

int generate_image(int imageFuncNum, uint8_t *buf_graf, int xcenter, int ycenter, int xmax, int ymax, int colormax)
{
  long x, y, dx, dy, dist, angle;
  long color;
  
  long a1, a2, a3, a4, x1,x2,x3,x4,y1,y2,y3,y4;
  
  x1 = RANDOM(40)-20;  x2 = RANDOM(40)-20;  x3 = RANDOM(40)-20;  x4 = RANDOM(40)-20;
  y1 = RANDOM(40)-20;  y2 = RANDOM(40)-20;  y3 = RANDOM(40)-20;  y4 = RANDOM(40)-20;
  
  a1 = RANDOM(ANGLE_UNIT);  a2 = RANDOM(ANGLE_UNIT);  a3 = RANDOM(ANGLE_UNIT);  a4 = RANDOM(ANGLE_UNIT);
  for (y = 0; y < ymax; ++y)
    {
      
      for (x = 0; x < xmax; ++x)
	{
	  dx = x - xcenter;
	  dy = y - ycenter;
	  
	  dist  = lut_dist (dx, dy);
	  angle = lut_angle (dx, dy);
	  
	  switch (imageFuncNum)
	    {
	      /* case -1:	Eight Arm Star -- produces weird discontinuity
                color = dist+ lut_sin(angle * (200 - dist)) / 32;
						break;
						*/
	    case 0: /* Rays plus 2D Waves */
	      color = angle + lut_sin (dist * 10) / 64 +
		lut_cos (x * ANGLE_UNIT / xmax * 2) / 32 +
		lut_cos (y * ANGLE_UNIT / ymax * 2) / 32;
	      break;
	      
	    case 1:	/* Rays plus 2D Waves */
	      color = angle + lut_sin (dist * 10) / 16 +
		lut_cos (x * ANGLE_UNIT / xmax * 2) / 8 +
		lut_cos (y * ANGLE_UNIT / ymax * 2) / 8;
	      break;
	      
	    case 2:
	      color = lut_sin (lut_dist(dx + x1, dy + y1) *  4) / 32 +
		lut_sin (lut_dist(dx + x2, dy + y2) *  8) / 32 +
		lut_sin (lut_dist(dx + x3, dy + y3) * 16) / 32 +
		lut_sin (lut_dist(dx + x4, dy + y4) * 32) / 32;
	      break;
	      
	    case 3:	/* Peacock */
	      color = angle + lut_sin (lut_dist(dx + 20, dy) * 10) / 32 +
		angle + lut_sin (lut_dist(dx - 20, dy) * 10) / 32;
	      break;
	      
	    case 4:
	      color = lut_sin (dist) / 16;
	      break;
	      
	    case 5:	/* 2D Wave + Spiral */
	      color = lut_cos (x * ANGLE_UNIT / xmax) / 8 +
		lut_cos (y * ANGLE_UNIT / ymax) / 8 +
		angle + lut_sin(dist) / 32;
	      break;
	      
	    case 6:	/* Peacock, three centers */
	      color = lut_sin (lut_dist(dx,      dy - 20) * 4) / 32+
		lut_sin (lut_dist(dx + 20, dy + 20) * 4) / 32+
		lut_sin (lut_dist(dx - 20, dy + 20) * 4) / 32;
	      break;
	      
	    case 7:	/* Peacock, three centers */
	      color = angle +
		lut_sin (lut_dist(dx,      dy - 20) * 8) / 32+
		lut_sin (lut_dist(dx + 20, dy + 20) * 8) / 32+
		lut_sin (lut_dist(dx - 20, dy + 20) * 8) / 32;
	      break;
	      
	    case 8:	/* Peacock, three centers */
	      color = lut_sin (lut_dist(dx,      dy - 20) * 12) / 32+
		lut_sin (lut_dist(dx + 20, dy + 20) * 12) / 32+
		lut_sin (lut_dist(dx - 20, dy + 20) * 12) / 32;
	      break;
	      
	    case 9:	/* Five Arm Star */
	      color = dist + lut_sin (5 * angle) / 64;
	      break;
	      
	    case 10:	/* 2D Wave */
	      color = lut_cos (x * ANGLE_UNIT / xmax * 2) / 4 +
		lut_cos (y * ANGLE_UNIT / ymax * 2) / 4;
	      break;
	      
	    case 11:	/* 2D Wave */
	      color = lut_cos (x * ANGLE_UNIT / xmax) / 8 +
		lut_cos (y * ANGLE_UNIT / ymax) / 8;
	      break;
	      
	    case 12:	/* Simple Concentric Rings */
	      color = dist;
	      break;
	      
	    case 13:	/* Simple Rays */
	      color = angle;
	      break;
	      
	    case 14:	/* Toothed Spiral Sharp */
	      color = angle + lut_sin(dist * 8)/32;
	      break;
	      
	    case 15:	/* Rings with sine */
	      color = lut_sin(dist * 4)/32;
	      break;
	      
	    case 16:	/* Rings with sine with sliding inner Rings */
	      color = dist+ lut_sin(dist * 4) / 32;
	      break;
	      
	    case 17:
	      color = lut_sin(lut_cos(2 * x * ANGLE_UNIT / xmax)) / (20 + dist)
                + lut_sin(lut_cos(2 * y * ANGLE_UNIT / ymax)) / (20 + dist);
	      break;
	      
	    case 18:	/* 2D Wave */
	      color = lut_cos(7 * x * ANGLE_UNIT / xmax)/(20 + dist) +
		lut_cos(7 * y * ANGLE_UNIT / ymax)/(20 + dist);
	      break;
	      
	    case 19:	/* 2D Wave */
	      color = lut_cos(17 * x * ANGLE_UNIT/xmax)/(20 + dist) +
		lut_cos(17 * y * ANGLE_UNIT/ymax)/(20 + dist);
	      break;
	      
	    case 20:	/* 2D Wave Interference */
	      color = lut_cos(17 * x * ANGLE_UNIT / xmax) / 32 +
		lut_cos(17 * y * ANGLE_UNIT / ymax) / 32 + dist + angle;
	      break;
	      
	    case 21:	/* 2D Wave Interference */
	      color = lut_cos(7 * x * ANGLE_UNIT / xmax) / 32 +
		lut_cos(7 * y * ANGLE_UNIT / ymax) / 32 + dist;
	      break;
	      
	    case 22:	/* 2D Wave Interference */
	      color = lut_cos( 7 * x * ANGLE_UNIT / xmax) / 32 +
		lut_cos( 7 * y * ANGLE_UNIT / ymax) / 32 +
		lut_cos(11 * x * ANGLE_UNIT / xmax) / 32 +
		lut_cos(11 * y * ANGLE_UNIT / ymax) / 32;
	      break;
	      
	    case 23:
	      color = lut_sin (angle * 7) / 32;
	      break;
	      
	    case 24:
	      color = lut_sin (lut_dist(dx + x1, dy + y1) * 2) / 12 +
		lut_sin (lut_dist(dx + x2, dy + y2) * 4) / 12 +
		lut_sin (lut_dist(dx + x3, dy + y3) * 6) / 12 +
		lut_sin (lut_dist(dx + x4, dy + y4) * 8) / 12;
	      break;
	      
	    case 25:
	      color = angle + lut_sin (lut_dist(dx + x1, dy + y1) * 2) / 16 +
		angle + lut_sin (lut_dist(dx + x2, dy + y2) * 4) / 16 +
		lut_sin (lut_dist(dx + x3, dy + y3) * 6) /  8 +
		lut_sin (lut_dist(dx + x4, dy + y4) * 8) /  8;
	      break;
	      
	    case 26:
	      color = angle + lut_sin (lut_dist(dx + x1, dy + y1) * 2) / 12 +
		angle + lut_sin (lut_dist(dx + x2, dy + y2) * 4) / 12 +
		angle + lut_sin (lut_dist(dx + x3, dy + y3) * 6) / 12 +
		angle + lut_sin (lut_dist(dx + x4, dy + y4) * 8) / 12;
	      break;
	      
	    case 27:
	      color = lut_sin (lut_dist(dx + x1, dy + y1) * 2) / 32 +
		lut_sin (lut_dist(dx + x2, dy + y2) * 4) / 32 +
		lut_sin (lut_dist(dx + x3, dy + y3) * 6) / 32 +
		lut_sin (lut_dist(dx + x4, dy + y4) * 8) / 32;
	      break;

	    case 28:	/* Random Curtain of Rain (in strong wind) */
	      if (y == 0 || x == 0)
		color = RANDOM (16);
	      else
		color = (  *(buf_graf + (xmax *  y   ) + (x-1))
			   + *(buf_graf + (xmax * (y-1)) +    x)) / 2
                  + RANDOM (16) - 8;
	      break;
	      
	    case 29:
	      if (y == 0 || x == 0)
		color = RANDOM (1024);
	      else
		color = dist/6 + (*(buf_graf + (xmax * y    ) + (x-1))
				  +  *(buf_graf + (xmax * (y-1)) +    x)) / 2
		+ RANDOM (16) - 8;
	      break;
	      
	    case 30:
	      color = lut_sin (lut_dist(dx,     dy - 20) * 4) / 32 ^
		lut_sin (lut_dist(dx + 20,dy + 20) * 4) / 32 ^
		lut_sin (lut_dist(dx - 20,dy + 20) * 4) / 32;
	      break;
	      
	    case 31:
	      color = (angle % (ANGLE_UNIT/4)) ^ dist;
	      break;
	      
	    case 32:
	      color = dy ^ dx;
	      break;
	      
	    case 33:	/* Variation on Rain */
	      if (y == 0 || x == 0)
		color = RANDOM (16);
	      else
		color = (  *(buf_graf + (xmax *  y   ) + (x-1))
			   + *(buf_graf + (xmax * (y-1)) +  x   )  ) / 2;
	      
	      color += RANDOM (2) - 1;
	      
	      if (color < 64)
		color += RANDOM (16) - 8;
	      break; 
	      
	    case 34:	/* Variation on Rain */
	      if (y == 0 || x == 0)
		color = RANDOM (16);
	      else
		color = (  *(buf_graf + (xmax *  y   ) + (x-1))
			   + *(buf_graf + (xmax * (y-1)) +  x   )  ) / 2;
	      
	      if (color < 100)
		color += RANDOM (16) - 8;
	      break;
	      
	    case 35:
	      color = angle + lut_sin(dist * 8)/32;
	      dx = x - xcenter;
	      dy = (y - ycenter)*2;
	      dist  = lut_dist (dx, dy);
          angle = lut_angle (dx, dy);
          color = (color + angle + lut_sin(dist * 8)/32) / 2;
	  break;
	  
	    case 36:
	      color = angle + lut_sin (dist * 10) / 16 +
		lut_cos (x * ANGLE_UNIT / xmax * 2) / 8 +
		lut_cos (y * ANGLE_UNIT / ymax * 2) / 8;
	      dx = x - xcenter;
	      dy = (y - ycenter)*2;
	      dist  = lut_dist (dx, dy);
	      angle = lut_angle (dx, dy);
	      color = (color + angle + lut_sin(dist * 8)/32) / 2;
	      break;
	      
	    case 37:
	      color = angle + lut_sin (dist * 10) / 16 +
		lut_cos (x * ANGLE_UNIT / xmax * 2) / 8 +
		lut_cos (y * ANGLE_UNIT / ymax * 2) / 8;
	      dx = x - xcenter;
	      dy = (y - ycenter)*2;
	      dist  = lut_dist (dx, dy);
          angle = lut_angle (dx, dy);
          color = (color + angle + lut_sin (dist * 10) / 16 +
		   lut_cos (x * ANGLE_UNIT / xmax * 2) / 8 +
		   lut_cos (y * ANGLE_UNIT / ymax * 2) / 8)  /  2;
	  break;
	  
	    case 38:
	      if (dy%2)
		{
		  dy *= 2;
		  dist  = lut_dist (dx, dy);
		  angle = lut_angle (dx, dy);
		}
	      color = angle + lut_sin(dist * 8)/32;
	      break;
	      
	    case 39:
	      color = (angle % (ANGLE_UNIT/4)) ^ dist;
	      dx = x - xcenter;
	      dy = (y - ycenter)*2;
	      dist = lut_dist (dx, dy);
	      angle = lut_angle (dx, dy);
	      color = (color +  ((angle % (ANGLE_UNIT/4)) ^ dist)) / 2;
	      break;
	      
	    case 40:
	      color = dy ^ dx;
	      dx = x - xcenter;
	      dy = (y - ycenter)*2;
	      color = (color +  (dy ^ dx)) / 2;
	      break;

	    default:
	      color = RANDOM (colormax - 1) + 1;
	      break;
	    }
	  
	  color = color % (colormax-1);
	  
	  if (color < 0)
	    color += (colormax - 1);
	  
	  ++color;
          /* color 0 is never used, so all colors are from 1 through 255 */
	  
	  *(buf_graf + (xmax * y) + x) = (uint8_t)color;
          /* Store the color in the buffer */
	}
      /* end for (y = 0; y < ymax; ++y)	*/
    }
  /* end for (x = 0; x < xmax; ++x)	*/
  
#if 0	/* For diagnosis, put palette display line at top of new image	*/
  for (x = 0; x < xmax; ++x)
    {
      color = (x <= 255) ? x : 0;
      
      for (y = 0; y < 3; ++y)
	*(buf_graf + (xmax * y) + x) = (uint8_t)color;
    }
#endif
  
  return (0);
}

// Window size defaults
int window_width = 319;
int window_height = 199;
int fullscreen = 0;

int userOptionImageFuncNum = -1; // -1 means random; can be set via --image-func argument

void parse_args(int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--width") == 0 && i+1 < argc) {
            window_width = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--height") == 0 && i+1 < argc) {
            window_height = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--fullscreen") == 0) {
            fullscreen = 1;
        } else if ((strcmp(argv[i], "--image-func") == 0 || strcmp(argv[i], "-f") == 0) && i+1 < argc) {
            userOptionImageFuncNum = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [--width N] [--height N] [--fullscreen] [--image-func N]\n", argv[0]);
            exit(0);
        }
    }
}

void print_fps(double fps) {
    printf("FPS: %.2f\n", fps);
}

// Helper for frame timing
long long current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

void handle_sigint(int sig) {
    restoreOldVideoMode();
    printf("\nExiting on Ctrl-C (SIGINT)\n");
    exit(0);
}

void main (int argc, char *argv[])
{
  int imageFuncList[NUM_IMAGE_FUNCTIONS];
  int paletteTypeNum = 0, userPaletteTypeNumOptionFlag = FALSE;
  int imageFuncListIndex=0;
  time_t ltime, mtime;

  parse_args(argc, argv);
  parse_renderer_flag(argc, argv);
  if (renderer_type == RENDERER_OPENGL) {
        renderer_gl_parse_flags(argc, argv);
        RendererGLConfig gl_cfg = { .width = window_width, .height = window_height };
        if (!renderer_gl_init(&gl_cfg)) {
            fprintf(stderr, "Failed to initialize OpenGL renderer.\n");
            exit(1);
        }
        // --- Classic Intro as OpenGL Texture ---
        // Generate classic intro image (Noah's Face)
        uint8_t *intro_buf = (uint8_t*)malloc(window_width * window_height);
        uint8_t intro_palette[PALETTE_SIZE * COLOR_CHANNELS];
        initPalArray(intro_palette, 0); // 0 = NOAHS_FACE
        writeBitmapImageToArray(intro_buf, NOAHS_FACE, window_width, window_height);
        uint32_t *intro_rgba = (uint32_t*)malloc(window_width * window_height * sizeof(uint32_t));
        convert_8bit_to_32bit(intro_buf, intro_rgba, window_width, window_height, intro_palette);
        renderer_gl_show_intro_texture(intro_rgba, window_width, window_height, 7000); // Show for 7 seconds or until keypress
        free(intro_buf);
        free(intro_rgba);
        // --- End Classic Intro ---
        renderer_gl_mainloop();
        return;
    }

  printf("Hotkeys: [p/SPACE]=Pause [n]=Next [l]=Lock [q/ESC]=Quit\n");

  RANDOMIZE();
  
  /* Default options */
  userPaletteTypeNumOptionFlag = 0;       /* User Palette option is OFF */
  userOptionImageFuncNum = -1; /* No particular functions goes first. */
  
  printf ("\nPlease wait...\n"
	  "\n\n*** Press Control-C to exit the program at any time. ***\n");
  printf ("\n\n%s\n", VERSION);
  
  graphicsinit();

  uint8_t MainPalArray [PALETTE_SIZE * COLOR_CHANNELS];
  uint8_t TargetPalArray [PALETTE_SIZE * COLOR_CHANNELS];
  initPalArray(MainPalArray, RGBW_LIGHTNING_PAL);
  CALL_DEBUG_PALETTE_RANGE(MainPalArray, 0, 8);
  CALL_DEBUG_PALETTE_RANGE(MainPalArray, 0, 19);
  CALL_DEBUG_PALETTE_RANGE(MainPalArray, 236, 255);
  initPalArray(TargetPalArray, RGBW_LIGHTNING_PAL);

  writeBitmapImageToArray(buf_graf, NOAHS_FACE, XMax, YMax);

  if (logo_time != 0) {
    /* show the logo for a while */
    static Uint32 *rgb_frame = NULL;
    if (!rgb_frame) rgb_frame = (Uint32 *)malloc(XMax * YMax * sizeof(Uint32));
    CALL_DEBUG_PALETTE_RANGE(MainPalArray, 0, 8);
    convert_8bit_to_32bit(buf_graf, rgb_frame, XMax, YMax, MainPalArray);
    SDL_UpdateTexture(texture, NULL, rgb_frame, XMax * 4);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
    ltime=time(NULL);
    mtime=ltime + logo_time;
    for(;;) {
      handle_sdl_events();
      processinput();
      if(is_running)
        rollMainPalArrayAndLoadDACRegs(MainPalArray);
      if(skip_image)
        break;
      ltime=time(NULL);
      if(ltime>mtime) 
        break; 
      // Render updated palette/animation
      CALL_DEBUG_PALETTE_RANGE(MainPalArray, 0, 8);
      convert_8bit_to_32bit(buf_graf, rgb_frame, XMax, YMax, MainPalArray);
      SDL_UpdateTexture(texture, NULL, rgb_frame, XMax * 4);
      SDL_RenderClear(renderer);
      SDL_RenderCopy(renderer, texture, NULL, NULL);
      SDL_RenderPresent(renderer);
      usleep(ROTATION_DELAY);
    }
    while(!FadeCompleteFlag) {
      handle_sdl_events();
      processinput();
      if(is_running)
        rolNFadeBlkMainPalArrayNLoadDAC(MainPalArray);
      if(skip_image)
        break;
      // Render fade
      CALL_DEBUG_PALETTE_RANGE(MainPalArray, 0, 8);
      convert_8bit_to_32bit(buf_graf, rgb_frame, XMax, YMax, MainPalArray);
      SDL_UpdateTexture(texture, NULL, rgb_frame, XMax * 4);
      SDL_RenderClear(renderer);
      SDL_RenderCopy(renderer, texture, NULL, NULL);
      SDL_RenderPresent(renderer);
      usleep(ROTATION_DELAY);
    }
    FadeCompleteFlag=!FadeCompleteFlag;
  } 
  
  skip_image = false;
  makeShuffledList(imageFuncList, NUM_IMAGE_FUNCTIONS);

  long long frame_count = 0;
  long long last_fps_time = current_time_ms();
  int frames_this_sec = 0;

  signal(SIGINT, handle_sigint);

  static Uint32 *pixel_buffer = NULL;
  static int pixel_buffer_size = 0;
  int required_size = XMax * YMax;
  if (!pixel_buffer || pixel_buffer_size != required_size) {
      if (pixel_buffer) free(pixel_buffer);
      pixel_buffer = (Uint32*)malloc(required_size * sizeof(Uint32));
      pixel_buffer_size = required_size;
  }

  for(;;) {
    handle_sdl_events();
    if (is_paused) {
        SDL_Delay(10);
        continue;
    }
    /* move to the next image */
    if (++imageFuncListIndex >= NUM_IMAGE_FUNCTIONS)
      {
	imageFuncListIndex = 0;
	makeShuffledList(imageFuncList, NUM_IMAGE_FUNCTIONS);
      }

    /* install a new image */
    int gen_ok = generate_image(
           (userOptionImageFuncNum < 0) ? 
           imageFuncList[imageFuncListIndex] : 
           userOptionImageFuncNum, 
           buf_graf, XMax/2, YMax/2, XMax, YMax, MAX_COLOR_VALUE);
    CALL_DEBUG_BUFFER(buf_graf, XMax+1, YMax+1);
    // Collect first 10 unique indices from buf_graf
    uint8_t unique_indices[MAX_UNIQUE_INDICES];
    int num_unique = 0;
    bool seen[256] = {0};
    for (int i = 0; i < MAX_UNIQUE_INDICES && i < XMax*YMax; ++i) {
        int idx = buf_graf[i];
        if (!seen[idx]) {
            unique_indices[num_unique++] = idx;
            seen[idx] = true;
            if (num_unique == MAX_UNIQUE_INDICES) break;
        }
    }
    CALL_DEBUG_PALETTE_INDICES(MainPalArray, unique_indices, num_unique);
    CALL_DEBUG_PALETTE_RANGE(MainPalArray, 0, 8);
    CALL_DEBUG_PALETTE_RANGE(MainPalArray, 0, 19);
    CALL_DEBUG_PALETTE_RANGE(MainPalArray, 236, 255);
    if (gen_ok != 0) {
        printf("[WARN] generate_image failed, drawing fallback pattern\n");
        for (int y = 0; y < YMax; ++y) for (int x = 0; x < XMax; ++x)
            buf_graf[y*XMax+x] = (x+y)%256;
    } else {
        printf("[INFO] generate_image succeeded\n");
    }
    CALL_DEBUG_BUFFER(buf_graf, XMax+1, YMax+1);
    CALL_DEBUG_PALETTE_INDICES(MainPalArray, buf_graf, 32);
    convert_8bit_to_32bit(buf_graf, pixel_buffer, XMax, YMax, MainPalArray);
    SDL_UpdateTexture(texture, NULL, pixel_buffer, XMax * sizeof(Uint32));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
    frame_count++;
    frames_this_sec++;
    long long now = current_time_ms();
    if (now - last_fps_time >= 1000) {
        print_fps(frames_this_sec * 1000.0 / (now - last_fps_time));
        frames_this_sec = 0;
        last_fps_time = now;
    }

    /* create new palette */
    paletteTypeNum = RANDOM(NUM_PALETTE_TYPES +1);
    initPalArray(TargetPalArray, paletteTypeNum);
    CALL_DEBUG_PALETTE_RANGE(TargetPalArray, 0, 19);
    CALL_DEBUG_PALETTE_RANGE(TargetPalArray, 236, 255);

    /* this is the fade in */
    while(!FadeCompleteFlag) {
      handle_sdl_events();
      if (is_paused) { SDL_Delay(10); continue; }
      processinput();
      if(is_running)
        rolNFadeMainPalAryToTargNLodDAC(MainPalArray,TargetPalArray);
      if(skip_image)
        break;
      convert_8bit_to_32bit(buf_graf, pixel_buffer, XMax, YMax, MainPalArray);
      SDL_UpdateTexture(texture, NULL, pixel_buffer, XMax * sizeof(Uint32));
      SDL_RenderClear(renderer);
      SDL_RenderCopy(renderer, texture, NULL, NULL);
      SDL_RenderPresent(renderer);
      usleep(ROTATION_DELAY);
    }

    FadeCompleteFlag=!FadeCompleteFlag;
    ltime = time(NULL);
    mtime = ltime + image_time;

    CALL_DEBUG_PALETTE_RANGE(MainPalArray, 0, 8);
    CALL_DEBUG_PALETTE_RANGE(MainPalArray, 0, 19);
    CALL_DEBUG_PALETTE_RANGE(MainPalArray, 236, 255);

    /* rotate the palette for a while */
    for(;;) {
      handle_sdl_events();
      if (is_paused) { SDL_Delay(10); continue; }
      processinput();
      if(is_running)
        rollMainPalArrayAndLoadDACRegs(MainPalArray);
      if(skip_image)
        break;
      if(new_palette_requested) {
        newpal();
        new_palette_requested = false;
      }
      ltime=time(NULL);
      if((ltime>mtime) && !palette_locked)
        break;
      convert_8bit_to_32bit(buf_graf, pixel_buffer, XMax, YMax, MainPalArray);
      SDL_UpdateTexture(texture, NULL, pixel_buffer, XMax * sizeof(Uint32));
      SDL_RenderClear(renderer);
      SDL_RenderCopy(renderer, texture, NULL, NULL);
      SDL_RenderPresent(renderer);
      usleep(ROTATION_DELAY);
    }

    /* fade out */
    while(!FadeCompleteFlag) {
      handle_sdl_events();
      if (is_paused) { SDL_Delay(10); continue; }
      processinput();
      if(is_running)
        if (fade_dir)
          rolNFadeBlkMainPalArrayNLoadDAC(MainPalArray);
        else
          rolNFadeWhtMainPalArrayNLoadDAC(MainPalArray);
      convert_8bit_to_32bit(buf_graf, pixel_buffer, XMax, YMax, MainPalArray);
      SDL_UpdateTexture(texture, NULL, pixel_buffer, XMax * sizeof(Uint32));
      SDL_RenderClear(renderer);
      SDL_RenderCopy(renderer, texture, NULL, NULL);
      SDL_RenderPresent(renderer);
      usleep(ROTATION_DELAY);
    }
    CALL_DEBUG_PALETTE_RANGE(MainPalArray, 0, 8);
    CALL_DEBUG_PALETTE_RANGE(MainPalArray, 0, 19);
    CALL_DEBUG_PALETTE_RANGE(MainPalArray, 236, 255);
    FadeCompleteFlag=!FadeCompleteFlag;
    skip_image = false;
  }
  /* exit */
  printStrArray(Command_summary_string);
  printf("%s\n", VERSION);
  restoreOldVideoMode();
}

/* ------------------------END MAIN----------------------------------------- */

void newpal()
{
  int paletteTypeNum;
  
  paletteTypeNum = RANDOM(NUM_PALETTE_TYPES +1);
  uint8_t MainPalArray [PALETTE_SIZE * COLOR_CHANNELS];
  initPalArray(MainPalArray, paletteTypeNum);
  CALL_DEBUG_PALETTE_RANGE(MainPalArray, 0, 8);
  CALL_DEBUG_PALETTE_RANGE(MainPalArray, 0, 19);
  CALL_DEBUG_PALETTE_RANGE(MainPalArray, 236, 255);
}

int checkinput()
{
  // Removed all legacy DOS/keyboard input code
  /* default case */
  return 0;
}

void processinput()
{
  switch(checkinput())
    {
    case 1:
      if(is_running)
	is_running = false;
      else
	is_running = true;
      break;
    case 2:
      skip_image = true;
      break;
    case 3:
      exit(0);
      break;
    case 4:
      new_palette_requested = true;
      break;
    case 5:
      if(palette_locked)
	palette_locked = false;
      else
	palette_locked = true;
      break;
    case 6:
      ROTATION_DELAY = ROTATION_DELAY - 5000;
      if (ROTATION_DELAY < 0)
	ROTATION_DELAY = 0;
      break;
    case 7:
      ROTATION_DELAY = ROTATION_DELAY + 5000;
      break;
    }
}

void graphicsinit(void)
{
  XMax = window_width;
  YMax = window_height;
  buf_graf = (uint8_t *)malloc(XMax * YMax); // Store image as 8-bit for algorithm

  // SDL2 window/renderer/texture
  SDL_Init(SDL_INIT_VIDEO);
  Uint32 win_flags = 0;
  if (fullscreen) win_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
  window = SDL_CreateWindow("Acidwarp", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, XMax, YMax, win_flags);
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, XMax, YMax);
}

void restoreOldVideoMode(void) {
  if (texture) SDL_DestroyTexture(texture);
  if (renderer) SDL_DestroyRenderer(renderer);
  if (window) SDL_DestroyWindow(window);
  SDL_Quit();
}
