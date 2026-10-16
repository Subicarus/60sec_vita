/* Platform interface used by the game logic (game.c).
 * Implemented by main.c (SDL2 on PS Vita / desktop) and by test/stub.c (headless tests).
 * All coordinates are in the original game's 640x480 space. */
#pragma once
#include <stdint.h>

#define LM_W 160            /* light-map resolution (each cell = 4x4 game pixels) */
#define LM_H 120

typedef struct Tex Tex;

typedef struct {            /* held state, like love.keyboard.isDown */
    int up, down, left, right;
    int dash;               /* original: Z */
    int plant;              /* original: X */
    int accept;             /* original: C / Enter / Space */
} PadState;

Tex  *plat_load_tex(const char *name, int with_desat_variants);
int   plat_tex_w(const Tex *t);
int   plat_tex_h(const Tex *t);
void  plat_draw_tex(const Tex *t, float x, float y, int desat_level /*0..10*/);
void  plat_fill_rect(float x, float y, float w, float h, float r, float g, float b);
void  plat_draw_rain(const Tex *t, float u, float v);     /* tiled, scrolling full-screen overlay */
void  plat_apply_lightmap(const uint8_t *rgba);           /* LM_W*LM_H*4, multiplied over the scene */
void  plat_apply_grain(float time);                       /* film grain over the scene */
void  plat_text_size(const char *s, float *w, float *h);
void  plat_draw_text(const char *s, float x, float y);
void  plat_rain_sound(int on);
int   plat_load_highscore(void);
void  plat_save_highscore(int score);
