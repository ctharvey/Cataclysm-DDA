#pragma once
#ifndef CATA_SRC_ADVANCED_INV_SOURCE_H
#define CATA_SRC_ADVANCED_INV_SOURCE_H

#include <functional>
#include <vector>

#include "advanced_inv_listitem.h"
#include "item_location.h"
#include "units.h"

class advanced_inv_area;
class avatar;
class item;

using advanced_inv_filter_predicate = std::function<bool( const item & )>;

/**
 * Endpoint rows and aggregate metrics produced by one AIM source enumeration.
 *
 * This is deliberately independent from pane state: a pane decides which endpoint
 * to enumerate and which rows to filter, while the source enumerator owns the
 * storage-specific item_location/stack construction.
 */
struct advanced_inv_source_snapshot {
    std::vector<advanced_inv_listitem> rows;
    units::volume volume = 0_ml;
    units::mass weight = 0_gram;
};

/** Enumerate the player's carried-inventory view. */
advanced_inv_source_snapshot enumerate_advanced_inv_inventory_source(
    avatar &you, const advanced_inv_filter_predicate &is_filtered );

/** Enumerate the worn/equipment view, including the wielded row. */
advanced_inv_source_snapshot enumerate_advanced_inv_worn_source(
    avatar &you, const advanced_inv_filter_predicate &is_filtered );

/** Enumerate the direct contents of the selected container. */
advanced_inv_source_snapshot enumerate_advanced_inv_container_source(
    const item_location &container, const advanced_inv_filter_predicate &is_filtered );

/**
 * Enumerate one ground or vehicle-cargo endpoint represented by an AIM area.
 *
 * @param square Area containing the map coordinate and optional vehicle cargo part.
 * @param in_vehicle Enumerate vehicle cargo when true, ground storage otherwise.
 * @param is_filtered Predicate returning true for items that should be hidden.
 */
advanced_inv_source_snapshot enumerate_advanced_inv_area_source(
    advanced_inv_area &square, bool in_vehicle,
    const advanced_inv_filter_predicate &is_filtered );

#endif // CATA_SRC_ADVANCED_INV_SOURCE_H
