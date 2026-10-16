/* Headless test: includes game.c to reach internals, stubs the platform layer. */
#include "../src/game.c"
#include <assert.h>
#include <stdlib.h>

struct Tex { int w, h; };
static struct Tex texs[1024]; static int ntex;
static int rain_on, draws, lm_calls, saved_hs = -1;
Tex *plat_load_tex(const char *n, int v) {
    (void)v; struct Tex *t = &texs[ntex++];
    t->w = 24; t->h = 24;
    if (!strcmp(n,"player1")||!strcmp(n,"gmonster1")||!strcmp(n,"rmonster1")) t->w = 27;
    if (!strcmp(n,"light")) t->w = t->h = 32;
    if (!strcmp(n,"banner")||!strcmp(n,"title")) { t->w = 640; t->h = 480; }
    if (!strcmp(n,"raintex")) t->w = t->h = 512;
    return t;
}
int plat_tex_w(const Tex *t){return t->w;} int plat_tex_h(const Tex *t){return t->h;}
void plat_draw_tex(const Tex*t,float x,float y,int l){(void)t;(void)x;(void)y;assert(l>=0&&l<=10);draws++;}
void plat_fill_rect(float x,float y,float w,float h,float r,float g,float b){(void)x;(void)y;(void)w;(void)h;assert(r>=0&&r<=1&&g>=0&&g<=1&&b>=0&&b<=1);}
void plat_draw_rain(const Tex*t,float u,float v){(void)t;(void)u;(void)v;}
void plat_apply_lightmap(const uint8_t*p){(void)p;lm_calls++;}
void plat_apply_grain(float t){(void)t;}
void plat_text_size(const char*s,float*w,float*h){*w=(float)strlen(s)*14;*h=28;}
void plat_draw_text(const char*s,float x,float y){(void)s;(void)x;(void)y;}
void plat_rain_sound(int on){rain_on=on;}
int plat_load_highscore(void){return 0;}
void plat_save_highscore(int s){saved_hs=s;}

static void run(float seconds, PadState p, int *dead_at) {
    int frames = (int)(seconds * 60);
    for (int i = 0; i < frames; i++) {
        game_update(1.0f/60, &p, 0);
        game_draw();
        assert(isfinite(player.x) && isfinite(player.y));
        assert(n_obj <= MAX_OBJ && n_bg <= MAX_IMG);
        for (int k = 0; k < n_obj; k++) assert(isfinite(objs[k]->x) && isfinite(objs[k]->y));
        if (gameover && dead_at && *dead_at < 0) *dead_at = i;
    }
}

int main(void) {
    game_init(12345);
    PadState none = {0}, accept = {0}; accept.accept = 1;

    /* banner -> game after 4s (or on accept after 1.5s) */
    run(1.0f, none, NULL);           assert(gamestate == 0);
    run(4.0f, none, NULL);           assert(gamestate == 1);
    printf("banner ok; player at (%.0f,%.0f) objs=%d\n", player.x, player.y, n_obj);
    assert(fabsf(player.x - 320) < 1 && fabsf(player.y - 240) < 1);

    /* skipping banner with accept */
    game_init(7); run(1.7f, accept, NULL); assert(gamestate == 1);

    /* idle player must eventually die; no monsters before ~5s */
    game_init(99); run(1.6f, accept, NULL); assert(gamestate == 1);
    int dead = -1, mons_before = 0;
    run(3.0f, none, &dead); mons_before = n_obj - 1; assert(dead < 0 && mons_before == 0);
    run(120, none, &dead);
    printf("idle player died after %.1fs (frame %d), objs=%d, gameover=%d paused=%d\n",
           timer, dead, n_obj, gameover, paused);
    assert(gameover && paused);

    /* retry */
    run(0.2f, accept, NULL);
    assert(!gameover && !paused && timer < 1 && linv == 6);
    printf("retry ok: timer=%.2f objs=%d\n", timer, n_obj);

    /* moving + dashing + planting: exercise code, check bounds and light bookkeeping */
    PadState mv = {0}; mv.right = 1; mv.dash = 1; mv.plant = 1; mv.up = 1;
    int minlinv = 99, maxbg = 0;
    for (int i = 0; i < 60*30 && !gameover; i++) {
        game_update(1.0f/60, &mv, 0); game_draw();
        if (linv < minlinv) minlinv = linv;
        if (n_bg > maxbg) maxbg = n_bg;
        if ((i / 90) % 2) { mv.right = 0; mv.left = 1; } else { mv.left = 0; mv.right = 1; }
        assert(player.x >= -300 && player.x <= 1000);
    }
    printf("move/plant: survived %.1fs, linv min=%d, max lights=%d, objs=%d\n", timer, minlinv, maxbg, n_obj);
    assert(minlinv >= 0 && minlinv <= 6);

    /* planting while standing still, with a monster-heavy late game: rain + light expiry */
    game_init(2024); run(4.1f, accept, NULL);
    PadState plant = {0}; plant.plant = 1;
    int lights_seen = 0, deaths = 0, rain_seen = 0;
    for (int i = 0; i < 60*100; i++) {
        PadState p = plant; if (gameover) p = accept;
        int was = gameover;
        game_update(1.0f/60, &p, 0); game_draw();
        if (gameover && !was) deaths++;
        if (n_bg > lights_seen) lights_seen = n_bg;
        if (rain_on) rain_seen = 1;
    }
    printf("planter: deaths=%d max bg=%d rain_seen=%d draws=%d lm=%d\n", deaths, lights_seen, rain_seen, draws, lm_calls);
    assert(linv >= 0 && linv <= 12);

    /* bump sanity: slide along a box, and tunnelling at dash speed */
    game_init(1); map_reset_empty(0);
    Obj *a = new_obj(), *b = new_obj();
    init_sprite(a, K_PLAYER, T_player); init_sprite(b, K_MON2, T_gm);
    obj_at(a, 100, 100); obj_at(b, 130, 100);
    objs[n_obj++] = a; objs[n_obj++] = b; add_bump();
    Col cols[MAX_COLS]; double ax, ay;
    int nc = bump_move(a, a->x + a->hox + 200, a->y + a->hoy, cols, &ax, &ay);   /* straight into b */
    printf("bump: ncols=%d stop x=%.2f (b box starts at %.2f, a width %.0f)\n", nc, ax, b->bx, a->bw);
    assert(nc == 1 && fabs(ax + a->bw - b->bx) < 1e-6 && fabs(ay - a->by) < 1e-9);
    printf("ALL OK\n");
    return 0;
}
