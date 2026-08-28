#include "advanced_inv_move_all.h"
#include "cata_catch.h"

TEST_CASE( "AIM_move_all_order_mirrors_activity_processing_direction",
           "[items][advanced_inv]" )
{
    const advanced_inv_move_all_sort_key small { 1, 5 };
    const advanced_inv_move_all_sort_key large { 2, 1 };
    const advanced_inv_move_all_sort_key same_primary_smaller_secondary { 1, 3 };

    SECTION( "ordinary destinations process ascending keys" ) {
        CHECK( advanced_inv_move_all_key_before( small, large, false ) );
        CHECK_FALSE( advanced_inv_move_all_key_before( large, small, false ) );
        CHECK_FALSE( advanced_inv_move_all_key_before(
                         small, same_primary_smaller_secondary, false ) );
    }

    SECTION( "inventory reverses storage order because pickup consumes from the back" ) {
        CHECK_FALSE( advanced_inv_move_all_key_before( small, large, true ) );
        CHECK( advanced_inv_move_all_key_before( large, small, true ) );
        CHECK( advanced_inv_move_all_key_before(
                   small, same_primary_smaller_secondary, true ) );
    }
}
