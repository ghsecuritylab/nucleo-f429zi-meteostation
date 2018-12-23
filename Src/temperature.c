#include "temperature.h"
#include "local_time.h"
#include "memory.h"
#include "string.h"
#include <stdlib.h>

I2C_HandleTypeDef *sensor_i2c_handle;
uint8_t *i2c_buffer;
char *string_buffer;
size_t buffer_length;

struct measure_config *config, *flash_config;

uint16_t measure_record_length;
struct measure_record *rec_border, *first, *next;
char flag = 0;

// Sensor calibration variables
uint16_t dig_T1, dig_P1;
int16_t dig_T2, dig_T3, dig_P2, dig_P3, dig_P4, dig_P5;
int16_t dig_P6, dig_P7, dig_P8, dig_P9;
uint8_t dig_H1, dig_H3;
uint16_t dig_H2, dig_H4, dig_H5;
int8_t dig_H6;

void get_calibration_data();
HAL_StatusTypeDef set_measure_calcs();

void set_i2c_handler (I2C_HandleTypeDef *i2c_handle) {
  sensor_i2c_handle = i2c_handle;
}

HAL_StatusTypeDef write_temperature_data (uint8_t *buffer, uint16_t data_size) {
  uint16_t dev_addr = BME280_ADDR << 1 | 0;
  return HAL_I2C_Master_Transmit(sensor_i2c_handle, dev_addr, buffer, data_size, I2C_TIMEOUT);
}

HAL_StatusTypeDef read_temperature_data (uint8_t *buffer, uint16_t data_size) {
  uint16_t dev_addr = BME280_ADDR << 1 | 1;
  return HAL_I2C_Master_Receive(sensor_i2c_handle, dev_addr, buffer, data_size, I2C_TIMEOUT);
}

HAL_StatusTypeDef burst_read (uint8_t *buffer, uint16_t data_size) {
  HAL_StatusTypeDef response_status;
  if (((response_status) = write_temperature_data(buffer, 1)) != HAL_OK) {
    return response_status;
  }
  return read_temperature_data(buffer, data_size);
}

uint8_t osampl_to_value(uint8_t human_value) {
  uint8_t value = 0;
  switch (human_value) {
    case 16:
      value++;
    case 8:
      value++;
    case 4:
      value++;
    case 2:
      value++;
    case 1:
      value++;
      break;
  }
  return value;
}

void temp_set_flag() {
  flag = 1;
}

void configure_mode_and_oversampling() {
  i2c_buffer = (uint8_t*)allocate_sram_memory(BUFFER_MAX_SIZE);
  config = (struct measure_config*)allocate_sram_memory(sizeof(struct measure_config));
  flash_config = (struct measure_config *)(
    (FLASH_END_ADDR + 1) -
    (CONFIG_OFFSET * FLASH_PAGE_SIZE)
  );
  uint8_t ft = flash_config->temperature_os;
  uint8_t fp = flash_config->pressure_os;
  uint8_t fh = flash_config->humidity_os;
  uint8_t fi = flash_config->iir_samples;
  config->temperature_os = ft > 0 && ft <= 16 ? ft : OVERSAMPLING_1;
  config->pressure_os = fp > 0 && fp <= 16 ? fp : OVERSAMPLING_1;
  config->humidity_os = fh > 0 && fh <= 16 ? fh : OVERSAMPLING_1;
  config->iir_samples = fi > 0 && fi <= 16 ? fi : OVERSAMPLING_1;

  get_calibration_data();

  measure_record_length = RECORDS_COUNT;
  rec_border = (struct measure_record *)allocate_sram_memory(sizeof(struct measure_record) * measure_record_length);
  first = next = rec_border;
  buffer_length = RECORDS_COUNT / 7 * RECORD_STRING_SIZE;
  string_buffer = (char *)allocate_sram_memory(buffer_length);
}

char *get_oversampling(size_t *length) {
  *length = sprintf(
    string_buffer,
    "Temperature oversampling:\t%u\nPressure oversampling:\t\t%u\nHumidity oversampling:\t\t%u\nIIR filter samples:\t\t%u\n", 
    config->temperature_os, config->pressure_os, config->humidity_os, config->iir_samples
  );
  return string_buffer;
}

