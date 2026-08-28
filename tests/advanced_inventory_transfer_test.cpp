#include "advanced_inv_endpoint.h"
#include "advanced_inv_transfer.h"
#include "cata_catch.h"

TEST_CASE( "AIM_transfer_plan_is_endpoint_driven", "[items][advanced_inv]" )
{
    const advanced_inv_endpoint inventory = advanced_inv_endpoint::inventory();
    const advanced_inv_endpoint worn = advanced_inv_endpoint::worn();
    const advanced_inv_endpoint wielded = advanced_inv_endpoint::wielded();
    const advanced_inv_endpoint ground = advanced_inv_endpoint::ground( tripoint_bub_ms::zero );
    const advanced_inv_endpoint other_ground = advanced_inv_endpoint::ground(
                tripoint_bub_ms( point_bub_ms( 1, 0 ), 0 ) );

    SECTION( "same endpoint is not a transfer" ) {
        CHECK_FALSE( plan_advanced_inv_transfer( ground, ground ).has_value() );
    }

    SECTION( "character inventory to ground drops" ) {
        const auto plan = plan_advanced_inv_transfer( inventory, ground );
        REQUIRE( plan.has_value() );
        CHECK( plan->kind == advanced_inv_transfer_kind::drop_from_character );
    }

    SECTION( "ground to inventory picks up" ) {
        const auto plan = plan_advanced_inv_transfer( ground, inventory );
        REQUIRE( plan.has_value() );
        CHECK( plan->kind == advanced_inv_transfer_kind::pickup_to_inventory );
    }

    SECTION( "world endpoint to another world endpoint moves" ) {
        const auto plan = plan_advanced_inv_transfer( ground, other_ground );
        REQUIRE( plan.has_value() );
        CHECK( plan->kind == advanced_inv_transfer_kind::move_world_item );
    }

    SECTION( "worn to inventory takes off" ) {
        const auto plan = plan_advanced_inv_transfer( worn, inventory );
        REQUIRE( plan.has_value() );
        CHECK( plan->kind == advanced_inv_transfer_kind::takeoff_to_inventory );
    }

    SECTION( "destinations select wear and wield" ) {
        const auto wear_plan = plan_advanced_inv_transfer( ground, worn );
        const auto wield_plan = plan_advanced_inv_transfer( ground, wielded );
        REQUIRE( wear_plan.has_value() );
        REQUIRE( wield_plan.has_value() );
        CHECK( wear_plan->kind == advanced_inv_transfer_kind::wear );
        CHECK( wield_plan->kind == advanced_inv_transfer_kind::wield );
    }
}
