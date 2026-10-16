# Survive 60 Seconds — PS Vita port

Native C / SDL2 port of *Survive 60 Seconds* (LÖVE 11 game by Kavirajar B. / Perspectilt Studios,
GMTK Game Jam 2020). The Lua code was re-implemented 1:1 in `src/game.c` (including the `bump.lua`
collision resolution); the GLSL lighting shader was replaced by a CPU light map, because the Vita's
GPU cannot run the original 300-light loop shader.

**Status: written and logic-tested on a PC, NOT yet compiled or run on a real Vita** (the build
environment had no VitaSDK/SDL2 and no console). Expect to fix a few build errors on first try —
send them to me and I'll patch.

## Build the VPK
```
# 1. VitaSDK installed, $VITASDK set, SDL2 package installed:   vdpm sdl2
mkdir build && cd build
cmake .. && make
# -> Survive60Seconds.vpk  (install with VitaShell on a HENkaku/Enso console)
```

## Try it on a PC first (optional)
```
make -f Makefile.pc && ./survive60
make -f Makefile.pc test        # headless logic test (ASan/UBSan)
```

## Controls (Vita)
| Button | Action |
|---|---|
| D-pad / left stick | move |
| Square (or L/R) | dash (tap while moving) |
| Cross (hold) | plant a light |
| Circle | skip intro / retry |
| Start | pause |
| Select | quit |

Highscore is stored in `ux0:data/Survive60Seconds/saves.txt`.

## What is different from the original
* Lighting: 160x120 light map multiplied over the scene (smooth, same falloff formula). Overlapping
  lights are clamped at full brightness instead of over-brightening.
* Film grain: additive noise texture instead of per-pixel shader noise.
* Grayscale-over-time: sprites are pre-desaturated in 11 steps instead of per-pixel.
* Audio: music converted to mono 22.05 kHz (ambience), mixed by a tiny built-in mixer (no SDL_mixer).
* The window is the fixed 640x480 arena scaled to 544p with side bars (same as the original's fullscreen).

## Layout
```
src/game.c        game logic (port of main/map/sprite/gameobjects/image/bump)
src/main.c        SDL2 + Vita platform layer (video, audio, input, save)
src/platform.h    interface between the two
src/font_data.h   generated font metrics
assets/           converted assets (RGBA blobs, PCM audio, font atlas)
tools/convert_assets.py   regenerates assets/ and the icon from the original game
test/             headless test + syntax-check stubs
```

## Credits / licence
Game, art and music: Kavirajar B. / Perspectilt Studios (https://perspectilt.itch.io), music from
freesound.org/s/133100 and PremiumBeat's free ambient tracks. The assets in this folder are theirs —
ask the author before publishing the VPK anywhere. The port code is yours to use as you like.
