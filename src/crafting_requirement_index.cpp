#include "crafting_requirement_index.h"

#include <algorithm>

#include "flag.h"
#include "inventory.h"
#include "item.h"
#include "itype.h"

crafting_inventory_snapshot::crafting_inventory_snapshot(
    const crafting_requirement_index &index, const inventory &inv )
{
    // The index allowlist: only these facts are ever counted. Group item
    // and tool facts by itype id; quality facts are grouped separately by
    // quality id and matched per level.
    const std::vector<crafting_requirement_fact_key> keys = index.fact_keys();
    fact_total_ = keys.size();

    std::map<std::string, std::vector<crafting_requirement_fact_key>> item_facts;
    std::map<std::string, std::vector<crafting_requirement_fact_key>> quality_facts;
    for( const crafting_requirement_fact_key &key : keys ) {
        switch( key.kind ) {
            case crafting_requirement_fact_kind::component_units:
            case crafting_requirement_fact_kind::component_charges:
            case crafting_requirement_fact_kind::tool_instances:
            case crafting_requirement_fact_kind::tool_charges:
                item_facts[key.id].push_back( key );
                break;
            case crafting_requirement_fact_kind::quality_providers:
                quality_facts[key.id].push_back( key );
                break;
        }
    }
    // Quality facts cannot be reproduced exactly: item::get_quality()
    // mirrors charged/contained quality resolution only in limited cases
    // and the menu path can couple on player state. Retain a capped count.
    for( const auto &entry : quality_facts ) {
        for( const crafting_requirement_fact_key &key : entry.second ) {
            inexact_keys_.insert( key );
        }
    }

    // Eligibility under one filter profile bitmask. Broken items are
    // rejected for all item/tool facts, matching legacy amount_of /
    // charges_of. Per-item property checks run once and produce a compact
    // rejection bitmask; a fact is eligible iff its profile avoids every
    // rejected bit. Non-magazines are never rejected by the full-magazine
    // rule.
    auto rejection_mask_for = [&]( const item & it ) {
        int mask = 0;
        if( it.rotten() ) {
            mask |= recipe_filter_rotten_forbidden;
        }
        if( it.is_favorite ) {
            mask |= recipe_filter_favorite_forbidden;
        }
        if( it.has_flag( flag_FROZEN ) && !it.has_flag( flag_EDIBLE_FROZEN ) ) {
            mask |= recipe_filter_frozen_forbidden;
        }
        if( it.is_magazine() ) {
            if( !it.has_ammo_data() || !it.ammo_data()->ammo ) {
                mask |= recipe_filter_full_magazine_required;
            } else if( it.ammo_remaining() <= 0 ||
                       it.ammo_remaining() < it.ammo_capacity( it.ammo_data()->ammo->type ) ) {
                mask |= recipe_filter_full_magazine_required;
            }
        }
        return mask;
    };

    inv.visit_items( [&]( item * e, item * ) {
        const item &it = *e;
        if( it.is_broken() ) {
            // Broken items match nothing item- or tool-related.
            return VisitResponse::NEXT;
        }
        const itype_id id = it.typeId();
        const bool pseudo = it.has_flag( flag_PSEUDO );
        const bool by_charges = it.count_by_charges();
        const bool component_eligible = is_crafting_component( it );

        // Overflow-safe capped addition; once a fact reaches its index
        // maximum it is recorded as saturated and stops updating.
        auto capped_add = [&]( const crafting_requirement_fact_key & key,
        int amount, int maximum ) {
            int &count = counts_[key];
            if( amount > 0 && count < maximum ) {
                count += std::min( amount, maximum - count );
            }
            if( count >= maximum ) {
                saturated_keys_.insert( key );
            }
        };

        const int rejection_mask = rejection_mask_for( it );

        auto itf = item_facts.find( id.str() );
        if( itf != item_facts.end() ) {
            for( const crafting_requirement_fact_key &key : itf->second ) {
                if( saturated_keys_.count( key ) ) {
                    continue;
                }
                if( key.filter_profile & rejection_mask ) {
                    continue;
                }
                const int maximum = index.maximum_for( key );
                switch( key.kind ) {
                    case crafting_requirement_fact_kind::component_units:
                        // Components exclude pseudo items (legacy
                        // amount_of(..., pseudo = false)).
                        if( !pseudo && component_eligible ) {
                            capped_add( key, 1, maximum );
                        }
                        break;
                    case crafting_requirement_fact_kind::tool_instances:
                        // Tool instance facts include pseudo items, as
                        // legacy amount_of(..., pseudo = true) does.
                        capped_add( key, 1, maximum );
                        break;
                    case crafting_requirement_fact_kind::component_charges:
                        // Pseudo excluded like component units; only
                        // count-by-charges items contribute their charges.
                        if( !pseudo && component_eligible && by_charges ) {
                            capped_add( key, it.charges, maximum );
                        }
                        break;
                    case crafting_requirement_fact_kind::tool_charges: {
                        // Local exact charges only: no linked, UPS, or
                        // bionic pools are consulted. Tools whose legacy
                        // count drains external pools (UPS, bionic power,
                        // multimag firing requirements) are marked so
                        // Phase 3 can return unknown for them.
                        if( by_charges ) {
                            capped_add( key, it.charges, maximum );
                        } else {
                            capped_add( key, it.ammo_remaining(), maximum );
                            if( it.has_flag( flag_USE_UPS ) ||
                                it.has_flag( flag_USES_BIONIC_POWER ) ||
                                it.uses_firing_requirements() ) {
                                inexact_keys_.insert( key );
                                unsupported_keys_.insert( key );
                            }
                        }
                        break;
                    }
                    default:
                        break;
                }
            }
        }

        if( !quality_facts.empty() ) {
            for( const auto &entry : quality_facts ) {
                const quality_id qual( entry.first );
                const int supplied = it.get_quality( qual );
                if( supplied <= 0 ) {
                    continue;
                }
                for( const crafting_requirement_fact_key &key : entry.second ) {
                    if( saturated_keys_.count( key ) ) {
                        continue;
                    }
                    if( key.level <= supplied ) {
                        capped_add( key, it.count(), index.maximum_for( key ) );
                    }
                }
            }
        }
        return VisitResponse::NEXT;
    } );
}

