#include "driver/i2c_master.h"
#include "esp_log.h"
#include "sm_timer.h"
#include <stdio.h>
#include "sci2c_cfg.h"
#include "sm_printf.h"
#include "i2c_esp32.h"


#define ACK_CHECK_EN                       0x1
#define ACK_CHECK_DIS                      0x0
#define ACK_VAL                            0x0
#define NACK_VAL                           0x1

// i2c_config_t conf;
i2c_esp32_status_t i2c_current_status = I2C_ESP32_STATUS_NOT_INIT;
// static i2c_master_bus_handle_t user_i2c_handle;

static const char *TAG = "Se5xI2C";

/* Handle NAK */
static int gBackoffDelay;

void ResetBackoffDelay() {
    gBackoffDelay = 0;
}

static void BackOffDelay_Wait() {
    if (gBackoffDelay < 200 )
        gBackoffDelay += 1;
    sm_sleep(gBackoffDelay);
}
#define I2C_LOG_PRINTF PRINTF

static i2c_master_bus_handle_t i2c_bus_config;
static i2c_master_dev_handle_t se50_dev_handle = NULL;
static uint8_t current_registered_addr = 0x00;
static i2c_port_t axSmDevice_port = I2C_ESP32_INTERFACE_NUM;
  
static uint32_t i2c_data_pdMS_TICKS = 0;
static uint32_t i2c_done_pdMS_TICKS = 0;

i2c_error_t axI2CInit(void **conn_ctx, const char *pDevName)
{
    if (i2c_current_status != I2C_ESP32_STATUS_INIT) {
        esp_err_t rc;

        i2c_data_pdMS_TICKS = MS_TO_TICKS(5000);
        i2c_done_pdMS_TICKS = MS_TO_TICKS(1000);

        ESP_LOGI(TAG, "SE Using I2C 0");
        rc = i2c_master_get_bus_handle(0, &i2c_bus_config);

        i2c_current_status = I2C_ESP32_STATUS_INIT;
        if (ESP_OK == rc)
            return I2C_OK;
        return I2C_FAILED;
    }
    return I2C_OK;
}

// Helper to register device on demand
static esp_err_t ensure_device_handle(unsigned char addr) {
    // CRITICAL: NXP passes the 8-bit address (0x90).
    // Convert to 7-bit (0x48) if it's in 8-bit format:
    uint16_t addr_7bit = (addr > 0x7F) ? (addr >> 1) : addr;

    if (se50_dev_handle != NULL && current_registered_addr == addr_7bit) {
        return ESP_OK; // Already registered with this address
    }

    // If already registered with a different address, delete first
    if (se50_dev_handle != NULL) {
        i2c_master_bus_rm_device(se50_dev_handle);
        se50_dev_handle = NULL;
    }

    ESP_LOGI(TAG, "Endereço ESP: %d", addr_7bit);

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr_7bit,
        .scl_speed_hz = 50000, // 50 kHz
    };

    esp_err_t err = i2c_master_bus_add_device(i2c_bus_config, &dev_cfg, &se50_dev_handle);
    if (err == ESP_OK) {
        current_registered_addr = addr_7bit;
    }
    return err;
}

/**
* Closes the communication channel to I2C device (not implemented)
*/
void axI2CTerm(void* conn_ctx, int mode)
{
    esp_err_t rc;
    AX_UNUSED_ARG(mode);
    if (se50_dev_handle != NULL) {
        i2c_master_bus_rm_device(se50_dev_handle);
        se50_dev_handle = NULL;
    }
    i2c_current_status = I2C_ESP32_STATUS_NOT_INIT;
    return;
}


i2c_error_t axI2CWrite(void* conn_ctx, unsigned char bus, unsigned char addr, unsigned char * pTx, unsigned short txLen)
{
    esp_err_t rc;
    if (bus != I2C_BUS_0)
    {
        return I2C_FAILED;
    }
       
    if (ensure_device_handle(addr) != ESP_OK) {
        return I2C_FAILED;
    }

    rc = i2c_master_transmit(se50_dev_handle, pTx, txLen, 2000);
    return (rc == ESP_OK) ? I2C_OK : I2C_FAILED;
}


i2c_error_t axI2CRead(void* conn_ctx, unsigned char bus, unsigned char addr, unsigned char * pRx, unsigned short rxLen)
{
    if (bus != I2C_BUS_0)
    {
        return I2C_FAILED;
    }

    esp_err_t rc;
    if (ensure_device_handle(addr) != ESP_OK) {
        return I2C_FAILED;
    }
    
    rc = i2c_master_receive(se50_dev_handle, pRx, rxLen, 2000);
    return (rc == ESP_OK) ? I2C_OK : I2C_FAILED;
    
}

i2c_esp32_status_t i2c_esp32_get_status(void) {
    return i2c_current_status;
}

void i2c_esp32_set_status(i2c_esp32_status_t i2c_esp32_status){
    i2c_current_status = i2c_esp32_status;
}

void i2c_esp32_set_port(i2c_port_t i2c_port) {
    axSmDevice_port = i2c_port;
}
