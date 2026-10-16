/* SDL2 platform layer: PS Vita (VitaSDK) and desktop (for testing the port on a PC).
 *
 * Rendering model: the game draws in its original 640x480 space; this file maps that to the
 * screen (Vita: 960x544 -> 725x544 centred, black bars left/right).
 * The original pixel shader is replaced by:
 *   - a 160x120 light map (built in game.c) multiplied over the scene   (lights)
 *   - a tiled noise texture added over the scene                         (film grain)
 *   - sprite textures pre-desaturated in 11 steps                        (grayscale over time)
 */
#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"
#include "font_data.h"

#ifdef __vita__
#include <psp2/ctrl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/processmgr.h>
#define ASSET_DIR "app0:assets/"
#define SAVE_DIR  "ux0:data/Survive60Seconds/"
#else
#define ASSET_DIR "assets/"
#define SAVE_DIR  "./"
#endif

#define GAME_W 640
#define GAME_H 480
#define DESAT_STEPS 11

#define MUSIC_VOL_NUM 7             /* 7/10 */
#define RAIN_VOL_NUM  7

struct Tex { SDL_Texture *v[DESAT_STEPS]; int nv, w, h; };

static SDL_Window   *win;
static SDL_Renderer *ren;
static float S = 1.0f;              /* game units -> pixels */
static int   OX = 0, OUT_W = 960, OUT_H = 544;
static Tex  *font_tex;
static SDL_Texture *lm_tex, *noise_tex;

/* ------------------------------------------------------------------ helpers */
static void die(const char *msg, const char *arg)
{
    fprintf(stderr, "FATAL: %s %s (%s)\n", msg, arg ? arg : "", SDL_GetError());
    SDL_Quit();
#ifdef __vita__
    sceKernelExitProcess(1);
#endif
    exit(1);
}

static uint8_t *read_file(const char *name, const char *ext, size_t *size)
{
    char path[256];
    snprintf(path, sizeof path, ASSET_DIR "%s%s", name, ext);
    FILE *f = fopen(path, "rb");
    if (!f) die("cannot open", path);
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = (uint8_t *)malloc((size_t)n);
    if (!buf || fread(buf, 1, (size_t)n, f) != (size_t)n) die("cannot read", path);
    fclose(f);
    *size = (size_t)n;
    return buf;
}

static SDL_Rect R(float x, float y, float w, float h)       /* game units -> screen pixels */
{
    int x0 = (int)floorf(x * S + 0.5f), y0 = (int)floorf(y * S + 0.5f);
    int x1 = (int)floorf((x + w) * S + 0.5f), y1 = (int)floorf((y + h) * S + 0.5f);
    SDL_Rect r = { OX + x0, y0, x1 - x0, y1 - y0 };
    return r;
}

/* ------------------------------------------------------------------ textures */
static SDL_Texture *make_tex(const uint8_t *rgba, int w, int h, SDL_BlendMode bm, const char *filter)
{
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, filter);
    SDL_Texture *t = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);
    if (!t) die("SDL_CreateTexture failed", NULL);
    SDL_UpdateTexture(t, NULL, rgba, w * 4);
    SDL_SetTextureBlendMode(t, bm);
    return t;
}

static Tex *load_tex(const char *name, int variants)
{
    size_t sz;
    uint8_t *raw = read_file(name, ".rgba", &sz);
    uint32_t w, h;
    memcpy(&w, raw, 4); memcpy(&h, raw + 4, 4);
    uint8_t *pix = raw + 8;
    Tex *t = (Tex *)calloc(1, sizeof *t);
    t->w = (int)w; t->h = (int)h;
    t->nv = variants ? DESAT_STEPS : 1;
    uint8_t *tmp = (uint8_t *)malloc((size_t)w * h * 4);
    for (int lvl = 0; lvl < t->nv; lvl++) {
        float f = lvl / 10.0f;
        for (size_t i = 0; i < (size_t)w * h; i++) {
            float r = pix[i * 4], g = pix[i * 4 + 1], b = pix[i * 4 + 2];
            float avg = 0.21f * r + 0.72f * g + 0.07f * b;
            tmp[i * 4]     = (uint8_t)(r + (avg - r) * f + 0.5f);
            tmp[i * 4 + 1] = (uint8_t)(g + (avg - g) * f + 0.5f);
            tmp[i * 4 + 2] = (uint8_t)(b + (avg - b) * f + 0.5f);
            tmp[i * 4 + 3] = pix[i * 4 + 3];
        }
        t->v[lvl] = make_tex(lvl == 0 ? pix : tmp, (int)w, (int)h, SDL_BLENDMODE_BLEND, "linear");
    }
    free(tmp); free(raw);
    return t;
}

