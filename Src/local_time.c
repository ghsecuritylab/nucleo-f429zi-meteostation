#include "local_time.h"

static time_t minutes_offset = 0;

time_t timestamp_conv_local (time_t gmt_time) {
  return gmt_time + minutes_offset;
}

void set_time_offset (uint32_t minutes_count) {
  minutes_offset = minutes_count * 60;
}