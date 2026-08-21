/*
 *  @author Tedd OKANO
 *
 *  Released under the MIT license
 */

#ifndef MCX_PIN_STATE_H
#define MCX_PIN_STATE_H

#include <Arduino.h>

/** Debug utility for mcx-arduino-core: reports which r01lib
 *  peripheral/GPIO objects are currently alive and which physical pins
 *  each one holds, flagging any pin claimed by more than one object at
 *  once. Also cross-checks the pin's actual, currently-live PORT MUX
 *  (ALT) register value against what its (sole) owner says it wanted,
 *  flagging a mismatch -- catching cases where a pin was correctly
 *  claimed at some point but then silently re-muxed to something else
 *  afterward (a real bug this library's own development turned up in
 *  mcx-arduino-core: Serial1's constructor re-muxing I3C's already-
 *  claimed pins on FRDM-MCXN947, well after I3C's own constructor had
 *  already set them and considered the job done).
 *
 *  Depends directly on mcx-arduino-core's internal pin representation --
 *  not usable with any other Arduino core.
 *
 *  Costs nothing unless used: mcx-arduino-core's peripheral classes call
 *  a pair of weak, empty hook functions (pin_registry_note()/
 *  pin_registry_forget(), declared in mcx-arduino-core's own
 *  pin_registry.h) from their constructor/destructor. Those calls are
 *  always present (a few bytes each), but do nothing on their own.
 *  Constructing a PinState object anywhere in a sketch links in this
 *  library's strong override of those two functions -- along with the
 *  actual registry and this print() method -- which is what actually
 *  starts recording ownership. Without a PinState instance, none of that
 *  code is even linked in.
 *
 *  Pins are reported as mcx-arduino-core's raw internal pin numbers (the
 *  same values io.h's per-chip pin enum assigns), not symbolic names like
 *  "D18" or "P1_16" -- resolving those would mean this library keeping
 *  its own per-board name table in sync with mcx-arduino-core's, which
 *  defeats the purpose of not needing board-specific maintenance here.
 */
class PinState
{
public:
	PinState() = default;

	/** Print the current pin-ownership table: one line per physical pin
	 *  currently claimed by at least one live object, in the form
	 *  `Pin <n> [ALT<x>]: <owner(s)>`, followed by:
	 *    - "*** CONFLICT ***" if more than one owner claims the pin, or
	 *    - "*** MISMATCH (wanted ALT<y>) ***" if there's exactly one
	 *      owner but the pin's live PORT MUX register doesn't match the
	 *      ALT value that owner registered wanting
	 *  Neither marker appears when the pin has exactly one owner and the
	 *  live register matches what that owner expects.
	 * @param out stream to print to, defaults to Serial
	 */
	void print( Print &out = Serial ) const;
};

#endif // MCX_PIN_STATE_H
