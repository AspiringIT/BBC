// some standard library/system headers
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>   // to actually use these we'll need to add "-lm" to our linking command (on *nix platforms, anyway)

// we use SDL2 for cross-platform graphics/sound/keyboard support
#include "SDL.h"    // the CMake build system finds/sets up the search paths to find this (if possible/available)

// we include the CHIP-8 VM API here
#include "chip8.h"  // this needs to be here in our working directory

// ------------- PREPROCESSOR DEFINES & MACROS --------------
// (optional reading, but helpful illustration of techniques)
// ----------------------------------------------------------

// default window size (width and height)
#define WIN_WIDTH 640   // 10x 64, or 5x 128
#define WIN_HEIGHT 320  // 10x 32, or 5x 64

// calculated "pixel size" of the CHIP-8 pixels as drawn inside our window
#define PIX_WIDTH (WIN_WIDTH / FB_COLS)
#define PIX_HEIGHT (WIN_HEIGHT / FB_ROWS)

// program name to show in window title bar
#define TITLE "CHIP-8"

// macro to simplify reporting SDL2 errors
#define SDL_PERRORF(fmt, ...) fprintf(stderr, fmt ": %s\n",##__VA_ARGS__, SDL_GetError())

#define MAX(a, b) (((a) > (b)) ? (a) : (b))

// makefile-overridable RGB colors for foreground/background
#ifndef GUI8_FG_R
#define GUI8_FG_R 255 
#endif
#ifndef GUI8_FG_G
#define GUI8_FG_G 255
#endif
#ifndef GUI8_FG_B 
#define GUI8_FG_B 255
#endif
#ifndef GUI8_BG_R 
#define GUI8_BG_R 0 
#endif
#ifndef GUI8_BG_G 
#define GUI8_BG_G 0 
#endif
#ifndef GUI8_BG_B
#define GUI8_BG_B 0 
#endif

// and for FPS and default-CPF targets
#ifndef GUI8_FPS
#define GUI8_FPS 60
#endif
#ifndef GUI8_DEFAULT_TARGET_CPF
#define GUI8_DEFAULT_TARGET_CPF 1000
#endif

// macro for condensing foreground/background color setting code for SDL2 render logic
#define FOREGROUND(ren) SDL_SetRenderDrawColor((ren), GUI8_FG_R, GUI8_FG_G, GUI8_FG_B, 255);
#define BACKGROUND(ren) SDL_SetRenderDrawColor((ren), GUI8_BG_R, GUI8_BG_G, GUI8_BG_B, 255);
#define GRIDCOLOR(ren) SDL_SetRenderDrawColor((ren), 255, 255, 255, 255);

// intervals for 60Hz and makefile-overridable-FPS-interval timers
#define Tms60Hz (1000 / 60)
#define TmsFrHz (1000 / GUI8_FPS)

// audio sampling rate (for generating the beep sound)
#define SAMPLING_RATE 48000

// makefile-overridable audio sampling function (options: square_sampler, sine_sampler, triangle_sampler, sawtooth_sampler)
#ifndef SAMPLER_FUNC
#define SAMPLER_FUNC triangle_sampler
#endif

// -------------------- KEYBOARD SUBSYSTEM --------------------
// ------------------------------------------------------------

// CHIP-8 Keypad Layout
//======================
// +---+---+---+---+
// | 1 | 2 | 3 | C |
// +---+---+---+---+
// | 4 | 5 | 6 | D |
// +---+---+---+---+
// | 7 | 8 | 9 | E |
// +---+---+---+---+
// | A | 0 | B | F |
// +---+---+---+---+
//
// QWERTY Bindings
//==================
// +---+---+---+---+
// | 1 | 2 | 3 | 4 |
// +---+---+---+---+
// | Q | W | E | R |
// +---+---+---+---+
// | A | S | D | F |
// +---+---+---+---+
// | Z | X | C | V |
// +---+---+---+---+
//
// QUERTY Alternates
//==================
// * Left Arrow -> A (i.e., CHIP-8 '7' key)
// * Right Arrow -> D (i.e., CHIP-8 '9' key)
// * Up Arrow -> W (i.e., CHIP-8 '5' key)
// * Down Arrow -> S (i.e., CHIP-8 '8' key)
// * Spacebar -> E (i.e., CHIP-8 '6' key)

