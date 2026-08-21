/*
 *  @author Tedd OKANO
 *
 *  Released under the MIT license
 */

#include	<cstdio>
#include	"PinState.h"
#include	"pin_registry.h"

namespace {

constexpr uint8_t	MAX_ENTRIES		= 32;
constexpr uint8_t	MAX_PINS_EACH	= 4;

// Arduino-level alias names (D0, A2, MB_SDA, ...), in exactly the order
// mcx-arduino-core's own arduino_io.h lists them when building
// arduino_pin_by_number[] (index i here <-> arduino_pin_by_number[i]).
//
// Deliberately not sourced from mcx-arduino-core itself: mcx-arduino-core
// keeps no string table of its own (see io.cpp's pin_registry_pin_name(),
// which synthesizes "P1_17"-style physical names from data it already has
// instead of carrying one) precisely so this cost is opt-in, paid only by
// sketches that construct a PinState. The *values* below can't drift --
// they come from arduino_pin_by_number[] itself, which <Arduino.h>
// defines and which this file includes. Only the *list of names/order*
// is hand-maintained here, checked against arduino_io.h's own array
// (hardware/nxp/mcx/cores/arduino/arduino_api/arduino_io.h) whenever
// mcx-arduino-core cuts a release; the static_assert below catches a
// length mismatch (an added/removed alias) at compile time so that check
// can't be skipped and forgotten -- it just won't build. It can't catch a
// same-length re-ordering, but arduino_io.h's array is append-only in
// practice and change history-visible, so that risk is low.
constexpr const char *ALIAS_NAMES[]	=
{
	"D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9",
	"D10", "D11", "D12", "D13", "D18", "D19",
	"A0", "A1", "A2", "A3", "A4", "A5",
	"SW2", "SW3",
	"MB_AN", "MB_RST", "MB_CS", "MB_SCK", "MB_MISO", "MB_MOSI",
	"MB_PWM", "MB_INT", "MB_RX", "MB_TX", "MB_SCL", "MB_SDA",
	"RED", "GREEN", "BLUE",
	"SPI_CS", "SPI_MOSI", "SPI_MISO", "SPI_SCLK",
	"ARD_CS", "ARD_MOSI", "ARD_MISO", "ARD_SCK",
	"PWM0", "PWM1", "PWM2", "PWM3", "PWM4", "PWM5",
};

static_assert(
	sizeof( ALIAS_NAMES ) / sizeof( ALIAS_NAMES[ 0 ] ) == sizeof( arduino_pin_by_number ) / sizeof( arduino_pin_by_number[ 0 ] ),
	"mcxPinState's ALIAS_NAMES is out of sync with mcx-arduino-core's "
	"arduino_pin_by_number[] (arduino_io.h) -- an alias was added or "
	"removed there. Update ALIAS_NAMES (same order) to match." );

constexpr uint8_t	ALIAS_COUNT	= sizeof( arduino_pin_by_number ) / sizeof( arduino_pin_by_number[ 0 ] );

// Every ALIAS_NAMES entry that shares raw_pin's value, comma-joined, in
// ALIAS_NAMES order (e.g. raw_pin == D10's pin -> "D10, SPI_CS, ARD_CS").
// Linear scan is fine here: this only runs inside print(), a few dozen
// entries, never in a hot path.
void names_for( uint8_t raw_pin, char *buf, uint8_t buf_size )
{
	uint8_t	pos		= 0;
	bool	first	= true;

	buf[ 0 ]	= '\0';

	for ( uint8_t i = 0; i < ALIAS_COUNT && pos < buf_size; i++ )
	{
		if ( arduino_pin_by_number[ i ] != raw_pin )
			continue;

		int	n	= snprintf( buf + pos, buf_size - pos, "%s%s", first ? "" : ", ", ALIAS_NAMES[ i ] );

		if ( n < 0 )
			break;

		pos		+= (uint8_t)n;
		first	= false;
	}
}

struct Entry
{
	const void	*owner					= nullptr;
	const char	*name					= nullptr;
	uint8_t		pins[ MAX_PINS_EACH ]	= {};
	uint8_t		count					= 0;
	uint8_t		wanted_mux				= 0;
	bool		in_use					= false;
};

Entry	registry[ MAX_ENTRIES ];

}	// namespace

