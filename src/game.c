/* SURVIVE 60 SECONDS - C port of the LOVE2D game by Kavirajar B. (Perspectilt Studios).
 *
 * This file is a line-by-line port of main.lua / map.lua / sprite.lua / gameobjects.lua /
 * image.lua and of the parts of bump.lua (AABB swept collision with "slide" response)
 * that the game uses. Quirks of the original (e.g. the dash speed ramp, skipping the next
 * list element when something is deleted mid-iteration) are kept on purpose.
 *
 * The pixel shader (lights, film grain, desaturation) is replaced by a CPU-built light map,
 * see build_lightmap(); everything else maps 1:1.
 */
#include "game.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define W 640
#define H 480
#define MAX_OBJ 512
#define MAX_IMG 96
#define MAX_COLS 64

/* ------------------------------------------------------------------ types */
enum { K_PLAYER, K_MON1, K_MON2 };

typedef struct { float x, y; } Vec;

typedef struct Obj {
    int alive, kind;
    float x, y, mapx, mapy, z;          /* x,y = sprite centre (like the Lua code) */
    float w, h;                         /* size of the current frame */
    Tex *idle_img, *walk_img[2];
    Tex *img;
    int walking;                        /* animation.NAME == 'walk' */
    float frame;                        /* 1-based like Lua */
    int has_hitbox;
    float hox, hoy, hw, hh;
    int in_world;                       /* bump world membership */
    double bx, by, bw, bh;              /* bump rect */
    float speed, boost, plant;
    Vec dir;
    int vis;                            /* bump "visited" scratch flag */
} Obj;

typedef struct Img {                    /* images_bg entries */
    int alive;
    int is_splash, is_light;
    Tex *tex;
    float x, y, w, h;
    double start;
    int red;                            /* turned red by a monster touching it */
} Img;

typedef struct {
    Obj *other;
    double ti, nx, ny, tx, ty, mx, my;
    int overlaps;
    double ix, iy, iw, ih, ox, oy, ow, oh;
} Col;

/* ------------------------------------------------------------------ state */
static Tex *T_player[2], *T_gm[2], *T_rm[2], *T_light, *T_banner, *T_title, *T_rain;

static Obj  opool[MAX_OBJ];
static Img  ipool[MAX_IMG];
static Obj *objs[MAX_OBJ];  static int n_obj;
static Img *bg[MAX_IMG];    static int n_bg;

static Obj  player;                     /* persistent, like the Lua global */
static Img *splash;

static float map_time;                  /* Map.Time (shader "time") */
static int   map_boo = -1;
static int   map_is_banner;

static PadState in;
static int   gamestate;                 /* 0 = banner, 1 = game */
static int   gameover, paused;
static float timer;
static int   linv = 6;
static int   highscore;
static double clock_s;                  /* love.timer.getTime() */
static float mesh_u, mesh_v;            /* rain scroll */
static int   rain_playing;
static char  text2[64];
static uint32_t rng = 2463534242u;

/* ------------------------------------------------------------------ helpers */
static int rnd(int lo, int hi)          /* love.math.random(lo, hi) */
{
    if (hi < lo) hi = lo;
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return lo + (int)(rng % (uint32_t)(hi - lo + 1));
}
static float vmag(Vec v) { return sqrtf(v.x * v.x + v.y * v.y); }
static void  vnorm(Vec *v) { float m = vmag(*v); v->x /= m; v->y /= m; }

/* ------------------------------------------------------------------ bump.lua (slide) */
#define DELTA 1e-10
static double sgn(double x) { return x > 0 ? 1 : (x == 0 ? 0 : -1); }
static double nearest(double x, double a, double b) { return fabs(a - x) < fabs(b - x) ? a : b; }

