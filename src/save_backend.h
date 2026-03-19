#ifndef SAVE_BACKEND_H
#define SAVE_BACKEND_H

#include <gba_types.h>

typedef enum {
    SAVE_BACKEND_SRAM         = 0,
    SAVE_BACKEND_FLASH_END_8K = 1,
} save_backend_t;

void save_backend_init(void);
void save_backend_set(save_backend_t backend);
save_backend_t save_backend_get(void);
const char *save_backend_name(save_backend_t backend);
int save_backend_write8(const u8 *src);
int save_backend_read8(u8 *dst);

#endif
