#include "advanced_inv_pane.h"

#include <cstddef>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "advanced_inv_area.h"
#include "advanced_inv_pagination.h"
#include "advanced_inv_source.h"
#include "advanced_inv_storage.h"
#include "avatar.h"
#include "cata_assert.h"
#include "character.h"
#include "enums.h"
#include "item.h"
#include "item_search.h"
#include "map.h"
#include "options.h"
#include "uistate.h"
#include "units.h"
#include "vehicle.h"

class item_category;

#if defined(__ANDROID__)
#   include <SDL_keyboard.h>
#endif

void advanced_inventory_pane::set_area( const advanced_inv_area &square, bool in_vehicle_cargo )
{
    prev_area = area;
    prev_viewing_cargo = viewing_cargo;
    area = square.id;
    viewing_cargo = square.can_store_in_vehicle() && ( in_vehicle_cargo || area == AIM_DRAGGED );
}

void advanced_inventory_pane::restore_area()
{
    area = prev_area;
    viewing_cargo = prev_viewing_cargo;
}

void advanced_inventory_pane::save_settings() const
{
    save_state->container = container;
    save_state->container_base_loc = container_base_loc;
    save_state->in_vehicle = in_vehicle();
    save_state->area_idx = get_area();
    save_state->selected_idx = index;
    save_state->filter = filter;
    save_state->sort_idx = sortby;
}

void advanced_inventory_pane::load_settings( int saved_area_idx,
        const std::array<advanced_inv_area, NUM_AIM_LOCATIONS> &squares, bool is_re_enter )
{
    const int i_location = ( get_option<bool>( "OPEN_DEFAULT_ADV_INV" ) &&
                             !is_re_enter ) ? saved_area_idx :
                           save_state->area_idx;
    const aim_location location = static_cast<aim_location>( i_location );
    const advanced_inv_area &square = squares[location];
    // determine the square's vehicle/map item presence
    bool has_veh_items = square.can_store_in_vehicle() && !square.get_vehicle_stack().empty();
    bool has_map_items = !get_map().i_at( square.pos ).empty();
    // determine based on map items and settings to show cargo
    bool show_vehicle = false;
    if( is_re_enter ) {
        show_vehicle = save_state->in_vehicle;
    } else if( has_veh_items == has_map_items ) {
        show_vehicle = save_state->in_vehicle && square.can_store_in_vehicle();
    } else {
        show_vehicle = has_veh_items;
    }
    set_area( square, show_vehicle );
    sortby = static_cast<advanced_inv_sortby>( save_state->sort_idx );
    index = save_state->selected_idx;
    set_filter( save_state->filter );
    if( area == AIM_CONTAINER ) {
        container = save_state->container;
    }
    container_base_loc = static_cast<aim_location>( save_state->container_base_loc );
}

bool advanced_inventory_pane::is_filtered( const advanced_inv_listitem &it ) const
{
    return is_filtered( *it.items.front() );
}

bool advanced_inventory_pane::is_filtered( const item &it ) const
{
    if( it.has_flag( json_flag_HIDDEN_ITEM ) ) {
        return true;
    }
    if( filter.empty() ) {
        return false;
    }
    return !filter_function( it );
}

void advanced_inventory_pane::add_items_from_area( advanced_inv_area &square,
        bool vehicle_override )
{
    cata_assert( square.id != AIM_ALL );
    if( !square.canputitems( container ) ) {
        return;
    }

    avatar &u = get_avatar();
    const advanced_inv_filter_predicate filtered = [this]( const item & it ) {
        return is_filtered( it );
    };

    // Existing items are *not* cleared on purpose.  This is called repeatedly when
    // AIM_ALL collects its individual ground/vehicle sources.
    advanced_inv_source_snapshot source;
    if( square.id == AIM_INVENTORY ) {
        source = enumerate_advanced_inv_inventory_source( u, filtered );
        square.volume = source.volume;
        square.weight = source.weight;
        items = std::move( source.rows );
        return;
    }

    if( square.id == AIM_WORN ) {
        source = enumerate_advanced_inv_worn_source( u, filtered );
        square.volume = source.volume;
        square.weight = source.weight;
    } else if( square.id == AIM_CONTAINER ) {
        source = enumerate_advanced_inv_container_source( container, filtered );
        square.volume = source.volume;
        square.weight = source.weight;
    } else {
        const bool is_in_vehicle = square.can_store_in_vehicle() && ( in_vehicle() || vehicle_override );
        source = enumerate_advanced_inv_area_source( square, is_in_vehicle, filtered );
        if( is_in_vehicle ) {
            square.volume_veh = source.volume;
            square.weight_veh = source.weight;
        } else {
            square.volume = source.volume;
            square.weight = source.weight;
        }
    }

    items.insert( items.end(),
                  std::make_move_iterator( source.rows.begin() ),
                  std::make_move_iterator( source.rows.end() ) );
}

void advanced_inventory_pane::fix_index()
{
    if( items.empty() ) {
        index = 0;
        return;
    }
    if( index < 0 ) {
        index = 0;
    } else if( static_cast<size_t>( index ) >= items.size() ) {
        index = static_cast<int>( items.size() ) - 1;
    }
}

