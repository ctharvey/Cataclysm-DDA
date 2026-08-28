#include "advanced_inv_destination.h"

#include <algorithm>
#include <optional>

#include "advanced_inv_area.h"
#include "advanced_inv_storage.h"
#include "character.h"
#include "item.h"
#include "itype.h"
#include "ret_val.h"
#include "type_id.h"
#include "units.h"

static const flag_id json_flag_NO_RELOAD( "NO_RELOAD" );

advanced_inv_destination_assessment assess_advanced_inv_destination_capacity(
    const advanced_inv_area &area, bool in_vehicle, const item_location &container,
    const item &it, int requested, bool stacks_with_existing_charges )
{
    advanced_inv_destination_assessment result;
    result.requested = requested;
    result.accepted = requested;

    if( requested <= 0 ) {
        result.accepted = 0;
        result.limit = advanced_inv_destination_limit::invalid_destination;
        return result;
    }

    const std::optional<advanced_inv_storage_state> storage =
        inspect_advanced_inv_storage( area, in_vehicle, container );
    if( !storage.has_value() ) {
        result.accepted = 0;
        result.limit = advanced_inv_destination_limit::invalid_destination;
        return result;
    }

    Character &player = get_player_character();
    const bool by_charges = it.count_by_charges();

    // Mirror query_charges: inventory first asks the pocket system how much can be stashed.
    if( area.id == AIM_INVENTORY ) {
        int copies_remaining = result.accepted;
        player.can_stash_partial( it, copies_remaining, /*ignore_pkt_settings=*/false );
        result.accepted -= copies_remaining;
        if( result.accepted <= 0 ) {
            result.accepted = 0;
            result.limit = advanced_inv_destination_limit::no_inventory_pocket;
            return result;
        }
    } else if( area.id != AIM_WORN ) {
        // Map, vehicle and direct-container destinations share the generic volume/mass pass.
        const int room_for = std::min( it.charges_per_volume( storage->free_volume ),
                                       it.charges_per_weight( storage->free_weight ) );
        if( room_for <= 0 ) {
            result.accepted = 0;
            result.limit = advanced_inv_destination_limit::volume_or_weight;
            return result;
        }
        if( room_for < result.accepted ) {
            result.accepted = room_for;
            result.limit = advanced_inv_destination_limit::volume_or_weight;
        }
    }

    // Mirror query_charges' item-count cap for map and vehicle storage.
    if( area.id != AIM_INVENTORY && area.id != AIM_WORN && area.id != AIM_CONTAINER ) {
        const int remaining_item_slots = area.max_size - storage->item_count;
        const bool adds_no_item_slot = by_charges && stacks_with_existing_charges;
        if( remaining_item_slots <= 0 && !adds_no_item_slot ) {
            result.accepted = 0;
            result.limit = advanced_inv_destination_limit::item_count;
            return result;
        }
        if( !by_charges && remaining_item_slots < result.accepted ) {
            result.accepted = std::max( 0, remaining_item_slots );
            result.limit = advanced_inv_destination_limit::item_count;
        }
    }

    // Mirror query_charges' pickup/overburden cap for carried destinations.
    if( area.id == AIM_INVENTORY || area.id == AIM_WORN ) {
        const units::mass unit_weight = it.weight() / ( by_charges ? it.charges : 1 );
        const units::mass max_weight = player.max_pickup_capacity() - player.weight_carried();
        if( unit_weight > 0_gram && unit_weight * result.accepted > max_weight ) {
            const int weight_max = max_weight / unit_weight;
            if( weight_max <= 0 ) {
                result.accepted = 0;
                result.limit = advanced_inv_destination_limit::pickup_weight;
                return result;
            }
            result.accepted = std::min( weight_max, result.accepted );
            result.limit = advanced_inv_destination_limit::pickup_weight;
        }
    }

    if( area.id == AIM_WORN ) {
        const itype_id &id = it.typeId();
        const int slots_available = id->max_worn - player.amount_worn( id );

        // Intentionally preserve current query_charges behavior: this uses the original
        // requested amount rather than the already-limited amount. That means this step
        // can overwrite an earlier pickup-weight limit. Treat any correction as a
        // separate behavior change after characterization.
        result.accepted = std::min( slots_available, requested );
        result.limit = result.accepted < requested ?
                       advanced_inv_destination_limit::worn_slots :
                       advanced_inv_destination_limit::none;
    }

    return result;
}

advanced_inv_destination_acceptance assess_advanced_inv_destination_acceptance(
    const advanced_inv_area &area, bool in_vehicle, const item_location &container,
    const item &it )
{
    advanced_inv_destination_acceptance result;

    const std::optional<advanced_inv_endpoint> endpoint = area.get_endpoint( in_vehicle, container );
    if( !endpoint.has_value() || !area.canputitems( container ) ) {
        result.kind = advanced_inv_destination_acceptance_kind::invalid_destination;
        return result;
    }

    if( area.id == AIM_CONTAINER ) {
        if( !container || !container->is_container() ) {
            result.kind = advanced_inv_destination_acceptance_kind::invalid_destination;
            return result;
        }
        if( container->has_flag( json_flag_NO_RELOAD ) ) {
            result.kind = advanced_inv_destination_acceptance_kind::no_reload;
            return result;
        }

        item candidate = it;
        ret_val<void> can_contain = container->can_contain( candidate );
        if( can_contain.success() ) {
            can_contain = container.parents_can_contain_recursive( &candidate );
        }
        if( !can_contain.success() ) {
            result.kind = advanced_inv_destination_acceptance_kind::container_rejected;
            result.reason = can_contain.str();
            return result;
        }
    }

    if( area.id == AIM_WORN ) {
        Character &player = get_player_character();
        const ret_val<void> can_wear = player.can_wear( it );
        if( can_wear.success() ) {
            result.kind = advanced_inv_destination_acceptance_kind::allowed;
            return result;
        }

        const ret_val<void> can_wield = player.can_wield( it );
        if( can_wield.success() ) {
            result.kind = advanced_inv_destination_acceptance_kind::wield_instead;
            result.reason = can_wear.str();
            result.alternative_destination = advanced_inv_endpoint::wielded();
            return result;
        }

        result.kind = advanced_inv_destination_acceptance_kind::wear_rejected;
        result.reason = can_wear.str();
        if( !can_wield.str().empty() ) {
            if( !result.reason.empty() ) {
                result.reason += "\n";
            }
            result.reason += can_wield.str();
        }
        return result;
    }

    result.kind = advanced_inv_destination_acceptance_kind::allowed;
    return result;
}