int crafting_inventory_snapshot::count_for(
    const crafting_requirement_fact_key &key ) const
{
    auto it = counts_.find( key );
    return it == counts_.end() ? 0 : it->second;
}

bool crafting_inventory_snapshot::meets(
    const crafting_requirement_fact_key &key, int threshold ) const
{
    if( threshold <= 0 ) {
        return false;
    }
    return count_for( key ) >= threshold;
}

std::size_t crafting_inventory_snapshot::fact_count() const
{
    return fact_total_;
}

std::size_t crafting_inventory_snapshot::saturated_fact_count() const
{
    return saturated_keys_.size();
}

bool crafting_inventory_snapshot::is_exact(
    const crafting_requirement_fact_key &key ) const
{
    return inexact_keys_.count( key ) == 0;
}

std::size_t crafting_inventory_snapshot::inexact_fact_count() const
{
    return inexact_keys_.size();
}

std::size_t crafting_inventory_snapshot::unsupported_fact_count() const
{
    return unsupported_keys_.size();
}

std::size_t crafting_inventory_snapshot::approximate_memory_bytes() const
{
    return sizeof( *this ) +
           counts_.size() * ( sizeof( crafting_requirement_fact_key ) + sizeof( int ) +
                              sizeof( void * ) * 2 ) +
           inexact_keys_.size() * ( sizeof( crafting_requirement_fact_key ) +
                                    sizeof( void * ) * 2 ) +
           unsupported_keys_.size() * ( sizeof( crafting_requirement_fact_key ) +
                                        sizeof( void * ) * 2 ) +
           saturated_keys_.size() * ( sizeof( crafting_requirement_fact_key ) +
                                      sizeof( void * ) * 2 );
}