void advanced_inventory_pane::mod_index( int offset )
{
    // 0 would make no sense
    cata_assert( offset != 0 );
    cata_assert( !items.empty() );
    index += offset;
    if( index < 0 ) {
        index = static_cast<int>( items.size() ) - 1;
    } else if( static_cast<size_t>( index ) >= items.size() ) {
        index = 0;
    }
}

void advanced_inventory_pane::scroll_by( int offset )
{
    // 0 would make no sense
    cata_assert( offset != 0 );
    if( items.empty() ) {
        return;
    }
    mod_index( offset );
}

void advanced_inventory_pane::scroll_page( int linesPerPage, int offset )
{
    // only those two offsets are allowed
    cata_assert( offset == -1 || offset == +1 );
    if( items.empty() ) {
        return;
    }
    const int size = static_cast<int>( items.size() );

    advanced_inventory_pagination old_pagination( linesPerPage, *this );
    for( int i = 0; i <= index; i++ ) {
        old_pagination.step( i );
    }

    // underflow
    if( old_pagination.page + offset < 0 ) {
        if( index > 0 ) {
            // scroll to top of first page
            index = 0;
        } else {
            // scroll wrap
            index = size - 1;
        }
        return;
    }

    int previous_line = -1; // matching line one up from our line
    advanced_inventory_pagination new_pagination( linesPerPage, *this );
    for( int i = 0; i < size; i++ ) {
        new_pagination.step( i );
        // right page
        if( new_pagination.page == old_pagination.page + offset ) {
            // right line
            if( new_pagination.line == old_pagination.line ) {
                index = i;
                return;
            }
            // one up from right line
            if( new_pagination.line == old_pagination.line - 1 ) {
                previous_line = i;
            }
        }
    }
    // second-best matching line
    if( previous_line != -1 ) {
        index = previous_line;
        return;
    }

    // overflow
    if( index < size - 1 ) {
        // scroll to end of last page
        index = size - 1;
    } else {
        // scroll wrap
        index = 0;
    }
}

void advanced_inventory_pane::scroll_category( int offset )
{
    // only those two offsets are allowed
    cata_assert( offset == -1 || offset == +1 );
    if( items.empty() ) {
        return;
    }
    // index must already be valid!
    cata_assert( get_cur_item_ptr() != nullptr );
    const item_category *cur_cat = items[index].cat;
    if( offset > 0 ) {
        while( items[index].cat == cur_cat ) {
            index++;
            // wrap to begin, stop there.
            if( static_cast<size_t>( index ) >= items.size() ) {
                index = 0;
                break;
            }
        }
    } else {
        while( items[index].cat == cur_cat ) {
            index--;
            // wrap to end, stop there.
            if( index < 0 ) {
                index = static_cast<int>( items.size() ) - 1;
                break;
            }
        }
    }
}

void advanced_inventory_pane::scroll_to_start()
{
    index = 0;
}

void advanced_inventory_pane::scroll_to_end()
{
    index = static_cast<int>( items.size() ) - 1;
}

advanced_inv_listitem *advanced_inventory_pane::get_cur_item_ptr()
{
    if( static_cast<size_t>( index ) >= items.size() ) {
        return nullptr;
    }
    return &items[index];
}

int advanced_inventory_pane::get_item_count( const advanced_inv_area &square ) const
{
    const std::optional<advanced_inv_storage_state> storage =
        inspect_advanced_inv_storage( square, in_vehicle(), container );
    return storage.has_value() ? storage->item_count : 0;
}

units::volume advanced_inventory_pane::free_volume( const advanced_inv_area &square ) const
{
    cata_assert( area != AIM_ALL );
    const std::optional<advanced_inv_storage_state> storage =
        inspect_advanced_inv_storage( square, in_vehicle(), container );
    return storage.has_value() ? storage->free_volume : 0_ml;
}

units::mass advanced_inventory_pane::free_weight_capacity( const advanced_inv_area &square ) const
{
    cata_assert( area != AIM_ALL );
    const std::optional<advanced_inv_storage_state> storage =
        inspect_advanced_inv_storage( square, in_vehicle(), container );
    return storage.has_value() ? storage->free_weight : 0_gram;
}

units::mass advanced_inventory_pane::free_weight_capacity() const
{
    // Legacy overload: this method lacks the area descriptor needed by the endpoint-aware
    // storage inspector. Keep it until all controller call sites pass the concrete area.
    cata_assert( area != AIM_ALL );
    if( area == AIM_CONTAINER ) {
        if( !container ) {
            return 0_gram;
        }
        return container->get_remaining_weight_capacity();
    } else if( area == AIM_INVENTORY || area == AIM_WORN ) {
        return get_player_character().free_weight_capacity();
    } else {
        return units::mass::max();
    }
}

void advanced_inventory_pane::set_filter( const std::string &new_filter )
{
    if( filter == new_filter ) {
        return;
    }
    filter = new_filter;
    filter_function = item_filter_from_string( filter );
    recalc = true;
}