Tex *plat_load_tex(const char *name, int variants) { return load_tex(name, variants); }
int  plat_tex_w(const Tex *t) { return t->w; }
int  plat_tex_h(const Tex *t) { return t->h; }

void plat_draw_tex(const Tex *t, float x, float y, int lvl)
{
    if (lvl >= t->nv) lvl = t->nv - 1;
    if (lvl < 0) lvl = 0;
    SDL_Rect d = R(x, y, (float)t->w, (float)t->h);
    SDL_RenderCopy(ren, t->v[lvl], NULL, &d);
}

void plat_fill_rect(float x, float y, float w, float h, float r, float g, float b)
{
    SDL_Rect d = R(x, y, w, h);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(ren, (Uint8)(r * 255 + 0.5f), (Uint8)(g * 255 + 0.5f), (Uint8)(b * 255 + 0.5f), 255);
    SDL_RenderFillRect(ren, &d);
}

void plat_draw_rain(const Tex *t, float u, float v)
{
    /* texture is stretched over the whole 640x480 area and wraps; scroll = (u,v) in texture widths */
    float fx = -u - floorf(-u), fy = -v - floorf(-v);
    float dx = fx * GAME_W, dy = fy * GAME_H;
    for (int ix = 0; ix < 2; ix++)
        for (int iy = 0; iy < 2; iy++) {
            SDL_Rect d = R(dx - ix * GAME_W, dy - iy * GAME_H, GAME_W, GAME_H);
            SDL_RenderCopy(ren, t->v[0], NULL, &d);
        }
}

void plat_apply_lightmap(const uint8_t *rgba)
{
    SDL_UpdateTexture(lm_tex, NULL, rgba, LM_W * 4);
    SDL_Rect d = R(0, 0, GAME_W, GAME_H);
    SDL_RenderCopy(ren, lm_tex, NULL, &d);
}

void plat_apply_grain(float time)
{
    uint32_t h = (uint32_t)(time * 60.0f) * 2654435761u;
    int ox = (int)((h >> 8) & 255), oy = (int)((h >> 18) & 255);
    int vw = (int)floorf(GAME_W * S + 0.5f);
    for (int y = -oy; y < OUT_H; y += 256)
        for (int x = OX - ox; x < OX + vw; x += 256) {
            SDL_Rect d = { x, y, 256, 256 };
            SDL_RenderCopy(ren, noise_tex, NULL, &d);
        }
}

/* ------------------------------------------------------------------ text (bitmap font atlas) */
void plat_text_size(const char *s, float *w, float *h)
{
    float cur = 0, maxw = 0; int lines = 1;
    for (; *s; s++) {
        if (*s == '\n') { if (cur > maxw) maxw = cur; cur = 0; lines++; }
        else if (*s >= 32 && *s <= 126) cur += font_adv[*s - 32];
    }
    if (cur > maxw) maxw = cur;
    *w = maxw; *h = (float)(lines * FONT_LINE_H);
}

void plat_draw_text(const char *s, float x, float y)
{
    float pen = x, line = y;
    for (; *s; s++) {
        if (*s == '\n') { pen = x; line += FONT_LINE_H; continue; }
        if (*s < 32 || *s > 126) continue;
        int idx = *s - 32;
        SDL_Rect src = { (idx % FONT_COLS) * FONT_CELL, (idx / FONT_COLS) * FONT_CELL, FONT_CELL, FONT_CELL };
        SDL_Rect d = R(pen - FONT_PAD_X, line, FONT_CELL, FONT_CELL);
        SDL_RenderCopy(ren, font_tex->v[0], &src, &d);
        pen += font_adv[idx];
    }
}

