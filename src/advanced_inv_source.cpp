#include "advanced_inv_source.h"

#include <cstddef>
#include <utility>
#include <vector>

#include "advanced_inv_area.h"
#include "item.h"
#include "item_location.h"
#include "map.h"
#include "map_selector.h"
#include "pocket_type.h"
#include "vehicle.h"
#include "vehicle_selector.h"

advanced_inv_source_snapshot enumerate_advanced_inv_area_source(
    advanced_inv_area &square, bool in_vehicle,
    const std::function<bool( const item & )> &is_filtered )
{
    advanced_inv_source_snapshot result;
    map &here = get_map();

    // Preserve AIM's existing rule: impassable fields make the source inaccessible.
    if( here.impassable_field_at( square.pos ) ) {
        return result;
    }

    const advanced_inv_area::itemstack &stacks = in_vehicle ?
            square.i_stacked( square.get_vehicle_stack() ) :
            square.i_stacked( here.i_at( square.pos ) );

    map_cursor map_loc( square.pos );
    for( size_t stack_index = 0; stack_index < stacks.size(); ++stack_index ) {
        std::vector<item_location> locations;
        locations.reserve( stacks[stack_index].size() );

        for( item *const it : stacks[stack_index] ) {
            if( in_vehicle ) {
                locations.emplace_back( vehicle_cursor( *square.veh, square.vstor ), it );
                continue;
            }

            locations.emplace_back( map_loc, it );

            // Ground corpses expose their contained loot as additional AIM rows.
            // Keep the existing ordering: contained rows precede the corpse row.
            if( it->is_corpse() ) {
                const item_location corpse_loc( map_loc, it );
                for( item *loot : it->all_items_top( pocket_type::CONTAINER ) ) {
                    if( is_filtered( *loot ) ) {
                        continue;
                    }
                    advanced_inv_listitem row( item_location( corpse_loc, loot ), 0, 1,
                                               square.id, false );
                    result.volume += row.volume;
                    result.weight += row.weight;
                    result.rows.push_back( std::move( row ) );
                }
            }
        }

        advanced_inv_listitem row( locations, stack_index, square.id, in_vehicle );
        if( is_filtered( *row.items.front() ) ) {
            continue;
        }

        result.volume += row.volume;
        result.weight += row.weight;
        result.rows.push_back( std::move( row ) );
    }

    return result;
}
