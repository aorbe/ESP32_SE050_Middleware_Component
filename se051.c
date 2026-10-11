#include "esp_log.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* NXP Plug & Trust headers */
#include "se05x_APDU.h"
#include "se05x_enums.h"
#include "smCom.h"
#include "sm_apdu.h"
#include "psa/crypto.h"
#include "se051.h"

static const char *TAG = "NfcSe5x";

/* Include the test suite header from previous step */
//#include "test_se051_suite.h"

/* Global Boot Context provided by Plug & Trust */
// static ex_sss_boot_ctx_t gex_sss_boot_ctx;
static void *g_conn_ctx = NULL;
// static sss_session_t session = NULL;

// /**
//  * @brief Pulse the SE051 ENA pin to hard-reset the chip before communication.
//  */
// static void se051_hardware_reset(void) {
//     gpio_config_t gpio_conf = {};                                                            
//     gpio_conf.intr_type = GPIO_INTR_DISABLE;                                             
//     gpio_conf.mode = GPIO_MODE_OUTPUT;                                                   
//     gpio_conf.pin_bit_mask = (1ULL << GPIO_SE_ENA);
//     gpio_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;                                      
//     gpio_conf.pull_up_en = GPIO_PULLUP_DISABLE;                                           
//     ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&gpio_conf));  

//     /* ENA low to power off / reset */
//     gpio_set_level((gpio_num_t)GPIO_SE_ENA,0);
//     vTaskDelay(pdMS_TO_TICKS(10));

//     /* ENA high to wake up SE051 */
//     gpio_set_level((gpio_num_t)GPIO_SE_ENA,1);
//     vTaskDelay(pdMS_TO_TICKS(10)); // Give SE time to boot
// }

/**
 * @brief Initializes I2C, authenticates SCP03, and extracts pSe05xSession_t.
 */
pSe05xSession_t se051_session_create(void) {
    // ESP_LOGI(TAG, "Resetting SE051 hardware via ENA pin...");
    // In future should be implemented to reduce consumption
    // se051_hardware_reset();

    // sss_session_t *pSession                 = &gex_sss_boot_ctx->session;
    // gex_sss_boot_ctx->se05x_open_ctx.skip_select_applet = 0;
    // gex_sss_boot_ctx->se05x_open_ctx.connType           = kType_SE_Conn_Type_T1oI2C;

    // ESP_LOGI(TAG, "Initializing session and performing SCP03 handshake...");

    // sss_status_t status = sss_session_open(&session, 
    //      kType_SSS_SE_SE05x,
    //      0,
    //      kSSS_ConnectionType_Encrypted,
    //       void *connectionData)¶

    // // Open the T=1 over I2C protocol stack
    // // Note: smCom expects the 8-bit address (0x90)
    // U16 smcom_status = smComT1oI2C_Open(&g_conn_ctx, 0x90);
    // if (smcom_status != SMCOM_OK) {
    //     ESP_LOGE(TAG, "smComT1oI2C_Open failed: 0x%04X", smcom_status);
    //     return SM_NOT_OK;
    // }


    // session_ctx->conn_context = g_conn_ctx;

    // /* Initialize boot context parameters */
    // memset(&gex_sss_boot_ctx, 0, sizeof(gex_sss_boot_ctx));

    /* 
     * ex_sss_boot_open executes:
     * 1. smComT1oI2C_Open (I2C @ 400kHz)
     * 2. Applet SELECT
     * 3. SCP03 mutual authentication using ENC/MAC/DEK keys defined in
     *    ex_sss_scp03_keys.h (enabled by SSS_HAVE_SCP_SCP03_SSS)
     */
    sss_status_t status = ex_sss_boot_open(&gex_sss_boot_ctx, NULL);
    if (status != kStatus_SSS_Success) {
        ESP_LOGE(TAG, "ex_sss_boot_open failed! (status = 0x%08X)", status);
        return NULL;
    }

    ESP_LOGI(TAG, "SCP03 Mutual Authentication Successful!");

    // /*
    //  * Cast the generic sss_session_t to the SE05x-specific session structure 
    //  * to extract the low-level Se05xSession_t (s_ctx).
    //  */
    sss_se05x_session_t *pSe05xSession = (sss_se05x_session_t *)&gex_sss_boot_ctx.session;
    pSe05xSession_t session_ctx = &pSe05xSession->s_ctx;
    // Step 4: Select SE051 IoT Applet and establish the session
    // smStatus_t sw = Se05x_API_SessionOpen(session_ctx);
    // if (sw != SM_OK) {
    //     ESP_LOGE(TAG, "Se05x_API_SessionOpen failed: 0x%04X", sw);
    //     smComT1oI2C_Close(g_conn_ctx);
    //     return sw;
    // }

    // ESP_LOGI(TAG, "SE051 Session successfully opened!");
    // ESP_LOGI(TAG, "Applet Version: %d.%d.%d", 
    //          session_ctx->applet_version >> 24,
    //          (session_ctx->applet_version >> 16) & 0xFF,
    //          (session_ctx->applet_version >> 8) & 0xFF);

    return session_ctx;
}

/**
 * @brief Closes SCP03 session, releases I2C bus and allocated memory.
 */
void se051_session_close(pSe05xSession_t session_ctx) {
    ESP_LOGI(TAG, "Closing SE051 session...");
    // if (session_ctx != NULL && session_ctx->conn_context != NULL) {
    //     // Send session close APDU
    //     Se05x_API_SessionClose(session_ctx);

    //     // Close T=1oI2C transport layer
    //     smComT1oI2C_Close(session_ctx->conn_context);
    //     session_ctx->conn_context = NULL;
    // }
    ex_sss_session_close(&gex_sss_boot_ctx);
    
    // /* Turn off ENA */
    // gpio_set_level(GPIO_SE_ENA, 0);
}