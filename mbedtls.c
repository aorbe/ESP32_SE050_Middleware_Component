#include "esp_random.h"
#include <stddef.h>

int mbedtls_entropy_func(void *data, unsigned char *output, size_t len) {
    (void)data;
    esp_fill_random(output, len);
    return 0;
}

/* If the linker also complains about missing entropy init/free symbols: */
void mbedtls_entropy_init(void *ctx) {
    (void)ctx;
}

void mbedtls_entropy_free(void *ctx) {
    (void)ctx;
}