// Key bindings mapping keyboard scancodes to CHIP-8 simulated keypad numbers/names
// (note that multiple keyboard keys can be bound to the same keypad button)
static const struct keypad_scancode {
    SDL_Scancode scancode;
    Uint16 keymask;
    char keycap;
} chip8_keypad_scancodes[] = {
    { SDL_SCANCODE_1, 		1 << 1, 	'1' },
    { SDL_SCANCODE_2, 		1 << 2, 	'2' },
    { SDL_SCANCODE_3, 		1 << 3, 	'3' },
    { SDL_SCANCODE_4, 		1 << 12,	'C' },
    { SDL_SCANCODE_Q, 		1 << 4, 	'4' },
    { SDL_SCANCODE_W, 		1 << 5, 	'5' },
    { SDL_SCANCODE_UP, 		1 << 5, 	'5' },
    { SDL_SCANCODE_E, 		1 << 6, 	'6' },
    { SDL_SCANCODE_SPACE, 	1 << 6, 	'6' },
    { SDL_SCANCODE_R, 		1 << 13, 	'D' },
    { SDL_SCANCODE_A, 		1 << 7, 	'7' },
    { SDL_SCANCODE_LEFT, 	1 << 7, 	'7' },
    { SDL_SCANCODE_S, 		1 << 8, 	'8' },
    { SDL_SCANCODE_DOWN, 	1 << 8, 	'8' },
    { SDL_SCANCODE_D, 		1 << 9, 	'9' },
    { SDL_SCANCODE_RIGHT, 	1 << 9, 	'9' },
    { SDL_SCANCODE_F, 		1 << 14, 	'E' },
    { SDL_SCANCODE_Z, 		1 << 10, 	'A' },
    { SDL_SCANCODE_X, 		1 << 0, 	'0' },
    { SDL_SCANCODE_C, 		1 << 11, 	'B' },
    { SDL_SCANCODE_V, 		1 << 15, 	'F' },
    { SDL_SCANCODE_UNKNOWN, -1, 		0 },	// "sentinal" value marking the end of the scancode list
};

// helper macro to simplify iterating over each keypad scancode record 
// (auto-generates a for-loop using "var" as a pointer to the next keypad_scancode object)
#define FOREACH_KEY(var) for (const struct keypad_scancode *var = chip8_keypad_scancodes; var->keycap; ++var)


// -------------------- GRAPHICS SUBSYSTEM --------------------
// ------------------------------------------------------------


// helper function to render the CHIP-8 VM's internal framebuffer to the screen
static void render_framebuffer(struct chip8_vm *vm, SDL_Renderer *ren, bool render_grid) {
    int dx = PIX_WIDTH;
    int dy = PIX_HEIGHT;

    // draw the framebuffer onto our app window/screen
    BACKGROUND(ren);
    SDL_RenderClear(ren);
    SDL_Rect rr = { .y = 0, .w = dx, .h = dy };
    for (int y = 0; y < FB_ROWS; ++y, rr.y += dy) {
        rr.x = 0;
        for (int x = 0; x < FB_COLS; ++x, rr.x += dx) {
            switch (vm->fb[y][x]) {
            case 0:
                break;
            case 1:
                FOREGROUND(ren);
                SDL_RenderFillRect(ren, &rr);
                break;
            default:
                GRIDCOLOR(ren);
                SDL_RenderFillRect(ren, &rr);
            }
            if (render_grid) {
                GRIDCOLOR(ren);
                SDL_RenderDrawRect(ren, &rr);
            }
        }
    }
    SDL_RenderPresent(ren);
}


