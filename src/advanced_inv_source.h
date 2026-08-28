#pragma once
#ifndef CATA_SRC_ADVANCED_INV_SOURCE_H
#define CATA_SRC_ADVANCED_INV_SOURCE_H

#include <functional>
#include <vector>

#include "advanced_inv_listitem.h"
#include "units.h"

class advanced_inv_area;
class item;

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

/**
 * Enumerate one ground or vehicle-cargo endpoint represented by an AIM area.
 *
 * @param square Area containing the map coordinate and optional vehicle cargo part.
 * @param in_vehicle Enumerate vehicle cargo when true, ground storage otherwise.
 * @param is_filtered Predicate returning true for items that should be hidden.
 */
advanced_inv_source_snapshot enumerate_advanced_inv_area_source(
    advanced_inv_area &square, bool in_vehicle,
    const std::function<bool( const item & )> &is_filtered );

#endif // CATA_SRC_ADVANCED_INV_SOURCE_H
