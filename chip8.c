#include "chip8.h" 
#include "stdio.h"
#include "stdlib.h"


#define PROG_START 0x200

#define ADDRESS_MASK 0x0fff

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#define FONT_ADDRESS 0x000

#define FONT_CHAR_SIZE 5

// Define the font sprites for the CHIP-8 interpreter
static const uint8_t chip8_font_sprites[] = {
    0xf0, 0x90, 0x90, 0x90, 0xf0, // "0"
    0x20, 0x60, 0x20, 0x20, 0x70, // "1"
    0xf0, 0x10, 0xf0, 0x80, 0xf0, // "2"
    0xf0, 0x10, 0xf0, 0x10, 0xf0, // "3"
    0x90, 0x90, 0xf0, 0x10, 0x10, // "4"
    0xf0, 0x80, 0xf0, 0x10, 0xf0, // "5"
    0xf0, 0x80, 0xf0, 0x90, 0xf0, // "6"
    0xf0, 0x10, 0x20, 0x40, 0x40, // "7"
    0xf0, 0x90, 0xf0, 0x90, 0xf0, // "8"
    0xf0, 0x90, 0xf0, 0x90, 0xf0, // "9"
    0xf0, 0x90, 0xf0, 0x90, 0x90, // "A"
    0xe0, 0x90, 0xe0, 0x90, 0xe0, // "B"
    0xf0, 0x80, 0x80, 0x80, 0xf0, // "C"
    0xe0, 0x90, 0x90, 0x90, 0xe0, // "D"
    0xf0, 0x80, 0xf0, 0x80, 0xf0, // "E"
    0xf0, 0x80, 0xf0, 0x80, 0x80  // "F"
};

// Function to load a program into the CHIP-8 VM
bool chip8_load(struct chip8_vm *vm, uint8_t *program, size_t proglen) {
    // Check if the program length exceeds available memory space
    if (proglen > (RAM_SIZE - PROG_START)) {
        return false; // Return false if program is too large
    }

    // Load font sprites into memory
    for (size_t i = 0; i < sizeof(chip8_font_sprites); i++) {
        vm->ram[FONT_ADDRESS + i] = chip8_font_sprites[i];
    }

    // Load the program into memory starting at PROG_START
    for (size_t i = 0; i < proglen; i++) {
        vm->ram[PROG_START + i] = program[i];
    }

    // Initialize the CHIP-8 VM registers and timers
    vm->pc = PROG_START; // Set the program counter to the start of the program
    vm->I = 0;           // Initialize the index register to 0
    vm->sp = 0;          // Initialize the stack pointer to 0
    vm->delay_timer = 0; // Initialize the delay timer to 0
    vm->sound_timer = 0; // Initialize the sound timer to 0

    return true; // Return true if the program was loaded successfully
}

