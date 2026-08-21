# mcxPinState

Debug utility for [mcx-arduino-core](https://github.com/teddokano/mcx-arduino-core).
Prints a table, one row per named pin (every pin with at least one
Arduino-level alias -- D0, A2, MB_SDA, etc; pins sharing one physical pin,
like D10/SPI_CS/ARD_CS, are grouped into a single row), showing which
r01lib peripheral/GPIO instance currently holds it, its live PORT MUX
(ALT) register value, input-buffer-enable/open-drain/pull-resistor state,
and a Status column flagging two kinds of problem: more than one live
object claiming the same pin (CONFLICT), or a pin whose live ALT register
doesn't match what its sole owner actually requested (MISMATCH -- catches
something having silently re-muxed the pin out from under its owner after
the fact).

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

  pins.print();
  // === Pin MUX state (named pins only) ===
  // Name(s)                    Pin      MUX  IBE  ODE  Pull  Owner        Status
  // --------------------------------------------------------------------------------
  // D2                         P0_29    0    ON   -    -     -            -
  // D10, SPI_CS, ARD_CS        P0_27    0    ON   -    -     -            -
  // D18                        P4_0     4    ON   -    -     Wire         OK
  // D19                        P4_1     4    ON   -    -     Wire         OK
  // ...
}

void loop() {
}
```

See `examples/MultiPeripheralDump` for a fuller example, and
`examples/ConflictDemo` for a deterministic, wiring-free demonstration of
the conflict flag.

Pin names combine a physical name ("P1_17" style, synthesized from data
mcx-arduino-core's own io.cpp already keeps for itself -- no string table
added there) with any Arduino-level aliases that share it (e.g. "D18",
"MB_SDA", or "D10, SPI_CS, ARD_CS" when several names point at the same
physical pin). The alias table lives entirely in this library, not
mcx-arduino-core: its *values* come straight from mcx-arduino-core's own
`arduino_pin_by_number[]` (via `<Arduino.h>`), so they can't drift, and a
`static_assert` in `PinState.cpp` catches a length mismatch (an alias
added or removed on the mcx-arduino-core side) at compile time. Only the
name *list and order* are hand-maintained here, against `arduino_io.h`'s
own array -- worth a quick glance whenever mcx-arduino-core cuts a
release. This keeps the string-table cost fully opt-in: paid only by
sketches that construct a `PinState`, never added to mcx-arduino-core's
own footprint.

Aliases that mcx-arduino-core defines but that don't correspond to a real
pin on the current board (e.g. FRDM-MCXN947's `A0`/`A1`/`MB_AN`, which are
all fixed to io.h's `DISABLED_PIN` sentinel) are left out of the table
entirely, rather than grouped into one misleading row that looks like
they share a physical pin.

`Owner` currently shows the *class* of object holding a pin ("GPIO",
"Serial", "SPI", "AnalogIn", "PwmOut"), not which specific global instance
(e.g. `Wire` vs `Wire1` both show as owned by an `I2C`-family
`DigitalInOut`, reported generically as "GPIO") -- telling those apart
would mean this library also keeping a list of mcx-arduino-core's
well-known global instances (`Wire`, `Wire1`, `Wire2`, `SPI`, `SPI1`,
`Serial`, `Serial1`) to match registry entries against by address; not
implemented yet.

## Status

Functional and verified on real hardware (both FRDM-MCXA153 and
FRDM-MCXN947) -- ownership tracking (conflict detection), the MUX
expectation cross-check, and IBE/open-drain/pull-resistor reporting.
Development turned up three real bugs in mcx-arduino-core along the way,
all fixed and confirmed on hardware there:

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
- A false-positive MISMATCH on I3C's SDA/SCL pins, found with
  `examples/CombinedPeripheralsAudit`: `I3C`'s constructor declared local
  `DigitalInOut` variables shadowing the same-named, persistent members it
  inherits (privately) from `I2C`, so the pins it actually configured were
  throwaway objects gone by the time the registry got read. The hardware
  itself was always configured correctly (PORT_PCR is per-pin, not
  per-object) -- fixed by making the inherited members accessible and
  muxing those directly instead of shadowing them

Physical-name and Arduino-alias display (see above) is also confirmed on
real hardware on both boards.
