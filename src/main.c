
#include <gba_console.h>
#include <gba_input.h>
#include <gba_interrupt.h>
#include <gba_systemcalls.h>
#include <gba_video.h>
#include <stdio.h>
#include <stdlib.h>

#include "image.h"
#include "save_backend.h"

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

static const u8 digit_3x5[10][5] = {
    {0x7, 0x5, 0x5, 0x5, 0x7}, {0x2, 0x6, 0x2, 0x2, 0x7}, {0x7, 0x1, 0x7, 0x4, 0x7}, {0x7, 0x1, 0x7, 0x1, 0x7}, {0x5, 0x5, 0x7, 0x1, 0x1},
    {0x7, 0x4, 0x7, 0x1, 0x7}, {0x7, 0x4, 0x7, 0x5, 0x7}, {0x7, 0x1, 0x1, 0x1, 0x1}, {0x7, 0x5, 0x7, 0x5, 0x7}, {0x7, 0x5, 0x7, 0x1, 0x7},
};

static void draw_status_box(u16 color) {
    for (int y = 4; y < 20; y++) {
        for (int x = 4; x < 20; x++) {
            vid_mem[y * SCREEN_WIDTH + x] = color;
        }
    }
}

static void draw_digit_3x5(int x, int y, int digit, u16 color) {
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 3; col++) {
            if (digit_3x5[digit][row] & (1 << (2 - col))) {
                vid_mem[(y + row) * SCREEN_WIDTH + (x + col)] = color;
            }
        }
    }
}

static void draw_counter(u32 counter) {
    char buffer[11];
    snprintf(buffer, sizeof(buffer), "%lu", (unsigned long)counter);

    const int area_x = 4;
    const int area_y = SCREEN_HEIGHT - 12;
    const int area_w = 96;
    const int area_h = 8;

    for (int y = 0; y < area_h; y++) {
        for (int x = 0; x < area_w; x++) {
            vid_mem[(area_y + y) * SCREEN_WIDTH + (area_x + x)] = 0x0000;
        }
    }

    int x = area_x + 2;
    for (int i = 0; buffer[i] != '\0'; i++) {
        int digit = buffer[i] - '0';
        if (digit >= 0 && digit <= 9) {
            draw_digit_3x5(x, area_y + 1, digit, 0x7FFF);
            x += 4;
        }
    }
}

static void save_pack_counter(u32 value, u8 *out) {
    out[0] = 'C';
    out[1] = 'N';
    out[2] = 'T';
    out[3] = '1';
    out[4] = (u8)(value & 0xFF);
    out[5] = (u8)((value >> 8) & 0xFF);
    out[6] = (u8)((value >> 16) & 0xFF);
    out[7] = (u8)((value >> 24) & 0xFF);
}

static int save_unpack_counter(const u8 *in, u32 *value) {
    if (in[0] != 'C' || in[1] != 'N' || in[2] != 'T' || in[3] != '1') {
        return 0;
    }

    *value = (u32)in[4] | ((u32)in[5] << 8) | ((u32)in[6] << 16) | ((u32)in[7] << 24);
    return 1;
}

static int load_counter_from_backend(u32 *counter) {
    u8 payload[8];
    if (!save_backend_read8(payload)) {
        *counter = 0;
        return 0;
    }

    if (!save_unpack_counter(payload, counter)) {
        *counter = 0;
        return 0;
    }

    return 1;
}

static int save_counter_to_backend(u32 counter) {
    u8 payload[8];
    save_pack_counter(counter, payload);
    return save_backend_write8(payload);
}

static void wait_vblank(void) {
    while (REG_VCOUNT >= 160) {
    }
    while (REG_VCOUNT < 160) {
    }
}

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
        vid_mem[i] = data[i];
    }

    save_backend_init();

    u32 counter = 0;

    if (load_counter_from_backend(&counter)) {
        draw_status_box(0x03E0);
    } else {
        draw_status_box(0x7FFF);
    }
    draw_counter(counter);

    // ((unsigned short*)0x06000000)[120+80*240] = 0x001F;
    // ((unsigned short*)0x06000000)[136+80*240] = 0x03E0;
    // ((unsigned short*)0x06000000)[120+96*240] = 0x7C00;

    // u32 i = 0;
    while (1) {
        // VBlankIntrWait();

        // iprintf("\x1b[10;5HSimon says Awesome %ld!\n", i);

        key_poll();

        if (key_is_down(KEY_A) && key_was_up(KEY_A)) {
            counter++;
            draw_counter(counter);

            int ok = save_counter_to_backend(counter);
            draw_status_box(ok ? 0x03FF : 0x7C1F);
        }

        if (key_is_down(KEY_B) && key_was_up(KEY_B)) {
            counter = 0;
            draw_counter(counter);

            int ok = save_counter_to_backend(counter);
            draw_status_box(ok ? 0x03E0 : 0x7C1F);
        }

        if (key_is_down(KEY_L) && key_was_up(KEY_L)) {
            save_backend_t backend = save_backend_get();
            if (backend == SAVE_BACKEND_SRAM) {
                save_backend_set(SAVE_BACKEND_FLASH_END_8K);
                if (load_counter_from_backend(&counter)) {
                    draw_status_box(0x03E0);
                } else {
                    draw_status_box(0x7C00);
                }
            } else {
                save_backend_set(SAVE_BACKEND_SRAM);
                if (load_counter_from_backend(&counter)) {
                    draw_status_box(0x03E0);
                } else {
                    draw_status_box(0x7C00);
                }
            }
            draw_counter(counter);
        }
        wait_vblank();
    }
}
