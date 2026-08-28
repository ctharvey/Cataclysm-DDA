#pragma once
#ifndef CATA_SRC_ADVANCED_INV_TRANSFER_H
#define CATA_SRC_ADVANCED_INV_TRANSFER_H

#include <cstdint>
#include <optional>

#include "advanced_inv_endpoint.h"

enum class advanced_inv_transfer_kind : std::uint8_t {
    insert_into_container,
    drop_from_character,
    pickup_to_inventory,
    move_world_item,
    wear,
    wield,
    takeoff_to_inventory
};

/**
 * Minimal behavior-free transfer plan classification.
 *
 * This says which execution mechanism AIM needs; it does not choose quantities,
 * prompt, assign activities, or mutate any item.
 */
struct advanced_inv_transfer_plan {
    advanced_inv_endpoint source;
    advanced_inv_endpoint destination;
    advanced_inv_transfer_kind kind;
};

/**
 * Classify an endpoint-to-endpoint transfer according to current AIM move-one routing.
 * Same-endpoint and unsupported endpoint combinations return std::nullopt.
 */
std::optional<advanced_inv_transfer_plan> plan_advanced_inv_transfer(
    const advanced_inv_endpoint &source, const advanced_inv_endpoint &destination );

#endif // CATA_SRC_ADVANCED_INV_TRANSFER_H
