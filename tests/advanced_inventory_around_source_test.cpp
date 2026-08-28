#include <array>
#include <optional>

#include "advanced_inv_area.h"
#include "advanced_inv_endpoint.h"
#include "advanced_inv_source.h"
#include "avatar.h"
#include "cata_catch.h"
#include "coordinates.h"
#include "item.h"
#include "map.h"
#include "map_helpers.h"
#include "point.h"
#include "type_id.h"
#include "units.h"
#include "vehicle.h"
#include "vpart_position.h"

static const itype_id itype_backpack( "backpack" );
static const itype_id itype_knife_combat( "knife_combat" );
static const vproto_id vehicle_prototype_shopping_cart( "shopping_cart" );

static std::array<advanced_inv_area, NUM_AIM_LOCATIONS> make_AIM_areas()
{
    std::array<advanced_inv_area, NUM_AIM_LOCATIONS> areas = {{
        advanced_inv_area( AIM_INVENTORY ),
        advanced_inv_area( AIM_SOUTHWEST ),
        advanced_inv_area( AIM_SOUTH ),
        advanced_inv_area( AIM_SOUTHEAST ),
        advanced_inv_area( AIM_WEST ),
        advanced_inv_area( AIM_CENTER ),
        advanced_inv_area( AIM_EAST ),
        advanced_inv_area( AIM_NORTHWEST ),
        advanced_inv_area( AIM_NORTH ),
        advanced_inv_area( AIM_NORTHEAST ),
        advanced_inv_area( AIM_DRAGGED ),
        advanced_inv_area( AIM_ALL ),
        advanced_inv_area( AIM_CONTAINER ),
        advanced_inv_area( AIM_PARENT ),
        advanced_inv_area( AIM_WORN )
    }};

    // Single-argument areas do not carry directional offsets; this test only needs east.
    areas[AIM_EAST].off = tripoint_rel_ms::east;
    for( advanced_inv_area &area : areas ) {
        area.init();
    }
    return areas;
}

static void add_cart_item( map &here, const tripoint_bub_ms &pos, const item &it )
{
    vehicle *cart = here.add_vehicle( vehicle_prototype_shopping_cart, pos, 0_degrees, 0,
                                     veh_spawn_status::UNDAMAGED );
    REQUIRE( cart != nullptr );

    const std::optional<vpart_reference> cargo = here.veh_at( pos ).cargo();
    REQUIRE( cargo.has_value() );
    vehicle_stack cargo_items = cargo->items();
    cargo_items.clear();
    cargo_items.insert( here, it );
}

TEST_CASE( "AIM_around_source_excludes_endpoint_not_square", "[items][advanced_inv]" )
{
    clear_avatar();
    clear_map_without_vision();
    clear_vehicles();

    avatar &u = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms east = u.pos_bub() + tripoint_rel_ms::east;

    here.add_item_or_charges( east, item( itype_knife_combat ) );
    add_cart_item( here, east, item( itype_backpack ) );

    std::array<advanced_inv_area, NUM_AIM_LOCATIONS> areas = make_AIM_areas();
    advanced_inv_area &east_area = areas[AIM_EAST];
    REQUIRE( east_area.get_endpoint( false ).has_value() );
    REQUIRE( east_area.get_endpoint( true ).has_value() );

    const advanced_inv_filter_predicate show_all = []( const item & ) {
        return false;
    };

    SECTION( "excluding east ground retains east vehicle cargo" ) {
        advanced_inv_source_snapshot snapshot = enumerate_advanced_inv_around_sources(
                areas, east_area.get_endpoint( false ), show_all );

        REQUIRE( snapshot.rows.size() == 1 );
        CHECK( snapshot.rows.front().id == itype_backpack );
        REQUIRE( snapshot.rows.front().endpoint.has_value() );
        CHECK( *snapshot.rows.front().endpoint == *east_area.get_endpoint( true ) );
        CHECK( snapshot.rows.front().from_vehicle );
    }

    SECTION( "excluding east cargo retains east ground" ) {
        advanced_inv_source_snapshot snapshot = enumerate_advanced_inv_around_sources(
                areas, east_area.get_endpoint( true ), show_all );

        REQUIRE( snapshot.rows.size() == 1 );
        CHECK( snapshot.rows.front().id == itype_knife_combat );
        REQUIRE( snapshot.rows.front().endpoint.has_value() );
        CHECK( *snapshot.rows.front().endpoint == *east_area.get_endpoint( false ) );
        CHECK_FALSE( snapshot.rows.front().from_vehicle );
    }

    SECTION( "no exclusion includes both endpoints" ) {
        advanced_inv_source_snapshot snapshot = enumerate_advanced_inv_around_sources(
                areas, std::nullopt, show_all );

        REQUIRE( snapshot.rows.size() == 2 );
        CHECK( snapshot.volume == snapshot.rows[0].volume + snapshot.rows[1].volume );
        CHECK( snapshot.weight == snapshot.rows[0].weight + snapshot.rows[1].weight );
    }
}