static int seg_indices(double x, double y, double w, double h,
                       double x1, double y1, double x2, double y2,
                       double *ti1, double *ti2, double *nx1, double *ny1, double *nx2, double *ny2)
{
    double t1 = *ti1, t2 = *ti2;
    double dx = x2 - x1, dy = y2 - y1;
    double nx, ny, p, q, r;
    *nx1 = *ny1 = *nx2 = *ny2 = 0;
    for (int side = 1; side <= 4; side++) {
        if (side == 1)      { nx = -1; ny = 0;  p = -dx; q = x1 - x; }
        else if (side == 2) { nx = 1;  ny = 0;  p = dx;  q = x + w - x1; }
        else if (side == 3) { nx = 0;  ny = -1; p = -dy; q = y1 - y; }
        else                { nx = 0;  ny = 1;  p = dy;  q = y + h - y1; }
        if (p == 0) {
            if (q <= 0) return 0;
        } else {
            r = q / p;
            if (p < 0) {
                if (r > t2) return 0;
                else if (r > t1) { t1 = r; *nx1 = nx; *ny1 = ny; }
            } else {
                if (r < t1) return 0;
                else if (r < t2) { t2 = r; *nx2 = nx; *ny2 = ny; }
            }
        }
    }
    *ti1 = t1; *ti2 = t2;
    return 1;
}

static int detect(double x1, double y1, double w1, double h1, const Obj *other,
                  double goalX, double goalY, Col *c)
{
    double x2 = other->bx, y2 = other->by, w2 = other->bw, h2 = other->bh;
    double dx = goalX - x1, dy = goalY - y1;
    double x = x2 - x1 - w1, y = y2 - y1 - h1, w = w1 + w2, h = h1 + h2;
    int overlaps = 0, have_ti = 0;
    double ti = 0, nx = 0, ny = 0;

    if (0 - x > DELTA && 0 - y > DELTA && x + w - 0 > DELTA && y + h - 0 > DELTA) {
        double px = nearest(0, x, x + w), py = nearest(0, y, y + h);
        double wi = fmin(w1, fabs(px)), hi = fmin(h1, fabs(py));
        ti = -wi * hi; overlaps = 1; have_ti = 1;
    } else {
        double t1 = -INFINITY, t2 = INFINITY, nx1, ny1, nx2, ny2;
        if (seg_indices(x, y, w, h, 0, 0, dx, dy, &t1, &t2, &nx1, &ny1, &nx2, &ny2)) {
            if (t1 < 1 && fabs(t1 - t2) >= DELTA && (0 < t1 + DELTA || (0 == t1 && t2 > 0))) {
                ti = t1; nx = nx1; ny = ny1; overlaps = 0; have_ti = 1;
            }
        }
    }
    if (!have_ti) return 0;

    double tx, ty;
    if (overlaps) {
        if (dx == 0 && dy == 0) {
            double px = nearest(0, x, x + w), py = nearest(0, y, y + h);
            if (fabs(px) < fabs(py)) py = 0; else px = 0;
            nx = sgn(px); ny = sgn(py);
            tx = x1 + px; ty = y1 + py;
        } else {
            double t1 = -INFINITY, t2 = 1, nx1, ny1, nx2, ny2;
            if (!seg_indices(x, y, w, h, 0, 0, dx, dy, &t1, &t2, &nx1, &ny1, &nx2, &ny2)) return 0;
            nx = nx1; ny = ny1;
            tx = x1 + dx * t1; ty = y1 + dy * t1;
        }
    } else {
        tx = x1 + dx * ti; ty = y1 + dy * ti;
    }
    c->other = (Obj *)other; c->ti = ti; c->nx = nx; c->ny = ny; c->tx = tx; c->ty = ty;
    c->mx = dx; c->my = dy; c->overlaps = overlaps;
    c->ix = x1; c->iy = y1; c->iw = w1; c->ih = h1; c->ox = x2; c->oy = y2; c->ow = w2; c->oh = h2;
    return 1;
}

static double sqdist(double x1, double y1, double w1, double h1, double x2, double y2, double w2, double h2)
{
    double dx = x1 - x2 + (w1 - w2) / 2, dy = y1 - y2 + (h1 - h2) / 2;
    return dx * dx + dy * dy;
}

static int col_less(const Col *a, const Col *b)
{
    if (a->ti == b->ti)
        return sqdist(a->ix, a->iy, a->iw, a->ih, a->ox, a->oy, a->ow, a->oh) <
               sqdist(b->ix, b->iy, b->iw, b->ih, b->ox, b->oy, b->ow, b->oh);
    return a->ti < b->ti;
}

