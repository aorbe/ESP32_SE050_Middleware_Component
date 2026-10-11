#ifndef _I2C_ESP32_H_
#define _I2C_ESP32_H_

#include "driver/i2c_master.h"

// Change the following values accordind to board. Please note that ESP32-C3 has only I2C_NUM_0.
#define I2C_ESP32_SDA_PIN           5
#define I2C_ESP32_SCL_PIN           6
#define I2C_ESP32_INTERFACE_NUM     I2C_NUM_0

typedef enum {
    I2C_ESP32_STATUS_NOT_INIT,
    I2C_ESP32_STATUS_INIT,
} i2c_esp32_status_t;

i2c_esp32_status_t i2c_esp32_get_status(void);
void i2c_esp32_set_status(i2c_esp32_status_t i2c_esp32_status);
void i2c_esp32_set_port(i2c_port_t i2c_port);

#endif