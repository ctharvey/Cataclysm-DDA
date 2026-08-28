#pragma once
#ifndef CATA_SRC_ADVANCED_INV_MOVE_ALL_H
#define CATA_SRC_ADVANCED_INV_MOVE_ALL_H

#include <cstdint>
#include <utility>
#include <vector>

#include "advanced_inv_listitem.h"
#include "item_location.h"

class Character;
class advanced_inv_area;
class item;

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

enum class advanced_inv_move_all_priority : std::uint8_t {
    none,
    volume,
    weight
};

using advanced_inv_move_all_sort_key = std::pair<int, int>;

struct advanced_inv_move_all_candidate {
    item_location location;
    int count = 0;
    advanced_inv_move_all_sort_key sort_key { 0, 0 };
};

struct advanced_inv_move_all_selection {
    std::vector<advanced_inv_move_all_candidate> normal;
    std::vector<advanced_inv_move_all_candidate> favorites;
    item_location deferred_bucket;
    bool deferred_wielded = false;
};

/**
 * Classify one item according to current move-all row policy.
 *
 * This is read-only: prompts, bucket spilling, unwielding, sorting, and activity
 * construction remain controller responsibilities until later tranches.
 */
advanced_inv_move_all_disposition assess_advanced_inv_move_all_item(
    Character &player, const item_location &it,
    const advanced_inv_area &destination_area,
    const item_location &destination_container, bool forbid_buckets );

/** Current move-all bucket policy derived from a concrete destination. */
bool advanced_inv_move_all_forbids_buckets(
    const advanced_inv_area &destination_area, bool destination_in_vehicle );

/** Mirror move-all's volume/weight candidate key. */
advanced_inv_move_all_sort_key advanced_inv_move_all_key(
    const item &it, advanced_inv_move_all_priority priority );

/**
 * Mirror move-all's actor-aware ordering. Pickup processes from the back, so inventory
 * destinations intentionally reverse the key ordering used by other destinations.
 */
bool advanced_inv_move_all_key_before(
    const advanced_inv_move_all_sort_key &lhs,
    const advanced_inv_move_all_sort_key &rhs,
    bool destination_is_inventory );

/**
 * Build the non-interactive portion of move-all's candidate lists.
 *
 * The result deliberately keeps bucket/unwield handling deferred so the caller can preserve
 * the current prompts without source-policy and sort logic living in the controller.
 */
advanced_inv_move_all_selection select_advanced_inv_move_all_items(
    Character &player, const std::vector<advanced_inv_listitem> &rows,
    const advanced_inv_area &destination_area,
    const item_location &destination_container, bool forbid_buckets,
    advanced_inv_move_all_priority priority, bool destination_is_inventory );

#endif // CATA_SRC_ADVANCED_INV_MOVE_ALL_H