/* world:project - all collisions of `item` (at x,y,w,h) moving to goal, sorted, visited excluded */
static int project(Obj *item, double x, double y, double w, double h, double gx, double gy, Col *out)
{
    int n = 0;
    for (int i = 0; i < n_obj; i++) {
        Obj *o = objs[i];
        if (!o->in_world || o->vis || o == item) continue;
        Col c;
        if (detect(x, y, w, h, o, gx, gy, &c) && n < MAX_COLS) out[n++] = c;
    }
    for (int i = 1; i < n; i++) {                     /* insertion sort */
        Col k = out[i]; int j = i - 1;
        while (j >= 0 && col_less(&k, &out[j])) { out[j + 1] = out[j]; j--; }
        out[j + 1] = k;
    }
    return n;
}

/* world:move(item, goalX, goalY) with the default 'slide' filter.
 * Returns the number of collisions; *ax,*ay = resulting top-left of the item's box. */
static int bump_move(Obj *item, double goalX, double goalY, Col *cols, double *ax, double *ay)
{
    Col proj[MAX_COLS];
    int ncols = 0, guard = 0;
    for (int i = 0; i < n_obj; i++) objs[i]->vis = 0;
    item->vis = 1;
    double x = item->bx, y = item->by, w = item->bw, h = item->bh;
    int np = project(item, x, y, w, h, goalX, goalY, proj);
    while (np > 0 && guard++ < 32) {
        Col col = proj[0];
        if (ncols < MAX_COLS) cols[ncols++] = col;
        col.other->vis = 1;
        if (col.mx != 0 || col.my != 0) {              /* slide response */
            if (col.nx != 0) goalX = col.tx; else goalY = col.ty;
        }
        np = project(item, col.tx, col.ty, w, h, goalX, goalY, proj);
    }
    for (int i = 0; i < n_obj; i++) objs[i]->vis = 0;
    item->bx = goalX; item->by = goalY;
    *ax = goalX; *ay = goalY;
    return ncols;
}

/* ------------------------------------------------------------------ map helpers */
static void add_bump(void)
{
    for (int i = 0; i < n_obj; i++) {
        Obj *v = objs[i];
        v->in_world = 0;
        if (v->has_hitbox) {
            v->in_world = 1;
            v->bx = v->x + v->hox; v->by = v->y + v->hoy; v->bw = v->hw; v->bh = v->hh;
        }
    }
}
static void delete_bump(void)
{
    for (int i = 0; i < n_obj; i++) objs[i]->in_world = 0;
}
static void bump_reset(void) { delete_bump(); add_bump(); }

static void delete_obj(Obj *o)          /* myMap:delete(obj) for map objects */
{
    o->has_hitbox = 0; o->in_world = 0;
    for (int i = 0; i < n_obj; i++) {
        if (objs[i] == o) {
            memmove(&objs[i], &objs[i + 1], (size_t)(n_obj - i - 1) * sizeof(Obj *));
            n_obj--;
            o->x = 0;
            if (o != &player) o->alive = 0;
            break;
        }
    }
}

static void delete_img(Img *l)          /* myMap:delete(obj) for images_bg entries */
{
    for (int i = 0; i < n_bg; i++) {
        if (bg[i] == l) {
            memmove(&bg[i], &bg[i + 1], (size_t)(n_bg - i - 1) * sizeof(Img *));
            n_bg--;
            l->alive = 0;
            break;
        }
    }
}

static Img *new_img(void)
{
    for (int i = 0; i < MAX_IMG; i++)
        if (!ipool[i].alive) { memset(&ipool[i], 0, sizeof(Img)); ipool[i].alive = 1; return &ipool[i]; }
    return NULL;
}

static Obj *new_obj(void)
{
    for (int i = 0; i < MAX_OBJ; i++)
        if (!opool[i].alive) { memset(&opool[i], 0, sizeof(Obj)); opool[i].alive = 1; return &opool[i]; }
    return NULL;
}

/* ------------------------------------------------------------------ sprites */
static void set_image(Obj *o, Tex *t)
{
    o->img = t; o->w = (float)plat_tex_w(t); o->h = (float)plat_tex_h(t);
}
static void play(Obj *o, int walk)
{
    o->walking = walk;
    o->frame = 1;
    set_image(o, walk ? o->walk_img[0] : o->idle_img);
}
static void update_anim(Obj *o, float dt)
{
    o->z = o->y + o->h;
    if (o->walking) {                               /* only 'walk' has a DELAY (0.2s) and loops */
        o->frame += dt / 0.2f;
        if (o->frame >= 3) o->frame = 1;
        set_image(o, o->walk_img[(int)floorf(o->frame) - 1]);
    }
}
static void obj_at(Obj *o, float x, float y) { o->mapx = o->x = x; o->mapy = o->y = y; }

