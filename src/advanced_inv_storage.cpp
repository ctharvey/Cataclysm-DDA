#include "advanced_inv_storage.h"

#include <optional>

#include "advanced_inv_area.h"
#include "character.h"
#include "character_attire.h"
#include "inventory.h"
#include "item.h"
#include "map.h"
#include "pimpl.h"
#include "vehicle.h"

std::optional<advanced_inv_storage_state> inspect_advanced_inv_storage(
    const advanced_inv_area &area, bool in_vehicle, const item_location &container )
{
    const std::optional<advanced_inv_endpoint> endpoint = area.get_endpoint( in_vehicle, container );
    if( !endpoint.has_value() ) {
        return std::nullopt;
    }

    Character &player = get_player_character();
    advanced_inv_storage_state result { *endpoint };

    switch( endpoint->kind() ) {
        case advanced_inv_endpoint_kind::inventory:
            result.item_count = player.inv->size();
            result.free_volume = player.free_space();
            result.free_weight = player.free_weight_capacity();
            break;
        case advanced_inv_endpoint_kind::worn:
            result.item_count = player.worn.size();
            result.free_volume = player.free_space();
            result.free_weight = player.free_weight_capacity();
            break;
        case advanced_inv_endpoint_kind::ground:
            result.item_count = get_map().i_at( area.pos ).size();
            result.free_volume = get_map().free_volume( area.pos );
            result.free_weight = units::mass::max();
            break;
        case advanced_inv_endpoint_kind::vehicle_cargo: {
            vehicle_stack cargo = area.get_vehicle_stack();
            result.item_count = cargo.size();
            result.free_volume = cargo.free_volume();
            result.free_weight = units::mass::max();
            break;
        }
        case advanced_inv_endpoint_kind::container:
            result.item_count = container ? container->num_item_stacks() : 0;
            result.free_volume = container ? container->get_remaining_volume() : 0_ml;
            result.free_weight = container ? container->get_remaining_weight_capacity() : 0_gram;
            break;
        case advanced_inv_endpoint_kind::wielded:
            // Wielded is a row endpoint, not a directly selectable storage destination.
            return std::nullopt;
    }

    return result;
}
