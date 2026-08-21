/** mcxPinState: deliberate pin-ownership conflict demo
 *
 *  Constructs two DigitalInOut objects on the same physical pin (D2) at
 *  once, without destroying the first -- a purely software conflict, no
 *  wiring needed. PinState::print() should show that pin with two "GPIO"
 *  owners and the "*** CONFLICT ***" marker.
 *
 *  This uses r01lib's DigitalInOut directly (bypassing pinMode()) to
 *  create the conflict deterministically -- see the existing
 *  Arduino_incompatible_API examples in mcx-arduino-core for this style
 *  of direct r01lib usage.
 */

#include <Arduino.h>
#include <PinState.h>

PinState pins;

DigitalInOut *a;
DigitalInOut *b;

void setup() {
  Serial.begin(115200);
  while (!Serial)
    ;

  a = new DigitalInOut(D2, DigitalInOut::OUTPUT);
  b = new DigitalInOut(D2, DigitalInOut::OUTPUT);  // same pin, both still alive

  Serial.println();
  Serial.println("Expect: Pin <D2's raw number> claimed by GPIO, GPIO -- *** CONFLICT ***");
  pins.print();
}

void loop() {
}
