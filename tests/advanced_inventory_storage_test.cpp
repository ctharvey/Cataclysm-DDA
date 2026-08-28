#include <optional>

#include "advanced_inv_area.h"
#include "advanced_inv_endpoint.h"
#include "advanced_inv_storage.h"
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

TEST_CASE( "AIM_storage_state_distinguishes_ground_and_cargo_counts", "[items][advanced_inv]" )
{
    clear_avatar();
    clear_map_without_vision();
    clear_vehicles();

    avatar &u = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms pos = u.pos_bub() + tripoint_rel_ms::east;

    here.add_item_or_charges( pos, item( itype_knife_combat ) );
    here.add_item_or_charges( pos, item( itype_knife_combat ) );

    vehicle *cart = here.add_vehicle( vehicle_prototype_shopping_cart, pos, 0_degrees, 0,
                                     veh_spawn_status::UNDAMAGED );
    REQUIRE( cart != nullptr );
    const std::optional<vpart_reference> cargo_part = here.veh_at( pos ).cargo();
    REQUIRE( cargo_part.has_value() );
    vehicle_stack cargo_items = cargo_part->items();
    cargo_items.clear();
    cargo_items.insert( here, item( itype_backpack ) );

    advanced_inv_area east( AIM_EAST );
    east.off = tripoint_rel_ms::east;
    east.init();

    const std::optional<advanced_inv_storage_state> ground =
        inspect_advanced_inv_storage( east, false );
    const std::optional<advanced_inv_storage_state> cargo =
        inspect_advanced_inv_storage( east, true );

    REQUIRE( ground.has_value() );
    REQUIRE( cargo.has_value() );
    CHECK( ground->endpoint.kind() == advanced_inv_endpoint_kind::ground );
    CHECK( cargo->endpoint.kind() == advanced_inv_endpoint_kind::vehicle_cargo );
    CHECK( ground->endpoint != cargo->endpoint );
    CHECK( ground->item_count == 2 );
    CHECK( cargo->item_count == 1 );
    CHECK( ground->free_volume == here.free_volume( pos ) );
    CHECK( cargo->free_volume == cargo_items.free_volume() );
}
