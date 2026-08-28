#include <cstddef>
#include <optional>

#include "advanced_inv.h"
#include "advanced_inv_area.h"
#include "advanced_inv_endpoint.h"
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
#include "units.h"
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

TEST_CASE( "AIM_endpoint_identity_is_storage_not_screen_position", "[items][advanced_inv]" )
{
    clear_AIM_source_test_state();

    avatar &u = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms pos = u.pos_bub();

    add_shopping_cart_item( here, pos, item( itype_backpack ) );

    advanced_inventory advinv;
    advinv.init();

    advanced_inv_area &center = advinv.get_one_square( AIM_CENTER );
    const std::optional<advanced_inv_endpoint> ground = center.get_endpoint( false );
    const std::optional<advanced_inv_endpoint> cargo = center.get_endpoint( true );

    REQUIRE( ground.has_value() );
    REQUIRE( cargo.has_value() );
    CHECK( ground->kind() == advanced_inv_endpoint_kind::ground );
    CHECK( cargo->kind() == advanced_inv_endpoint_kind::vehicle_cargo );
    CHECK( *ground != *cargo );

    advanced_inventory_pane &pane = advinv.get_pane( advinv.get_src() );
    pane.set_area( center, false );
    const std::optional<advanced_inv_endpoint> pane_ground = pane.get_endpoint( center );
    REQUIRE( pane_ground.has_value() );
    CHECK( *pane_ground == *ground );

    pane.set_area( center, true );
    const std::optional<advanced_inv_endpoint> pane_cargo = pane.get_endpoint( center );
    REQUIRE( pane_cargo.has_value() );
    CHECK( *pane_cargo == *cargo );

    advanced_inv_area ground_alias( AIM_EAST );
    ground_alias.pos = center.pos;
    const std::optional<advanced_inv_endpoint> aliased_ground = ground_alias.get_endpoint( false );
    REQUIRE( aliased_ground.has_value() );
    CHECK( *ground == *aliased_ground );

    advanced_inv_area dragged_alias( AIM_DRAGGED );
    dragged_alias.pos = center.pos;
    dragged_alias.veh = center.veh;
    dragged_alias.vstor = center.vstor;
    const std::optional<advanced_inv_endpoint> aliased_cargo = dragged_alias.get_endpoint( true );
    REQUIRE( aliased_cargo.has_value() );
    CHECK( *cargo == *aliased_cargo );
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
