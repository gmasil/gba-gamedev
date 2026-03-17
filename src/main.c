
#include <gba_console.h>
#include <gba_input.h>
#include <gba_interrupt.h>
#include <gba_systemcalls.h>
#include <gba_video.h>
#include <stdio.h>
#include <stdlib.h>

#include "data.h"

u16 __key_curr = 0;
u16 __key_prev = 0;

#define KEY_A 0x0001
#define KEY_B 0x0002
#define KEY_SELECT 0x0004
#define KEY_START 0x0008
#define KEY_RIGHT 0x0010
#define KEY_LEFT 0x0020
#define KEY_UP 0x0040
#define KEY_DOWN 0x0080
#define KEY_R 0x0100
#define KEY_L 0x0200

#define KEY_MASK 0x03FF

inline void key_poll() {
    __key_prev = __key_curr;
    __key_curr = ~REG_KEYINPUT & KEY_MASK;
}

inline u32 key_curr_state() {
    return __key_curr;
}
inline u32 key_prev_state() {
    return __key_prev;
}
inline u32 key_is_down(u32 key) {
    return __key_curr & key;
}
inline u32 key_is_up(u32 key) {
    return ~__key_curr & key;
}
inline u32 key_was_down(u32 key) {
    return __key_prev & key;
}
inline u32 key_was_up(u32 key) {
    return ~__key_prev & key;
}

#define MEM_IO 0x04000000
#define MEM_VRAM 0x06000000

// #define REG_DISPCNT     *((volatile u32*)(MEM_IO+0x0000))

// === (from tonc_memdef.h) ===========================================

// --- REG_DISPCNT defines ---
#define DCNT_MODE0 0x0000
#define DCNT_MODE1 0x0001
#define DCNT_MODE2 0x0002
#define DCNT_MODE3 0x0003
#define DCNT_MODE4 0x0004
#define DCNT_MODE5 0x0005
// layers
#define DCNT_BG0 0x0100
#define DCNT_BG1 0x0200
#define DCNT_BG2 0x0400
#define DCNT_BG3 0x0800
#define DCNT_OBJ 0x1000

// === (from tonc_video.h) ============================================

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 160

#define vid_mem ((u16 *)MEM_VRAM)

//---------------------------------------------------------------------------------
// Program entry point
//---------------------------------------------------------------------------------
int main(void) {
    //---------------------------------------------------------------------------------

    // the vblank interrupt must be enabled for VBlankIntrWait() to work
    // since the default dispatcher handles the bios flags no vblank handler
    // is required
    // irqInit();
    // irqEnable(IRQ_VBLANK);

    // consoleDemoInit();

    // ansi escape sequence to set print co-ordinates
    // /x1b[line;columnH
    // iprintf("\x1b[10;5HSimon says Awesome!\n");

    REG_DISPCNT = DCNT_MODE3 | DCNT_BG2;

    for (u32 i = 0; i < 240 * 160; i++) {
        vid_mem[i] = data[240 * 160 - i];
    }

    // ((unsigned short*)0x06000000)[120+80*240] = 0x001F;
    // ((unsigned short*)0x06000000)[136+80*240] = 0x03E0;
    // ((unsigned short*)0x06000000)[120+96*240] = 0x7C00;

    // u32 i = 0;
    while (1) {
        // VBlankIntrWait();

        // iprintf("\x1b[10;5HSimon says Awesome %ld!\n", i);

        // key_poll();

        // if (key_is_down(KEY_A)) {
        //     i++;
        // }
    }
}