static void init_sprite(Obj *o, int kind, Tex *a[2])
{
    o->kind = kind;
    o->idle_img = a[0];
    o->walk_img[0] = a[1];
    o->walk_img[1] = a[0];
    play(o, 1);
    o->has_hitbox = 1;
    o->hox = 0; o->hoy = o->h / 2; o->hw = o->w; o->hh = o->h / 2;
}

/* ------------------------------------------------------------------ images / lights */
static Img *add_light(float x, float y)
{
    Img *l = new_img();
    if (!l || n_bg >= MAX_IMG) return NULL;
    l->is_light = 1; l->tex = T_light;
    l->w = (float)plat_tex_w(T_light); l->h = (float)plat_tex_h(T_light);
    l->x = x; l->y = y; l->start = clock_s;
    bg[n_bg++] = l;
    return l;
}
static float light_power(const Img *l) { return (float)(60.0 * (clock_s - l->start + 1.0) / 8.0); }

/* light.light.color(): may delete the light (and refund a slot). returns rgb */
static void light_color(Img *l, float rgb[3])
{
    if (light_power(l) > 150) { linv++; delete_img(l); }
    if (l->red) {
        double age = clock_s - l->start; if (age < 1e-3) age = 1e-3;
        rgb[0] = 1; rgb[1] = rgb[2] = (float)(0.2 / age);
    } else { rgb[0] = rgb[1] = rgb[2] = 1; }
}

/* sprite:collided_group('light') for monsters */
static Img *collided_light(const Obj *m)
{
    float l0 = m->x + m->hox, t0 = m->y + m->hoy, r0 = l0 + m->hw, b0 = t0 + m->hh;
    for (int i = 0; i < n_bg; i++) {
        Img *v = bg[i];
        if (!v->is_light) continue;
        float l1 = v->x, t1 = v->y, r1 = v->x + v->w, b1 = v->y + v->h;   /* image.Rect uses x,y as top-left */
        if (l0 < r1 && t0 < b1 && r0 > l1 && b0 > t1) return v;
    }
    return NULL;
}

/* ------------------------------------------------------------------ object updates */
static void player_update(Obj *p, float dt)
{
    update_anim(p, dt);
    Vec v = {0, 0};
    if (in.up) v.y -= 1;
    if (in.down) v.y += 1;
    if (in.left) v.x -= 1;
    if (in.right) v.x += 1;

    if (in.dash) {                                  /* quirky ramp kept from the original */
        if (p->boost < 1000) { p->speed += p->boost; p->boost += 200; }
        else p->speed = 100;
    } else { p->speed = 200; p->boost = 0; }

    if (in.plant) {
        if (linv > 0) p->plant += (vmag(v) > 0.5f) ? 0.5f : 2.0f;
        if (p->plant > 100) {
            add_light(p->x, p->y);
            p->plant = 0;
            linv--;
        }
    } else p->plant = 0;

    if (vmag(v) > 0) {
        vnorm(&v);
        p->dir = v;
        if (!p->walking) play(p, 1);
    } else play(p, 0);

    if (p->x > W) { p->mapx = W; p->x = W; }
    if (p->x < 0) { p->mapx = 0; p->x = 0; }
    if (p->y > H) { p->mapy = H; p->y = H; }
    if (p->y < 0) { p->mapy = 0; p->y = 0; }

    p->mapx += v.x * p->speed * dt;
    p->mapy += v.y * p->speed * dt;
}

static void monster_update(Obj *m, float dt)
{
    update_anim(m, dt);
    m->z = m->y + m->h;
    Vec vel = m->dir;
    if (m->kind == K_MON1) { vel.x = player.x - m->x; vel.y = player.y - m->y; }
    if (vmag(vel) > 0) {
        vnorm(&vel);
        m->dir = vel;
        if (!m->walking) play(m, 1);
    } else play(m, 0);

    Img *l = collided_light(m);
    if (l) {
        l->red = 1;
        delete_obj(m);
        bump_reset();
    }
    float lim = (m->kind == K_MON1) ? 1.0f : 10.0f;
    if (m->x > W + lim || m->x < -lim || m->y > H + lim || m->y < -lim) {
        delete_obj(m);
        bump_reset();
    }
    m->mapx += vel.x * m->speed * dt;
    m->mapy += vel.y * m->speed * dt;
}

