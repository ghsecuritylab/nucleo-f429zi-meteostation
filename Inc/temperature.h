#ifndef TEMERATURE_H
#define TEMERATURE_H

#include "stm32f4xx_hal.h"
#include "stdint.h"
#include "time.h"

#define I2C_TIMEOUT 1000
#define BME280_ADDR 0x76
#define BUFFER_MAX_SIZE 26

#define CTRL_HUM_REG 0xF2
#define CTRL_MEAS_REG 0xF4
#define CONFIG_REG 0xF5

#define ADC_BURST_READ 0xF7
#define ADC_BURST_READ_LENGTH 8

#define CONF_BURST_FIRST 0x88
#define CONF_BURST_FIRST_LENGTH 26

#define CONF_BURST_SECOND 0xE1
#define CONF_BURST_SECOND_LENGTH 8

#define FORCE_MODE 0x2

#define OVERSAMPLING_1  1
#define OVERSAMPLING_2  2
#define OVERSAMPLING_4  4
#define OVERSAMPLING_8  8
#define OVERSAMPLING_16 16

#define SAMPLES_1       1
#define SAMPLES_2       2
#define SAMPLES_4       5
#define SAMPLES_8       11
#define SAMPLES_16      22

#define CONFIG_OFFSET 34
#define POINTERS_OFFSET 33
#define DATA_OFFSET 32

void collect_sensor_data();
void set_i2c_handler (I2C_HandleTypeDef *i2c_handle);
HAL_StatusTypeDef read_temperature_data (uint8_t *buffer, uint16_t data_size);
HAL_StatusTypeDef write_temperature_data (uint8_t *buffer, uint16_t data_size);

void configure_mode_and_oversampling();
void update_records();
void temp_set_flag();

struct measure_record {
  time_t timestamp;
  float temperature;
  float pressure;
  float humidity;
};


// Structure should be 32 byte divisible 
struct measure_config {
  uint8_t temperature_os;
  uint8_t pressure_os;
  uint8_t humidity_os;
  uint8_t iir_samples;
};

void perform_calculation (struct measure_record *result);
char *stringify_single_record(struct measure_record *record, size_t *length);
size_t record_to_csv (char *buffer, struct measure_record *record);
char *import_csv_history (uint16_t *offset, size_t *length);
char *get_oversampling(size_t *length);
char *set_oversampling (size_t *length);

#define RECORDS_INTERVAL_M 5

#define RECORDS_COUNT (60 / RECORDS_INTERVAL_M) * 24 * 7

#define RECORD_STRING_SIZE 65

#endif //TEMERATURE_H