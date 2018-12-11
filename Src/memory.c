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

