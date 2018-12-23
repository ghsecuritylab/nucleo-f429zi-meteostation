#ifndef MEM_USER_H
#define MEM_USER_H

#include "stdint.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_flash.h"

void *allocate_sram_memory(uint16_t size);
void flash_write_32(uint32_t *data, uint8_t sec_offset, uint16_t offset, uint16_t length, char erase);

#define FLASH_START_ADDR        0x8000000
#define FLASH_END_ADDR          0x81fffff
#define FLASH_PAGE_SIZE         0x400

#endif //MEM_USER_H