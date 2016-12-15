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
#include <Arduino.h>
#include <DimmerSwitch.h>
#include <Wire.h>

// +---------------------------------------------------------------------------+
// | VL6180X PERIPHERAL
// +---------------------------------------------------------------------------+

static const char *vl6180x_get_error(uint8_t error) __attribute__((unused));
static uint8_t read_from_vl6180x_range(uint16_t start_addr, boolean send_stop, uint8_t *buffer,
                                       size_t buffer_len) __attribute__((unused));
static void vl6180x_emit_version() __attribute__((unused));

// +--[WIRING PINS]-----------------------------------------------------------+
#define PIN_SCL A5
#define PIN_SDA A4
// shutdown pin to reset the sensor.
#define PIN_SHUTDOWN A3
// Interrupts when a threshold is breached.
#define PIN_INT A2

// +--[I2C HELPERS]-----------------------------------------------------------+

#define VL6180X_I2C_ADDRESS 0x29

static uint8_t read_from_vl6180x(uint16_t reg_addr, boolean send_stop)
{
    Wire.beginTransmission(VL6180X_I2C_ADDRESS);
    Wire.write(0xFF & (reg_addr >> 0x8));
    Wire.write(0xFF & reg_addr);
    Wire.endTransmission(false);
    Wire.requestFrom(VL6180X_I2C_ADDRESS, 1, send_stop);
    if (!Wire.available()) {
        return 0;
    }
    return Wire.read();
}

static uint8_t read_from_vl6180x_range(uint16_t start_addr, boolean send_stop, uint8_t *buffer,
                                       size_t buffer_len)
{
    Wire.beginTransmission(VL6180X_I2C_ADDRESS);
    Wire.write(0xFF & (start_addr >> 0x8));
    Wire.write(0xFF & start_addr);
    Wire.endTransmission(false);
    Wire.requestFrom(VL6180X_I2C_ADDRESS, buffer_len, send_stop);
    size_t bytes_read = 0;
    for (; bytes_read < buffer_len; ++bytes_read) {
        while (!Wire.available()) {
        }
        buffer[bytes_read] = Wire.read();
    }
    return bytes_read;
}

static void write_to_vl6180x(uint16_t reg_addr, uint8_t value, boolean send_stop)
{
    Wire.beginTransmission(VL6180X_I2C_ADDRESS);
    Wire.write(0xFF & (reg_addr >> 0x8));
    Wire.write(0xFF & reg_addr);
    Wire.write(value);
    Wire.endTransmission(send_stop);
}

static void write_to_vl6180x_16(uint16_t reg_addr, uint16_t value, boolean send_stop)
{
    Wire.beginTransmission(VL6180X_I2C_ADDRESS);
    Wire.write(0xFF & (reg_addr >> 0x8));
    Wire.write(0xFF & reg_addr);
    Wire.write(0xFF & (value >> 0x8));
    Wire.write(0xFF & value);
    Wire.endTransmission(send_stop);
}

// +--[VL6180X INTERFACE]-----------------------------------------------------+
#define VL6180X_REG_IDENTIFICATION__MODEL_ID 0x000

#define VL6180X_REG_SYSTEM_MODE_GPIO1 0x011
#define VL6180X_REG_SYSTEM_INTERRUPT_CONFIG_GPIO 0x014
#define VL6180X_REG_SYSTEM_INTERRUPT_CLEAR 0x015
#define VL6180X_REG_SYSTEM_FRESH_OUT_OF_RESET 0x016
#define VL6180X_REG_SYSTEM_GROUPED_PARAMETER_HOLD 0x017

#define VL6180X_REG_SYSRANGE_START 0x018
#define VL6180X_REG_SYSRANGE_THRESH_LOW 0x01A
#define VL6180X_REG_SYSRANGE_INTERMEASUREMENT_PERIOD 0x01B
#define VL6180X_REG_SYSRANGE_MAX_CONVERGENCE_TIME 0x01C
#define VL6180X_REG_SYSRANGE_EARLY_CONVERGENCE_ESTIMATE 0x022

#define VL6180X_REG_RESULT_RANGE_STATUS 0x04D
#define VL6180X_REG_RESULT_INTERRUPT_STATUS_GPIO 0x04F
#define VL6180X_REG_RESULT_RANGE_VAL 0x062

