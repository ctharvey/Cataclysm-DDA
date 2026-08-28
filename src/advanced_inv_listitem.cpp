#include "advanced_inv_listitem.h"

#include <optional>

#include "advanced_inv_area.h"
#include "auto_pickup.h"
#include "avatar.h"
#include "cata_assert.h"
#include "item.h"
#include "item_tname.h"
#include "map.h"
#include "vehicle_selector.h"

namespace
{
std::optional<advanced_inv_endpoint> endpoint_for_item( const item_location &loc,
        aim_location area )
{
    switch( area ) {
        case AIM_INVENTORY:
            return advanced_inv_endpoint::inventory();
        case AIM_WORN:
            if( loc == get_avatar().get_wielded_item() ) {
                return advanced_inv_endpoint::wielded();
            }
            return advanced_inv_endpoint::worn();
        case AIM_CONTAINER:
            if( loc.has_parent() ) {
                return advanced_inv_endpoint::item_container( loc.parent_item() );
            }
            return std::nullopt;
        default:
            break;
    }

    // Some AIM rows expose contents of an item that itself lives on the map or in
    // vehicle cargo (for example corpse contents).  The nested item_location does
    // not necessarily retain the vehicle cursor, so resolve the top-level owner
    // before determining the storage endpoint.
    item_location storage_loc = loc;
    while( storage_loc.has_parent() ) {
        storage_loc = storage_loc.parent_item();
    }

    switch( storage_loc.where() ) {
        case item_location::type::map:
            return advanced_inv_endpoint::ground( storage_loc.pos_bub( get_map() ) );
        case item_location::type::vehicle: {
            const vehicle_cursor *cursor = storage_loc.veh_cursor();
            if( cursor != nullptr ) {
                return advanced_inv_endpoint::vehicle_cargo( &cursor->veh,
                        static_cast<int>( cursor->part ) );
            }
            return std::nullopt;
        }
        case item_location::type::character:
        case item_location::type::container:
        case item_location::type::invalid:
            return std::nullopt;
    }
    return std::nullopt;
}

bool endpoint_is_vehicle_cargo( const std::optional<advanced_inv_endpoint> &endpoint )
{
    return endpoint.has_value() && endpoint->kind() == advanced_inv_endpoint_kind::vehicle_cargo;
}
} // namespace

advanced_inv_listitem::advanced_inv_listitem( const item_location &an_item, int index, int count,
        aim_location area, bool expected_from_vehicle )
    : idx( index )
    , area( area )
    , id( an_item->typeId() )
    , name( an_item->tname( count ) )
    , name_without_prefix( an_item->tname( 1, tname::tname_sort_key ) )
    , contents_count( an_item->aggregated_contents().count )
    , autopickup( get_auto_pickup().has_rule( & * an_item ) )
    , stacks( count )
    , volume( an_item->volume() * stacks )
    , weight( an_item->weight() * stacks )
    , cat( &an_item->get_category_of_contents() )
    , endpoint( endpoint_for_item( an_item, area ) )
{
    items.push_back( an_item );
    from_vehicle = endpoint_is_vehicle_cargo( endpoint );
    cata_assert( from_vehicle == expected_from_vehicle );
    cata_assert( stacks >= 1 );
}

advanced_inv_listitem::advanced_inv_listitem( const std::vector<item_location> &list, int index,
        aim_location area, bool expected_from_vehicle ) :
    idx( index ),
    area( area ),
    id( list.front()->typeId() ),
    items( list ),
    name( list.front()->tname( 1 ) ),
    name_without_prefix( list.front()->tname( 1, tname::tname_sort_key ) ),
    contents_count( list.front()->aggregated_contents().count ),
    autopickup( get_auto_pickup().has_rule( & * list.front() ) ),
    stacks( list.size() ),
    volume( list.front()->volume() * stacks ),
    weight( list.front()->weight() * stacks ),
    cat( &list.front()->get_category_of_contents() ),
    endpoint( endpoint_for_item( list.front(), area ) )
{
    from_vehicle = endpoint_is_vehicle_cargo( endpoint );
    cata_assert( from_vehicle == expected_from_vehicle );
    cata_assert( stacks >= 1 );
}