static void spawn_monster(void)
{
    int rp = rnd(1, 4);
    float rx = 0, ry = 0; Vec d = {0, 0};
    if (rp == 1)      { rx = (float)rnd(1, W - 1); ry = -1;       d.x = (float)rnd(-10, 10); d.y = (float)rnd(0, 20); }
    else if (rp == 2) { rx = W + 1;                ry = (float)rnd(1, H - 1); d.x = (float)rnd(-20, 0); d.y = (float)rnd(-10, 10); }
    else if (rp == 3) { rx = (float)rnd(1, W - 1); ry = H + 1;    d.x = (float)rnd(-10, 10); d.y = (float)rnd(-20, 0); }
    else              { rx = -1;                   ry = (float)rnd(1, H - 1); d.x = (float)rnd(0, 20);  d.y = (float)rnd(-10, 10); }

    int luck = rnd(1, 100);
    Obj *m = new_obj();
    if (!m || n_obj >= MAX_OBJ) return;
    if (luck < 1 + (int)(timer / 7)) {      /* NB: Lua compares with the float; int() is off by <1 */
        init_sprite(m, K_MON1, T_rm);
        m->speed = (float)rnd(50, (int)(55 + timer / 2));
    } else {
        init_sprite(m, K_MON2, T_gm);
        m->hw = m->w / 2;
        m->speed = (float)rnd(80, (int)(180 + timer));
    }
    obj_at(m, rx, ry);
    m->dir = d;
    objs[n_obj++] = m;
    m->in_world = 1;
    m->bx = m->x + m->hox; m->by = m->y + m->hoy; m->bw = m->hw; m->bh = m->hh;
}

/* ------------------------------------------------------------------ map.update */
static void map_update(float dt)
{
    for (int i = 0; i < n_obj; i++) {               /* n_obj re-read each pass == ipairs semantics */
        Obj *v = objs[i];
        if (v->kind == K_PLAYER) player_update(v, dt); else monster_update(v, dt);

        if (v->has_hitbox && v->in_world) {
            Col cols[MAX_COLS]; double ax, ay;
            int nc = bump_move(v, v->mapx + v->hox, v->mapy + v->hoy, cols, &ax, &ay);
            v->x = (float)ax - v->hox;
            v->y = (float)ay - v->hoy;
            v->mapx = v->x; v->mapy = v->y;
            for (int k = 0; k < nc; k++) {
                if (v->kind != K_PLAYER && cols[k].other->kind == K_PLAYER) {
                    gameover = 1;               /* the pcall()s in the original are no-ops */
                    paused = 1;
                }
            }
        } else {
            v->x = v->mapx; v->y = v->mapy;
        }
    }

    map_time += dt;
    if (gamestate == 1 && map_time > 5 && ((int)floorf(fmodf(map_time, 10.0f)) % 2) == 0
        && (int)floorf(map_time * 10) != map_boo) {
        map_boo = (int)floorf(map_time * 10);
        spawn_monster();
    }
}

/* ------------------------------------------------------------------ map creation */
static void map_reset_empty(int banner)
{
    n_obj = 0; n_bg = 0;
    for (int i = 0; i < MAX_OBJ; i++) if (&opool[i] != &player) opool[i].alive = 0;
    for (int i = 0; i < MAX_IMG; i++) ipool[i].alive = 0;
    map_time = 0; map_boo = -1; map_is_banner = banner;
}

static void start_new(void)
{
    timer = 0;
    map_reset_empty(0);
    obj_at(&player, W / 2.0f, H / 2.0f);
    player.alive = 1;
    objs[n_obj++] = &player;
    add_bump();
    plat_rain_sound(0); rain_playing = 0;
}