// Function to execute one cycle of the CHIP-8 VM
bool chip8_cycle(struct chip8_vm *vm, uint16_t keys, size_t vtick, bool *sound) {
    uint16_t opcode = (vm->ram[vm->pc] << 8) | vm->ram[vm->pc + 1];
    uint8_t x = (opcode & 0x0F00) >> 8;
    uint8_t y = (opcode & 0x00F0) >> 4;
    uint16_t sum = vm->V[x] + vm->V[y];
    printf("%u\n", vm->V[y]);

    printf("PC: 0x%04X, Opcode: 0x%04X\n", vm->pc, opcode);
    vm->pc += 2;

    switch (opcode & 0xF000) {
        case 0xA000:
            vm->I = opcode & 0x0FFF;
            break;
        case 0xC000:
            vm->V[x] = (rand() % 256) & (opcode & 0x00FF);
            break;
        case 0xD000:
            // Implement drawing logic here
            break;
        case 0xE000:
            switch (opcode & 0x00FF) {
                case 0x009E:
                    if (vm->keys[vm->V[x]]) {
                        vm->pc += 2;
                    }
                    break;
                case 0x00A1:
                    if (!vm->keys[vm->V[x]]) {
                        vm->pc += 2;
                    }
                    break;
            }
            break;
        case 0xF000:
            switch (opcode & 0x00FF) {
                case 0x0055:
                    for (uint8_t i = 0; i <= x; i++) {
                        vm->ram[vm->I + i] = vm->V[i];
                    }
                    vm->I += x + 1;
                    break;
                case 0x0065:
                    for (uint8_t i = 0; i <= x; i++) {
                        vm->V[i] = vm->ram[vm->I + i];
                    }
                    vm->I += x + 1;
                    break;
            }
            break;
        case 0x0000:
            if (opcode == 0x00EE) {
                if (vm->sp > 0) {
                    vm->pc = vm->stack[--vm->sp];
                } else {
                    printf("Stack underflow\n");
                    return false;
                }
            } else {
                printf("Unknown 0x0000 opcode: 0x%04X\n", opcode);
                return false;
            }
            break;
        case 0x1000:
            vm->pc = opcode & 0x0FFF;
            break;
        case 0x2000:
            if (vm->sp < STACK_SLOTS) {
                vm->stack[vm->sp++] = vm->pc;
                vm->pc = opcode & 0x0FFF;
            } else {
                printf("Stack overflow\n");
                return false;
            }
            break;
        case 0x3000: {
            uint8_t nn = opcode & 0x00FF;
            if (vm->V[x] == nn) {
                vm->pc += 2;
            }
            break;
        }
        case 0x4000: {
            uint8_t nn = opcode & 0x00FF;
            if (vm->V[x] != nn) {
                vm->pc += 2;
            }
            break;
        }
        case 0x5000: {
            if (vm->V[x] == vm->V[y]) {
                vm->pc += 2;
            }
            break;
        }
        case 0x6000: {
            uint8_t nn = opcode & 0x00FF;
            vm->V[x] = nn;
            break;
        }
        case 0x7000: {
            uint8_t nn = opcode & 0x00FF;
            vm->V[x] += nn;
            break;
        }
        case 0x8000:
            switch (opcode & 0x000F) {
                case 0x0000:
                    vm->V[x] = vm->V[y];
                    vm->V[0xF] = 0;
                    break;
                case 0x0001:
                    vm->V[x] |= vm->V[y];
                    vm->V[0xF] = 0;
                    break;
                case 0x0002:
                    vm->V[x] &= vm->V[y];
                    vm->V[0xF] = 0;
                    break;
                case 0x0003:
                    vm->V[x] ^= vm->V[y];
                    vm->V[0xF] = 0;
                    break;
                case 0x0004:
                printf("%u",vm->V[y]);
                    
                      
                    if (vm->V[x] + vm->V[y] > 0xFF) {
                        vm->V[0xF] = 1;
                    } else {
                        vm->V[0xF] = 0;
                    }
                    vm->V[x] = (vm->V[x] + vm->V[y]) & 0xFF;
                    
                    break;
                case 0x0005:
                    vm->V[0xF] = (vm->V[x] > vm->V[y]) ? 1 : 0;
                    vm->V[x] -= vm->V[y];
                    break;
                case 0x0006:
                    vm->V[0xF] = 0;              // Clear carry flag (VF)
                    vm->V[0xF] = vm->V[x] & 0x1; // Set VF to the least significant bit of V[x]
                    vm->V[x] >> 1;              // Shift V[x] right by 1
                case 0x0007:
                    vm->V[0xF] = (vm->V[y] > vm->V[x]) ? 1 : 0;
                    vm->V[x] = vm->V[y] - vm->V[x];
                    break;
                case 0x000E:
                    vm->V[0xF] = (vm->V[x] & 0x80) >> 7;
                    vm->V[x] <<= 1;
                    break;
                default:
                    printf("Unknown 0x8000 opcode: 0x%04X\n", opcode);
                    return false;
            }
            break;
        case 0x9000: {
            if (vm->V[x] != vm->V[y]) {
                vm->pc += 2;
            }
            break;
        }
        default:
            printf("Unknown opcode: 0x%04X\n", opcode);
            return false;
    }

    if (vm->delay_timer > 0) vm->delay_timer--;
    if (vm->sound_timer > 0) {
        *sound = true;
        vm->sound_timer--;
    } else {
        *sound = false;
    }

    return true;
}


// Function to get the current value of the program counter
uint16_t chip8_get_pc(struct chip8_vm *vm) {
    return vm->pc; // Return the value of the program counter
}

// Function to set a new value for the program counter
void chip8_set_pc(struct chip8_vm *vm, uint16_t new_pc) {
    vm->pc = new_pc; // Set the program counter to the new value
}

// Function to get the current value of the index register
uint16_t chip8_get_i(struct chip8_vm *vm) {
    return vm->I; // Return the value of the index register
}

// Function to set a new value for the index register
void chip8_set_i(struct chip8_vm *vm, uint16_t new_i) {
    vm->I = new_i; // Set the index register to the new value
}

// Function to get the value of a specific V register
uint8_t chip8_get_vr(struct chip8_vm *vm, int index) {
    if (index >= 0 && index < 16) {
        return vm->V[index]; // Return the value of the V register at the specified index
    }
    return 0; // Return 0 if the index is out of bounds
}

// Function to set a new value for a specific V register
void chip8_set_vr(struct chip8_vm *vm, int index, uint8_t new_val) {
    if (index >= 0 && index < 16) {
        vm->V[index] = new_val; // Set the V register at the specified index to the new value
    }
}

// Function to get the value of a specific memory address
uint8_t chip8_get_ram(struct chip8_vm *vm, uint16_t address) {
    if (address < RAM_SIZE) {
        return vm->ram[address]; // Return the value at the specified memory address
    }
    return 0; // Return 0 if the address is out of bounds
}

// Function to set a new value for a specific memory address
void chip8_set_ram(struct chip8_vm *vm, uint16_t address, uint8_t new_val) {
    if (address < RAM_SIZE) {
        vm->ram[address] = new_val; // Set the memory address to the new value
    }
}