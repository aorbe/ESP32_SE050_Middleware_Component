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

#define SCP03_MAX_AUTH_KEY_SIZE     32

static sss_session_t g_session;
static bool g_session_active = false;
static SE_Connect_Ctx_t g_connect_ctx;

// Default NXP Factory SCP03 base key (32 bytes AES)
unsigned char KEY_CODE_ENC[]  = {                                                                                    \
        0x00, 0x00, 0x00, 0x02, 0xE2, 0x9B, 0x12, 0x4E, 0xF2, 0xDC, 0xA8, 0xE3, 0x2D, 0x7A, 0xB3, 0x98, 0x56, 0x3E, \
            0x0A, 0x0F, 0x66, 0xCF, 0xB2, 0x37, 0x31, 0xBD, 0xD4, 0xD4, 0x42, 0x27, 0x73, 0x92, 0x23, 0xCC, 0xA7,   \
            0xE7, 0x51, 0xA4, 0x99, 0x91, 0x19, 0x68, 0x74, 0x92, 0xC9, 0x9D, 0xF2, 0x9F, 0x5B, 0x6E, 0x5E, 0x81    \
    };

unsigned char KEY_CODE_MAC[] =                                                                                      \
    {                                                                                                               \
        0x00, 0x00, 0x00, 0x02, 0x81, 0x54, 0x3E, 0x5D, 0x47, 0xDE, 0x23, 0x7C, 0x00, 0x1B, 0x16, 0xBE, 0x1B, 0x05, \
            0xED, 0xD2, 0xD5, 0xB2, 0x4D, 0x3C, 0xD3, 0xDD, 0xD5, 0xA9, 0x40, 0x5E, 0x7D, 0x90, 0x73, 0x74, 0xDE,   \
            0x05, 0xAC, 0x76, 0x7D, 0x87, 0xB6, 0x5E, 0x1F, 0x8E, 0xB5, 0x93, 0x53, 0x41, 0x51, 0x27, 0xE9, 0xF9    \
    };

unsigned char KEY_CODE_DEK[] =                                                                                      \
    {                                                                                                               \
        0x00, 0x00, 0x00, 0x02, 0x88, 0xE0, 0x9A, 0x2B, 0x23, 0x77, 0xC3, 0xF5, 0xEE, 0x28, 0x4F, 0x7C, 0x5B, 0xD8, \
            0x9C, 0xF5, 0xA8, 0xC9, 0xE4, 0xE3, 0xDC, 0x8D, 0x34, 0x3C, 0x00, 0x39, 0x7E, 0xA3, 0x35, 0x39, 0xFD,   \
            0xD1, 0xE4, 0x8D, 0xA9, 0x8C, 0x41, 0xAF, 0x8C, 0x8D, 0x50, 0xFE, 0x63, 0x96, 0x46, 0x2E, 0x4D, 0xEB    \
    };

// Host context objects
static sss_session_t   g_host_session;
static sss_key_store_t g_host_ks;

// SCP03 Static and Dynamic state containers
static NXSCP03_StaticCtx_t g_scp03_static_ctx;
static NXSCP03_DynCtx_t    g_scp03_dyn_ctx;

static sss_status_t alloc_and_set_host_key(sss_key_store_t *pHostKs,
                                           sss_object_t *pKeyObj,
                                           uint32_t keyId,
                                           const uint8_t *pKeyData,
                                           size_t keyLen)
{
    sss_status_t status = sss_key_object_init(pKeyObj, pHostKs);
    if (status != kStatus_SSS_Success) return status;

    status = sss_key_object_allocate_handle(
        pKeyObj,
        keyId,
        kSSS_KeyPart_Default,
        kSSS_CipherType_AES,
        keyLen,
        kKeyObject_Mode_Transient
    );
    if (status != kStatus_SSS_Success) return status;

    if (pKeyData != NULL) {
        status = sss_key_store_set_key(pHostKs, pKeyObj, pKeyData, keyLen, keyLen * 8, NULL, 0);
    }
    return status;
}

