#include <gba_types.h>

#define MEM_IO 0x04000000
#define MEM_VRAM 0x06000000

#define REG_DISPCNT (*(volatile u16 *)(MEM_IO + 0x0000))
#define REG_VCOUNT (*(volatile u16 *)(MEM_IO + 0x0006))
#define REG_KEYINPUT (*(volatile u16 *)(MEM_IO + 0x0130))

#define REG_DMA3SAD (*(volatile const void **)(MEM_IO + 0x00D4))
#define REG_DMA3DAD (*(volatile void **)(MEM_IO + 0x00D8))
#define REG_DMA3CNT (*(volatile u32 *)(MEM_IO + 0x00DC))

#define DCNT_MODE3 0x0003
#define DCNT_BG2 0x0400

#define KEY_A 0x0001
#define KEY_RIGHT 0x0010
#define KEY_LEFT 0x0020
#define KEY_UP 0x0040
#define KEY_DOWN 0x0080
#define KEY_MASK 0x03FF

#define CURSOR_REPEAT_DELAY 10
#define CURSOR_REPEAT_RATE 3

#define DMA_ENABLE (1u << 31)
#define DMA_32BIT (1u << 26)

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 160
#define SCREEN_PIXELS (SCREEN_WIDTH * SCREEN_HEIGHT)

#define WORLD_SIZE 16

#define TILE_W 12
#define TILE_H 8
#define TILE_W_HALF (TILE_W / 2)
#define TILE_H_HALF (TILE_H / 2)

#define HEIGHT_STEP 4

#define ORIGIN_X 120
#define ORIGIN_Y 12

#define RGB5(r, g, b) ((u16)(((r) & 31) | (((g) & 31) << 5) | (((b) & 31) << 10)))

#define SKY_COLOR RGB5(18, 20, 24)
#define WATER_SPARKLE RGB5(19, 24, 31)
#define WATER_WAVE_LIGHT RGB5(10, 16, 30)
#define WATER_WAVE_DARK RGB5(4, 8, 19)

#define CURSOR_GOLD_DARK RGB5(18, 13, 1)
#define CURSOR_GOLD RGB5(27, 20, 4)
#define CURSOR_GOLD_BRIGHT RGB5(31, 25, 7)
#define CURSOR_GOLD_GLINT RGB5(31, 31, 18)

#define vram_mem ((u16 *)MEM_VRAM)

typedef struct {
    u16 base;
    u16 light;
    u16 dark;
    u16 side_left;
    u16 side_right;
    u8 pattern[8];
} tile_sprite_t;

static const tile_sprite_t g_tile_sprites[3] = {
    {
        RGB5(7, 23, 7),
        RGB5(11, 28, 11),
        RGB5(4, 15, 4),
        RGB5(5, 14, 5),
        RGB5(6, 17, 6),
        {0xAA, 0x55, 0xAA, 0x55, 0x99, 0x66, 0x99, 0x66},
    },
    {
        RGB5(23, 18, 9),
        RGB5(28, 23, 13),
        RGB5(14, 11, 5),
        RGB5(12, 9, 5),
        RGB5(16, 12, 6),
        {0x81, 0x24, 0x18, 0x42, 0x24, 0x18, 0x42, 0x81},
    },
    {
        RGB5(5, 10, 23),
        RGB5(11, 17, 31),
        RGB5(3, 6, 14),
        RGB5(2, 5, 11),
        RGB5(3, 7, 14),
        {0x3C, 0x66, 0xC3, 0x66, 0x3C, 0x66, 0xC3, 0x66},
    },
};

static const u8 g_span_lut[TILE_H + 1] = {0, 1, 3, 4, 6, 4, 3, 1, 0};

static u8 g_world_kind[WORLD_SIZE][WORLD_SIZE];
static u8 g_world_height[WORLD_SIZE][WORLD_SIZE];

static u16 g_backbuffer[SCREEN_PIXELS] __attribute__((section(".ewram"), aligned(4)));
static u16 g_worldbuffer[SCREEN_PIXELS] __attribute__((section(".ewram"), aligned(4)));

static u32 g_rng_state = 0x42C0FFEEu;
static u16 g_key_curr  = 0;
static u16 g_key_prev  = 0;

static u32 g_frame_counter = 0;
static int g_cursor_x      = WORLD_SIZE / 2;
static int g_cursor_y      = WORLD_SIZE / 2;
static u8 g_repeat_left    = 0;
static u8 g_repeat_right   = 0;
static u8 g_repeat_up      = 0;
static u8 g_repeat_down    = 0;

