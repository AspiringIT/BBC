#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "chip8.h"


// how wide should the "Test XX (blah blah).......OK/FAIL" lines be (up to/not including the "OK/FAIL")
#define TEST_BANNER_WIDTH 72

// fail-test-with-incriminating-message macros (assumes a local "cleanup:" label it can goto)
#define FAIL(msg) do{printf("FAIL\n\t%s:%d: %s\n", __FILE__, __LINE__, msg);goto cleanup;}while(0);
#define FAILF(fmt, ...) do{printf("FAIL\n\t%s:%d:" fmt "\n", __FILE__, __LINE__, __VA_ARGS__);goto cleanup;}while(0);

// macros to assert or cycle-then-assert various aspects of CHIP-8 VM state (via the debugging API)
// (all assume local variables vm (type struct chip8_vm), keys (type uint16_t), vticks (type size_t), and sound (type bool))
#define CYCLE_PC(pcval) if (!chip8_cycle(&vm, keys, vticks, &sound) || (chip8_get_pc(&vm) != (pcval))) {\
    FAILF("cycle failed and/or PC != 0x%04x (0x%04x instead)", (pcval), chip8_get_pc(&vm));\
}
#define ASSERT_PC(pcval) if (chip8_get_pc(&vm) != (pcval)) {\
    FAILF("PC != 0x%04x (0x%04x instead)", (pcval), chip8_get_pc(&vm));\
}
#define CYCLE_VX(vx, vxval) if (!chip8_cycle(&vm, keys, vticks, &sound) || ((chip8_get_vr(&vm, (vx)) != (vxval)))) {\
    FAILF("cycle failed and/or V%X != 0x%02x (0x%02x instead)", (vx), (vxval), chip8_get_vr(&vm, (vx)));\
}
#define ASSERT_VX(vx, vxval) if (chip8_get_vr(&vm, (vx)) != (vxval)) {\
    FAILF("V%X != 0x%02x (0x%02x instead)", (vx), (vxval), chip8_get_vr(&vm, (vx)));\
}
#define CYCLE_I(ival) if (!chip8_cycle(&vm, keys, vticks, &sound) || (chip8_get_i(&vm) != (ival))) {\
    FAILF("cycle failed and/or I != 0x%04x (0x%04x instead)", (ival), chip8_get_i(&vm));\
}
#define ASSERT_I(ival) if (chip8_get_i(&vm) != (ival)) {\
    FAILF("I != 0x%04x (0x%04x instead)", (ival), chip8_get_i(&vm));\
}
#define ASSERT_RAMB(address, bval) if (chip8_get_ram(&vm, (address)) != (bval)) {\
    FAILF("RAM[0x%X] != 0x%02x (0x%02x instead)", (address), (bval), chip8_get_ram(&vm, (address)));\
}
#define ASSERT_RAMW(address, wval) do{\
    uint8_t hi = ((wval) & 0xff00) >> 8;\
    uint8_t lo = ((wval) & 0x00ff);\
    if ((chip8_get_ram(&vm, (address)) != hi) || (chip8_get_ram(&vm, (address)+1) != lo)) {\
        FAILF("RAM[0x%X] != 0x%04x (0x%02x%02x instead)", (address), (wval), chip8_get_ram(&vm, (address)), chip8_get_ram(&vm, (address)+1));\
    }\
} while(0);

// helper to print each major test's number/description/padded line of "."s
void test_banner(int n, char *desc) {
    int width = printf("Test %d (%s)", n, desc);
    while (width++ < TEST_BANNER_WIDTH) {
        putchar('.');
    }
}

// metaprogramming macro to visually simplify defining 2-byte-big-endian instructions in byte arrays
#define I(word) ((word & 0xff00) >> 8), (word & 0x00ff)

