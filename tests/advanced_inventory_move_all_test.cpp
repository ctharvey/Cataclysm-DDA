#include "advanced_inv_area.h"
#include "advanced_inv_move_all.h"
#include "avatar.h"
#include "cata_catch.h"
#include "item.h"
#include "item_location.h"
#include "map.h"
#include "map_helpers.h"
#include "map_selector.h"
#include "type_id.h"

static const itype_id itype_knife_combat( "knife_combat" );
static const itype_id itype_water_clean( "water_clean" );

TEST_CASE( "AIM_move_all_row_disposition_is_policy_only", "[items][advanced_inv]" )
{
    clear_avatar();
    clear_map_without_vision();

    avatar &u = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms pos = u.pos_bub();

    advanced_inv_area ground( AIM_CENTER );
    ground.init();

    SECTION( "ordinary and favorite items are classified separately" ) {
        item &knife = here.add_item_or_charges( pos, item( itype_knife_combat ) );
        item_location knife_loc( map_cursor( pos ), &knife );

        CHECK( assess_advanced_inv_move_all_item( u, knife_loc, ground,
                item_location::nowhere, false ) == advanced_inv_move_all_disposition::normal );

        knife.set_favorite( true );
        CHECK( assess_advanced_inv_move_all_item( u, knife_loc, ground,
                item_location::nowhere, false ) == advanced_inv_move_all_disposition::favorite );
    }

    SECTION( "spilled liquid is skipped" ) {
        item water( itype_water_clean );
        REQUIRE( water.made_of_from_type( phase_id::LIQUID ) );
        item &map_water = here.add_item_or_charges( pos, water );
        item_location water_loc( map_cursor( pos ), &map_water );

        CHECK( assess_advanced_inv_move_all_item( u, water_loc, ground,
                item_location::nowhere, false ) ==
               advanced_inv_move_all_disposition::skip_liquid_or_gas );
    }

    SECTION( "inventory without a pocket rejects the row" ) {
        item &knife = here.add_item_or_charges( pos, item( itype_knife_combat ) );
        item_location knife_loc( map_cursor( pos ), &knife );
        advanced_inv_area inventory( AIM_INVENTORY );
        inventory.init();

        CHECK( assess_advanced_inv_move_all_item( u, knife_loc, inventory,
                item_location::nowhere, true ) ==
               advanced_inv_move_all_disposition::skip_destination_rejected );
    }

    SECTION( "wielded item is deferred" ) {
        u.set_wielded_item( item( itype_knife_combat ) );
        REQUIRE( u.get_wielded_item() );

        CHECK( assess_advanced_inv_move_all_item( u, u.get_wielded_item(), ground,
                item_location::nowhere, false ) ==
               advanced_inv_move_all_disposition::defer_wielded );
    }
}

TEST_CASE( "AIM_move_all_bucket_policy_depends_on_destination_shape", "[items][advanced_inv]" )
{
    clear_avatar();
    clear_map_without_vision();

    advanced_inv_area ground( AIM_CENTER );
    ground.init();
    advanced_inv_area inventory( AIM_INVENTORY );
    inventory.init();

    CHECK_FALSE( advanced_inv_move_all_forbids_buckets( ground, false ) );
    CHECK( advanced_inv_move_all_forbids_buckets( ground, true ) );
    CHECK( advanced_inv_move_all_forbids_buckets( inventory, false ) );
}
