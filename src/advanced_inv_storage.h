#pragma once
#ifndef CATA_SRC_ADVANCED_INV_STORAGE_H
#define CATA_SRC_ADVANCED_INV_STORAGE_H

#include <optional>

#include "advanced_inv_endpoint.h"
#include "item_location.h"
#include "units.h"

class advanced_inv_area;

/**
 * Read-only state for one concrete AIM storage endpoint.
 *
 * This separates storage facts from pane presentation.  In particular, ground and
 * vehicle cargo at one coordinate have independent item counts and free volume.
 */
struct advanced_inv_storage_state {
    advanced_inv_endpoint endpoint;
    int item_count = 0;
    units::volume free_volume = 0_ml;
    units::mass free_weight = 0_gram;
};

/**
 * Inspect the concrete endpoint represented by an AIM area/storage mode.
 * Aggregate/navigation areas and invalid containers return std::nullopt.
 */
std::optional<advanced_inv_storage_state> inspect_advanced_inv_storage(
    const advanced_inv_area &area, bool in_vehicle,
    const item_location &container = item_location::nowhere );

#endif // CATA_SRC_ADVANCED_INV_STORAGE_H
