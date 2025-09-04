#ifndef _CHIP8_H
#define _CHIP8_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// HELPFUL CONSTANTS
//--------------------------------------------------------------

#define RAM_SIZE 4096	// 4KiB of RAM
#define STACK_SLOTS 16	// 16 slots each capable of storing a 12-bit saved PC value
#define FB_COLS 64	// framebuffer is 64 pixels _wide_
#define FB_ROWS 32	// by 32 pixels _tall_


// THE CORE CHIP-8 VIRTUAL MACHINE (VM) OBJECT TYPE
//--------------------------------------------------------------

struct chip8_vm {

    // TODO: add fields as necessary to track all CHIP-8 internal state (RAM, registers, stack, clock ticks, etc.)


    //4KiB
    uint16_t ram[RAM_SIZE];

    // Registers: 16 8-bit general-purpose registers (V0 to VF)
    uint8_t V[16];

    //Stack: 16 sixteen bit values for storing addresses
    uint16_t stack[STACK_SLOTS];

    //Program counter (sixteen bit) register
    uint16_t pc;

    //index register (sixteen bits)
    uint16_t I;

    //Delay timer (eight bits)
    uint16_t delay_timer;

    //sound timer (eight bits)
    uint16_t sound_timer;

    //Keyboard state (sixteen bit) vector
    uint16_t keys[16];

    //stack pointer
    uint16_t sp;


    // framebuffer: 1 byte per pixel in a FB_COLS x FB_ROWS matrix
    // (0 = pixel off, 1 = pixel on, all other values = undefined/error)
    // *(this part of the `chip8_vm` struct *must* be exactly like this for compatibility with `gui.c`'s rendering code*
    uint8_t fb[FB_ROWS][FB_COLS];
};


// API FUNCTIONS YOU MUST IMPLEMENT FOR gui.c AND test.c TO WORK
//--------------------------------------------------------------

// initialize a CHIP-8 VM with a new program (false on error, true on success)
// (reasons for failure: program too large for RAM)
bool chip8_load(struct chip8_vm *vm, uint8_t *program, size_t proglen);

// single-step the CHIP-8 VM interpreter (false on error, true on success)
// `keys` := 16-bit bit vector given the up (0) or down (1) state of the 16-key keypad
// `vtick` := 60Hz vsync clock (or alternatively, the frame count)
// `sound` := pointer to bool that controls the on/off of the beep generator (true == on, false == off)
// (reason for failure: tried to execute an invalid/unsupported machine instruction, stack overflow/underflow)
bool chip8_cycle(struct chip8_vm *vm, uint16_t keys, size_t vtick, bool *sound);

// debugging functions: getters/setters for various pieces of standard CHIP-8 state
// (included so that automated tests can run, and so that the GUI can report some errors)
//---------------------------------------------------------------------------------------

// get/set PC (program counter)
uint16_t chip8_get_pc(struct chip8_vm *vm);
void chip8_set_pc(struct chip8_vm *vm, uint16_t new_pc);

// get/set I (address register)
uint16_t chip8_get_i(struct chip8_vm *vm);
void chip8_set_i(struct chip8_vm *vm, uint16_t new_i);

// get/set an 8-bit register (V0, V1, ..., VF)
uint8_t chip8_get_vr(struct chip8_vm *vm, int index);
void chip8_set_vr(struct chip8_vm *vm, int index, uint8_t new_val);

// get/set a single byte of CHIP-8 VM RAM
uint8_t chip8_get_ram(struct chip8_vm *vm, uint16_t address);
void chip8_set_ram(struct chip8_vm *vm, uint16_t address, uint8_t new_val);

#endif