// CHIP-8 program ROM for test1
uint8_t test_prog1[] = {
/* 0x200 */ I(0x1204), // jump to address 204 (third instruction)
/* 0x202 */ I(0x0000), // TRAP (shouldn't land here)
/* 0x204 */ I(0x6028), // V0 = 0x28 (40)
/* 0x206 */ I(0x6129), // V1 = 0x29 (41)
/* 0x208 */ I(0x622A), // V2 = 0x2A (42)
/* 0x20A */ I(0x632B), // V3 = 0x2B (43)
/* 0x20C */ I(0x7002), // V0 += 2 ( -> 0x2A)
/* 0x20E */ I(0x7102), // V1 += 2 ( -> 0x2B)
/* 0x210 */ I(0x7202), // V2 += 2 ( -> 0x2C)
/* 0x212 */ I(0x7302), // V3 += 2 ( -> 0x2D)
/* 0x214 */ I(0x302A), // skip if V0 == 0x2A (TAKEN)
/* 0x216 */ I(0x0000), // TRAP (shouldn't land here)
/* 0x218 */ I(0x402A), // skip if V0 != 0x2A (NOT TAKEN)
/* 0x21A */ I(0x4299), // skip if V2 != 0x99 (TAKEN)
/* 0x21C */ I(0x0000), // TRAP (shouldn't land here)
/* 0x21E */ I(0x1230), // skip over the subroutine ahead
/* 0x220 */ I(0x9120), // skip if V1 != V2 (TAKEN)
/* 0x222 */ I(0x0000), // TRAP (shouldn't land here)
/* 0x224 */ I(0xA20A), // I = 0x20A
/* 0x226 */ I(0x7002), // V0 += 2 ( -> 0x2C)
/* 0x228 */ I(0x5020), // skip if V0 == V2 (TAKEN)
/* 0x22A */ I(0x0000), // TRAP (shouldn't land here)
/* 0x22C */ I(0xF165), // V0 = RAM[0x20A], V1 = RAM[0x20B, I = 0x20C
/* 0x22E */ I(0x00EE), // return from subroutine
/* 0x230 */ I(0x2220), // call 0x220
/* 0x232 */ I(0xA300), // I = 0x300
/* 0x234 */ I(0xF355), // store V0-V3 into RAM[I..I+3] (and I += 4)
};

// general control-flow tests (no I/O, no ALU-with-carry-flag)
bool test1() {
    bool ret = false;
    struct chip8_vm vm;
    uint16_t keys = 0u;
    size_t vticks = 0;
    bool sound = false;

    if (!chip8_load(&vm, test_prog1, sizeof test_prog1)) {
        FAIL("chip8_load can't load test_prog1");
    }
    ASSERT_PC(0x200);
    ASSERT_RAMB(0x200, test_prog1[0]);
    ASSERT_RAMB(0x201, test_prog1[1]);
    
    CYCLE_PC(0x204);
    CYCLE_VX(0, 0x28); 
    CYCLE_VX(1, 0x29); 
    CYCLE_VX(2, 0x2A); 
    CYCLE_VX(3, 0x2B);
    CYCLE_VX(0, 0x2A); 
    CYCLE_VX(1, 0x2B); 
    CYCLE_VX(2, 0x2C); 
    CYCLE_VX(3, 0x2D);
    CYCLE_PC(0x218);
    CYCLE_PC(0x21A);
    CYCLE_PC(0x21E);
    CYCLE_PC(0x230);
    CYCLE_PC(0x220);
    CYCLE_PC(0x224);
    CYCLE_I(0x20A);
    CYCLE_VX(0, 0x2C); 
    CYCLE_PC(0x22C);
    CYCLE_I(0x20C);
    ASSERT_VX(0, 0x63);
    ASSERT_VX(1, 0x2B);
    CYCLE_PC(0x232);
    CYCLE_I(0x300);
    CYCLE_I(0x304);
    ASSERT_RAMW(0x300, 0x632B);
    ASSERT_RAMW(0x302, 0x2C2D);

    ret = true;
cleanup:
    return ret;
}

