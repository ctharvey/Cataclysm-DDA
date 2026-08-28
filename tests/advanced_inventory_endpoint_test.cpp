#include <algorithm>

#include "advanced_inv.h"
#include "advanced_inv_endpoint.h"
#include "advanced_inv_listitem.h"
#include "advanced_inv_pane.h"
#include "avatar.h"
#include "cata_catch.h"
#include "item.h"
#include "map_helpers.h"
#include "type_id.h"

static const itype_id itype_knife_combat( "knife_combat" );

TEST_CASE( "AIM_worn_view_distinguishes_wielded_row_endpoint", "[items][advanced_inv]" )
{
    clear_avatar();
    clear_map_without_vision();

    avatar &u = get_avatar();
    u.set_wielded_item( item( itype_knife_combat ) );
    REQUIRE( u.get_wielded_item() );

    advanced_inventory advinv;
    advinv.init();

    advanced_inventory_pane &pane = advinv.get_pane( advinv.get_src() );
    advanced_inv_area &worn = advinv.get_one_square( AIM_WORN );
    pane.set_area( worn );
    advinv.recalc_pane( advinv.get_src() );

    const auto wielded_row = std::find_if( pane.items.begin(), pane.items.end(),
    [&u]( const advanced_inv_listitem & row ) {
        return !row.items.empty() && row.items.front() == u.get_wielded_item();
    } );

    REQUIRE( wielded_row != pane.items.end() );
    REQUIRE( wielded_row->endpoint.has_value() );
    CHECK( wielded_row->endpoint->kind() == advanced_inv_endpoint_kind::wielded );

    const std::optional<advanced_inv_endpoint> pane_endpoint = pane.get_endpoint( worn );
    REQUIRE( pane_endpoint.has_value() );
    CHECK( pane_endpoint->kind() == advanced_inv_endpoint_kind::worn );
}
