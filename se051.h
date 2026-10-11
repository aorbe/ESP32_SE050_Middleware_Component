#ifndef SE051_H
#define SE051_H

/* NXP Plug & Trust Headers */
#include "ex_sss_boot.h"
#include "fsl_sss_se05x_apis.h"
#include "se05x_APDU.h"

/* Include the test suite header from previous step */
// #include "test_se051_suite.h"

/* Global Boot Context provided by Plug & Trust */
static ex_sss_boot_ctx_t gex_sss_boot_ctx;

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief Initializes I2C, authenticates SCP03, and extracts pSe05xSession_t.
 */
pSe05xSession_t se051_session_create(void);

/**
 * @brief Closes SCP03 session, releases I2C bus and allocated memory.
 */
void se051_session_close(pSe05xSession_t session_ctx);

#ifdef __cplusplus
}
#endif

#endif