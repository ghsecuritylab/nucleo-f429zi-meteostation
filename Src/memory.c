#include "memory.h"
#include "stdio.h"

static uint32_t start = 0x20001000;
static uint32_t end = 0x2000FFFF;

void *allocate_sram_memory(uint16_t size) {
  void * result;
  if (start + size <= end) {
    result = (void *)start;
    start += size;
    if (size % 4 != 0) {
       start += 4 - (size % 4);
    }
    return result;
  }
  printf("Out of memory\n");
  return 0;
}

void flash_write_32(uint32_t *data, uint8_t sec_offset, uint16_t offset, uint16_t length, char erase) {
  HAL_FLASH_Unlock();
  uint32_t sectorAddr = FLASH_END_ADDR + 1 - (sec_offset * FLASH_PAGE_SIZE);
  uint8_t counter = 0;
  
  if (erase) {
    uint32_t sectorError;  
    FLASH_EraseInitTypeDef erase_struct = { FLASH_TYPEERASE_SECTORS, 0, sectorAddr, 1, FLASH_VOLTAGE_RANGE_4 };
    
    HAL_FLASHEx_Erase(&erase_struct,&sectorError);
  }
    
  while (length > 0) {
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, sectorAddr + offset, *data);
    data++;
    offset += 4;
    length--;
  }
  
  HAL_FLASH_Lock();
}