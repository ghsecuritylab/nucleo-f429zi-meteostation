#include "memory.h"
#include "stdio.h"

static uint32_t start = 0x20001000;
static uint32_t end = 0x2000FFFF;

static FLASH_EraseInitTypeDef *erase_struct = 0;
static uint32_t *sector_error;

void init() {
  erase_struct = allocate_sram_memory(sizeof(FLASH_EraseInitTypeDef));
  sector_error = allocate_sram_memory(sizeof(uint32_t));
}

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

void flash_write_32(uint32_t *data, uint16_t sec_offset, uint16_t offset, uint16_t length, char erase) {
  if (erase_struct == 0) init();
  HAL_FLASH_Unlock();
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_PGSERR );
  uint32_t sector_addr = FLASH_END_ADDR + 1 - (sec_offset * FLASH_PAGE_SIZE);
  if (erase) {
    erase_struct->TypeErase = FLASH_TYPEERASE_SECTORS;
    erase_struct->Banks = 0;
    erase_struct->Sector = FLASH_SECTOR_16 - sec_offset;
    erase_struct->NbSectors = 1;
    erase_struct->VoltageRange = FLASH_VOLTAGE_RANGE_3;
    
    HAL_FLASHEx_Erase(erase_struct, sector_error);
  }
  while (length > 0) {
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, sector_addr + offset, *data);
    data++;
    offset += 4;
    length--;
  }
  HAL_FLASH_Lock();
}

void *get_sector_addr(void *addr) {
  uint32_t address = (uint32_t)addr;
  if (address % FLASH_PAGE_SIZE == 0) return addr;
  return (void*)(address - (address % FLASH_PAGE_SIZE));
}