/* ------------------------------------------------------------------ init */
void game_init(uint32_t seed)
{
    rng = seed ? seed : 2463534242u;
    T_player[0] = plat_load_tex("player1", 1);  T_player[1] = plat_load_tex("player2", 1);
    T_gm[0] = plat_load_tex("gmonster1", 1);    T_gm[1] = plat_load_tex("gmonster2", 1);
    T_rm[0] = plat_load_tex("rmonster1", 1);    T_rm[1] = plat_load_tex("rmonster2", 1);
    T_light = plat_load_tex("light", 1);
    T_banner = plat_load_tex("banner", 0);
    T_title = plat_load_tex("title", 0);
    T_rain = plat_load_tex("raintex", 0);

    memset(&player, 0, sizeof player);
    player.alive = 1;
    init_sprite(&player, K_PLAYER, T_player);
    obj_at(&player, 30, 40);
    player.speed = 200;

    highscore = plat_load_highscore();
    gamestate = 0; gameover = 0; paused = 0; timer = 0; linv = 6;

    map_reset_empty(1);
    splash = new_img();
    splash->is_splash = 1; splash->tex = T_banner;
    splash->w = (float)plat_tex_w(T_banner); splash->h = (float)plat_tex_h(T_banner);
    splash->x = W / 2.0f; splash->y = H / 2.0f;
    bg[n_bg++] = splash;
    text2[0] = 0;
}

/* ------------------------------------------------------------------ love.update */
void game_update(float dt, const PadState *pad, int pause_pressed)
{
    in = *pad;
    if (dt > 0.05f) dt = 0.05f;
    clock_s += dt;

    if (pause_pressed) paused = !paused;

    if (!paused) {
        timer += dt;
        map_update(dt);
        if (timer > 40) {
            if (!rain_playing) { plat_rain_sound(1); rain_playing = 1; }
            mesh_u -= dt / 5; mesh_v -= dt;
        }
    } else if (gameover && in.accept) {
        map_reset_empty(0);
        start_new();
        paused = 0; gameover = 0; timer = 0; linv = 6;
    }

    if (gamestate == 0) {
        map_update(dt);
        if (timer > 1.5f) {
            splash->tex = T_title;
            if (highscore > 60) snprintf(text2, sizeof text2, "SURVIVE %d SECONDS", highscore);
            else snprintf(text2, sizeof text2, "SURVIVE 60 SECONDS");
            if (in.accept) { gamestate = 1; start_new(); }
            if (gamestate == 0 && timer > 4) { gamestate = 1; start_new(); }
        }
    }
}

/* ------------------------------------------------------------------ rendering */
typedef struct { float x, y, r, g, b, power; } Light;

static void build_lightmap(const Light *ls, int n, uint8_t *out)
{
    static float acc[LM_H][LM_W][3];
    memset(acc, 0, sizeof acc);
    for (int k = 0; k < n; k++) {
        const Light *L = &ls[k];
        float lnx = L->x / W, lny = L->y / H;
        float rn = fabsf(L->power) > 0.5f ? 60.0f / fabsf(L->power) : 2.0f;   /* radius where attenuation < 1% */
        int x0 = (int)floorf((lnx - rn) * LM_W), x1 = (int)ceilf((lnx + rn) * LM_W);
        int y0 = (int)floorf((lny - rn) * LM_H), y1 = (int)ceilf((lny + rn) * LM_H);
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 > LM_W) x1 = LM_W;
        if (y1 > LM_H) y1 = LM_H;
        for (int y = y0; y < y1; y++) {
            float ny = (y + 0.5f) / LM_H - lny;
            for (int x = x0; x < x1; x++) {
                float nx = (x + 0.5f) / LM_W - lnx;
                float d = sqrtf(nx * nx + ny * ny) * L->power;
                float a = 1.0f / (1.0f + 0.09f * d + 0.032f * d * d);
                acc[y][x][0] += L->r * a; acc[y][x][1] += L->g * a; acc[y][x][2] += L->b * a;
            }
        }
    }
    for (int y = 0; y < LM_H; y++)
        for (int x = 0; x < LM_W; x++) {
            uint8_t *o = out + (y * LM_W + x) * 4;
            for (int c = 0; c < 3; c++) {
                float v = acc[y][x][c];
                o[c] = (uint8_t)(v >= 1 ? 255 : (v <= 0 ? 0 : v * 255 + 0.5f));
            }
            o[3] = 255;
        }
}