#define VL6180X_REG_FIRMWARE_BOOTUP 0x119

#define NEAR_THRESHOLD_MM 255

typedef struct VL6180X_ID_t {
    uint8_t id : 8;
    uint8_t reserved_0 : 5;
    uint8_t model_maj : 3;
    uint8_t reserved_1 : 5;
    uint8_t model_min : 3;
    uint8_t reserved_2 : 5;
    uint8_t mod_maj : 3;
    uint8_t reserved_3 : 5;
    uint8_t mod_min : 3;
    uint8_t man_year : 4;
    uint8_t man_mon : 4;
    uint8_t man_day : 5;
    uint8_t man_phase : 3;
    uint16_t man_time : 16;
} VL6180X_ID;

static void vl6180x_emit_version()
{
    VL6180X_ID id;
    memset(&id, 0, sizeof(id));
    read_from_vl6180x_range(VL6180X_REG_IDENTIFICATION__MODEL_ID, true, (uint8_t *)&id, sizeof(id));
    Serial.print("VL6180X{ id: ");
    Serial.print(id.id);
    Serial.print(", model : ");
    Serial.print(id.model_maj);
    Serial.print('.');
    Serial.print(id.model_min);
    Serial.print(", module : ");
    Serial.print(id.mod_maj);
    Serial.print('.');
    Serial.print(id.model_min);
    Serial.println(", manufactured { ");
    Serial.print("    year : ");
    Serial.print(id.man_year);
    Serial.print(", month : ");
    Serial.print(id.man_mon);
    Serial.print(", day : ");
    Serial.print(id.man_day);
    Serial.print(", phase : ");
    Serial.print(id.man_phase);
    Serial.print(", time : ");
    Serial.print(id.man_time);
    Serial.println(" }}");
}

static int vl6180x_setup_for_range()
{
    if (!(0x1 & read_from_vl6180x(VL6180X_REG_RESULT_RANGE_STATUS, true))) {
        return 0;
    }
    write_to_vl6180x(VL6180X_REG_SYSTEM_GROUPED_PARAMETER_HOLD, 0x1, false);

    write_to_vl6180x(VL6180X_REG_SYSTEM_MODE_GPIO1, 0x10, false);
    write_to_vl6180x(VL6180X_REG_SYSTEM_INTERRUPT_CONFIG_GPIO, 0x01, false);
    write_to_vl6180x(VL6180X_REG_SYSRANGE_THRESH_LOW, NEAR_THRESHOLD_MM, false);
    write_to_vl6180x(VL6180X_REG_SYSRANGE_MAX_CONVERGENCE_TIME, 30, false);
    write_to_vl6180x(VL6180X_REG_SYSRANGE_INTERMEASUREMENT_PERIOD, 10, false);
    write_to_vl6180x_16(VL6180X_REG_SYSRANGE_EARLY_CONVERGENCE_ESTIMATE, 204, false);

    write_to_vl6180x(VL6180X_REG_SYSTEM_GROUPED_PARAMETER_HOLD, 0x0, false);
    return 1;
}

static void write_vl6180x_sr03(boolean send_stop)
{
    // Required by datasheet
    // http://www.st.com/st-web-ui/static/active/en/resource/technical/document/application_note/DM00122600.pdf
    write_to_vl6180x(0x0207, 0x01, false);
    write_to_vl6180x(0x0208, 0x01, false);
    write_to_vl6180x(0x0096, 0x00, false);
    write_to_vl6180x(0x0097, 0xfd, false);
    write_to_vl6180x(0x00e3, 0x00, false);
    write_to_vl6180x(0x00e4, 0x04, false);
    write_to_vl6180x(0x00e5, 0x02, false);
    write_to_vl6180x(0x00e6, 0x01, false);
    write_to_vl6180x(0x00e7, 0x03, false);
    write_to_vl6180x(0x00f5, 0x02, false);
    write_to_vl6180x(0x00d9, 0x05, false);
    write_to_vl6180x(0x00db, 0xce, false);
    write_to_vl6180x(0x00dc, 0x03, false);
    write_to_vl6180x(0x00dd, 0xf8, false);
    write_to_vl6180x(0x009f, 0x00, false);
    write_to_vl6180x(0x00a3, 0x3c, false);
    write_to_vl6180x(0x00b7, 0x00, false);
    write_to_vl6180x(0x00bb, 0x3c, false);
    write_to_vl6180x(0x00b2, 0x09, false);
    write_to_vl6180x(0x00ca, 0x09, false);
    write_to_vl6180x(0x0198, 0x01, false);
    write_to_vl6180x(0x01b0, 0x17, false);
    write_to_vl6180x(0x01ad, 0x00, false);
    write_to_vl6180x(0x00ff, 0x05, false);
    write_to_vl6180x(0x0100, 0x05, false);
    write_to_vl6180x(0x0199, 0x05, false);
    write_to_vl6180x(0x01a6, 0x1b, false);
    write_to_vl6180x(0x01ac, 0x3e, false);
    write_to_vl6180x(0x01a7, 0x1f, false);
    write_to_vl6180x(0x0030, 0x00, send_stop);
}

