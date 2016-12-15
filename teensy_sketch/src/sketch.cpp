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
#include "FastLED.h"

// +---------------------------------------------------------------------------+
// | DEFINES AND CONSTANTS
// +---------------------------------------------------------------------------+
#ifndef UNUSED
#define UNUSED(STATEMENT) (void) STATEMENT
#endif

#define WS2812_DATA (11)
#define TEENSY_LED (13)
#define DIMVALUE_AVERAGE_SIZE (64)

// +---------------------------------------------------------------------------+
// | STATIC DATA
// +---------------------------------------------------------------------------+
/**
 * Set this to the colour to use for the WS2812 LED on the dev board.
 */
static const CRGB _on_colour(CRGB::White);

static const size_t leds_count = 5;
static CRGB leds[leds_count];
static DimmerSwitch *_light_switch;
static CRGB _colour(_on_colour);
static uint32_t _target_brightness = 255;

// +---------------------------------------------------------------------------+
// | DimmerSwitch CALLBACKS
// +---------------------------------------------------------------------------+
static void _on_switch(DimmerSwitch *lightswitch, bool is_on, void *user_data)
{
    UNUSED(lightswitch);
    UNUSED(user_data);
    if (!is_on) {
        _colour = CRGB::Black;
    } else {
        _colour = _on_colour;
    }
}

static void _on_dim(DimmerSwitch *lightswitch, uint8_t dim_value, void *user_data)
{
    const uint32_t current_brightness = _target_brightness;
    _target_brightness *= DIMVALUE_AVERAGE_SIZE;
    _target_brightness -= current_brightness;
    _target_brightness += dim_value;
    _target_brightness = _target_brightness / DIMVALUE_AVERAGE_SIZE;
}

// +---------------------------------------------------------------------------+
// | ARDUINO SKETCH
// +---------------------------------------------------------------------------+
void setup()
{
    FastLED.addLeds<WS2812B, WS2812_DATA, GRB>(leds, leds_count);
    _light_switch = get_instance_switch();
    _light_switch->set_indicator_pin(_light_switch, TEENSY_LED, true);
    _light_switch->set_on_switch(_light_switch, _on_switch, 0);
    _light_switch->set_on_dim(_light_switch, _on_dim, 0);
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    Serial.println("Starting starlight...");
}

void loop()
{
    _light_switch->service(_light_switch);
    FastLED.setBrightness(_target_brightness);
    FastLED.showColor(_colour);
}
