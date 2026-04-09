#ifndef BUCK_4V_H
#define BUCK_4V_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Enable the 4 V buck converter and wait for its power-good signal.
//
// The enable pin is driven high first. The function then blocks until the
// power-good pin reads high, which indicates the LED supply rail is ready.
esp_err_t buck_4v_enable_and_wait(void);

// Disable the 4 V buck converter by driving its enable pin low.
esp_err_t buck_4v_disable(void);

#ifdef __cplusplus
}
#endif

#endif // BUCK_4V_H