// Strong overrides of mcx-arduino-core's weak pin_registry_note()/
// pin_registry_forget() -- see PinState.h for why constructing a
// PinState object is what makes this translation unit (and so these
// definitions) link in at all.
extern "C" {

void pin_registry_note( const void *owner, const char *owner_name, const uint8_t *pins, uint8_t pin_count, uint8_t wanted_mux )
{
	Entry	*slot	= nullptr;

	// Re-registering the same owner (e.g. a pin re-muxed after begin())
	// reuses its existing slot rather than leaking a second one.
	for ( Entry &e : registry )
	{
		if ( e.in_use && e.owner == owner )
		{
			slot	= &e;
			break;
		}
	}

	if ( !slot )
	{
		for ( Entry &e : registry )
		{
			if ( !e.in_use )
			{
				slot	= &e;
				break;
			}
		}
	}

	// Table full -- this is a diagnostic aid, not something that should
	// be able to crash or panic a sketch, so just drop the registration.
	if ( !slot )
		return;

	slot->owner			= owner;
	slot->name			= owner_name;
	slot->count			= ( pin_count > MAX_PINS_EACH ) ? MAX_PINS_EACH : pin_count;
	slot->wanted_mux	= wanted_mux;

	for ( uint8_t i = 0; i < slot->count; i++ )
		slot->pins[ i ]	= pins[ i ];

	slot->in_use	= true;
}

void pin_registry_forget( const void *owner )
{
	for ( Entry &e : registry )
	{
		if ( e.in_use && e.owner == owner )
		{
			e.in_use	= false;
			e.owner		= nullptr;
			return;
		}
	}
}

}	// extern "C"

void PinState::print( Print &out ) const
{
	out.println( "=== Pin MUX state (named pins only) ===" );
	out.println( "Name(s)                    Pin      MUX  IBE  ODE  Pull  Owner        Status" );
	out.println( "--------------------------------------------------------------------------------" );

	uint8_t	seen[ ALIAS_COUNT ];
	uint8_t	seen_count	= 0;

	for ( uint8_t i = 0; i < ALIAS_COUNT; i++ )
	{
		uint8_t	pin		= (uint8_t)arduino_pin_by_number[ i ];

		// io.h's pin enum puts DISABLED_PIN (an alias with no physical pin
		// on this board at all, e.g. N947's A0/A1/MB_AN) at value 0, always
		// -- not a real, shared pin, so grouping every disabled alias into
		// one misleading "they share a pin" row (and reporting live PCR
		// state for pin 0, which is meaningless) would be worse than just
		// leaving them out of a table that's specifically about pins.
		if ( 0 == pin )
			continue;

		bool	already	= false;

		for ( uint8_t s = 0; s < seen_count; s++ )
		{
			if ( seen[ s ] == pin )
			{
				already	= true;
				break;
			}
		}

		if ( already )
			continue;

		seen[ seen_count++ ]	= pin;

		char	names_buf[ 40 ];
		char	pin_buf[ 12 ];
		char	owner_buf[ 24 ]	= "-";

		names_for( pin, names_buf, sizeof( names_buf ) );
		pin_registry_pin_name( pin, pin_buf, sizeof( pin_buf ) );

		PinPcrInfo	pcr			= pin_registry_read_pcr( pin );
		uint8_t		actual_mux	= pcr.mux;
		bool		valid		= actual_mux != 0xFF;

		int		owners			= 0;
		uint8_t	single_wanted	= 0;
		uint8_t	owner_pos		= 0;

		for ( const Entry &e : registry )
		{
			if ( !e.in_use )
				continue;

			for ( uint8_t j = 0; j < e.count; j++ )
			{
				if ( e.pins[ j ] != pin )
					continue;

				int	n	= snprintf( owner_buf + owner_pos, sizeof( owner_buf ) - owner_pos, "%s%s", (owners > 0) ? ", " : "", e.name );

				if ( n > 0 )
					owner_pos	+= (uint8_t)n;

				single_wanted	= e.wanted_mux;
				owners++;
				break;
			}
		}

		if ( owners == 0 )
			owner_buf[ 0 ]	= '-', owner_buf[ 1 ] = '\0';

		const char	*status;

		if ( owners == 0 )
			status	= "-";
		else if ( owners > 1 )
			status	= "CONFLICT";
		else if ( !valid || actual_mux != single_wanted )
			status	= "MISMATCH";
		else
			status	= "OK";

		char	line[ 100 ];

		if ( valid )
		{
			char	ibe_s[ 3 ]	= "-";
			char	ode_s[ 3 ]	= "-";
			char	pull_s[ 3 ]	= "-";

			if ( pcr.ibe )
				snprintf( ibe_s, sizeof( ibe_s ), "ON" );
			if ( pcr.ode )
				snprintf( ode_s, sizeof( ode_s ), "ON" );
			if ( pcr.pull == 1 )
				snprintf( pull_s, sizeof( pull_s ), "PD" );
			else if ( pcr.pull == 2 )
				snprintf( pull_s, sizeof( pull_s ), "PU" );

			snprintf( line, sizeof( line ), "%-26s %-8s %-4u %-4s %-4s %-5s %-12s %s",
				names_buf, pin_buf, actual_mux, ibe_s, ode_s, pull_s, owner_buf, status );
		}
		else
		{
			snprintf( line, sizeof( line ), "%-26s %-8s %-4s %-4s %-4s %-5s %-12s %s",
				names_buf, pin_buf, "-", "-", "-", "-", owner_buf, status );
		}

		out.println( line );
	}
}
