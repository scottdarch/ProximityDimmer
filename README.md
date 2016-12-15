# ProximityDimmer
TOF sensor based dimmer switch demonstration.

![Teensy32 demonstration board](https://github.com/scottdarch/ProximityDimmer/raw/master/docs/toy_small.jpg "Teensy32 demonstration board")

Build using [platformio](http://platformio.org/):

    cd teensy_sketch
    platformio run -t upload

## Use

Turns the LED on the dev board on/off when an object (e.g. your hand)
is quickly waved across the TOF sensor (but not too quickly).

Dimms the LED if an object hovers over the sensor. The dim amount
is proportional to the object's distance. Closer is dimmer.