// ------------------------- AUDIO SUBSYSTEM ------------------------
// (there be math _and_ pointer dragons in here, so shield your eyes)
// ------------------------------------------------------------------

// extent and current position within a looping buffer of U8 audio samples
struct tone_loop {
    Uint8 *start;
    Uint8 *end;
    Uint8 *cursor;
};

// constructor to allocate a `tone_loop` buffer and filling it with one second
// of audio samples (at sampling rate `srate`; i.e., `srate` sample values)
// calculated using the provided `sampler` function pointer (which takes a
// floating point offset from 0.0 to 1.0 (start to finish) and an opaque
// "context" pointer (type `void *`) to maintain its own arguments/state
struct tone_loop *gen_tone_loop(int srate, Uint8 (*sampler)(float, void *), void *ctx) {
    char *hunk = malloc(sizeof(struct tone_loop) + srate);
    if (!hunk) return NULL;
    struct tone_loop *tlp = (struct tone_loop *)hunk;
    tlp->start = (Uint8*)&tlp[1];
    tlp->end = tlp->start + srate;
    tlp->cursor = tlp->start;

    float srr = 1.0f / (float)srate;
    for (int i = 0; i < srate; ++i) {
        tlp->start[i] = sampler((float)i * srr, ctx);
    }

    return tlp;
}

// waveform parameters used by the sampler functions (frequency, volume level)
// (all of our sampler functions expect a pointer to one of these for `ctx`)
struct tone_context {
    float freq;
    float level;
};

// sample a single cycle of a pure sine-wave signal (sounds very mellow)
Uint8 sine_sampler(float t, void *ctx) {
    struct tone_context *tcp = (struct tone_context *)ctx;
    return (Uint8)(tcp->level * sinf(2.f * M_PI * tcp->freq * t) + tcp->level);
}

// sample a single full cycle of a square-wave ("on/off") signal (sounds very beepy/computery)
Uint8 square_sampler(float t, void *ctx) {
    struct tone_context *tcp = (struct tone_context *)ctx;
    return (Uint8)(tcp->level * copysignf(1.0f, sinf(2.f * M_PI * tcp->freq * t)) + tcp->level);
}

// sample a single full cycle of a sawtooth-wave (ramp-up/cliff-down) signal (sounds very harsh/nasal)
Uint8 sawtooth_sampler(float t, void *ctx) {
    struct tone_context *tcp = (struct tone_context *)ctx;
    float p = 1.0f / tcp->freq;
    float t_p = t / p;
    return (Uint8)(tcp->level * (2.f * (t_p - floorf(0.5f + t_p))) + tcp->level);
}

// sample a single full cycle of a triangle-wave (ramp-up/ramp-down) signal (a less-mellow sine-like sound)
Uint8 triangle_sampler(float t, void *ctx) {
    struct tone_context *tcp = (struct tone_context *)ctx;
    float p = 1.0f / tcp->freq;
    float t_p = t / p;
    return (Uint8)(4.f * fabsf(t_p - floorf(t_p + .5f)) * tcp->level);

}

// callback function to feed audio sample data on-demand to the SDL2 sound system during playback
void tone_loop_cb(void *userdata, Uint8 *buffer, int len) {
    struct tone_loop *tlp = (struct tone_loop *)userdata;
    while (len > 0) {
        int sz = tlp->end - tlp->cursor;
        if (sz > len) sz = len;
        memcpy(buffer, tlp->cursor, sz);
        tlp->cursor += sz;
        buffer += sz;
        len -= sz;
        if (tlp->cursor == tlp->end) tlp->cursor = tlp->start;
    }
}


// ------------------------- MAIN PROGRAM LOGIC ------------------------
// ---------------------------------------------------------------------