static const char *const VL6180X_ERR_0000 = "No error";
static const char *const VL6180X_ERR_0001 = "VCSEL Continuity Test";
static const char *const VL6180X_ERR_0010 = "VCSEL Watchdog Test";
static const char *const VL6180X_ERR_0011 = "VCSEL Watchdog";
static const char *const VL6180X_ERR_0100 = "PLL1 Lock";
static const char *const VL6180X_ERR_0101 = "PLL2 Lock";
static const char *const VL6180X_ERR_0110 = "Early Convergence Estimate";
static const char *const VL6180X_ERR_0111 = "Max Convergence";
static const char *const VL6180X_ERR_1000 = "No Target Ignore";
static const char *const VL6180X_ERR_1001 = "Not used";
static const char *const VL6180X_ERR_1010 = "Not used";
static const char *const VL6180X_ERR_1011 = "Max Signal To Noise Ratio";
static const char *const VL6180X_ERR_1100 = "Raw Ranging Algo Underflow";
static const char *const VL6180X_ERR_1101 = "Raw Ranging Algo Overflow";
static const char *const VL6180X_ERR_1110 = "Ranging Algo Underflow";
static const char *const VL6180X_ERR_1111 = "Ranging Algo Overflow";
static const char *const VL6180X_ERR_XXXX = "(unknown)";

static const char *vl6180x_get_error(uint8_t error)
{
    switch (error) {
    case 0:
        return VL6180X_ERR_0000;
    case 1:
        return VL6180X_ERR_0001;
    case 2:
        return VL6180X_ERR_0010;
    case 3:
        return VL6180X_ERR_0011;
    case 4:
        return VL6180X_ERR_0100;
    case 5:
        return VL6180X_ERR_0101;
    case 6:
        return VL6180X_ERR_0110;
    case 7:
        return VL6180X_ERR_0111;
    case 8:
        return VL6180X_ERR_1000;
    case 9:
        return VL6180X_ERR_1001;
    case 10:
        return VL6180X_ERR_1010;
    case 11:
        return VL6180X_ERR_1011;
    case 12:
        return VL6180X_ERR_1100;
    case 13:
        return VL6180X_ERR_1101;
    case 14:
        return VL6180X_ERR_1110;
    case 15:
        return VL6180X_ERR_1111;
    default:
        return VL6180X_ERR_XXXX;
    }
}

// +---------------------------------------------------------------------------+
// | DimmerSwitch :: PRIVATE DATA
// +---------------------------------------------------------------------------+
#define CHECK_FOR_RESET_EVERY_N_CYCLES 1000
#define RESET_WAIT_MILLIS 1200
#define CLICK_TIMEOUT 500

typedef enum {
    Vl6180STATE_NOT_INIT = 0,
    Vl6180STATE_WAITING_FOR_RESET,
    Vl6180STATE_POWERED,
    Vl6180STATE_FRESH_OUT_OF_RESET,
    Vl6180STATE_SR03_PROGRAMMED,
    Vl6180STATE_CONFIGURED,
    Vl6180STATE_INITIALIZED,
    Vl6180STATE_RANGING,
    Vl6180STATE_NEAR,
} Vl6180State;