/* ------------------------------------------------------------------ audio (tiny 2-voice looping mixer) */
static int16_t *pcm_music, *pcm_rain;
static size_t n_music, n_rain, pos_music, pos_rain;
static volatile int rain_on;
static SDL_AudioDeviceID audio_dev;

static void audio_cb(void *ud, Uint8 *stream, int len)
{
    (void)ud;
    int16_t *out = (int16_t *)stream;
    int n = len / 2;
    for (int i = 0; i < n; i++) {
        int s = pcm_music[pos_music] * MUSIC_VOL_NUM / 10;
        if (++pos_music >= n_music) pos_music = 0;
        if (rain_on) {
            s += pcm_rain[pos_rain] * RAIN_VOL_NUM / 10;
            if (++pos_rain >= n_rain) pos_rain = 0;
        }
        out[i] = (int16_t)(s > 32767 ? 32767 : (s < -32768 ? -32768 : s));
    }
}

static void audio_init(void)
{
    size_t sz;
    pcm_music = (int16_t *)read_file("horror", ".pcm", &sz); n_music = sz / 2;
    pcm_rain  = (int16_t *)read_file("rain", ".pcm", &sz);   n_rain = sz / 2;
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = 22050; want.format = AUDIO_S16SYS; want.channels = 1; want.samples = 1024;
    want.callback = audio_cb;
    audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);   /* SDL converts to the device format */
    if (!audio_dev) { fprintf(stderr, "audio disabled: %s\n", SDL_GetError()); return; }
    SDL_PauseAudioDevice(audio_dev, 0);
}

void plat_rain_sound(int on)
{
    if (audio_dev) SDL_LockAudioDevice(audio_dev);
    if (on && !rain_on) pos_rain = 0;
    rain_on = on;
    if (audio_dev) SDL_UnlockAudioDevice(audio_dev);
}

/* ------------------------------------------------------------------ save data */
int plat_load_highscore(void)
{
    int v = 0;
    FILE *f = fopen(SAVE_DIR "saves.txt", "r");
    if (f) { if (fscanf(f, "%d", &v) != 1) v = 0; fclose(f); }
    return v;
}

void plat_save_highscore(int score)
{
#ifdef __vita__
    sceIoMkdir("ux0:data", 0777);
    sceIoMkdir(SAVE_DIR, 0777);
#endif
    FILE *f = fopen(SAVE_DIR "saves.txt", "w");
    if (f) { fprintf(f, "%d\n", score); fclose(f); }
}

/* ------------------------------------------------------------------ input */
static int quit_requested;

static void read_input(PadState *p, int *pause_pressed)
{
    static int prev_pause = 0, prev_quit = 0;
    int pause_now = 0, quit_now = 0;
    memset(p, 0, sizeof *p);
#ifdef __vita__
    SceCtrlData pad;
    sceCtrlPeekBufferPositive(0, &pad, 1);
    unsigned b = pad.buttons;
    p->up     = (b & SCE_CTRL_UP)    || pad.ly < 128 - 48;
    p->down   = (b & SCE_CTRL_DOWN)  || pad.ly > 128 + 48;
    p->left   = (b & SCE_CTRL_LEFT)  || pad.lx < 128 - 48;
    p->right  = (b & SCE_CTRL_RIGHT) || pad.lx > 128 + 48;
    p->dash   = (b & (SCE_CTRL_SQUARE | SCE_CTRL_LTRIGGER | SCE_CTRL_RTRIGGER)) != 0;
    p->plant  = (b & SCE_CTRL_CROSS) != 0;
    p->accept = (b & SCE_CTRL_CIRCLE) != 0;
    pause_now = (b & SCE_CTRL_START) != 0;
    quit_now  = (b & SCE_CTRL_SELECT) != 0;
#else
    const Uint8 *k = SDL_GetKeyboardState(NULL);
    p->up = k[SDL_SCANCODE_UP] || k[SDL_SCANCODE_W];
    p->down = k[SDL_SCANCODE_DOWN] || k[SDL_SCANCODE_S];
    p->left = k[SDL_SCANCODE_LEFT] || k[SDL_SCANCODE_A];
    p->right = k[SDL_SCANCODE_RIGHT] || k[SDL_SCANCODE_D];
    p->dash = k[SDL_SCANCODE_Z];
    p->plant = k[SDL_SCANCODE_X];
    p->accept = k[SDL_SCANCODE_C] || k[SDL_SCANCODE_RETURN] || k[SDL_SCANCODE_SPACE];
    pause_now = k[SDL_SCANCODE_P];
    quit_now = k[SDL_SCANCODE_ESCAPE];
#endif
    *pause_pressed = pause_now && !prev_pause;
    prev_pause = pause_now;
    if (quit_now && !prev_quit) quit_requested = 1;
    prev_quit = quit_now;
}

