#ifndef LOCAL_TIME_USER_H
#define LOCAL_TIME_USER_H

#include "stdint.h"
#include "time.h"


time_t timestamp_conv_local(time_t gmt_time);
void set_time_offset(uint32_t minutes_count);

#endif //LOCAL_TIME_USER_H