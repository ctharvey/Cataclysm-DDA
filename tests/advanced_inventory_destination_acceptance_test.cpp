#include <vector>

#include "advanced_inv_area.h"
#include "advanced_inv_destination.h"
#include "advanced_inv_endpoint.h"
#include "avatar.h"
#include "cata_catch.h"
#include "character_attire.h"
#include "item.h"
#include "item_location.h"
#include "map_helpers.h"
#include "type_id.h"

static const itype_id itype_backpack( "backpack" );
static const itype_id itype_knife_combat( "knife_combat" );

TEST_CASE( "AIM_worn_destination_exposes_wield_fallback", "[items][advanced_inv]" )
{
    clear_avatar();
    clear_map_without_vision();

    advanced_inv_area worn( AIM_WORN );
    worn.init();

    SECTION( "wearable item is accepted" ) {
        const advanced_inv_destination_acceptance result =
            assess_advanced_inv_destination_acceptance( worn, false, item_location::nowhere,
                    item( itype_backpack ) );

        CHECK( result.accepted() );
        CHECK_FALSE( result.has_alternative() );
    }

    SECTION( "wieldable non-wearable item exposes wield alternative" ) {
        const advanced_inv_destination_acceptance result =
            assess_advanced_inv_destination_acceptance( worn, false, item_location::nowhere,
                    item( itype_knife_combat ) );

        CHECK( result.kind == advanced_inv_destination_acceptance_kind::wield_instead );
        REQUIRE( result.alternative_destination.has_value() );
        CHECK( result.alternative_destination->kind() == advanced_inv_endpoint_kind::wielded );
    }
}

TEST_CASE( "AIM_container_destination_acceptance_uses_selected_container",
           "[items][advanced_inv]" )
{
    clear_avatar();
    clear_map_without_vision();

    avatar &u = get_avatar();
    REQUIRE( u.worn.wear_item( u, item( itype_backpack ), false, false ).has_value() );
    std::vector<item_location> worn_items = u.worn.top_items_loc( u );
    REQUIRE( worn_items.size() == 1 );
    const item_location backpack = worn_items.front();

    advanced_inv_area container_area( AIM_CONTAINER );
    container_area.init();

    const advanced_inv_destination_acceptance result =
        assess_advanced_inv_destination_acceptance( container_area, false, backpack,
                item( itype_knife_combat ) );

    CHECK( result.accepted() );
}
