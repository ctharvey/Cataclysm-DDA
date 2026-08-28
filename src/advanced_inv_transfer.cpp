#include "advanced_inv_transfer.h"

#include <optional>

namespace
{
bool is_character_owned_source( advanced_inv_endpoint_kind kind )
{
    return kind == advanced_inv_endpoint_kind::inventory ||
           kind == advanced_inv_endpoint_kind::worn ||
           kind == advanced_inv_endpoint_kind::wielded;
}

bool is_world_destination( advanced_inv_endpoint_kind kind )
{
    return kind == advanced_inv_endpoint_kind::ground ||
           kind == advanced_inv_endpoint_kind::vehicle_cargo;
}
} // namespace

std::optional<advanced_inv_transfer_plan> plan_advanced_inv_transfer(
    const advanced_inv_endpoint &source, const advanced_inv_endpoint &destination )
{
    if( source == destination ) {
        return std::nullopt;
    }

    const advanced_inv_endpoint_kind src = source.kind();
    const advanced_inv_endpoint_kind dst = destination.kind();

    if( dst == advanced_inv_endpoint_kind::container ) {
        return advanced_inv_transfer_plan { source, destination,
                                            advanced_inv_transfer_kind::insert_into_container };
    }

    if( dst == advanced_inv_endpoint_kind::worn ) {
        return advanced_inv_transfer_plan { source, destination,
                                            advanced_inv_transfer_kind::wear };
    }

    if( dst == advanced_inv_endpoint_kind::wielded ) {
        return advanced_inv_transfer_plan { source, destination,
                                            advanced_inv_transfer_kind::wield };
    }

    if( src == advanced_inv_endpoint_kind::worn &&
        dst == advanced_inv_endpoint_kind::inventory ) {
        return advanced_inv_transfer_plan { source, destination,
                                            advanced_inv_transfer_kind::takeoff_to_inventory };
    }

    if( is_character_owned_source( src ) && is_world_destination( dst ) ) {
        return advanced_inv_transfer_plan { source, destination,
                                            advanced_inv_transfer_kind::drop_from_character };
    }

    if( dst == advanced_inv_endpoint_kind::inventory ) {
        return advanced_inv_transfer_plan { source, destination,
                                            advanced_inv_transfer_kind::pickup_to_inventory };
    }

    if( is_world_destination( dst ) ) {
        return advanced_inv_transfer_plan { source, destination,
                                            advanced_inv_transfer_kind::move_world_item };
    }

    return std::nullopt;
}
