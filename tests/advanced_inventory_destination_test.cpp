#include <optional>

#include "advanced_inv_area.h"
#include "advanced_inv_destination.h"
#include "avatar.h"
#include "cata_catch.h"
#include "coordinates.h"
#include "item.h"
#include "map.h"
#include "map_helpers.h"
#include "point.h"
#include "type_id.h"
#include "vehicle.h"
#include "vpart_position.h"

static const itype_id itype_backpack( "backpack" );
static const itype_id itype_knife_combat( "knife_combat" );
static const vproto_id vehicle_prototype_shopping_cart( "shopping_cart" );

TEST_CASE( "AIM_destination_capacity_uses_concrete_endpoint_state", "[items][advanced_inv]" )
{
    clear_avatar();
    clear_map_without_vision();
    clear_vehicles();

    avatar &u = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms east_pos = u.pos_bub() + tripoint_rel_ms::east;

    here.add_item_or_charges( east_pos, item( itype_knife_combat ) );
    here.add_item_or_charges( east_pos, item( itype_knife_combat ) );

    vehicle *cart = here.add_vehicle( vehicle_prototype_shopping_cart, east_pos, 0_degrees, 0,
                                     veh_spawn_status::UNDAMAGED );
    REQUIRE( cart != nullptr );
    const std::optional<vpart_reference> cargo_part = here.veh_at( east_pos ).cargo();
    REQUIRE( cargo_part.has_value() );
    vehicle_stack cargo_items = cargo_part->items();
    cargo_items.clear();
    cargo_items.insert( here, item( itype_backpack ) );

    advanced_inv_area east( AIM_EAST );
    east.off = tripoint_rel_ms::east;
    east.init();
    // Use a tiny limit so the endpoint-count behavior is easy to characterize.
    east.max_size = 2;

    const item moving_item( itype_knife_combat );

    SECTION( "full ground does not make same-tile cargo full" ) {
        const advanced_inv_destination_assessment ground =
            assess_advanced_inv_destination_capacity( east, false, item_location::nowhere,
                    moving_item, 1 );
        const advanced_inv_destination_assessment cargo =
            assess_advanced_inv_destination_capacity( east, true, item_location::nowhere,
                    moving_item, 1 );

        CHECK_FALSE( ground.accepts_any() );
        CHECK( ground.limit == advanced_inv_destination_limit::item_count );
        CHECK( cargo.accepted == 1 );
        CHECK( cargo.limit == advanced_inv_destination_limit::none );
    }
}

TEST_CASE( "AIM_inventory_capacity_requires_a_usable_pocket", "[items][advanced_inv]" )
{
    clear_avatar();
    clear_map_without_vision();

    avatar &u = get_avatar();
    advanced_inv_area inventory( AIM_INVENTORY );
    inventory.init();
    const item moving_item( itype_knife_combat );

    SECTION( "without storage" ) {
        const advanced_inv_destination_assessment result =
            assess_advanced_inv_destination_capacity( inventory, false, item_location::nowhere,
                    moving_item, 1 );

        CHECK_FALSE( result.accepts_any() );
        CHECK( result.limit == advanced_inv_destination_limit::no_inventory_pocket );
    }

    SECTION( "with a backpack" ) {
        item backpack( itype_backpack );
        REQUIRE( u.worn.wear_item( u, backpack, false, false ).has_value() );

        const advanced_inv_destination_assessment result =
            assess_advanced_inv_destination_capacity( inventory, false, item_location::nowhere,
                    moving_item, 1 );

        CHECK( result.accepted == 1 );
    }
}
