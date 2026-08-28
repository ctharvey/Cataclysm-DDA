#include "advanced_inv_source.h"

#include <cstddef>
#include <iterator>
#include <list>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

#include "advanced_inv_area.h"
#include "avatar.h"
#include "character_attire.h"
#include "flag.h"
#include "item.h"
#include "item_location.h"
#include "map.h"
#include "map_selector.h"
#include "pocket_type.h"
#include "type_id.h"
#include "vehicle.h"
#include "vehicle_selector.h"

namespace
{
using raw_item_stacks = std::vector<std::vector<item *>>;

template<typename T>
raw_item_stacks stack_area_items( T items )
{
    // Preserve the original advanced_inv_area::i_stacked algorithm exactly while
    // moving ownership of source shaping out of the area descriptor.
    raw_item_stacks stacks;
    std::unordered_map<itype_id, std::set<int>> cache;
    for( item &elem : items ) {
        const itype_id id = elem.typeId();
        auto iter = cache.find( id );
        bool got_stacked = false;
        if( iter != cache.end() ) {
            for( const int &idx : iter->second ) {
                for( item *&it : stacks[idx] ) {
                    if( ( got_stacked = it->display_stacked_with( elem ) ) ) {
                        stacks[idx].push_back( &elem );
                        break;
                    }
                }
                if( got_stacked ) {
                    break;
                }
            }
        }
        if( !got_stacked ) {
            cache[id].insert( stacks.size() );
            stacks.push_back( { &elem } );
        }
    }
    return stacks;
}

std::vector<std::vector<item_location>> stack_container_items(
    const item_location &parent, std::list<item *> item_list )
{
    std::vector<std::vector<item_location>> result;
    for( auto outer = item_list.begin(); outer != item_list.end(); ++outer ) {
        std::vector<item_location> stack( { item_location( parent, *outer ) } );
        for( auto inner = std::next( outer ); inner != item_list.end(); ) {
            if( ( *outer )->display_stacked_with( **inner ) ) {
                stack.emplace_back( parent, *inner );
                inner = item_list.erase( inner );
            } else {
                ++inner;
            }
        }
        result.push_back( std::move( stack ) );
    }
    return result;
}

void append_visible_row( advanced_inv_source_snapshot &snapshot,
                         advanced_inv_listitem row,
                         const advanced_inv_filter_predicate &is_filtered )
{
    if( is_filtered( *row.items.front() ) ) {
        return;
    }
    snapshot.volume += row.volume;
    snapshot.weight += row.weight;
    snapshot.rows.push_back( std::move( row ) );
}
} // namespace

advanced_inv_source_snapshot enumerate_advanced_inv_inventory_source(
    avatar &you, const advanced_inv_filter_predicate &is_filtered )
{
    advanced_inv_source_snapshot result;
    size_t item_index = 0;

    for( item_location worn_loc : you.worn.top_items_loc( you ) ) {
        item &worn_item = *worn_loc;
        if( worn_item.empty() || worn_item.has_flag( flag_NO_UNLOAD ) ) {
            continue;
        }

        for( const std::vector<item_location> &stack : stack_container_items(
                 worn_loc, worn_item.all_items_top( pocket_type::CONTAINER ) ) ) {
            // Preserve AIM's existing rule: loose liquid contents are not inventory rows.
            if( !stack.empty() && stack.front()->made_of_from_type( phase_id::LIQUID ) &&
                !stack.front()->is_frozen_liquid() ) {
                continue;
            }

            append_visible_row( result,
                                advanced_inv_listitem( stack, item_index++, AIM_INVENTORY, false ),
                                is_filtered );
        }
    }

    item_location weapon = you.get_wielded_item();
    if( weapon && weapon->is_container() ) {
        for( const std::vector<item_location> &stack : stack_container_items(
                 weapon, weapon->all_items_top( pocket_type::CONTAINER ) ) ) {
            if( !stack.empty() && stack.front()->made_of_from_type( phase_id::LIQUID ) &&
                !stack.front()->is_frozen_liquid() ) {
                continue;
            }

            append_visible_row( result,
                                advanced_inv_listitem( stack, item_index++, AIM_INVENTORY, false ),
                                is_filtered );
        }
    }

    return result;
}

advanced_inv_source_snapshot enumerate_advanced_inv_worn_source(
    avatar &you, const advanced_inv_filter_predicate &is_filtered )
{
    advanced_inv_source_snapshot result;

    item_location weapon = you.get_wielded_item();
    if( weapon ) {
        append_visible_row( result,
                            advanced_inv_listitem( weapon, 0, 1, AIM_WORN, false ),
                            is_filtered );
    }

    size_t index = 1;
    for( item_location worn_loc : you.worn.top_items_loc( you ) ) {
        append_visible_row( result,
                            advanced_inv_listitem( worn_loc, index++, 1, AIM_WORN, false ),
                            is_filtered );
    }

    return result;
}

advanced_inv_source_snapshot enumerate_advanced_inv_container_source(
    const item_location &container, const advanced_inv_filter_predicate &is_filtered )
{
    advanced_inv_source_snapshot result;
    if( !container || container->is_container_empty() ) {
        return result;
    }

    size_t item_index = 0;
    for( const std::vector<item_location> &stack : stack_container_items(
             container, container->all_items_top() ) ) {
        append_visible_row( result,
                            advanced_inv_listitem( stack, item_index++, AIM_CONTAINER, false ),
                            is_filtered );
    }
    return result;
}

advanced_inv_source_snapshot enumerate_advanced_inv_area_source(
    advanced_inv_area &square, bool in_vehicle,
    const advanced_inv_filter_predicate &is_filtered )
{
    advanced_inv_source_snapshot result;
    map &here = get_map();

    // Preserve AIM's existing rule: impassable fields make the source inaccessible.
    if( here.impassable_field_at( square.pos ) ) {
        return result;
    }

    const raw_item_stacks stacks = in_vehicle ?
            stack_area_items( square.get_vehicle_stack() ) :
            stack_area_items( here.i_at( square.pos ) );

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

        append_visible_row( result,
                            advanced_inv_listitem( locations, stack_index, square.id, in_vehicle ),
                            is_filtered );
    }

    return result;
}
