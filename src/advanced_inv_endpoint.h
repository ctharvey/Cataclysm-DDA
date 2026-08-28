#pragma once
#ifndef CATA_SRC_ADVANCED_INV_ENDPOINT_H
#define CATA_SRC_ADVANCED_INV_ENDPOINT_H

#include <cstdint>

#include "coordinates.h"
#include "item_location.h"

class vehicle;

/**
 * The logical storage endpoint represented by an Advanced Inventory pane or list item.
 *
 * Endpoint identity is deliberately independent from screen location.  Two endpoints may
 * occupy the same map coordinate (for example ground and vehicle cargo) without being the
 * same endpoint, while two AIM areas may refer to the same endpoint (for example a dragged
 * vehicle cargo part and the directional square containing that cargo part).
 */
enum class advanced_inv_endpoint_kind : std::uint8_t {
    inventory,
    worn,
    wielded,
    ground,
    vehicle_cargo,
    container
};

class advanced_inv_endpoint
{
    public:
        static advanced_inv_endpoint inventory() {
            return advanced_inv_endpoint( advanced_inv_endpoint_kind::inventory );
        }

        static advanced_inv_endpoint worn() {
            return advanced_inv_endpoint( advanced_inv_endpoint_kind::worn );
        }

        static advanced_inv_endpoint wielded() {
            return advanced_inv_endpoint( advanced_inv_endpoint_kind::wielded );
        }

        static advanced_inv_endpoint ground( const tripoint_bub_ms &pos ) {
            advanced_inv_endpoint endpoint( advanced_inv_endpoint_kind::ground );
            endpoint.pos_ = pos;
            return endpoint;
        }

        static advanced_inv_endpoint vehicle_cargo( const vehicle *veh, int cargo_part ) {
            advanced_inv_endpoint endpoint( advanced_inv_endpoint_kind::vehicle_cargo );
            endpoint.vehicle_ = veh;
            endpoint.cargo_part_ = cargo_part;
            return endpoint;
        }

        static advanced_inv_endpoint item_container( const item_location &container ) {
            advanced_inv_endpoint endpoint( advanced_inv_endpoint_kind::container );
            endpoint.container_ = container;
            return endpoint;
        }

        advanced_inv_endpoint_kind kind() const {
            return kind_;
        }

        bool operator==( const advanced_inv_endpoint &other ) const {
            if( kind_ != other.kind_ ) {
                return false;
            }

            switch( kind_ ) {
                case advanced_inv_endpoint_kind::inventory:
                case advanced_inv_endpoint_kind::worn:
                case advanced_inv_endpoint_kind::wielded:
                    return true;
                case advanced_inv_endpoint_kind::ground:
                    return pos_ == other.pos_;
                case advanced_inv_endpoint_kind::vehicle_cargo:
                    return vehicle_ == other.vehicle_ && cargo_part_ == other.cargo_part_;
                case advanced_inv_endpoint_kind::container:
                    return container_ == other.container_;
            }
            return false;
        }

        bool operator!=( const advanced_inv_endpoint &other ) const {
            return !( *this == other );
        }

    private:
        explicit advanced_inv_endpoint( advanced_inv_endpoint_kind kind ) : kind_( kind ) {}

        advanced_inv_endpoint_kind kind_;
        tripoint_bub_ms pos_;
        const vehicle *vehicle_ = nullptr;
        int cargo_part_ = -1;
        item_location container_ = item_location::nowhere;
};

#endif // CATA_SRC_ADVANCED_INV_ENDPOINT_H