/* ------------------------------------------------------------------ main */
int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) die("SDL_Init failed", NULL);
#ifdef __vita__
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
#endif
    win = SDL_CreateWindow("Survive 60 Seconds", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           960, 544, SDL_WINDOW_SHOWN);
    if (!win) die("SDL_CreateWindow failed", NULL);
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, 0);
    if (!ren) die("SDL_CreateRenderer failed", NULL);

    SDL_GetRendererOutputSize(ren, &OUT_W, &OUT_H);
    S = (float)OUT_H / GAME_H;
    if (S * GAME_W > OUT_W) S = (float)OUT_W / GAME_W;
    OX = (OUT_W - (int)floorf(GAME_W * S + 0.5f)) / 2;

    /* font atlas, light map, grain texture */
    font_tex = load_tex("font", 0);
    {
        uint8_t zero[LM_W * LM_H * 4];
        memset(zero, 255, sizeof zero);
        lm_tex = make_tex(zero, LM_W, LM_H, SDL_BLENDMODE_MOD, "linear");
        static uint8_t noise[256 * 256 * 4];
        uint32_t s = 0x1234567u;
        for (int i = 0; i < 256 * 256; i++) {
            s = s * 1664525u + 1013904223u;
            uint8_t g = (uint8_t)((s >> 24) % 17);          /* 0..16 of 255, added on top */
            noise[i * 4] = noise[i * 4 + 1] = noise[i * 4 + 2] = g; noise[i * 4 + 3] = 255;
        }
        noise_tex = make_tex(noise, 256, 256, SDL_BLENDMODE_ADD, "nearest");
    }

    audio_init();
    game_init((uint32_t)SDL_GetTicks() | 1u);

    Uint64 last = SDL_GetPerformanceCounter();
    const double freq = (double)SDL_GetPerformanceFrequency();
    while (!quit_requested) {
        SDL_Event e;
        while (SDL_PollEvent(&e))
            if (e.type == SDL_QUIT) quit_requested = 1;

        Uint64 now = SDL_GetPerformanceCounter();
        float dt = (float)((double)(now - last) / freq);
        last = now;

        PadState pad; int pause_pressed;
        read_input(&pad, &pause_pressed);
        game_update(dt, &pad, pause_pressed);

        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        game_draw();

        /* black bars (the original draws these too) */
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        int vw = (int)floorf(GAME_W * S + 0.5f);
        SDL_Rect left = { 0, 0, OX, OUT_H };
        SDL_Rect right = { OX + vw, 0, OUT_W - (OX + vw), OUT_H };
        if (OX > 0) { SDL_RenderFillRect(ren, &left); SDL_RenderFillRect(ren, &right); }
        SDL_RenderPresent(ren);
    }

    if (audio_dev) SDL_CloseAudioDevice(audio_dev);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
#ifdef __vita__
    sceKernelExitProcess(0);
#endif
    return 0;
}