/* Global Boot Context provided by Plug & Trust */

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

    sss_status_t status;
    // ESP_LOGI(TAG, "Resetting SE051 hardware via ENA pin...");
    // In future should be implemented to reduce consumption
    // se051_hardware_reset();

    if (!g_session_active) {

        // Ensure I2C peripheral is ready
        axI2CInit(NULL, NULL);

        // Open host software crypto session (uses mbedTLS on ESP32)
        // sss_status_t status = sss_host_session_open(&g_host_session, kType_SSS_mbedTLS, 0, kSSS_ConnectionType_Plain, NULL);
        // if (status != kStatus_SSS_Success) {
        //     ESP_LOGE("SCP03", "Failed to open host session: 0x%08X", status);
        //     return false;
        // }

        // vTaskDelay(pdMS_TO_TICKS(100));
        // ESP_LOGI(TAG, "sss_host_key_store_context_init");

        // status = sss_host_key_store_context_init(&g_host_ks, &g_host_session);
        // if (status != kStatus_SSS_Success) {
        //     return false;
        // }

        // // Populate Static Keys (ENC, MAC, DEK)
        // memset(&g_scp03_static_ctx, 0, sizeof(g_scp03_static_ctx));
        // g_scp03_static_ctx.keyVerNo = 0x02; // 0x01/0x70 for Platform SCP

        // // Allocate & set static base keys
        // alloc_and_set_host_key(&g_host_ks, &g_scp03_static_ctx.Enc, 1, KEY_CODE_ENC, SCP03_MAX_AUTH_KEY_SIZE);
        // alloc_and_set_host_key(&g_host_ks, &g_scp03_static_ctx.Mac, 2, KEY_CODE_MAC, SCP03_MAX_AUTH_KEY_SIZE);
        // alloc_and_set_host_key(&g_host_ks, &g_scp03_static_ctx.Dek, 3, KEY_CODE_DEK, SCP03_MAX_AUTH_KEY_SIZE);

        // // Allocate Dynamic Session Keys (handles only, values generated on handshake)
        // memset(&g_scp03_dyn_ctx, 0, sizeof(g_scp03_dyn_ctx));
        // alloc_and_set_host_key(&g_host_ks, &g_scp03_dyn_ctx.Enc,  4, NULL, SCP03_MAX_AUTH_KEY_SIZE);
        // alloc_and_set_host_key(&g_host_ks, &g_scp03_dyn_ctx.Mac,  5, NULL, SCP03_MAX_AUTH_KEY_SIZE);
        // alloc_and_set_host_key(&g_host_ks, &g_scp03_dyn_ctx.Rmac, 6, NULL, SCP03_MAX_AUTH_KEY_SIZE);

        // status = sss_host_key_store_allocate(&g_host_ks, __LINE__);
        // if (kStatus_SSS_Success != status) {
        //     ESP_LOGE(TAG, "Host: sss_key_store_allocate failed");
        //     return false;
        // }

        // Populate SE_Connect_Ctx_t for SCP03
        memset(&g_connect_ctx, 0, sizeof(g_connect_ctx));
        g_connect_ctx.connType = kType_SE_Conn_Type_T1oI2C;
        g_connect_ctx.portName = NULL;
        // Set SCP03 authentication mode and link context pointers
        g_connect_ctx.auth.authType = kSSS_AuthType_None;
        // g_connect_ctx.auth.ctx.scp03.pStatic_ctx = &g_scp03_static_ctx;
        // g_connect_ctx.auth.ctx.scp03.pDyn_ctx    = &g_scp03_dyn_ctx;

        memset(&g_session, 0, sizeof(g_session));

        // Open clean, lightweight SSS session
        status = sss_session_open(&g_session, kType_SSS_SE_SE05x, 0, kSSS_ConnectionType_Plain, &g_connect_ctx);

        if (status != kStatus_SSS_Success) {
            ESP_LOGE("SE051", "sss_session_open failed: 0x%08X", status);
            return false;
        }

        g_session_active = true;
    }

    // Extract low-level session pointer for Se05x_API_* functions
    sss_se05x_session_t *p_se05x = (sss_se05x_session_t *)&g_session;
    return &p_se05x->s_ctx;
}

/**
 * @brief Closes SCP03 session, releases I2C bus and allocated memory.
 */
void se051_session_close(void) {
    ESP_LOGI(TAG, "Closing SE051 session...");
    
    if (!g_session_active) {
        return;
    }

    // Cleanly close the session
    sss_session_close(&g_session);
    memset(&g_session, 0, sizeof(g_session));

    g_session_active = false;
    
    // if (session_ctx != NULL && session_ctx->conn_context != NULL) {
    //     // Send session close APDU
    //     Se05x_API_SessionClose(session_ctx);

    //     // Close T=1oI2C transport layer
    //     smComT1oI2C_Close(session_ctx->conn_context);
    //     session_ctx->conn_context = NULL;
    // }
    // ex_sss_session_close(gex_sss_boot_ctx);
    
    // /* Turn off ENA */
    // gpio_set_level(GPIO_SE_ENA, 0);
}