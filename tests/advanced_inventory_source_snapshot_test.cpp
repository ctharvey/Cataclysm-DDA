#include <optional>

#include "advanced_inv_endpoint.h"
#include "advanced_inv_source.h"
#include "avatar.h"
#include "cata_catch.h"
#include "character_attire.h"
#include "item.h"
#include "item_location.h"
#include "map_helpers.h"
#include "type_id.h"

static const itype_id itype_backpack( "backpack" );
static const itype_id itype_knife_combat( "knife_combat" );

TEST_CASE( "AIM_source_snapshots_preserve_logical_transfer_boundaries",
           "[items][advanced_inv]" )
{
    clear_avatar();
    clear_map_without_vision();

    avatar &u = get_avatar();
    item backpack( itype_backpack );
    const std::optional<std::list<item>::iterator> worn =
        u.worn.wear_item( u, backpack, false, false );
    REQUIRE( worn.has_value() );

    item_location stored_knife = u.try_add( item( itype_knife_combat ), nullptr, nullptr,
                                           false, false );
    REQUIRE( stored_knife );

    const advanced_inv_filter_predicate show_all = []( const item & ) {
        return false;
    };

    SECTION( "inventory source treats carried pocket contents as inventory" ) {
        advanced_inv_source_snapshot snapshot =
            enumerate_advanced_inv_inventory_source( u, show_all );

        REQUIRE( snapshot.rows.size() == 1 );
        CHECK( snapshot.rows.front().id == itype_knife_combat );
        REQUIRE( snapshot.rows.front().endpoint.has_value() );
        CHECK( snapshot.rows.front().endpoint->kind() == advanced_inv_endpoint_kind::inventory );
        CHECK_FALSE( snapshot.rows.front().from_vehicle );
        CHECK( snapshot.volume == snapshot.rows.front().volume );
        CHECK( snapshot.weight == snapshot.rows.front().weight );
    }

    SECTION( "worn source exposes the backpack as worn" ) {
        advanced_inv_source_snapshot snapshot =
            enumerate_advanced_inv_worn_source( u, show_all );

        REQUIRE( snapshot.rows.size() == 1 );
        CHECK( snapshot.rows.front().id == itype_backpack );
        REQUIRE( snapshot.rows.front().endpoint.has_value() );
        CHECK( snapshot.rows.front().endpoint->kind() == advanced_inv_endpoint_kind::worn );
    }

    SECTION( "container source identifies the actual parent container" ) {
        std::vector<item_location> worn_items = u.worn.top_items_loc( u );
        REQUIRE( worn_items.size() == 1 );
        const item_location backpack_loc = worn_items.front();

        advanced_inv_source_snapshot snapshot =
            enumerate_advanced_inv_container_source( backpack_loc, show_all );

        REQUIRE( snapshot.rows.size() == 1 );
        CHECK( snapshot.rows.front().id == itype_knife_combat );
        REQUIRE( snapshot.rows.front().endpoint.has_value() );
        CHECK( *snapshot.rows.front().endpoint ==
               advanced_inv_endpoint::item_container( backpack_loc ) );
    }

    SECTION( "filtering is applied before metrics are exposed" ) {
        const advanced_inv_filter_predicate hide_all = []( const item & ) {
            return true;
        };
        advanced_inv_source_snapshot snapshot =
            enumerate_advanced_inv_inventory_source( u, hide_all );

        CHECK( snapshot.rows.empty() );
        CHECK( snapshot.volume == 0_ml );
        CHECK( snapshot.weight == 0_gram );
    }
}