bool crafting_requirement_fact_key::operator==(
    const crafting_requirement_fact_key &rhs ) const
{
    return kind == rhs.kind &&
           id == rhs.id &&
           level == rhs.level &&
           filter_profile == rhs.filter_profile;
}

bool crafting_requirement_fact_key::operator!=(
    const crafting_requirement_fact_key &rhs ) const
{
    return !( *this == rhs );
}

// Strict lexicographic ordering suitable for std::map.
bool crafting_requirement_fact_key::operator<(
    const crafting_requirement_fact_key &rhs ) const
{
    if( kind != rhs.kind ) {
        return kind < rhs.kind;
    }
    if( id != rhs.id ) {
        return id < rhs.id;
    }
    if( level != rhs.level ) {
        return level < rhs.level;
    }
    return filter_profile < rhs.filter_profile;
}

bool crafting_requirement_edge::operator==(
    const crafting_requirement_edge &rhs ) const
{
    return id == rhs.id &&
           menu == rhs.menu &&
           alternative == rhs.alternative &&
           group_kind == rhs.group_kind &&
           group == rhs.group &&
           option == rhs.option &&
           threshold == rhs.threshold;
}

bool crafting_requirement_edge::operator!=(
    const crafting_requirement_edge &rhs ) const
{
    return !( *this == rhs );
}

bool crafting_requirement_edge::operator<(
    const crafting_requirement_edge &rhs ) const
{
    if( id != rhs.id ) {
        return id < rhs.id;
    }
    if( menu != rhs.menu ) {
        return menu < rhs.menu;
    }
    if( alternative != rhs.alternative ) {
        return alternative < rhs.alternative;
    }
    if( group_kind != rhs.group_kind ) {
        return group_kind < rhs.group_kind;
    }
    if( group != rhs.group ) {
        return group < rhs.group;
    }
    if( option != rhs.option ) {
        return option < rhs.option;
    }
    return threshold < rhs.threshold;
}

namespace
{
// The fact kind(s) an option may carry within a group of the given kind.
bool kind_matches_group( requirement_group_kind group,
                         crafting_requirement_fact_kind kind )
{
    switch( group ) {
        case requirement_group_kind::component:
            return kind == crafting_requirement_fact_kind::component_units ||
                   kind == crafting_requirement_fact_kind::component_charges;
        case requirement_group_kind::tool:
            return kind == crafting_requirement_fact_kind::tool_instances ||
                   kind == crafting_requirement_fact_kind::tool_charges;
        case requirement_group_kind::quality:
            return kind == crafting_requirement_fact_kind::quality_providers;
    }
    return false;
}

bool uses_component_filter( crafting_requirement_fact_kind kind )
{
    return kind == crafting_requirement_fact_kind::component_units ||
           kind == crafting_requirement_fact_kind::component_charges;
}

// Components get one key per menu mode; tools and qualities use profile 0.
int menu_modes_for_kind( crafting_requirement_fact_kind kind )
{
    return uses_component_filter( kind ) ? menu_filter_mode_count : 1;
}

menu_filter_mode nth_menu_mode( int n )
{
    return static_cast<menu_filter_mode>( n );
}

crafting_requirement_fact_key derive_key( const crafting_requirement_option &opt,
        int filter_profile )
{
    crafting_requirement_fact_key key;
    key.kind = opt.kind;
    key.id = opt.id;
    key.level = opt.level;
    key.filter_profile = filter_profile;
    return key;
}
} // namespace