static void key_poll(void) {
    g_key_prev = g_key_curr;
    g_key_curr = (u16)(~REG_KEYINPUT) & KEY_MASK;
}

static int key_pressed(u16 key) {
    return (g_key_curr & key) && !(g_key_prev & key);
}

static int key_repeat(u16 key, u8 *counter) {
    if (g_key_curr & key) {
        if (!(g_key_prev & key)) {
            *counter = CURSOR_REPEAT_DELAY;
            return 1;
        }

        if (*counter == 0) {
            *counter = CURSOR_REPEAT_RATE;
            return 1;
        }

        (*counter)--;
    } else {
        *counter = 0;
    }

    return 0;
}

static void wait_vblank(void) {
    while (REG_VCOUNT >= 160) {
    }
    while (REG_VCOUNT < 160) {
    }
}

static u32 rng_next(void) {
    g_rng_state = g_rng_state * 1664525u + 1013904223u;
    return g_rng_state;
}

static void seed_rng(void) {
    g_rng_state ^= ((u32)REG_VCOUNT << 16) | (u32)REG_KEYINPUT;
    g_rng_state ^= g_frame_counter;
    if (g_rng_state == 0) {
        g_rng_state = 1;
    }
    for (int i = 0; i < 64; i++) {
        rng_next();
    }
}

static inline void fill_run(u16 *dst, int count, u16 color) {
    for (int i = 0; i < count; i++) {
        dst[i] = color;
    }
}

static inline void put_pixel_safe(u16 *dst, int x, int y, u16 color) {
    if ((unsigned int)x >= SCREEN_WIDTH || (unsigned int)y >= SCREEN_HEIGHT) {
        return;
    }
    dst[y * SCREEN_WIDTH + x] = color;
}

static inline int is_water_surface_color(u16 color) {
    const tile_sprite_t *water = &g_tile_sprites[2];
    return color == water->base || color == water->light || color == water->dark || color == WATER_SPARKLE || color == WATER_WAVE_LIGHT || color == WATER_WAVE_DARK;
}

static inline void put_water_pixel(u16 *dst, int x, int y, u16 color) {
    if ((unsigned int)x >= SCREEN_WIDTH || (unsigned int)y >= SCREEN_HEIGHT) {
        return;
    }

    u16 *p = &dst[y * SCREEN_WIDTH + x];
    if (is_water_surface_color(*p)) {
        *p = color;
    }
}

static void clear_backbuffer(u16 color) {
    fill_run(g_backbuffer, SCREEN_PIXELS, color);
}

static void copy_words32(void *dst, const void *src, int words) {
    u32 *d       = (u32 *)dst;
    const u32 *s = (const u32 *)src;

    for (int i = 0; i < words; i++) {
        d[i] = s[i];
    }
}

static void copy_world_to_backbuffer(void) {
    copy_words32(g_backbuffer, g_worldbuffer, SCREEN_PIXELS / 2);
}

static void snapshot_worldbuffer(void) {
    copy_words32(g_worldbuffer, g_backbuffer, SCREEN_PIXELS / 2);
}

static void present_backbuffer(void) {
    REG_DMA3CNT = 0;
    REG_DMA3SAD = (const void *)g_backbuffer;
    REG_DMA3DAD = (void *)vram_mem;
    REG_DMA3CNT = DMA_ENABLE | DMA_32BIT | (SCREEN_PIXELS / 2);
}

static void draw_tile(int center_x, int top_y, int tile_kind, int height_level) {
    const tile_sprite_t *sprite = &g_tile_sprites[tile_kind % 3];
    int height_px               = height_level * HEIGHT_STEP;
    int phase                   = (int)((g_frame_counter >> 3) & 3u);

    if (height_px > 0) {
        for (int y = TILE_H_HALF; y <= TILE_H; y++) {
            int span   = g_span_lut[y];
            int left   = center_x - span;
            int width  = (span << 1) + 1;
            int split  = center_x - left;
            int base_y = top_y + y + 1;

            for (int d = 0; d < height_px; d++) {
                u16 *row = &g_backbuffer[(base_y + d) * SCREEN_WIDTH + left];

                if (d == height_px - 1) {
                    fill_run(row, width, sprite->dark);
                    continue;
                }

                fill_run(row, split, sprite->side_left);
                fill_run(row + split, width - split, sprite->side_right);
                row[0]         = sprite->dark;
                row[width - 1] = sprite->dark;
            }
        }
    }

    for (int y = 0; y <= TILE_H; y++) {
        int span  = g_span_lut[y];
        int left  = center_x - span;
        int width = (span << 1) + 1;
        u16 *row  = &g_backbuffer[(top_y + y) * SCREEN_WIDTH + left];

        if (y == 0 || y == TILE_H) {
            row[0] = sprite->dark;
            continue;
        }

        u8 bits;
        if (tile_kind == 2) {
            bits = sprite->pattern[(y + phase) & 7];
            if (phase != 0) {
                bits = (u8)((bits << phase) | (bits >> (8 - phase)));
            }
        } else {
            bits = sprite->pattern[y & 7];
        }

        int local_x = TILE_W_HALF - span;
        int bit     = 7 - (local_x & 7);

        for (int i = 0; i < width; i++) {
            row[i] = ((bits >> bit) & 1) ? sprite->light : sprite->base;
            bit--;
            if (bit < 0) {
                bit = 7;
            }
        }

        row[0]         = sprite->dark;
        row[width - 1] = sprite->dark;
    }
}