typedef struct _Vl6180Switch {
    DimmerSwitch super;
    on_switch_func on_click_callback;
    void *on_click_user_data;
    on_dim_func on_down_callback;
    void *on_down_user_data;
    Vl6180State state;
    uint32_t range_count;
    uint32_t powered_on_at_millis;
    uint32_t near_at_millis;
    bool is_on;
    unsigned int indicator_pin;
    bool indicator_active_high;
} Vl6180Switch;

static Vl6180Switch _singleton;
static DimmerSwitch *_singleton_ptr = 0;

// +---------------------------------------------------------------------------+
// | DimmerSwitch :: PRIVATE METHODS
// +---------------------------------------------------------------------------+
static void _turn_on_indicator(Vl6180Switch *vlself)
{
    if (vlself->indicator_pin != 0) {
        digitalWrite(vlself->indicator_pin, (vlself->indicator_active_high) ? HIGH : LOW);
    }
}

static void _notify_down(Vl6180Switch *vlself, uint8_t distance_mm)
{
    noInterrupts();
    on_dim_func on_down = vlself->on_down_callback;
    void *user_data = vlself->on_down_user_data;
    interrupts();

    if (on_down) {
        on_down(&vlself->super,
                map(constrain(distance_mm, 10, NEAR_THRESHOLD_MM), 10, NEAR_THRESHOLD_MM, 0, 255),
                user_data);
    }
}

static void _notify_switch(Vl6180Switch *vlself)
{
    noInterrupts();
    on_switch_func callback = vlself->on_click_callback;
    void *user_data = vlself->on_click_user_data;
    interrupts();
    if (callback) {
        callback(&vlself->super, vlself->is_on, user_data);
    }
}

static void _turn_off_indicator(Vl6180Switch *vlself)
{
    if (vlself->indicator_pin != 0) {
        digitalWrite(vlself->indicator_pin, (vlself->indicator_active_high) ? LOW : HIGH);
    }
}

static void _handle_near(Vl6180Switch *vlself)
{
    if (Vl6180STATE_RANGING == vlself->state) {
        vlself->state = Vl6180STATE_NEAR;
        _turn_on_indicator(vlself);
        vlself->near_at_millis = millis();
    }
}

static void _handle_still_near(Vl6180Switch *vlself, uint8_t distance_mm)
{
    if (millis() - vlself->near_at_millis >= CLICK_TIMEOUT && !vlself->is_on) {
        vlself->is_on = true;
        _notify_switch(vlself);
    }
    _notify_down(vlself, distance_mm);
}

static void _handle_not_near(Vl6180Switch *vlself)
{
    if (Vl6180STATE_NEAR == vlself->state) {
        _turn_off_indicator(vlself);
        vlself->state = Vl6180STATE_RANGING;
        if (millis() - vlself->near_at_millis < CLICK_TIMEOUT) {
            // This was a fast pass which we interpret as a "click"
            vlself->is_on = !vlself->is_on;
            _notify_switch(vlself);
        }
    }
}

static bool _periodic_reset_check(Vl6180Switch *vlself)
{
    vlself->range_count++;
    if (vlself->range_count % CHECK_FOR_RESET_EVERY_N_CYCLES) {
        return !read_from_vl6180x(VL6180X_REG_SYSTEM_FRESH_OUT_OF_RESET, true);
    } else {
        return true;
    }
}

static void _handle_hot_plug(Vl6180Switch *vlself)
{
    vlself->state = Vl6180STATE_NOT_INIT;
    digitalWrite(PIN_SHUTDOWN, LOW);
}

// +---------------------------------------------------------------------------+
// | DimmerSwitch :: PUBLIC
// +---------------------------------------------------------------------------+

