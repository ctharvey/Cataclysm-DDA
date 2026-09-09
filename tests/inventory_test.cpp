#include <optional>
#include <vector>

#include "calendar.h"
#include "cata_catch.h"
#include "coordinates.h"
#include "enums.h"
#include "inventory.h"
#include "item.h"
#include "map.h"
#include "map_helpers.h"
#include "point.h"
#include "type_id.h"
#include "vehicle.h"
#include "vpart_position.h"

static const itype_id itype_rock( "rock" );
static const itype_id itype_stick( "stick" );
static const itype_id itype_string_36( "string_36" );

static const vproto_id vehicle_prototype_shopping_cart( "shopping_cart" );

TEST_CASE( "inventory_form_from_map_bulk_batching", "[inventory][map]" )
{
    clear_map_without_vision();
    map &m = get_map();

    // Set up item positions
    tripoint_bub_ms p1( 0, 0, 0 );
    tripoint_bub_ms p2( 1, 0, 0 );
    tripoint_bub_ms p3( 2, 0, 0 );

    // Add items to the map
    item rock( itype_rock, calendar::turn );
    item stick( itype_stick, calendar::turn );
    item string( itype_string_36, calendar::turn );

    for( int i = 0; i < 5; ++i ) {
        m.add_item_or_charges( p1, rock );
        m.add_item_or_charges( p1, stick );
        m.add_item_or_charges( p1, stick ); // Extra stick
        m.add_item_or_charges( p2, stick );
        m.add_item_or_charges( p2, string );
        m.add_item_or_charges( p3, rock );
    }

    std::vector<tripoint_bub_ms> pts = { p1, p2, p3 };

    inventory inv;
    inv.form_from_map( m, pts, nullptr, false );

    // Check counts
    // Rocks: 5 at p1 + 5 at p3 = 10 rocks
    // Sticks: 10 at p1 + 5 at p2 = 15 sticks
    // Strings: 5 at p2 = 5 strings

    CHECK( inv.count_item( itype_rock ) == 10 );
    CHECK( inv.count_item( itype_stick ) == 15 );
    CHECK( inv.count_item( itype_string_36 ) == 5 );
}

TEST_CASE( "inventory_form_from_map_batches_vehicle_cargo", "[inventory][map][vehicle]" )
{
    clear_map_without_vision();
    map &m = get_map();

    const tripoint_bub_ms vehicle_pos( 0, 0, 0 );
    vehicle *veh = m.add_vehicle( vehicle_prototype_shopping_cart, vehicle_pos,
                                  0_degrees, 0, veh_spawn_status::UNDAMAGED );
    REQUIRE( veh );

    std::optional<vpart_reference> cargo = m.veh_at( m.get_abs( vehicle_pos ) ).cargo();
    REQUIRE( cargo );

    for( int i = 0; i < 5; ++i ) {
        REQUIRE( veh->add_item( m, cargo->part(),
                                item( itype_rock, calendar::turn ) ).has_value() );
        REQUIRE( veh->add_item( m, cargo->part(),
                                item( itype_stick, calendar::turn ) ).has_value() );
    }

    inventory inv;
    inv.form_from_map( m, { vehicle_pos }, nullptr, false );

    CHECK( inv.count_item( itype_rock ) == 5 );
    CHECK( inv.count_item( itype_stick ) == 5 );
}
