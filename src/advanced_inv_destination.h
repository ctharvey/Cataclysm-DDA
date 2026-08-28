#pragma once
#ifndef CATA_SRC_ADVANCED_INV_DESTINATION_H
#define CATA_SRC_ADVANCED_INV_DESTINATION_H

#include <cstdint>

#include "item_location.h"

class advanced_inv_area;
class item;

enum class advanced_inv_destination_limit : std::uint8_t {
    none,
    invalid_destination,
    no_inventory_pocket,
    volume_or_weight,
    item_count,
    pickup_weight,
    worn_slots
};

/**
 * Read-only result of AIM's quantity/capacity checks for one destination.
 *
 * This intentionally mirrors the current query_charges ordering. It does not yet
 * include item-specific container acceptance, NO_RELOAD, wear/wield validity,
 * prompts, or activity selection.
 */
struct advanced_inv_destination_assessment {
    int requested = 0;
    int accepted = 0;
    advanced_inv_destination_limit limit = advanced_inv_destination_limit::none;

    bool accepts_any() const {
        return accepted > 0;
    }

    bool is_limited() const {
        return accepted < requested;
    }
};

/**
 * Mirror the capacity/quantity portion of advanced_inventory::query_charges().
 *
 * @param area Concrete selectable destination area; AIM_ALL/AIM_PARENT are invalid.
 * @param in_vehicle For map areas, select cargo rather than ground.
 * @param container Active container when area is AIM_CONTAINER.
 * @param it Item being moved.
 * @param requested Initial requested item/charge count.
 * @param stacks_with_existing_charges Whether a count-by-charges item can merge into
 *        an existing destination stack without consuming another item-count slot.
 */
advanced_inv_destination_assessment assess_advanced_inv_destination_capacity(
    const advanced_inv_area &area, bool in_vehicle, const item_location &container,
    const item &it, int requested, bool stacks_with_existing_charges = false );

#endif // CATA_SRC_ADVANCED_INV_DESTINATION_H