// helper function to checkif `ms` millesecond-scale ticks have elapsed since the value of `*ticks`;
// if so, updates the value of `*ticks` to be "now"
bool has_elapsed(Uint64 *ticks, int ms) {
    Uint64 now = SDL_GetTicks64();
    int diff = now - *ticks;
    bool elapsed = diff >= ms;
    if (elapsed) *ticks = now;
    return elapsed;
}


// entry point that fires up SDL2 and drives the CHIP-8 fetch/decode/execute cycle
int main(int argc, char **argv) {
    int ret = EXIT_FAILURE;
    bool sdl_init = false;
    SDL_Window *win = NULL;
    SDL_Renderer *ren = NULL;
    SDL_AudioDeviceID snd = 0;
    struct tone_loop *tlp = NULL;

    char progbuf[RAM_SIZE];
    struct chip8_vm vm;
    FILE *romfile = NULL;
    int target_cpf = GUI8_DEFAULT_TARGET_CPF;

    // if we have no ROM file name as a CLI arg, print a usage message and quit
    if (argc < 2) {
        fprintf(stderr, "usage: %s ROM_FILE [TARGET_CPF]\n", argv[0]);
        goto cleanup;
    }

    if (argc > 2) {
        target_cpf = atoi(argv[2]);
    }

    // open the ROM file (for binary reading) and read up to (sizeof progbuf) bytes into `progbuf`
    printf("loading ROM '%s'...\n", argv[1]);
    if ((romfile = fopen(argv[1], "rb")) == NULL) {
        fprintf(stderr, "ERROR: cannot open '%s'\n", argv[1]);
        goto cleanup;
    }
    size_t proglen = fread(progbuf, sizeof(char), sizeof progbuf, romfile);

    // load the CHIP-8 VM with the desired program 
    if (!chip8_load(&vm, (uint8_t *)progbuf, proglen)) {
        fprintf(stderr, "ERROR: cannot load program\n");
        goto cleanup;
    }

    // initialze the SDL2 library and set up a window/rendering system
    printf("initializing SDL...\n");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO) != 0) {
        SDL_PERRORF("SDL_Init");
        goto cleanup;
    }
    sdl_init = true;

    if ((win = SDL_CreateWindow(TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIN_WIDTH, WIN_HEIGHT, SDL_WINDOW_SHOWN)) == NULL) {
        SDL_PERRORF("SDL_CreateWindow");
        goto cleanup;
    }

    if ((ren = SDL_CreateRenderer(win, -1, 0)) == NULL) {
        SDL_PERRORF("SDL_CreateRenderer");
        goto cleanup;
    }

    // generate the audio samples for our CHIP-8 buzzer/beeper tone
    struct tone_context tc = {
        .freq = 440.f,
        .level = 127.5f,
    };
    if ((tlp = gen_tone_loop(SAMPLING_RATE, SAMPLER_FUNC, (void *)&tc)) == NULL) {
        fprintf(stderr, "ERROR allocating audio samples\n");
        goto cleanup;
    }

    // initialize the SDL2 sound system to play our audio samples
    const SDL_AudioSpec spec_wanted = {
        .freq = SAMPLING_RATE,
        .format = AUDIO_U8,
        .channels = 1,
        .samples = 512,
        .callback = tone_loop_cb,
        .userdata = (void *)tlp,
    };
    SDL_AudioSpec spec_got;
    if ((snd = SDL_OpenAudioDevice(NULL, 0, &spec_wanted, &spec_got, 0)) == 0) {
        SDL_PERRORF("SDL_OpenAudioDevice");
        goto cleanup;
    }

    // get access to SDL2's "keystate array" (one byte per key; 0 if up, 1 if down)
    int keystate_len;
    const Uint8 *keystate = SDL_GetKeyboardState(&keystate_len);
   
    // PREPARE TO ENTER THE MAIN GAME LOOP...	
    SDL_Event ev;
    bool running = true, sound_on = false;		// game loop termination flag, sound on/off flag
    int frames = 0, cycles = 0, cpf = 0;	    // counters for tracking frames-per-second (FPS) and cycles-per-frame (CPF)
    Uint64 vsync_ticks = SDL_GetTicks64();		// clock ticks for the primary 60Hz "vsync timer" that CHIP-8 depends on for timing
    Uint64 fps_ticks = vsync_ticks;				// clock ticks for FPS timer (i.e., once-per-second)
    Uint64 frame_ticks = 0;						// clock ticks for frame-render (i.e., our target FPS)
    size_t vtick = 0;							// count the elapsed "vsync timer ticks," for helping CHIP-8 know what time it is
    bool render_grid = false;					// debugging help: draw a visible pixel grid (on/off)

    // ENTER THE MAIN GAME LOOP!
    while (running) {
        // check for critical events like app-exit
        while (SDL_PollEvent(&ev)) {	
            switch (ev.type) {
            case SDL_QUIT:  // via hotkey/close-button click
quitting:
                printf("quitting...\n");
                running = false;
                break;
            case SDL_KEYDOWN: // or via Escape key press
                switch (ev.key.keysym.scancode) {
                case SDL_SCANCODE_ESCAPE:
                    goto quitting;
                case SDL_SCANCODE_F1:
                    render_grid = !render_grid;
                    break;
                default:
                    break;
                }
                break;
            default:
            break;
            }
        }

        // update `vticks` on a 60Hz timer interval
        if (has_elapsed(&vsync_ticks, Tms60Hz)) {
            ++vtick;
        }

        // once per second, update the visual FPS/CPF counters (in the window title bar)
        if (has_elapsed(&fps_ticks, 1000)) {
            char title_buff[128];
            snprintf(title_buff, sizeof title_buff, "CHIP-8 (FPS=%d, CPF=%d)", frames, cpf);
            SDL_SetWindowTitle(win, title_buff);
            frames = 0;
        }

        // scan through our defined key bindings and set the "keypad keys down" bitmask used by CHIP-8
        // (i.e., figure out what keys are currently down/up)
        uint16_t keybits = 0u;
        FOREACH_KEY(kp) {
            if (keystate[kp->scancode]) keybits |= kp->keymask;
        }

        // EXECUTE A SINGLE CHIP-8 VM FETCH/DECODE/EXECUTE cycle
        bool old_sound_on = sound_on;
        uint16_t old_pc = chip8_get_pc(&vm);
        if (!chip8_cycle(&vm, keybits, vtick, &sound_on)) {
            fprintf(stderr, "ERROR: illegal instruction @ PC=0x%04x (instruction=0x%02x%02x)\n",
                    old_pc,
                    chip8_get_ram(&vm, old_pc),
                    chip8_get_ram(&vm, old_pc + 1));
            running = false;
        }
        ++cycles;

        // if CHIP-8 turned the sound ON, un-pause the audio device to get our tone generator going
        if (old_sound_on && !sound_on) {
            SDL_PauseAudioDevice(snd, 1);
        } else if (!old_sound_on && sound_on) {
            // otherwise, if we turned the sound OFF, pause the tone generator
            SDL_PauseAudioDevice(snd, 0);
        }

        // is it time to render a new frame?
        if (has_elapsed(&frame_ticks, TmsFrHz)) {
            render_framebuffer(&vm, ren, render_grid);
            ++frames;
            cpf = cycles;
            cycles = 0;
        } else if (target_cpf && (cycles >= target_cpf)) {
            // we can sleep until the next frame is ready
            Uint64 next = (frame_ticks + TmsFrHz);
            Uint64 now = SDL_GetTicks64();
            SDL_Delay(MAX(next - now - 1, 1));
        }
    }

    ret = EXIT_SUCCESS;
cleanup:
    if (snd) SDL_CloseAudioDevice(snd);
    if (ren) SDL_DestroyRenderer(ren);
    if (win) SDL_DestroyWindow(win);
    if (sdl_init) SDL_Quit();
    if (romfile) fclose(romfile);
    return ret;
}