// CHIP-8 program ROM for test2
uint8_t test_prog2[] = {
/* 0x200 */ I(0x6001), // V0 = 0x01
/* 0x202 */ I(0x6102), // V1 = 0x02
/* 0x204 */ I(0x62FE), // V2 = 0xfe
/* 0x206 */ I(0x63FF), // V3 = 0xff
/* 0x208 */ I(0x8400), // V4 = V0 (0x01)
/* 0x20A */ I(0x8011), // V0 |= V1 (0x01 | 0x02 == 0x03; VF = 0)
/* 0x20C */ I(0x8022), // V0 &= V2 (0x03 & 0xfe == 0x02; VF = 0)
/* 0x20E */ I(0x70FF), // V0 += 0xFF (0x02 + 0xff == 0x01; VF = unchanged)
/* 0x210 */ I(0x8303), // V3 ^= V0 (0xff ^ 0x01 == 0xfe; VF = 0)
/* 0x212 */ I(0x8303), // V3 ^= V0 (0xfe ^ 0x01 == 0xff; VF = 0)
/* 0x214 */ I(0x8024), // V0 += V2 (0x01 + 0xfe == 0xff; VF = 0)
/* 0x216 */ I(0x6001), // V0 = 0x01
/* 0x218 */ I(0x8034), // V0 += V3 (0x01 + 0xff == 0x00; VF = 1)
/* 0x21A */ I(0x6001), // V0 = 0x01
/* 0x21C */ I(0x8105), // V1 -= V0 (0x02 - 0x01 == 0x01; VF = 1)
/* 0x21E */ I(0x6102), // V1 = 0x02
/* 0x220 */ I(0x8015), // V0 -= V1 (0x01 - 0x02 == 0xff; VF = 0)
/* 0x222 */ I(0x6001), // V0 = 0x01
/* 0x224 */ I(0x8017), // V0 = V1 - V0 (0x02 - 0x01 == 0x01; VF = 1)
/* 0x226 */ I(0x6001), // V0 = 0x01
/* 0x228 */ I(0x8107), // V1 = V0 - V1 (0x01 - 0x02 == 0xff; VF = 0)
/* 0x22A */ I(0x6102), // V1 = 0x02
/* 0x22C */ I(0x8E06), // VE = V0 >> 1 (0x1 >> 1 == 0x00; VF = 1)
/* 0x22E */ I(0x8E16), // VE = V1 >> 1 (0x2 >> 1 == 0x01; VF = 0)
/* 0x230 */ I(0x6A7F), // VA = 0x7f
/* 0x232 */ I(0x8EAE), // VE = VA << 1 (0x7f << 1 == 0xfe; VF = 0)
/* 0x234 */ I(0x8E3E), // VE = V3 << 1 (0xff << 1 == 0xfe; VF = 1)
};

// core ALU (8XYN) operations with carry flag (VF) setting/clearing tests
bool test2() {
    bool ret = false;
    struct chip8_vm vm;
    uint16_t keys = 0u;
    size_t vticks = 0;
    bool sound = false;

    if (!chip8_load(&vm, test_prog2, sizeof test_prog2)) {
        FAIL("chip8_load can't load test_prog2");
    }
    CYCLE_VX(0, 0x01);
    CYCLE_VX(1, 0x02);
    CYCLE_VX(2, 0xfe);
    CYCLE_VX(3, 0xff);
    CYCLE_VX(4, 0x01);
    chip8_set_vr(&vm, 15, 1); // set carry flag register before OR operation (to test original CHIP-8 clear-VF-on-bitop quirk)
    CYCLE_VX(0, 0x03);
    ASSERT_VX(15, 0);
    chip8_set_vr(&vm, 15, 1); // set carry flag register before AND operation (to test original CHIP-8 clear-VF-on-bitop quirk)
    CYCLE_VX(0, 0x02);
    ASSERT_VX(15, 0);
    chip8_set_vr(&vm, 15, 42); // set carry flag register to non [0, 1] value before non-ALU addition (prove that VF is unchanged)
    CYCLE_VX(0, 0x01);
    ASSERT_VX(15, 42);
    CYCLE_VX(3, 0xfe);
    ASSERT_VX(15, 0);
    CYCLE_VX(3, 0xff);
    chip8_set_vr(&vm, 15, 1); // set carry flag before non-carrying ADD to prove it sets VF=0
    CYCLE_VX(0, 0xff);
    ASSERT_VX(15, 0);
    CYCLE_VX(0, 0x01);
    CYCLE_VX(0, 0x00);
    ASSERT_VX(15, 1);
    CYCLE_VX(0, 0x01);
    chip8_set_vr(&vm, 15, 0); // clear carry flag before non-borrowing SUB to prove it sets VF=1
    CYCLE_VX(1, 0x01);
    ASSERT_VX(15, 1);
    CYCLE_VX(1, 0x02);
    CYCLE_VX(0, 0xff);
    ASSERT_VX(15, 0);
    CYCLE_VX(0, 0x01);
    chip8_set_vr(&vm, 15, 0); // clear carry flag before non-borrowing BUS to prove it sets VF=1
    CYCLE_VX(0, 0x01);
    ASSERT_VX(15, 1);
    CYCLE_VX(0, 0x01);
    CYCLE_VX(1, 0xff);
    ASSERT_VX(15, 0);
    CYCLE_VX(1, 0x02);
    chip8_set_vr(&vm, 15, 0); // clear carry flag before bit-shift-off-right to prove it sets VF=1
    CYCLE_VX(14, 0x00);
    ASSERT_VX(15, 1);
    CYCLE_VX(14, 0x01);
    ASSERT_VX(15, 0);
    CYCLE_VX(10, 0x7f);
    CYCLE_VX(14, 0xfe);
    ASSERT_VX(15, 0);
    CYCLE_VX(14, 0xfe);
    ASSERT_VX(15, 1);

    ret = true;
cleanup:
    return ret;
}

