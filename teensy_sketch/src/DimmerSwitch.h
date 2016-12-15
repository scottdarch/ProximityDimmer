/**
 * Copyright 2016 Scott Dixon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
#include <stdbool.h>

struct _DimmerSwitch;

/**
 * Callback function when the switch turns on/off.
 * @param  self            The object to apply the function to.
 * @param  is_on        True if the switch turned on (is now on) else false if the
 *                      switch turned off (is now off).
 * @param  user_data    Pointer
 */
typedef void (*on_switch_func)(struct _DimmerSwitch *self, bool is_on, void *user_data);
typedef void (*on_dim_func)(struct _DimmerSwitch *self, uint8_t dim_value, void *user_data);

typedef struct _DimmerSwitch {
    void (*service)(struct _DimmerSwitch *self);
    void (*set_on_switch)(struct _DimmerSwitch *self, on_switch_func callback, void *user_data);
    void (*set_on_dim)(struct _DimmerSwitch *self, on_dim_func callback, void *user_data);
    void (*set_indicator_pin)(struct _DimmerSwitch *self, unsigned int indicator_pin,
                              bool active_high);
} DimmerSwitch;

DimmerSwitch *get_instance_switch();

#ifdef __cplusplus
}
#endif