bool crafting_requirement_index::add_recipe(
    const recipe_id &recipe_id, const crafting_requirement_plan &plan,
    const std::array<int, menu_filter_mode_count> &effective_profiles,
    std::string *error )
{
    auto fail = [&]( const std::string & msg ) {
        if( error ) {
            *error = msg;
        }
        return false;
    };
    if( finalized_ ) {
        return fail( "index is finalized" );
    }
    if( recipes_.count( recipe_id ) ) {
        return fail( "duplicate recipe id" );
    }
    if( plan.alternatives.empty() ) {
        return fail( "plan has no alternatives" );
    }

    // Validate the entire plan and stage the mutation before committing, so
    // a malformed plan cannot partially modify the index.
    struct staged_edge {
        crafting_requirement_fact_key key;
        crafting_requirement_edge edge;
    };
    std::map<crafting_requirement_fact_key, std::vector<int>> staged_thresholds;
    std::vector<staged_edge> staged_edges;

    for( std::size_t a = 0; a < plan.alternatives.size(); ++a ) {
        const crafting_requirement_alternative &alt = plan.alternatives[a];
        // Zero groups means the inventory portion is trivially satisfied;
        // only empty option groups are invalid.
        for( std::size_t g = 0; g < alt.size(); ++g ) {
            const crafting_requirement_group &grp = alt[g];
            if( grp.options.empty() ) {
                return fail( "group with no options" );
            }
            for( std::size_t o = 0; o < grp.options.size(); ++o ) {
                const crafting_requirement_option &opt = grp.options[o];
                if( opt.threshold <= 0 ) {
                    return fail( "nonpositive threshold" );
                }
                if( !kind_matches_group( grp.kind, opt.kind ) ) {
                    return fail( "option kind does not match group kind" );
                }
                const int menu_count = menu_modes_for_kind( opt.kind );
                for( int m = 0; m < menu_count; ++m ) {
                    // Only components use recipe filters in the legacy
                    // solver. Tool and quality facts are always unfiltered.
                    const int profile = uses_component_filter( opt.kind )
                                        ? effective_profiles[m]
                                        : recipe_filter_none;
                    const crafting_requirement_fact_key key = derive_key( opt, profile );
                    staged_thresholds[key].push_back( opt.threshold );
                    crafting_requirement_edge edge;
                    edge.id = recipe_id;
                    edge.menu = nth_menu_mode( m );
                    edge.alternative = static_cast<int>( a );
                    edge.group_kind = grp.kind;
                    edge.group = static_cast<int>( g );
                    edge.option = static_cast<int>( o );
                    edge.threshold = opt.threshold;
                    staged_edges.push_back( { key, edge } );
                }
            }
        }
    }

    // Commit.
    crafting_recipe_record record;
    record.id = recipe_id;
    record.support.state = recipe_support_state::supported;
    record.plan = plan;
    recipes_.emplace( recipe_id, record );
    recipe_profiles_[recipe_id] = effective_profiles;
    for( const auto &entry : staged_thresholds ) {
        std::vector<int> &list = thresholds_[entry.first];
        list.insert( list.end(), entry.second.begin(), entry.second.end() );
    }
    for( const staged_edge &se : staged_edges ) {
        edges_[se.key].push_back( se.edge );
    }
    if( error ) {
        error->clear();
    }
    return true;
}

bool crafting_requirement_index::add_unsupported_recipe(
    const recipe_id &recipe_id, const std::string &reason, std::string *error )
{
    if( finalized_ ) {
        if( error ) {
            *error = "index is finalized";
        }
        return false;
    }
    if( recipes_.count( recipe_id ) ) {
        if( error ) {
            *error = "duplicate recipe id";
        }
        return false;
    }
    crafting_recipe_record record;
    record.id = recipe_id;
    record.support.state = recipe_support_state::unsupported;
    record.support.reason = reason;
    recipes_.emplace( recipe_id, record );
    if( error ) {
        error->clear();
    }
    return true;
}

void crafting_requirement_index::finalize()
{
    if( finalized_ ) {
        return;
    }
    for( auto &entry : thresholds_ ) {
        std::vector<int> &list = entry.second;
        std::sort( list.begin(), list.end() );
        list.erase( std::unique( list.begin(), list.end() ), list.end() );
        maxima_[entry.first] = list.empty() ? 0 : list.back();
    }
    for( auto &entry : edges_ ) {
        std::vector<crafting_requirement_edge> &list = entry.second;
        std::sort( list.begin(), list.end() );
        list.erase( std::unique( list.begin(), list.end() ), list.end() );
    }
    finalized_ = true;
}

bool crafting_requirement_index::is_finalized() const
{
    return finalized_;
}