static void generate_world(void) {
    for (int y = 0; y < WORLD_SIZE; y++) {
        for (int x = 0; x < WORLD_SIZE; x++) {
            u32 r = rng_next() % 100u;
            u8 kind;
            u8 h;

            if (r < 42u) {
                kind = 0;
            } else if (r < 74u) {
                kind = 1;
            } else {
                kind = 2;
            }

            h = (u8)((rng_next() >> 29) & 3u);

            if (kind == 2) {
                h = (u8)((rng_next() & 7u) == 0u);
            } else if (kind == 1) {
                if (h > 2u) {
                    h = 2u;
                }
            } else {
                h = (u8)(1u + (h % 3u));
            }

            g_world_kind[y][x]   = kind;
            g_world_height[y][x] = h;
        }
    }

    for (int y = 0; y < WORLD_SIZE; y++) {
        for (int x = 0; x < WORLD_SIZE; x++) {
            if (g_world_kind[y][x] == 2) {
                g_world_height[y][x] = 0;
                continue;
            }

            int sum   = g_world_height[y][x] * 2;
            int count = 2;
            int h;

            if (x > 0) {
                sum += g_world_height[y][x - 1];
                count++;
            }
            if (y > 0) {
                sum += g_world_height[y - 1][x];
                count++;
            }
            if (x > 0 && y > 0) {
                sum += g_world_height[y - 1][x - 1];
                count++;
            }

            h = sum / count;
            if (h > 3) {
                h = 3;
            }
            if (g_world_kind[y][x] == 0 && h == 0) {
                h = 1;
            }

            g_world_height[y][x] = (u8)h;
        }
    }
}

static void draw_world(void) {
    clear_backbuffer(SKY_COLOR);

    for (int sum = 0; sum <= (WORLD_SIZE - 1) * 2; sum++) {
        for (int y = 0; y < WORLD_SIZE; y++) {
            int x = sum - y;
            int center_x;
            int base_top_y;
            int top_y;

            if (x < 0 || x >= WORLD_SIZE) {
                continue;
            }

            center_x   = ORIGIN_X + (x - y) * TILE_W_HALF;
            base_top_y = ORIGIN_Y + (x + y) * TILE_H_HALF;
            top_y      = base_top_y - g_world_height[y][x] * HEIGHT_STEP;

            draw_tile(center_x, top_y, g_world_kind[y][x], g_world_height[y][x]);
        }
    }
}

static void draw_water_overlay(u16 *dst) {
    int phase = (int)((g_frame_counter >> 2) & 31u);

    for (int y = 0; y < WORLD_SIZE; y++) {
        for (int x = 0; x < WORLD_SIZE; x++) {
            if (g_world_kind[y][x] != 2) {
                continue;
            }

            int center_x   = ORIGIN_X + (x - y) * TILE_W_HALF;
            int base_top_y = ORIGIN_Y + (x + y) * TILE_H_HALF;
            int top_y      = base_top_y - g_world_height[y][x] * HEIGHT_STEP;
            int row_a      = 2 + ((phase + x + y) & 1);
            int row_b      = 4 + ((phase + x + y + 1) & 1);

            int rows[2] = {row_a, row_b};

            for (int r = 0; r < 2; r++) {
                int row   = rows[r];
                int span  = g_span_lut[row];
                int left  = center_x - span;
                int right = center_x + span;
                int sy    = top_y + row;

                if (right - left <= 2) {
                    continue;
                }

                for (int px = left + 1; px < right; px++) {
                    int pattern = (px + phase * (r == 0 ? 2 : -2) + x * 3 + y * 5 + r * 7) & 7;
                    if (pattern <= 1) {
                        put_water_pixel(dst, px, sy, WATER_WAVE_LIGHT);
                    } else if (pattern == 4) {
                        put_water_pixel(dst, px, sy, WATER_WAVE_DARK);
                    }
                }

                int sparkle_pos = left + 1 + ((phase * 3 + x * 7 + y * 11 + r * 5) % (right - left - 1));
                put_water_pixel(dst, sparkle_pos, sy, WATER_SPARKLE);
            }
        }
    }
}

