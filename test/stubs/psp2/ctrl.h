#pragma once
typedef struct { unsigned buttons; unsigned char lx, ly, rx, ry; } SceCtrlData;
#define SCE_CTRL_SELECT 1u
#define SCE_CTRL_START 8u
#define SCE_CTRL_UP 0x10u
#define SCE_CTRL_RIGHT 0x20u
#define SCE_CTRL_DOWN 0x40u
#define SCE_CTRL_LEFT 0x80u
#define SCE_CTRL_LTRIGGER 0x100u
#define SCE_CTRL_RTRIGGER 0x200u
#define SCE_CTRL_TRIANGLE 0x1000u
#define SCE_CTRL_CIRCLE 0x2000u
#define SCE_CTRL_CROSS 0x4000u
#define SCE_CTRL_SQUARE 0x8000u
#define SCE_CTRL_MODE_ANALOG 1
int sceCtrlPeekBufferPositive(int, SceCtrlData *, int); int sceCtrlSetSamplingMode(int);