char *set_oversampling (size_t *length) {
  *length = 0;
  char *part = strtok(0, " ");
  uint8_t *dest, value;
  if (part == 0) {
    *length = sprintf(string_buffer, "Too few arguments\n");
    return string_buffer;
  }
  
  switch (*part) {
    case 't':
      dest = &(config->temperature_os);
      break;
    case 'p':
      dest = &(config->pressure_os);
      break;
    case 'h':
      dest = &(config->humidity_os);
      break;
    case 'i':
      dest = &(config->iir_samples);
      break;
    default:
      *length = sprintf(string_buffer, "Argument should be one of those: 't,p,h,i'\n");
      return string_buffer;
  }
  part = strtok(0, " ");
  value = atoi(part);

  if (value <= 0 || value > 16) {
    *length = sprintf(string_buffer, "Value is abscent or incorrect\n");
    return string_buffer;
  } else {
    switch (value) {
      case OVERSAMPLING_1:
      case OVERSAMPLING_2:
      case OVERSAMPLING_4:
      case OVERSAMPLING_8:
      case OVERSAMPLING_16:
        *dest = value;
        flash_write_32((uint32_t *)config, CONFIG_OFFSET, 0, 1, 1);
        *length = sprintf(string_buffer, "Value is written successully\n");
        break;
      default:
        *length = sprintf(string_buffer, "Value should be power of 2\n");
        break;
    }
  }
  return string_buffer;
}

char *import_csv_history (uint16_t *offset, size_t *length) {
  *length = 0;
  struct measure_record *pointer;
  while (*length < buffer_length) {
    pointer = first;
    if (*offset > 0) {
      if (pointer + *offset >  rec_border + measure_record_length) {
        *offset = (pointer + *offset) - (rec_border + measure_record_length);
        pointer = rec_border + *offset;
      } else {
        pointer += *offset;
      }
    }
    if (pointer == next) break;
    *length += record_to_csv(string_buffer + *length, pointer);
    *offset += 1;
  }
  return string_buffer;
}

size_t record_to_csv (char *buffer, struct measure_record *record) {
  struct tm *ts;
  size_t length;
  time_t timestamp = timestamp_conv_local(record->timestamp);
  ts = localtime(&timestamp);
  length = strftime(buffer, buffer_length, "\"%H:%M:%S %Y-%m-%d\",", ts);
  length += sprintf(buffer + length, "%1.2f,%f,%f\n",
          record->temperature, record->pressure, record->humidity);
  return length;
}

char *stringify_single_record(struct measure_record *record, size_t *length) {
  struct tm *ts;
  time_t timestamp = timestamp_conv_local(record->timestamp);
  ts = localtime(&timestamp);
  *length = strftime(string_buffer, buffer_length, "Time:\t\t%H:%M:%S %Y-%m-%d\n", ts);
  *length += sprintf(string_buffer + *length, "Temperature:\t%1.2f\nPressure:\t%f\nHumidity:\t%f\n",
          record->temperature, record->pressure, record->humidity);
  return string_buffer;
}

void update_records() {
  if (flag) {
    perform_calculation(next);
    if (next >= rec_border + measure_record_length -1) {
      next = rec_border;
    } else {
      next++;
    }
    if (next == first) {
      first++;
    }
    flag = 0;
  }
}

HAL_StatusTypeDef set_measure_calcs () {
  i2c_buffer[0] = CTRL_HUM_REG;
  i2c_buffer[1] = osampl_to_value(config->humidity_os);
  write_temperature_data(i2c_buffer, 2);
  
  i2c_buffer[0] = CONFIG_REG;
  i2c_buffer[1] = osampl_to_value(config->iir_samples - 1);
  write_temperature_data(i2c_buffer, 2);
  
  uint8_t ctrl_meas_config = osampl_to_value(config->temperature_os) << 5 |
    osampl_to_value(config->pressure_os) << 2 | FORCE_MODE;
  printf("0x%x\n", ctrl_meas_config);
  
  i2c_buffer[0] = CTRL_MEAS_REG;
  i2c_buffer[1] = ctrl_meas_config;
  return write_temperature_data(i2c_buffer, 2);
}