static void draw_cursor(u16 *dst) {
    int center_x   = ORIGIN_X + (g_cursor_x - g_cursor_y) * TILE_W_HALF;
    int base_top_y = ORIGIN_Y + (g_cursor_x + g_cursor_y) * TILE_H_HALF;
    int top_y      = base_top_y - g_world_height[g_cursor_y][g_cursor_x] * HEIGHT_STEP;
    int shimmer    = (int)((g_frame_counter >> 2) & 15);

    for (int y = 0; y <= TILE_H; y++) {
        int span        = g_span_lut[y];
        int left        = center_x - span;
        int right       = center_x + span;
        int left_outer  = left - 2;
        int right_outer = right + 2;
        int row_w       = (span << 1) + 1;
        int sweep_x     = left + ((shimmer + y * 2) % row_w);
        u16 edge_color  = CURSOR_GOLD_BRIGHT;

        if (((y + shimmer) & 7) <= 1) {
            edge_color = CURSOR_GOLD_GLINT;
        }

        put_pixel_safe(dst, left_outer, top_y + y, CURSOR_GOLD_DARK);
        put_pixel_safe(dst, left_outer + 1, top_y + y, CURSOR_GOLD);
        put_pixel_safe(dst, right_outer - 1, top_y + y, CURSOR_GOLD);
        put_pixel_safe(dst, right_outer, top_y + y, CURSOR_GOLD_DARK);

        put_pixel_safe(dst, left, top_y + y, edge_color);
        put_pixel_safe(dst, right, top_y + y, edge_color);

        if (span > 1) {
            put_pixel_safe(dst, left + 1, top_y + y, CURSOR_GOLD);
            put_pixel_safe(dst, right - 1, top_y + y, CURSOR_GOLD);
        }

        put_pixel_safe(dst, sweep_x, top_y + y, CURSOR_GOLD_GLINT);
        if (sweep_x < right) {
            put_pixel_safe(dst, sweep_x + 1, top_y + y, CURSOR_GOLD_BRIGHT);
        }
    }

    int beacon_h = 8 + ((shimmer >> 2) & 1);
    for (int y = 1; y <= beacon_h; y++) {
        u16 color = (y <= 2) ? CURSOR_GOLD_GLINT : CURSOR_GOLD_BRIGHT;
        put_pixel_safe(dst, center_x, top_y - y, color);
    }
    put_pixel_safe(dst, center_x - 1, top_y - beacon_h, CURSOR_GOLD_GLINT);
    put_pixel_safe(dst, center_x + 1, top_y - beacon_h, CURSOR_GOLD_GLINT);
    put_pixel_safe(dst, center_x, top_y - beacon_h - 1, CURSOR_GOLD_GLINT);
}

static int move_cursor(int dx, int dy) {
    int new_x = g_cursor_x + dx;
    int new_y = g_cursor_y + dy;

    if (new_x < 0 || new_x >= WORLD_SIZE || new_y < 0 || new_y >= WORLD_SIZE) {
        return 0;
    }

    g_cursor_x = new_x;
    g_cursor_y = new_y;
    return 1;
}

int main(void) {
    REG_DISPCNT = DCNT_MODE3 | DCNT_BG2;

    seed_rng();
    generate_world();
    draw_world();
    snapshot_worldbuffer();
    copy_world_to_backbuffer();
    draw_water_overlay(g_backbuffer);
    draw_cursor(g_backbuffer);
    wait_vblank();
    present_backbuffer();

    while (1) {
        wait_vblank();
        g_frame_counter++;
        key_poll();

        if (key_repeat(KEY_LEFT, &g_repeat_left)) {
            move_cursor(0, 1);
        }
        if (key_repeat(KEY_RIGHT, &g_repeat_right)) {
            move_cursor(0, -1);
        }
        if (key_repeat(KEY_UP, &g_repeat_up)) {
            move_cursor(-1, 0);
        }
        if (key_repeat(KEY_DOWN, &g_repeat_down)) {
            move_cursor(1, 0);
        }

        if (key_pressed(KEY_A)) {
            seed_rng();
            generate_world();
            draw_world();
            snapshot_worldbuffer();
        }

        copy_world_to_backbuffer();
        draw_water_overlay(g_backbuffer);
        draw_cursor(g_backbuffer);
        present_backbuffer();
    }
}
