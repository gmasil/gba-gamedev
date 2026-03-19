#include "save_backend.h"

#define SRAM_BASE ((volatile u8 *)0x0E000000)
#define FLASH_BASE ((volatile u16 *)0x08000000)

#define REG_IME (*(volatile u16 *)0x04000208)

#define FLASH_SAVE_SECTOR_BYTE_ADDR 0x00FFE000u
#define FLASH_SAVE_SECTOR_INDEX (FLASH_SAVE_SECTOR_BYTE_ADDR >> 1)

#define FLASH_MAGIC0 0x53u
#define FLASH_MAGIC1 0x42u
#define FLASH_MAGIC2 0x45u
#define FLASH_MAGIC3 0x31u

static save_backend_t g_backend = SAVE_BACKEND_FLASH_END_8K;

static int sram_write8_array(const u8 *src) {
    for (int i = 0; i < 8; i++) {
        SRAM_BASE[i] = src[i];
    }
    return 1;
}

static int sram_read8_array(u8 *dst) {
    for (int i = 0; i < 8; i++) {
        dst[i] = SRAM_BASE[i];
    }
    return 1;
}

static u16 save_checksum8(const u8 *src) {
    u16 sum = 0;
    for (int i = 0; i < 8; i++) {
        sum = (u16)(sum + src[i]);
    }
    return sum;
}

static void save_pack_words(const u8 *src, u16 *words_out) {
    u16 checksum = save_checksum8(src);
    words_out[0] = (u16)(FLASH_MAGIC0 | (FLASH_MAGIC1 << 8));
    words_out[1] = (u16)(FLASH_MAGIC2 | (FLASH_MAGIC3 << 8));
    words_out[2] = (u16)(src[0] | (src[1] << 8));
    words_out[3] = (u16)(src[2] | (src[3] << 8));
    words_out[4] = (u16)(src[4] | (src[5] << 8));
    words_out[5] = (u16)(src[6] | (src[7] << 8));
    words_out[6] = checksum;
    words_out[7] = (u16)~checksum;
}

static int save_unpack_words(const u16 *words, u8 *dst) {
    if ((words[0] & 0xFFu) != FLASH_MAGIC0 || ((words[0] >> 8) & 0xFFu) != FLASH_MAGIC1 || (words[1] & 0xFFu) != FLASH_MAGIC2 || ((words[1] >> 8) & 0xFFu) != FLASH_MAGIC3) {
        return 0;
    }

    dst[0] = (u8)(words[2] & 0xFFu);
    dst[1] = (u8)((words[2] >> 8) & 0xFFu);
    dst[2] = (u8)(words[3] & 0xFFu);
    dst[3] = (u8)((words[3] >> 8) & 0xFFu);
    dst[4] = (u8)(words[4] & 0xFFu);
    dst[5] = (u8)((words[4] >> 8) & 0xFFu);
    dst[6] = (u8)(words[5] & 0xFFu);
    dst[7] = (u8)((words[5] >> 8) & 0xFFu);

    u16 checksum = save_checksum8(dst);
    if (words[6] != checksum || words[7] != (u16)~checksum) {
        return 0;
    }

    return 1;
}

__attribute__((section(".iwram"), long_call)) static int flash_wait_value(u32 index, u16 value, u32 timeout) {
    while (timeout--) {
        if (FLASH_BASE[index] == value) {
            return 1;
        }
    }
    return 0;
}

__attribute__((section(".iwram"), long_call)) static int flash_erase_last_8k_sector(void) {
    const u32 sa = FLASH_SAVE_SECTOR_INDEX;

    FLASH_BASE[sa]                = 0x0060;
    FLASH_BASE[sa]                = 0x0060;
    FLASH_BASE[sa + (0x84u >> 1)] = 0x0060;
    FLASH_BASE[0]                 = 0x00F0;

    FLASH_BASE[0x555] = 0x00A9;
    FLASH_BASE[0x2AA] = 0x0056;
    FLASH_BASE[0x555] = 0x0080;
    FLASH_BASE[0x555] = 0x00A9;
    FLASH_BASE[0x2AA] = 0x0056;
    FLASH_BASE[sa]    = 0x0030;

    return flash_wait_value(sa, 0xFFFF, 1000000u);
}

__attribute__((section(".iwram"), long_call)) static int flash_program_word(u32 index, u16 data) {
    FLASH_BASE[0x555] = 0x00A9;
    FLASH_BASE[0x2AA] = 0x0056;
    FLASH_BASE[0x555] = 0x00A0;
    FLASH_BASE[index] = data;

    return flash_wait_value(index, data, 200000u);
}

__attribute__((section(".iwram"), long_call)) static int flash_write_save_words(const u16 *words, int count) {
    const u16 ime = REG_IME;
    REG_IME       = 0;

    if (!flash_erase_last_8k_sector()) {
        REG_IME = ime;
        return 0;
    }

    for (int i = 0; i < count; i++) {
        if (!flash_program_word(FLASH_SAVE_SECTOR_INDEX + (u32)i, words[i])) {
            REG_IME = ime;
            return 0;
        }
    }

    REG_IME = ime;
    return 1;
}

static int flash_end_write8(const u8 *src) {
    u16 words[8];
    save_pack_words(src, words);
    return flash_write_save_words(words, 8);
}

static int flash_end_read8(u8 *dst) {
    u16 words[8];
    for (int i = 0; i < 8; i++) {
        words[i] = FLASH_BASE[FLASH_SAVE_SECTOR_INDEX + (u32)i];
    }
    return save_unpack_words(words, dst);
}

void save_backend_init(void) {
    g_backend = SAVE_BACKEND_FLASH_END_8K;
}

void save_backend_set(save_backend_t backend) {
    g_backend = backend;
}

save_backend_t save_backend_get(void) {
    return g_backend;
}

const char *save_backend_name(save_backend_t backend) {
    if (backend == SAVE_BACKEND_SRAM) {
        return "SRAM";
    }
    return "FLASH_END_8K";
}

int save_backend_write8(const u8 *src) {
    if (g_backend == SAVE_BACKEND_SRAM) {
        return sram_write8_array(src);
    }
    return flash_end_write8(src);
}

int save_backend_read8(u8 *dst) {
    if (g_backend == SAVE_BACKEND_SRAM) {
        return sram_read8_array(dst);
    }
    return flash_end_read8(dst);
}
