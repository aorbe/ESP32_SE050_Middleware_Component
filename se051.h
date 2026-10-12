#ifndef SE051_H
#define SE051_H

/* NXP Plug & Trust Headers */
#include "ex_sss_boot.h"
#include "fsl_sss_se05x_apis.h"
#include "se05x_APDU.h"
#include "se05x_APDU_apis.h"
#include "i2c_esp32.h"

/* Include the test suite header from previous step */
// #include "test_se051_suite.h"

/* Global Boot Context provided by Plug & Trust */


#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief Initializes I2C, authenticates SCP03, and extracts pSe05xSession_t.
 */
bool se051_session_create(pSe05xSession_t *session);

/**
 * @brief Closes SCP03 session, releases I2C bus and allocated memory.
 */
void se051_session_close(void);

#ifdef __cplusplus
}
#endif

#endif