void perform_calculation(struct measure_record *result) {
  /*Sensor variables*/
  uint32_t adc_T, adc_P, adc_H;
  int32_t t_fine, temp_actual, hum_act, p = 0;
  
  uint8_t *data = i2c_buffer;
  
  set_measure_calcs();
  
  uint8_t iir_coef;
  switch(config->iir_samples) {
    case OVERSAMPLING_1:
      iir_coef = SAMPLES_1;
    case OVERSAMPLING_2:
      iir_coef = SAMPLES_2;
    case OVERSAMPLING_4:
      iir_coef = SAMPLES_4;
      break;
    case OVERSAMPLING_8:
      iir_coef = SAMPLES_8;
      break;
    case OVERSAMPLING_16:
      iir_coef = SAMPLES_16;
      break;
  }

  uint16_t delay = iir_coef * (uint16_t)(2.5 + 2.3 * (config->humidity_os + config->pressure_os + config->temperature_os));
  HAL_Delay(delay);
  data[0] = ADC_BURST_READ;
  burst_read(data, ADC_BURST_READ_LENGTH);
  
  adc_P = (data[0] << 12) | (data[1] << 4) | (data[2] >> 4);
  adc_T = (data[3] << 12) | (data[4] << 4) | (data[5] >> 4);
  adc_H = (data[6] << 8) | data[7];
  
  //SOME COPY-PASTE MAGIC FROM SENSOR MANUAL
  /*COUNT TEMP BEGIN*/
  int32_t var1, var2;
  var1 = ((((adc_T >> 3) - ((signed long int)dig_T1<<1))) * ((signed long int)dig_T2)) >> 11;
  var2 = (((((adc_T >> 4) - ((signed long int)dig_T1)) * ((adc_T>>4) - ((signed long int)dig_T1))) >> 12) * ((signed long int)dig_T3)) >> 14;
  t_fine = var1+var2;
  temp_actual = ((t_fine * 5 + 128) >> 8);
  
  result->temperature = temp_actual / 100.0;
  /*COUNT TEMP END*/

  /*COUNT PRESS BEGIN*/
  var1 = 0;
  var1 = (((int32_t)t_fine)>>1) - (int32_t)64000;
  var2 = (((var1>>2) * (var1>>2)) >> 11 ) * ((int32_t)dig_P6);
  var2 = var2 + ((var1*((int32_t)dig_P5))<<1);
  var2 = (var2>>2)+(((int32_t)dig_P4)<<16);
  var1 = (((dig_P3 * (((var1>>2) * (var1>>2)) >> 13 )) >> 3) + ((((int32_t)dig_P2) * var1)>>1))>>18;
  var1 =((((32768+var1))*((int32_t)dig_P1))>>15);
  if (var1 == 0) {
    result->pressure = 0;
  } else {
    p = (((int32_t)(((int32_t)1048576)-adc_P)-(var2>>12)))*3125;
    if (p < 0x80000000) {
      p = (p << 1) / ((uint32_t)var1);
    } else {
      p = (p / (uint32_t)var1) * 2;
    }
    var1 = (((int32_t)dig_P9) * ((int32_t)(((p>>3) * (p>>3))>>13)))>>12;
    var2 = (((int32_t)(p>>2)) * ((int32_t)dig_P8))>>13;
    p = (int32_t)((int32_t)p + ((var1 + var2 + dig_P7) >> 4));
    
    result->pressure = p / 133.2;
  }
  
  /*COUNT PRESS END*/

  /*COUNT HUMILITY BEGIN*/
  signed long int v_x1;

  v_x1 = (t_fine - ((signed long int)76800));
  v_x1 = (((((adc_H << 14) -(((signed long int)dig_H4) << 20) - (((signed long int)dig_H5) * v_x1)) +
            ((signed long int)16384)) >> 15) * (((((((v_x1 * ((signed long int)dig_H6)) >> 10) *
            (((v_x1 * ((signed long int)dig_H3)) >> 11) + ((signed long int) 32768))) >> 10) + (( signed long int)2097152)) *
            ((signed long int)dig_H2) + 8192) >> 14));
  v_x1 = (v_x1 - (((((v_x1 >> 15) * (v_x1 >> 15)) >> 7) * ((signed long int)dig_H1)) >> 4));
  v_x1 = (v_x1 < 0 ? 0 : v_x1);
  v_x1 = (v_x1 > 419430400 ? 419430400 : v_x1);
  hum_act = v_x1 >> 12;
  
  result->humidity = hum_act / 1024.0;
  /*COUNT HUMILITY END*/
  result->timestamp = time(0);
}

void get_calibration_data () {
  uint8_t *data = i2c_buffer;
  
  data[0] = CONF_BURST_FIRST;
  burst_read(data, CONF_BURST_FIRST_LENGTH);
  
  dig_T1 = (data[1] << 8) | *data;
  dig_T2 = (data[3] << 8) | *(data+2);
  dig_T3 = (data[5] << 8) | *(data+4);
  dig_P1 = (data[7] << 8) | data[6];
  dig_P2 = (data[9] << 8) | data[8];
  dig_P3 = (data[11] << 8) | data[10];
  dig_P4 = (data[13] << 8) | data[12];
  dig_P5 = (data[15] << 8) | data[14];
  dig_P6 = (data[17] << 8) | data[16];
  dig_P7 = (data[19] << 8) | data[18];
  dig_P8 = (data[21] << 8) | data[20];
  dig_P9 = (data[23] << 8) | data[22];
  dig_H1 = data[25];
  
  data[0] = CONF_BURST_SECOND;
  burst_read(data, CONF_BURST_SECOND_LENGTH);
  
  dig_H2 = (data[1] << 8) | data[0];
  dig_H3 = data[2];
  dig_H4 = (data[4] << 4) | data[3];
  dig_H5 = (data[5] << 8) | data[6];
  dig_H6 = data[7];
}