void crafting_requirement_index::clear()
{
    recipes_.clear();
    recipe_profiles_.clear();
    thresholds_.clear();
    maxima_.clear();
    edges_.clear();
    finalized_ = false;
}

std::size_t crafting_requirement_index::recipe_count() const
{
    return recipes_.size();
}

std::vector<recipe_id> crafting_requirement_index::recipe_ids() const
{
    std::vector<recipe_id> ids;
    ids.reserve( recipes_.size() );
    for( const auto &entry : recipes_ ) {
        ids.push_back( entry.first );
    }
    return ids;
}

bool crafting_requirement_index::has_recipe( const recipe_id &recipe_id ) const
{
    return recipes_.count( recipe_id ) != 0;
}

std::size_t crafting_requirement_index::fact_count() const
{
    return thresholds_.size();
}

std::vector<crafting_requirement_fact_key> crafting_requirement_index::fact_keys() const
{
    std::vector<crafting_requirement_fact_key> keys;
    keys.reserve( thresholds_.size() );
    for( const auto &entry : thresholds_ ) {
        keys.push_back( entry.first );
    }
    return keys;
}

const crafting_recipe_support *crafting_requirement_index::support_for(
    const recipe_id &recipe_id ) const
{
    auto it = recipes_.find( recipe_id );
    return it == recipes_.end() ? nullptr : &it->second.support;
}

const crafting_requirement_plan *crafting_requirement_index::plan_for(
    const recipe_id &recipe_id ) const
{
    auto it = recipes_.find( recipe_id );
    if( it == recipes_.end() ) {
        return nullptr;
    }
    const crafting_recipe_record &record = it->second;
    if( record.support.state != recipe_support_state::supported ) {
        return nullptr;
    }
    return &record.plan;
}

int crafting_requirement_index::effective_profile_for(
    const recipe_id &recipe_id, menu_filter_mode menu ) const
{
    auto it = recipe_profiles_.find( recipe_id );
    return it == recipe_profiles_.end()
           ? recipe_filter_none
           : it->second[static_cast<int>( menu )];
}

int crafting_requirement_index::maximum_for(
    const crafting_requirement_fact_key &key ) const
{
    auto it = maxima_.find( key );
    return it == maxima_.end() ? 0 : it->second;
}

const std::vector<int> &crafting_requirement_index::thresholds_for(
    const crafting_requirement_fact_key &key ) const
{
    static const std::vector<int> empty;
    auto it = thresholds_.find( key );
    return it == thresholds_.end() ? empty : it->second;
}

const std::vector<crafting_requirement_edge> &
crafting_requirement_index::edges_for(
    const crafting_requirement_fact_key &key ) const
{
    static const std::vector<crafting_requirement_edge> empty;
    auto it = edges_.find( key );
    return it == edges_.end() ? empty : it->second;
}

crafting_requirement_tally::crafting_requirement_tally(
    const crafting_requirement_index &index ) : index_( index )
{
}

std::vector<int> crafting_requirement_tally::add(
    const crafting_requirement_fact_key &key, int amount )
{
    std::vector<int> newly_crossed;
    if( amount <= 0 ) {
        return newly_crossed;
    }
    const std::vector<int> &thresholds = index_.thresholds_for( key );
    if( thresholds.empty() ) {
        return newly_crossed;
    }
    // Cap at the index maximum; count + amount cannot overflow because both
    // are bounded by the maximum (a registered positive threshold).
    const int maximum = index_.maximum_for( key );
    int &count = counts_[key];
    const int previous = count;
    const int target = maximum - previous;
    if( target <= 0 ) {
        return newly_crossed;
    }
    count = previous + std::min( amount, target );

    int &crossed = crossed_[key];
    while( crossed < static_cast<int>( thresholds.size() ) &&
           thresholds[crossed] <= count ) {
        newly_crossed.push_back( thresholds[crossed] );
        ++crossed;
    }
    return newly_crossed;
}

int crafting_requirement_tally::count_for(
    const crafting_requirement_fact_key &key ) const
{
    auto it = counts_.find( key );
    return it == counts_.end() ? 0 : it->second;
}

