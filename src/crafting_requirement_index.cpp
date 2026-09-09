#include "crafting_requirement_index.h"

#include <algorithm>

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

// Number of fact keys derived from one option: quality shares a single
// profile-0 key; everything else gets one key per menu mode.
int menu_modes_for_kind( crafting_requirement_fact_kind kind )
{
    return kind == crafting_requirement_fact_kind::quality_providers
           ? 1
           : menu_filter_mode_count;
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
                    // Item/tool facts carry the effective profile for their
                    // menu mode; quality facts always use profile 0.
                    const int profile = opt.kind ==
                                        crafting_requirement_fact_kind::quality_providers
                                        ? recipe_filter_none
                                        : effective_profiles[m];
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