// CHIP-8 program ROM for test3
// (not a real program, just a vector of instructions)
uint8_t test_prog3[] = {
/* 0x200 */ I(0x1200), // jump-to-self (infinite loop useful for chewing cycles)
/* 0x202 */ I(0xE09E), // skip-if-key-VX-is-down
/* 0x204 */ I(0xE0A1), // skip-if-key-VX-is-NOT-down
/* 0x206 */ I(0xF107), // copy delay timer into V1
/* 0x208 */ I(0xF115), // set delay timer to value from V1
/* 0x20A */ I(0xF218), // set sound timer to value from V2
/* 0x20C */ I(0xF30A), // wait for keypress, store index in V3
/* 0x20E */ I(0x8000), // V0 = V0 (i.e., NOOP)
/* 0x210 */ I(0x1210), // another infinite loop in place
};

// timer, sound-state, and key status/press tests
bool test3() {
    bool ret = false;
    struct chip8_vm vm;
    uint16_t keys = 0;
    size_t vticks = 0;
    bool sound = false;

    if (!chip8_load(&vm, test_prog3, sizeof test_prog3)) {
        FAIL("chip8_load can't load test_prog3");
    }

    // test Ex9E (skip-if-key-down)
    chip8_set_pc(&vm, 0x202);
    chip8_set_vr(&vm, 0, 0); // key 0 (not set; should fail to skip)
    keys = 0x0002;
    CYCLE_PC(0x204); // no-skip
    chip8_set_pc(&vm, 0x202);
    chip8_set_vr(&vm, 0, 1); // key 1 (set; should skip)
    CYCLE_PC(0x206); // yes-skip

    // test ExA1 (skip-if-key-up)
    chip8_set_pc(&vm, 0x204);
    chip8_set_vr(&vm, 0, 0); // key 0 (not set; should skip)
    CYCLE_PC(0x208); // yes-skip
    chip8_set_pc(&vm, 0x204);
    chip8_set_vr(&vm, 0, 1); // key 1 (set; should not skip)
    CYCLE_PC(0x206); // no-skip

    // test Fx15/Fx07 (the delay timer)
    //----------------------------------

    // simple setting/getting
    keys = 0;
    chip8_set_pc(&vm, 0x208); // F115 [set timer to V1 (3)]
    chip8_set_vr(&vm, 1, 3);
    CYCLE_PC(0x20A);
    chip8_set_pc(&vm, 0x206); // F107 [read timer into V1]
    chip8_set_vr(&vm, 1, 0);
    CYCLE_VX(1, 3);

    // single-step vtick increment 
    for (int i = 0; i < 1000; ++i) { // no matter how many cycles, until vtick goes up, the delay timer should stay the same
        chip8_set_pc(&vm, 0x206);
        chip8_set_vr(&vm, 1, 0xff);
        CYCLE_VX(1, 3);
    }
    chip8_set_pc(&vm, 0x206);
    chip8_set_vr(&vm, 1, 0xff);
    ++vticks;
    CYCLE_VX(1, 2);

    // multi-step vtick increment
    for (int i = 0; i < 1000; ++i) { // no matter how many cycles, until vtick goes up, the delay timer should stay the same
        chip8_set_pc(&vm, 0x206);
        chip8_set_vr(&vm, 1, 0xff);
        CYCLE_VX(1, 2);
    }
    chip8_set_pc(&vm, 0x206);
    chip8_set_vr(&vm, 1, 0xff);
    vticks += 2;
    CYCLE_VX(1, 0);

    // sound timer tests
    //--------------------------
    chip8_set_pc(&vm, 0x20A); // set sound timer from V2
    chip8_set_vr(&vm, 2, 1);  // CHIP-8 quirk: sound shouldn't go on unless timer is set to >1
    sound = false;
    CYCLE_PC(0x20C);
    if (sound) FAIL("minimum sound activation tick test failed");

    chip8_set_pc(&vm, 0x20A); 
    chip8_set_vr(&vm, 2, 2);
    sound = false;
    CYCLE_PC(0x20C);
    if (!sound) FAIL("sound activation test failed");

    chip8_set_pc(&vm, 0x200); // spin in an infinite loop for a while while testing sound timer
    for (int i = 0; i < 1000; ++i) {
        CYCLE_PC(0x200);
        if (!sound) FAILF("sound deactivated early (loop #%d)", i);
    }
    ++vticks;
    CYCLE_PC(0x200);
    if (!sound) FAIL("sound deactivated early (1 vtick)");
    ++vticks;
    CYCLE_PC(0x200);
    if (sound) FAIL("sound failed to deactivate on timeout");

    // wait-for-keystroke test
    //---------------------------
    chip8_set_pc(&vm, 0x20C); // wait for keypress, store in V3
    keys = 0x8000; // just for fun, start with key 15 ('F') already pressed (so ignored)
    for (int i = 0; i < 1000; ++i) {
        CYCLE_PC(0x20E); // should stay in place until the keystate changes!
    }
    keys |= 0x10; // "press" key 4 down
    CYCLE_PC(0x20E); // should stay in place until the keystate changes!
    for (int i = 0; i < 1000; ++i) {
        CYCLE_PC(0x20E); // no matter how long!
    }
    keys &= ~0x10; // "release" key 4 up
    CYCLE_PC(0x210); // should have now advanced to the next instruction...
    ASSERT_VX(3, 0x04); // ...and V3 should contain the value 0x04

    // wait-for-keystroke-WHILE-DELAY-TIMER-WORKS test
    //--------------------------------------------------
    chip8_set_vr(&vm, 1, 2);
    chip8_set_pc(&vm, 0x208); // F115 [set timer to V1 (2)]
    CYCLE_PC(0x20A);

    chip8_set_pc(&vm, 0x20C); // wait for keypress, store in V3
    keys = 0x8000; 
    CYCLE_PC(0x20E);

    keys |= 0x20; // "press" key 5 down
    CYCLE_PC(0x20E);

    ++vticks;
    CYCLE_PC(0x20E); // PC stays put while we wait (but bump the vticks count)

    keys &= ~0x20; // "release" key 5 up
    CYCLE_PC(0x210); // should have now advanced to the next instruction...
    ASSERT_VX(3, 0x05); // ...and V3 should contain the value 0x05

    chip8_set_vr(&vm, 1, 0xff);
    chip8_set_pc(&vm, 0x206); // read delay timer ticks into V1 (which we've set to garbage)
    CYCLE_VX(1, 1); // should be ONE tick left (started with TWO)

    ret = true;
cleanup:
    return ret;
}

// test suite entry point (no CLI args, just run all the tests until completion or first failure)
int main() {
    int ret = EXIT_FAILURE;

    test_banner(0, "compiling, linking and running");
    puts("OK"); // implied by getting to this point

    test_banner(1, "control flow, load/store, basic register ops");
    if (test1()) { puts("OK"); } else { goto cleanup; }
    
    test_banner(2, "core ALU [8XY?] operations with carry flag [VF]");
    if (test2()) { puts("OK"); } else { goto cleanup; }

    test_banner(3, "timer, sound-state, and key status/press tests");
    if (test3()) { puts("OK"); } else { goto cleanup; }

    ret = EXIT_SUCCESS;
cleanup:
    return ret;
}
