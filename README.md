# mcxPinState

Debug utility for [mcx-arduino-core](https://github.com/teddokano/mcx-arduino-core).
Lists which r01lib peripheral/GPIO instances are currently alive and which
physical pins each one holds, flagging pins claimed by more than one
instance at once -- and, for pins with exactly one owner, cross-checking
that pin's live PORT MUX register against the ALT value that owner
actually requested, flagging a mismatch if something re-muxed it
afterward. Also reports each pin's input-buffer-enable, open-drain, and
pull-resistor state, straight off the live PCR register.

Depends on mcx-arduino-core's internal pin representation directly — this
library is not usable with any other Arduino core.

## Zero cost unless used

mcx-arduino-core's peripheral classes call a pair of weak, empty hook
functions on construction/destruction. Those hooks cost a few bytes per
call site and nothing else, in every sketch, always.

Constructing a `PinState` object anywhere in a sketch pulls in this
library's strong override of those hooks, along with the actual pin
registry and reporting code. Without that object, none of it links in.

## Usage

```cpp
#include <Arduino.h>
#include <PinState.h>

PinState pins;

void setup() {
  Serial.begin(115200);
  while (!Serial)
    ;

  pinMode(D2, OUTPUT);
  Wire.begin();
  SPI.begin();

  pins.print();   // e.g. "Pin 16 [ALT3 IBE]: Wire" -- one line per
                   // claimed pin, its live ALT and flags (IBE/OD/PD/PU),
                   // and "*** CONFLICT ***" if more than one live object
                   // holds it, or "*** MISMATCH (wanted ALTn) ***" if its
                   // single owner's requested ALT doesn't match what's
                   // actually in the register
}

void loop() {
}
```

See `examples/MultiPeripheralDump` for a fuller example, and
`examples/ConflictDemo` for a deterministic, wiring-free demonstration of
the conflict flag.

Pins are reported as mcx-arduino-core's raw internal pin numbers (io.h's
per-chip pin enum), not symbolic names like "D18" -- resolving those would
mean keeping a per-board name table in sync with mcx-arduino-core's own,
which defeats the point of this library needing zero board-specific
maintenance.

## Status

Functional and verified on real hardware (both FRDM-MCXA153 and
FRDM-MCXN947) -- ownership tracking (conflict detection) and the MUX
expectation cross-check. Development turned up two real bugs in
mcx-arduino-core along the way, both fixed there:

- A false-positive CONFLICT on SPI's CS pin (its internal bookkeeping
  object versus a sketch's own `pinMode(SS, ...)`)
- A false-positive MISMATCH on any pin a peripheral class re-muxes after
  building it as a plain `DigitalInOut` first (e.g. I2C/I3C's SDA/SCL) --
  `DigitalInOut::pin_mux()` wasn't keeping the registry's expectation in
  sync with the ALT it had just set
- A real, pre-existing bug in `PORT_SetPinPullUpDown()`, found by adding
  the IBE/open-drain/pull-resistor reporting and then confirming on real
  hardware with `examples/PullModeCheck`: its `enable`/`logic` parameters
  landed in the PS/PE fields swapped, which meant
  `pinMode(pin, INPUT_PULLDOWN)` silently left the pin with no pull
  enabled at all (`INPUT_PULLUP` happened to still work, since its
  enable=1/logic=1 combination is symmetric either way round)