static void _service(DimmerSwitch *self)
{
    Vl6180Switch *vlself = (Vl6180Switch *)self;
    switch (vlself->state) {
    case Vl6180STATE_NOT_INIT: {
        digitalWrite(PIN_SHUTDOWN, HIGH);
        vlself->state                = Vl6180STATE_WAITING_FOR_RESET;
        vlself->powered_on_at_millis = millis();
    } break;
    case Vl6180STATE_WAITING_FOR_RESET: {
        if (millis() - vlself->powered_on_at_millis > RESET_WAIT_MILLIS) {
            vlself->state = Vl6180STATE_POWERED;
        }
    } break;
    case Vl6180STATE_POWERED: {
        if (read_from_vl6180x(VL6180X_REG_SYSTEM_FRESH_OUT_OF_RESET, true)) {
            vlself->state = Vl6180STATE_FRESH_OUT_OF_RESET;
        }
    } break;
    case Vl6180STATE_FRESH_OUT_OF_RESET: {
        write_vl6180x_sr03(true);
        vlself->state = Vl6180STATE_SR03_PROGRAMMED;
    } break;
    case Vl6180STATE_SR03_PROGRAMMED: {
        if (vl6180x_setup_for_range()) {
            vlself->state = Vl6180STATE_CONFIGURED;
        }
    } break;
    case Vl6180STATE_CONFIGURED: {
        write_to_vl6180x(VL6180X_REG_SYSTEM_FRESH_OUT_OF_RESET, 0x00, true);
        vlself->state = Vl6180STATE_INITIALIZED;
    } break;
    case Vl6180STATE_INITIALIZED: {
        write_to_vl6180x(VL6180X_REG_SYSRANGE_START, 0x03, true);
        vlself->state = Vl6180STATE_RANGING;
    } break;
    case Vl6180STATE_NEAR:
    case Vl6180STATE_RANGING: {
        if (!_periodic_reset_check(vlself)) {
            _handle_hot_plug(vlself);
        } else {
            const uint8_t int_status =
                read_from_vl6180x(VL6180X_REG_RESULT_INTERRUPT_STATUS_GPIO, true);
            if (int_status & 1) {
                // near
                write_to_vl6180x(VL6180X_REG_SYSTEM_INTERRUPT_CLEAR, 1, true);
                _handle_near(vlself);
            } else {
                const uint8_t status = read_from_vl6180x(VL6180X_REG_RESULT_RANGE_STATUS, true);
                if (!(0xF0 & status)) {
                    uint8_t range_mm = read_from_vl6180x(VL6180X_REG_RESULT_RANGE_VAL, true);
                    if (range_mm > NEAR_THRESHOLD_MM) {
                        _handle_not_near(vlself);
                    } else {
                        _handle_still_near(vlself, range_mm);
                    }
                } else {
                    // uncomment to spew internal sensor state.
                    // Serial.println(vl6180x_get_error(status >> 4));
                    _handle_not_near(vlself);
                }
            }
        }
    } break;
    default: {
    }
    }
}

static void _set_on_switch(DimmerSwitch *self, on_switch_func callback, void *user_data)
{
    Vl6180Switch *vlself = (Vl6180Switch *)self;
    noInterrupts();
    vlself->on_click_callback  = callback;
    vlself->on_click_user_data = user_data;
    interrupts();
}

static void _set_on_down(DimmerSwitch *self, on_dim_func callback, void *user_data)
{
    Vl6180Switch *vlself = (Vl6180Switch *)self;
    noInterrupts();
    vlself->on_down_callback  = callback;
    vlself->on_down_user_data = user_data;
    interrupts();
}

static void _set_indicator_pin(DimmerSwitch *self, unsigned int pin, bool active_high)
{
    Vl6180Switch *vlself          = (Vl6180Switch *)self;
    vlself->indicator_pin         = pin;
    vlself->indicator_active_high = active_high;
    pinMode(pin, OUTPUT);
    _turn_off_indicator(vlself);
}

static Vl6180Switch *init_vl6180switch(Vl6180Switch *self)
{
    if (self) {
        pinMode(PIN_SHUTDOWN, OUTPUT);
        pinMode(PIN_INT, INPUT_PULLUP);
        digitalWrite(PIN_SHUTDOWN, LOW);
        Wire.setSDA(PIN_SDA);
        Wire.setSCL(PIN_SCL);
        Wire.begin();
        memset(self, 0, sizeof(Vl6180Switch));
        self->super.set_on_switch     = _set_on_switch;
        self->super.set_on_dim        = _set_on_down;
        self->super.set_indicator_pin = _set_indicator_pin;
        self->super.service           = _service;
        self->state                   = Vl6180STATE_NOT_INIT;
    }
    return self;
}

DimmerSwitch *get_instance_switch()
{
    noInterrupts();
    if (!_singleton_ptr) {
        _singleton_ptr = &init_vl6180switch(&_singleton)->super;
    }
    interrupts();
    return _singleton_ptr;
}