static void z_sort(void)                       /* stable by z_index, like draw()'s table.sort */
{
    for (int i = 1; i < n_obj; i++) {
        Obj *k = objs[i]; int j = i - 1;
        while (j >= 0 && objs[j]->z > k->z) { objs[j + 1] = objs[j]; j--; }
        objs[j + 1] = k;
    }
}

static void desat_color(float *r, float *g, float *b, float f)
{
    float avg = 0.21f * *r + 0.72f * *g + 0.07f * *b;
    *r += (avg - *r) * f; *g += (avg - *g) * f; *b += (avg - *b) * f;
}

static void draw_text_at(const char *s, float x, float y) { plat_draw_text(s, x, y); }

void game_draw(void)
{
    static Light ls[MAX_IMG + 4];
    static uint8_t lm[LM_W * LM_H * 4];
    char buf[96];
    int n = 0;

    float f = 0.1f + map_time / 200.0f; if (f > 1) f = 1;
    int lvl = (int)(f * 10 + 0.5f);

    /* base colour (grayscale shader applied to the vertex colour) */
    float r = 0.1f, g = 0.5f, b = 0.0f;
    desat_color(&r, &g, &b, f);
    plat_fill_rect(0, 0, W, H, r, g, b);

    z_sort();

    /* lights: collected exactly like map.draw (objects first, then images_bg, color() may delete) */
    for (int i = 0; i < n_obj; i++)
        if (objs[i]->kind == K_PLAYER && n < MAX_IMG)
            ls[n++] = (Light){objs[i]->x, objs[i]->y, 1.0f, 0.6f, 0.6f, 150.0f - objs[i]->plant};
    for (int i = 0; i < n_bg; i++) {
        Img *v = bg[i];
        if (v->is_splash) {
            ls[n++] = (Light){0, 0, 1, 1, 1, 8.0f - timer * 4.0f};
        } else if (v->is_light) {
            float c[3];
            Light L = {v->x, v->y, 0, 0, 0, light_power(v)};
            light_color(v, c);               /* may remove v: the next entry is skipped, as in the original */
            L.r = c[0]; L.g = c[1]; L.b = c[2];
            if (n < MAX_IMG) ls[n++] = L;
        }
    }

    for (int i = 0; i < n_bg; i++) {
        Img *v = bg[i];
        plat_draw_tex(v->tex, v->x - v->w / 2, v->y - v->h / 2, v->is_splash ? 0 : lvl);
    }
    for (int i = 0; i < n_obj; i++) {
        Obj *o = objs[i];
        plat_draw_tex(o->img, o->x - o->w / 2, o->y - o->h / 2, lvl);
    }
    if (timer > 41) plat_draw_rain(T_rain, mesh_u, mesh_v);

    build_lightmap(ls, n, lm);
    plat_apply_lightmap(lm);
    plat_apply_grain(map_time);

    /* HUD */
    float tw, th;
    if (gameover) {
        draw_text_at("press O to retry", 0, 0);
        int score = (int)floorf(timer);
        if (score > highscore) { highscore = score; plat_save_highscore(highscore); }
        snprintf(buf, sizeof buf, "YOUR SCORE %d\nHIGH SCORE %d", score, highscore);
        plat_text_size(buf, &tw, &th);
        draw_text_at(buf, W / 2.0f - tw / 2, H - 100);
    }
    if (gamestate == 1) {
        const char *top; 
        if (gameover) top = "GAME OVER";
        else if (paused) top = "paused";
        else { snprintf(buf, sizeof buf, "%d", (int)floorf(timer)); top = buf; }
        char top_copy[96]; snprintf(top_copy, sizeof top_copy, "%s", top);
        plat_text_size(top_copy, &tw, &th);
        draw_text_at(top_copy, W / 2.0f - tw / 2, 20);
        if (!gameover) {
            char lb[16]; snprintf(lb, sizeof lb, "%d", linv);
            plat_text_size(lb, &tw, &th);
            draw_text_at(lb, floorf(W / 2.0f - tw / 2), H - 100);
        }
    } else if (text2[0]) {
        plat_text_size(text2, &tw, &th);
        draw_text_at(text2, floorf(W / 2.0f - tw / 2), H / 2.0f - th / 2);
    }
}
