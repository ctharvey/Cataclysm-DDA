#pragma once
#ifndef CATA_SRC_ADVANCED_INV_MOVE_ALL_H
#define CATA_SRC_ADVANCED_INV_MOVE_ALL_H

#include <cstdint>

#include "item_location.h"

class Character;
class advanced_inv_area;

enum class advanced_inv_move_all_disposition : std::uint8_t {
    normal,
    favorite,
    skip_destination_item,
    skip_liquid_or_gas,
    skip_destination_rejected,
    skip_nonempty_corpse,
    defer_bucket,
    defer_wielded
};

/**
 * Classify one item according to current move-all row policy.
 *
 * This is read-only: prompts, bucket spilling, unwielding, sorting, and activity
 * construction remain controller responsibilities until later tranches.
 */
advanced_inv_move_all_disposition assess_advanced_inv_move_all_item(
    Character &player, const item_location &it,
    const advanced_inv_area &destination_area, bool destination_in_vehicle,
    const item_location &destination_container, bool forbid_buckets );

/** Current move-all bucket policy derived from a concrete destination. */
bool advanced_inv_move_all_forbids_buckets(
    const advanced_inv_area &destination_area, bool destination_in_vehicle,
    const item_location &destination_container );

#endif // CATA_SRC_ADVANCED_INV_MOVE_ALL_H
