# mcxPinState

Debug utility for [mcx-arduino-core](https://github.com/teddokano/mcx-arduino-core).
Lists which r01lib peripheral/GPIO instances are currently alive and which
physical pins each one holds, flagging pins claimed by more than one
instance at once.

Depends on mcx-arduino-core's internal pin representation directly — this
library is not usable with any other Arduino core.

## Zero cost unless used

mcx-arduino-core's peripheral classes call a pair of weak, empty hook
functions on construction/destruction. Those hooks cost a few bytes per
call site and nothing else, in every sketch, always.

Constructing a `PinState` object anywhere in a sketch pulls in this
library's strong override of those hooks, along with the actual pin
registry and reporting code. Without that object, none of it links in.

## Status

Design in progress -- not yet functional.
