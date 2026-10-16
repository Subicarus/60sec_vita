/* minimal declarations (from the SDL2 API) used only to syntax-check src/main.c without SDL installed */
#pragma once
#include <stdint.h>
#include <string.h>
typedef uint8_t Uint8; typedef uint32_t Uint32; typedef uint64_t Uint64; typedef int16_t Sint16;
typedef struct SDL_Window SDL_Window; typedef struct SDL_Renderer SDL_Renderer; typedef struct SDL_Texture SDL_Texture;
typedef struct { int x, y, w, h; } SDL_Rect;
typedef enum { SDL_BLENDMODE_NONE = 0, SDL_BLENDMODE_BLEND = 1, SDL_BLENDMODE_ADD = 2, SDL_BLENDMODE_MOD = 4 } SDL_BlendMode;
#define SDL_HINT_RENDER_SCALE_QUALITY "SDL_RENDER_SCALE_QUALITY"
#define SDL_PIXELFORMAT_RGBA32 1
#define SDL_TEXTUREACCESS_STATIC 0
#define SDL_INIT_VIDEO 0x20
#define SDL_INIT_AUDIO 0x10
#define SDL_WINDOW_SHOWN 4
#define SDL_WINDOWPOS_CENTERED 0x2FFF0000
#define SDL_RENDERER_ACCELERATED 2
#define SDL_RENDERER_PRESENTVSYNC 4
#define SDL_QUIT 0x100
#define AUDIO_S16SYS 0x8010
typedef uint32_t SDL_AudioDeviceID;
typedef void (*SDL_AudioCallback)(void *userdata, Uint8 *stream, int len);
typedef struct { int freq; uint16_t format; Uint8 channels; Uint8 silence; uint16_t samples; uint32_t size; SDL_AudioCallback callback; void *userdata; } SDL_AudioSpec;
typedef struct { Uint32 type; Uint8 pad[56]; } SDL_Event;
typedef enum { SDL_SCANCODE_A = 4, SDL_SCANCODE_C = 6, SDL_SCANCODE_D = 7, SDL_SCANCODE_P = 19, SDL_SCANCODE_S = 22, SDL_SCANCODE_W = 26,
  SDL_SCANCODE_X = 27, SDL_SCANCODE_Z = 29, SDL_SCANCODE_RETURN = 40, SDL_SCANCODE_ESCAPE = 41, SDL_SCANCODE_SPACE = 44,
  SDL_SCANCODE_RIGHT = 79, SDL_SCANCODE_LEFT = 80, SDL_SCANCODE_DOWN = 81, SDL_SCANCODE_UP = 82 } SDL_Scancode;
#define SDL_zero(x) memset(&(x), 0, sizeof(x))
int SDL_Init(Uint32); void SDL_Quit(void); const char *SDL_GetError(void); Uint32 SDL_GetTicks(void);
Uint64 SDL_GetPerformanceCounter(void); Uint64 SDL_GetPerformanceFrequency(void);
int SDL_SetHint(const char *, const char *);
SDL_Window *SDL_CreateWindow(const char *, int, int, int, int, Uint32); void SDL_DestroyWindow(SDL_Window *);
SDL_Renderer *SDL_CreateRenderer(SDL_Window *, int, Uint32); void SDL_DestroyRenderer(SDL_Renderer *);
int SDL_GetRendererOutputSize(SDL_Renderer *, int *, int *);
SDL_Texture *SDL_CreateTexture(SDL_Renderer *, Uint32, int, int, int);
int SDL_UpdateTexture(SDL_Texture *, const SDL_Rect *, const void *, int);
int SDL_SetTextureBlendMode(SDL_Texture *, SDL_BlendMode);
int SDL_RenderCopy(SDL_Renderer *, SDL_Texture *, const SDL_Rect *, const SDL_Rect *);
int SDL_SetRenderDrawBlendMode(SDL_Renderer *, SDL_BlendMode);
int SDL_SetRenderDrawColor(SDL_Renderer *, Uint8, Uint8, Uint8, Uint8);
int SDL_RenderFillRect(SDL_Renderer *, const SDL_Rect *); int SDL_RenderClear(SDL_Renderer *); void SDL_RenderPresent(SDL_Renderer *);
SDL_AudioDeviceID SDL_OpenAudioDevice(const char *, int, const SDL_AudioSpec *, SDL_AudioSpec *, int);
void SDL_PauseAudioDevice(SDL_AudioDeviceID, int); void SDL_LockAudioDevice(SDL_AudioDeviceID); void SDL_UnlockAudioDevice(SDL_AudioDeviceID);
void SDL_CloseAudioDevice(SDL_AudioDeviceID);
int SDL_PollEvent(SDL_Event *); const Uint8 *SDL_GetKeyboardState(int *);
