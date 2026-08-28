#include <cstddef>

#include "advanced_inv.h"
#include "advanced_inv_area.h"
#include "advanced_inv_listitem.h"
#include "advanced_inv_pane.h"
#include "avatar.h"
#include "cata_catch.h"
#include "coordinates.h"
#include "enums.h"
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

static void clear_AIM_source_test_state()
{
    clear_avatar();
    clear_map_without_vision();
    clear_vehicles();
}

static void add_shopping_cart_item( map &here, const tripoint_bub_ms &pos, const item &it )
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

TEST_CASE( "AIM_ground_and_vehicle_cargo_are_distinct_sources", "[items][advanced_inv]" )
{
    clear_AIM_source_test_state();

    avatar &u = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms pos = u.pos_bub();

    here.add_item_or_charges( pos, item( itype_knife_combat ) );
    add_shopping_cart_item( here, pos, item( itype_backpack ) );

    advanced_inventory advinv;
    advinv.init();

    const advanced_inventory::side src = advinv.get_src();
    advanced_inventory_pane &pane = advinv.get_pane( src );
    advanced_inv_area &center = advinv.get_one_square( AIM_CENTER );

    SECTION( "ground view exposes only ground items" ) {
        pane.set_area( center, false );
        advinv.recalc_pane( src );

        REQUIRE( pane.items.size() == 1 );
        CHECK( pane.items.front().id == itype_knife_combat );
        CHECK_FALSE( pane.items.front().from_vehicle );
    }

    SECTION( "vehicle view exposes only cargo items" ) {
        pane.set_area( center, true );
        advinv.recalc_pane( src );

        REQUIRE( pane.items.size() == 1 );
        CHECK( pane.items.front().id == itype_backpack );
        CHECK( pane.items.front().from_vehicle );
    }
}

TEST_CASE( "AIM_all_excludes_only_the_matching_ground_endpoint", "[items][advanced_inv]" )
{
    clear_AIM_source_test_state();

    avatar &u = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms east = u.pos_bub() + tripoint_rel_ms::east;

    here.add_item_or_charges( east, item( itype_knife_combat ) );
    add_shopping_cart_item( here, east, item( itype_backpack ) );

    advanced_inventory advinv;
    advinv.init();

    const advanced_inventory::side src = advinv.get_src();
    const advanced_inventory::side dest = advinv.get_dest();
    advanced_inventory_pane &spane = advinv.get_pane( src );
    advanced_inventory_pane &dpane = advinv.get_pane( dest );
    advanced_inv_area &east_area = advinv.get_one_square( AIM_EAST );

    dpane.set_area( east_area, false );
    spane.set_area( advinv.get_one_square( AIM_ALL ) );
    advinv.recalc_pane( dest );
    advinv.recalc_pane( src );

    REQUIRE( spane.items.size() == 1 );
    CHECK( spane.items.front().id == itype_backpack );
    CHECK( spane.items.front().area == AIM_EAST );
    CHECK( spane.items.front().from_vehicle );
}

TEST_CASE( "AIM_all_excludes_only_the_matching_vehicle_endpoint", "[items][advanced_inv]" )
{
    clear_AIM_source_test_state();

    avatar &u = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms east = u.pos_bub() + tripoint_rel_ms::east;

    here.add_item_or_charges( east, item( itype_knife_combat ) );
    add_shopping_cart_item( here, east, item( itype_backpack ) );

    advanced_inventory advinv;
    advinv.init();

    const advanced_inventory::side src = advinv.get_src();
    const advanced_inventory::side dest = advinv.get_dest();
    advanced_inventory_pane &spane = advinv.get_pane( src );
    advanced_inventory_pane &dpane = advinv.get_pane( dest );
    advanced_inv_area &east_area = advinv.get_one_square( AIM_EAST );

    dpane.set_area( east_area, true );
    spane.set_area( advinv.get_one_square( AIM_ALL ) );
    advinv.recalc_pane( dest );
    advinv.recalc_pane( src );

    REQUIRE( spane.items.size() == 1 );
    CHECK( spane.items.front().id == itype_knife_combat );
    CHECK( spane.items.front().area == AIM_EAST );
    CHECK_FALSE( spane.items.front().from_vehicle );
}
