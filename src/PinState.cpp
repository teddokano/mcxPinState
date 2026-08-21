/*
 *  @author Tedd OKANO
 *
 *  Released under the MIT license
 */

#include	"PinState.h"
#include	"pin_registry.h"

namespace {

constexpr uint8_t	MAX_ENTRIES		= 32;
constexpr uint8_t	MAX_PINS_EACH	= 4;

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
	out.println( "=== Pin ownership ===" );

	uint8_t	seen[ MAX_ENTRIES * MAX_PINS_EACH ];
	uint8_t	seen_count	= 0;

	for ( const Entry &e : registry )
	{
		if ( !e.in_use )
			continue;

		for ( uint8_t i = 0; i < e.count; i++ )
		{
			uint8_t	pin		= e.pins[ i ];
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

			if ( seen_count < sizeof( seen ) )
				seen[ seen_count++ ]	= pin;

			out.print( "Pin " );
			out.print( pin );

			uint8_t	actual_mux	= pin_registry_read_mux( pin );

			if ( actual_mux != 0xFF )
			{
				out.print( " [ALT" );
				out.print( actual_mux );
				out.print( "]" );
			}

			out.print( ": " );

			int		owners			= 0;
			uint8_t	single_wanted	= 0;

			for ( const Entry &e2 : registry )
			{
				if ( !e2.in_use )
					continue;

				for ( uint8_t j = 0; j < e2.count; j++ )
				{
					if ( e2.pins[ j ] == pin )
					{
						if ( owners > 0 )
							out.print( ", " );
						out.print( e2.name );
						single_wanted	= e2.wanted_mux;
						owners++;
						break;
					}
				}
			}

			if ( owners > 1 )
			{
				out.print( "  *** CONFLICT ***" );
			}
			else if ( actual_mux != 0xFF && actual_mux != single_wanted )
			{
				out.print( "  *** MISMATCH (wanted ALT" );
				out.print( single_wanted );
				out.print( ") ***" );
			}

			out.println();
		}
	}

	if ( seen_count == 0 )
		out.println( "(no pins currently claimed)" );
}