bool crafting_requirement_tally::meets(
    const crafting_requirement_fact_key &key, int threshold ) const
{
    if( threshold <= 0 ) {
        return false;
    }
    return count_for( key ) >= threshold;
}

// ---------------------------------------------------------------------------
// Phase 3A: conservative tri-state shadow evaluation.
// ---------------------------------------------------------------------------

namespace
{

// Conservative tri-state OR: satisfied wins; otherwise unknown beats
// unsatisfied.
crafting_requirement_result or_reduce( crafting_requirement_result a,
                                       crafting_requirement_result b )
{
    if( a == crafting_requirement_result::satisfied ||
        b == crafting_requirement_result::satisfied ) {
        return crafting_requirement_result::satisfied;
    }
    if( a == crafting_requirement_result::unknown ||
        b == crafting_requirement_result::unknown ) {
        return crafting_requirement_result::unknown;
    }
    return crafting_requirement_result::unsatisfied;
}

// Conservative tri-state AND: unsatisfied wins; otherwise unknown beats
// satisfied.
crafting_requirement_result and_reduce( crafting_requirement_result a,
                                        crafting_requirement_result b )
{
    if( a == crafting_requirement_result::unsatisfied ||
        b == crafting_requirement_result::unsatisfied ) {
        return crafting_requirement_result::unsatisfied;
    }
    if( a == crafting_requirement_result::unknown ||
        b == crafting_requirement_result::unknown ) {
        return crafting_requirement_result::unknown;
    }
    return crafting_requirement_result::satisfied;
}

// Conservative allocation guard: a component item id that appears in more
// than one distinct group, or in any tool group, could be double-counted
// by a naive check, so this alternative is unknown. Tools are reusable, so
// an item id in multiple tool groups is not a problem; only component ids
// are consumed.
crafting_requirement_result allocation_guard(
    const crafting_requirement_alternative &alt )
{
    std::set<std::string> component_ids;
    std::set<std::string> tool_ids;
    for( const crafting_requirement_group &grp : alt ) {
        std::set<std::string> group_ids;
        for( const crafting_requirement_option &opt : grp.options ) {
            if( !group_ids.insert( opt.id ).second ) {
                continue;
            }
            if( grp.kind == requirement_group_kind::component ) {
                if( tool_ids.count( opt.id ) ) {
                    return crafting_requirement_result::unknown;
                }
                if( component_ids.count( opt.id ) ) {
                    // Same component id in more than one distinct
                    // component group: allocation is ambiguous.
                    return crafting_requirement_result::unknown;
                }
                component_ids.insert( opt.id );
            } else if( grp.kind == requirement_group_kind::tool ) {
                if( component_ids.count( opt.id ) ) {
                    // Component id also appears in a tool group.
                    return crafting_requirement_result::unknown;
                }
                tool_ids.insert( opt.id );
            }
        }
    }
    return crafting_requirement_result::satisfied;
}

} // namespace

crafting_requirement_evaluator::crafting_requirement_evaluator(
    const crafting_requirement_index &index,
    const crafting_inventory_snapshot &snapshot ) : index_( index ),
    snapshot_( snapshot )
{
}

