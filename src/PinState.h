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
 *  once.
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
	 *  currently claimed by at least one live object, listing every
	 *  owner's name, with "*** CONFLICT ***" appended when more than one
	 *  owner claims the same pin.
	 * @param out stream to print to, defaults to Serial
	 */
	void print( Print &out = Serial ) const;
};

#endif // MCX_PIN_STATE_H
