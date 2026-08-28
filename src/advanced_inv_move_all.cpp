#include "advanced_inv_move_all.h"

#include <climits>

#include "advanced_inv_area.h"
#include "character.h"
#include "item.h"
#include "ret_val.h"

advanced_inv_move_all_disposition assess_advanced_inv_move_all_item(
    Character &player, const item_location &it,
    const advanced_inv_area &destination_area,
    const item_location &destination_container, bool forbid_buckets )
{
    if( destination_container && it == destination_container ) {
        return advanced_inv_move_all_disposition::skip_destination_item;
    }

    if( ( it->made_of_from_type( phase_id::LIQUID ) && !it->is_frozen_liquid() ) ||
        it->made_of_from_type( phase_id::GAS ) ) {
        return advanced_inv_move_all_disposition::skip_liquid_or_gas;
    }

    if( destination_area.id == AIM_INVENTORY ) {
        if( !player.can_stash_partial( *it ) ) {
            return advanced_inv_move_all_disposition::skip_destination_rejected;
        }
    } else if( destination_container &&
               !destination_container->can_contain_directly( *it ).success() ) {
        return advanced_inv_move_all_disposition::skip_destination_rejected;
    }

    if( it->is_corpse() && !it->empty_container() ) {
        return advanced_inv_move_all_disposition::skip_nonempty_corpse;
    }

    if( forbid_buckets && it->is_bucket_nonempty() ) {
        return advanced_inv_move_all_disposition::defer_bucket;
    }

    if( it == player.get_wielded_item() ) {
        return advanced_inv_move_all_disposition::defer_wielded;
    }

    return it->is_favorite ?
           advanced_inv_move_all_disposition::favorite :
           advanced_inv_move_all_disposition::normal;
}

bool advanced_inv_move_all_forbids_buckets(
    const advanced_inv_area &destination_area, bool destination_in_vehicle )
{
    return destination_area.id == AIM_INVENTORY ||
           destination_area.id == AIM_WORN ||
           destination_area.id == AIM_CONTAINER ||
           destination_in_vehicle;
}

advanced_inv_move_all_sort_key advanced_inv_move_all_key(
    const item &it, advanced_inv_move_all_priority priority )
{
    if( priority == advanced_inv_move_all_priority::none ) {
        return { 0, 0 };
    }

    const int weight = it.weight().value() > INT_MAX ? INT_MAX :
                       static_cast<int>( it.weight().value() );
    const int volume = it.volume().value();

    return priority == advanced_inv_move_all_priority::volume ?
           advanced_inv_move_all_sort_key( volume, weight ) :
           advanced_inv_move_all_sort_key( weight, volume );
}

bool advanced_inv_move_all_key_before(
    const advanced_inv_move_all_sort_key &lhs,
    const advanced_inv_move_all_sort_key &rhs,
    bool destination_is_inventory )
{
    if( lhs.first == rhs.first ) {
        return destination_is_inventory ? lhs.second > rhs.second : lhs.second < rhs.second;
    }
    return destination_is_inventory ? lhs.first > rhs.first : lhs.first < rhs.first;
}