crafting_requirement_result crafting_requirement_evaluator::evaluate(
    const recipe_id &recipe_id, menu_filter_mode menu ) const
{
    const int menu_index = static_cast<int>( menu );
    if( menu_index < 0 || menu_index >= menu_filter_mode_count ) {
        return crafting_requirement_result::unknown;
    }
    if( !index_.is_finalized() || !index_.has_recipe( recipe_id ) ) {
        return crafting_requirement_result::unknown;
    }
    const crafting_recipe_support *support = index_.support_for( recipe_id );
    if( support == nullptr ||
        support->state != recipe_support_state::supported ) {
        return crafting_requirement_result::unknown;
    }
    const crafting_requirement_plan *plan = index_.plan_for( recipe_id );
    if( plan == nullptr || plan->alternatives.empty() ) {
        return crafting_requirement_result::unknown;
    }

    const int effective_profile = index_.effective_profile_for( recipe_id, menu );
    crafting_requirement_result result = crafting_requirement_result::unsatisfied;
    for( const crafting_requirement_alternative &alt : plan->alternatives ) {
        // Empty alternative (zero groups) is trivially satisfied.
        crafting_requirement_result alt_result =
            alt.empty()
            ? crafting_requirement_result::satisfied
            : allocation_guard( alt );
        if( alt_result != crafting_requirement_result::unknown ) {
            for( const crafting_requirement_group &grp : alt ) {
                // OR of options within the group; empty groups are
                // rejected by the index, but stay conservative anyway.
                crafting_requirement_result group_result =
                    grp.options.empty()
                    ? crafting_requirement_result::unknown
                    : crafting_requirement_result::unsatisfied;
                for( const crafting_requirement_option &opt : grp.options ) {
                    crafting_requirement_fact_key key = derive_key( opt,
                            uses_component_filter( opt.kind )
                            ? effective_profile
                            : recipe_filter_none );
                    if( key.id == "any" || !snapshot_.is_exact( key ) ) {
                        group_result = or_reduce( group_result,
                                                  crafting_requirement_result::unknown );
                    } else {
                        group_result = or_reduce( group_result,
                                                  snapshot_.meets( key, opt.threshold )
                                                  ? crafting_requirement_result::satisfied
                                                  : crafting_requirement_result::unsatisfied );
                    }
                    if( group_result == crafting_requirement_result::satisfied ) {
                        break;
                    }
                }
                alt_result = and_reduce( alt_result, group_result );
                if( alt_result == crafting_requirement_result::unsatisfied ) {
                    break;
                }
            }
        }
        result = or_reduce( result, alt_result );
        if( result == crafting_requirement_result::satisfied ) {
            break;
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// Phase 3C: production-only bulk result cache.
// ---------------------------------------------------------------------------

crafting_requirement_result_cache::crafting_requirement_result_cache(
    const crafting_requirement_index &index,
    const crafting_inventory_snapshot &snapshot )
{
    // An unfinalized index carries unsorted/undeduplicated thresholds and
    // edges; refuse to guess and leave the cache empty.
    if( !index.is_finalized() ) {
        return;
    }

    // Plan-shaped group accumulators: one tri-state per group per menu mode.
    // Recipes without a usable plan (unsupported, missing, or malformed)
    // never enter the accumulator map and cache unknown for all modes.
    struct plan_state {
        std::vector<std::vector<crafting_requirement_result>> groups;
    };
    std::map<recipe_id, std::array<plan_state, menu_filter_mode_count>> states;
    const std::vector<recipe_id> ids = index.recipe_ids();
    for( const recipe_id &id : ids ) {
        const crafting_requirement_plan *plan = index.plan_for( id );
        if( plan == nullptr || plan->alternatives.empty() ) {
            continue;
        }
        bool malformed = false;
        for( const crafting_requirement_alternative &alt : plan->alternatives ) {
            for( const crafting_requirement_group &grp : alt ) {
                if( grp.options.empty() ) {
                    malformed = true;
                }
            }
        }
        if( malformed ) {
            continue;
        }
        std::array<plan_state, menu_filter_mode_count> &st = states[id];
        for( plan_state &ps : st ) {
            ps.groups.resize( plan->alternatives.size() );
            for( std::size_t a = 0; a < plan->alternatives.size(); ++a ) {
                ps.groups[a].assign( plan->alternatives[a].size(),
                                     crafting_requirement_result::unsatisfied );
            }
        }
    }

    // One pass over every fact key: compute the highest registered threshold
    // crossed by the snapshot count once, then visit every dependent edge.
    for( const crafting_requirement_fact_key &key : index.fact_keys() ) {
        const int count = snapshot.count_for( key );
        const bool exact = snapshot.is_exact( key );
        const bool wildcard = key.id == "any";
        int highest_crossed = 0;
        for( int threshold : index.thresholds_for( key ) ) {
            if( threshold <= count && threshold > highest_crossed ) {
                highest_crossed = threshold;
            }
        }
        const bool component_fact = uses_component_filter( key.kind );
        for( const crafting_requirement_edge &edge : index.edges_for( key ) ) {
            auto it = states.find( edge.id );
            if( it == states.end() ) {
                continue;
            }
            const crafting_requirement_result option_state =
                ( wildcard || !exact )
                ? crafting_requirement_result::unknown
                : ( edge.threshold <= highest_crossed
                    ? crafting_requirement_result::satisfied
                    : crafting_requirement_result::unsatisfied );
            // Component facts are filter-profile specific: only the edge's
            // own menu mode is touched. Tool/quality facts are unfiltered
            // and broadcast to all menu modes.
            const int first_mode = component_fact
                                   ? static_cast<int>( edge.menu )
                                   : 0;
            const int mode_limit = component_fact
                                   ? first_mode + 1
                                   : menu_filter_mode_count;
            for( int m = first_mode; m < mode_limit; ++m ) {
                std::vector<std::vector<crafting_requirement_result>> &groups =
                    it->second[static_cast<std::size_t>( m )].groups;
                if( static_cast<std::size_t>( edge.alternative ) >= groups.size() ) {
                    continue;
                }
                std::vector<crafting_requirement_result> &grp =
                    groups[static_cast<std::size_t>( edge.alternative )];
                if( static_cast<std::size_t>( edge.group ) >= grp.size() ) {
                    continue;
                }
                grp[static_cast<std::size_t>( edge.group )] =
                    or_reduce( grp[static_cast<std::size_t>( edge.group )], option_state );
            }
        }
    }

    // Reduce option OR -> group, group AND -> alternative, alternative
    // OR -> recipe, exactly as the direct evaluator does.
    for( const recipe_id &id : ids ) {
        std::array<crafting_requirement_result, menu_filter_mode_count> res;
        res.fill( crafting_requirement_result::unknown );
        auto it = states.find( id );
        if( it != states.end() ) {
            const crafting_requirement_plan *plan = index.plan_for( id );
            for( int m = 0; m < menu_filter_mode_count; ++m ) {
                const std::vector<std::vector<crafting_requirement_result>> &groups =
                    it->second[static_cast<std::size_t>( m )].groups;
                crafting_requirement_result mode_result =
                    crafting_requirement_result::unsatisfied;
                for( std::size_t a = 0; a < plan->alternatives.size(); ++a ) {
                    const crafting_requirement_alternative &alt = plan->alternatives[a];
                    crafting_requirement_result alt_result =
                        alt.empty()
                        ? crafting_requirement_result::satisfied
                        : allocation_guard( alt );
                    if( alt_result != crafting_requirement_result::unknown ) {
                        for( std::size_t g = 0; g < alt.size(); ++g ) {
                            const crafting_requirement_result group_result =
                                alt[g].options.empty()
                                ? crafting_requirement_result::unknown
                                : groups[a][g];
                            alt_result = and_reduce( alt_result, group_result );
                            if( alt_result == crafting_requirement_result::unsatisfied ) {
                                break;
                            }
                        }
                    }
                    mode_result = or_reduce( mode_result, alt_result );
                    if( mode_result == crafting_requirement_result::satisfied ) {
                        break;
                    }
                }
                res[static_cast<std::size_t>( m )] = mode_result;
            }
        }
        results_.emplace( id, res );
    }
}

crafting_requirement_result crafting_requirement_result_cache::evaluate(
    const recipe_id &recipe_id, menu_filter_mode menu ) const
{
    const int menu_index = static_cast<int>( menu );
    if( menu_index < 0 || menu_index >= menu_filter_mode_count ) {
        return crafting_requirement_result::unknown;
    }
    auto it = results_.find( recipe_id );
    if( it == results_.end() ) {
        return crafting_requirement_result::unknown;
    }
    return it->second[static_cast<std::size_t>( menu_index )];
}

std::size_t crafting_requirement_result_cache::cached_recipe_count() const
{
    return results_.size();
}
