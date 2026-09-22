// Crafting inventory scenario matrix: a table-driven set of hand-authored
// inventory scenarios evaluated against the loaded recipe dictionary, with
// independent expectations for the graph (shadow) path, the bulk cache, the
// legacy requirement check, and representative snapshot facts.
//
// All inventories are constructed in code; no saves, userdata, filesystem
// fixtures, or running game state are consulted by the matrix itself.

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "avatar.h"
#include "calendar.h"
#include "cata_catch.h"
#include "character.h"
#include "color.h"
#include "crafting.h"
#include "crafting_gui_helpers.h"
#include "crafting_requirement_index.h"
#include "flag.h"
#include "inventory.h"
#include "item.h"
#include "itype.h"
#include "map.h"
#include "map_helpers.h"
#include "player_helpers.h"
#include "recipe.h"
#include "recipe_dictionary.h"
#include "requirements.h"
#include "type_id.h"
#include "uistate.h"
#include "units.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vpart_position.h"

using fact_kind = crafting_requirement_fact_kind;
using menu_mode = menu_filter_mode;

static const itype_id itype_2x4( "2x4" );
static const itype_id itype_fat( "fat" );
static const itype_id itype_knife_hunting( "knife_hunting" );
static const itype_id itype_rock( "rock" );
static const itype_id itype_stick( "stick" );

static const recipe_id recipe_cudgel_test_no_tools( "cudgel_test_no_tools" );
static const recipe_id recipe_test_tallow( "test_tallow" );

static const crafting_category_id crafting_category_armor( "CC_ARMOR" );
static const vproto_id vehicle_prototype_test_shopping_cart( "test_shopping_cart" );

static const skill_id skill_cooking( "cooking" );
static const skill_id skill_fabrication( "fabrication" );
static const skill_id skill_melee( "melee" );

// Tri-state plus a hand-authored legacy expectation for the unknown branch:
// when the shadow evaluator reports `unknown`, the menu must fall back to the
// legacy requirement check, whose outcome is asserted independently here.
enum class scenario_expectation {
    satisfied,
    unsatisfied,
    unknown_legacy_false,
    unknown_legacy_true
};

static const char *expectation_name( scenario_expectation e )
{
    switch( e ) {
        case scenario_expectation::satisfied:
            return "satisfied";
        case scenario_expectation::unsatisfied:
            return "unsatisfied";
        case scenario_expectation::unknown_legacy_false:
            return "unknown(legacy=false)";
        case scenario_expectation::unknown_legacy_true:
            return "unknown(legacy=true)";
    }
    return "?";
}

static const char *result_name( crafting_requirement_result r )
{
    switch( r ) {
        case crafting_requirement_result::satisfied:
            return "satisfied";
        case crafting_requirement_result::unsatisfied:
            return "unsatisfied";
        case crafting_requirement_result::unknown:
            return "unknown";
    }
    return "?";
}

namespace
{

constexpr std::size_t matrix_menu_count = menu_filter_mode_count;

const std::array<menu_mode, matrix_menu_count> matrix_menus = { {
        menu_mode::normal, menu_mode::no_rotten, menu_mode::no_favorite
    }
};

const std::array<recipe_filter_flags, matrix_menu_count> matrix_menu_flags = { {
        recipe_filter_flags::none,
        recipe_filter_flags::no_rotten,
        recipe_filter_flags::no_favorite
    }
};

const std::array<const char *, matrix_menu_count> matrix_menu_names = { {
        "normal", "no_rotten", "no_favorite"
    }
};

constexpr std::array<int, 4> volume_item_counts = { { 0, 1, 25, 10000 } };
constexpr std::array<int, 5> benchmark_item_counts = { { 0, 1, 25, 10000, 100000 } };
constexpr int extreme_item_count = 100000;

inventory make_board_inventory( int count )
{
    inventory inv;
    for( int i = 0; i < count; ++i ) {
        inv.add_item( item( itype_2x4 ) );
    }
    return inv;
}

void add_sticks( inventory &inv, int count, bool favorite = false, bool broken = false )
{
    for( int i = 0; i < count; ++i ) {
        item stick( itype_stick );
        if( favorite ) {
            stick.is_favorite = true;
        }
        if( broken ) {
            stick.set_flag( flag_ITEM_BROKEN );
        }
        inv.add_item( stick );
    }
}

void add_fat( inventory &inv, int count, bool rotten = false )
{
    for( int i = 0; i < count; ++i ) {
        item fat( itype_fat );
        if( rotten ) {
            fat.set_rot( 1000_hours );
        }
        inv.add_item( fat );
    }
}

// One table row: a readable scenario name, an explicit description of every
// item placed into the code-constructed inventory, the inventory builder,
// hand-authored expectations per recipe and menu mode, and minimum snapshot
// counts for representative facts (negative means "do not assert").
struct matrix_scenario {
    const char *name;
    const char *inventory_description;
    void ( *build )( inventory &inv );
    // Expectations for cudgel_test_no_tools (component: 1x 2x4, no tools),
    // indexed by menu mode (normal, no_rotten, no_favorite).
    std::array<scenario_expectation, matrix_menu_count> cudgel;
    // Expectations for test_tallow (2x fat component plus CUT 2 quality
    // tool).  A missing indexed component is exactly unsatisfied; once the
    // component is present, the unsupported quality check falls back.
    std::array<scenario_expectation, matrix_menu_count> tallow;
    // Minimum unfiltered snapshot counts; negative skips the assertion.
    int min_2x4_count;
    int min_fat_count;
};

const std::vector<matrix_scenario> &matrix_scenarios()
{
    static const std::vector<matrix_scenario> table = {
        {
            "empty", "no items at all",
            []( inventory & inv ) { add_fat( inv, 0 ); },
            {   {
                    scenario_expectation::unsatisfied, scenario_expectation::unsatisfied,
                    scenario_expectation::unsatisfied
                }
            },
            {   {
                    scenario_expectation::unsatisfied, scenario_expectation::unsatisfied,
                    scenario_expectation::unsatisfied
                }
            },
            0, 0
        },
        {
            "fat_without_tool", "two fresh fat, no CUT 2 tool: components are present but the tallow quality check must fall back",
            []( inventory & inv ) { add_fat( inv, 2 ); },
            {   {
                    scenario_expectation::unsatisfied, scenario_expectation::unsatisfied,
                    scenario_expectation::unsatisfied
                }
            },
            {   {
                    scenario_expectation::unknown_legacy_false, scenario_expectation::unknown_legacy_false,
                    scenario_expectation::unknown_legacy_false
                }
            },
            0, 2
        },
        {
            "exact_threshold", "one 2x4 and two fresh fat: exactly the cudgel component and tallow component counts",
            []( inventory & inv )
            {
                inv.add_item( item( itype_2x4 ) );
                add_fat( inv, 2 );
            },
            {   {
                    scenario_expectation::satisfied, scenario_expectation::satisfied,
                    scenario_expectation::satisfied
                }
            },
            {   {
                    scenario_expectation::unknown_legacy_false, scenario_expectation::unknown_legacy_false,
                    scenario_expectation::unknown_legacy_false
                }
            },
            1, 2
        },
        {
            "excess_saturation", "twenty 2x4 and twenty fresh fat: far beyond every requirement threshold",
            []( inventory & inv )
            {
                for( int i = 0; i < 20; ++i ) {
                    inv.add_item( item( itype_2x4 ) );
                }
                add_fat( inv, 20 );
            },
            {   {
                    scenario_expectation::satisfied, scenario_expectation::satisfied,
                    scenario_expectation::satisfied
                }
            },
            {   {
                    scenario_expectation::unknown_legacy_false, scenario_expectation::unknown_legacy_false,
                    scenario_expectation::unknown_legacy_false
                }
            },
            1, 1
        },
        {
            "favorite_component", "one favorite 2x4: satisfies the cudgel normally but not under the no-favorite filter",
            []( inventory & inv )
            {
                item board( itype_2x4 );
                board.is_favorite = true;
                inv.add_item( board );
            },
            {   {
                    scenario_expectation::satisfied, scenario_expectation::satisfied,
                    scenario_expectation::unsatisfied
                }
            },
            {   {
                    scenario_expectation::unsatisfied, scenario_expectation::unsatisfied,
                    scenario_expectation::unsatisfied
                }
            },
            1, 0
        },
        {
            "broken_component", "one broken 2x4: broken items never count toward any requirement",
            []( inventory & inv )
            {
                item board( itype_2x4 );
                board.set_flag( flag_ITEM_BROKEN );
                inv.add_item( board );
            },
            {   {
                    scenario_expectation::unsatisfied, scenario_expectation::unsatisfied,
                    scenario_expectation::unsatisfied
                }
            },
            {   {
                    scenario_expectation::unsatisfied, scenario_expectation::unsatisfied,
                    scenario_expectation::unsatisfied
                }
            },
            0, 0
        },
        {
            "tool_only", "one hunting knife (CUT 2): supplies the tallow tool but neither component",
            []( inventory & inv ) { inv.add_item( item( itype_knife_hunting ) ); },
            {   {
                    scenario_expectation::unsatisfied, scenario_expectation::unsatisfied,
                    scenario_expectation::unsatisfied
                }
            },
            {   {
                    scenario_expectation::unsatisfied, scenario_expectation::unsatisfied,
                    scenario_expectation::unsatisfied
                }
            },
            0, 0
        },
        {
            "tallow_ready", "one hunting knife and two fresh fat: the tallow is craftable via the legacy fallback",
            []( inventory & inv )
            {
                inv.add_item( item( itype_knife_hunting ) );
                add_fat( inv, 2 );
            },
            {   {
                    scenario_expectation::unsatisfied, scenario_expectation::unsatisfied,
                    scenario_expectation::unsatisfied
                }
            },
            {   {
                    scenario_expectation::unknown_legacy_true, scenario_expectation::unknown_legacy_true,
                    scenario_expectation::unknown_legacy_true
                }
            },
            0, 2
        },
        {
            "rotten_components", "two rotten fat without a CUT 2 tool: normal and no-favorite modes fall back, while no-rotten is exactly unavailable",
            []( inventory & inv ) { add_fat( inv, 2, true ); },
            {   {
                    scenario_expectation::unsatisfied, scenario_expectation::unsatisfied,
                    scenario_expectation::unsatisfied
                }
            },
            {   {
                    scenario_expectation::unknown_legacy_false, scenario_expectation::unsatisfied,
                    scenario_expectation::unknown_legacy_false
                }
            },
            0, 2
        },
    };
    return table;
}

struct matrix_outcome {
    crafting_requirement_result graph;
    crafting_requirement_result cache;
    bool legacy;
};

// Runs one (recipe, inventory, menu mode) cell: shadow evaluation, the bulk
// cache, and the legacy requirement check over the same inventory.
matrix_outcome run_matrix_case( const recipe &r, const inventory &inv, menu_mode menu,
                                recipe_filter_flags menu_flag )
{
    const crafting_requirement_index &index = recipe_dict.requirement_index();
    const crafting_inventory_snapshot snapshot( index, inv );
    const crafting_requirement_result_cache cache( index, snapshot );
    const crafting_requirement_evaluator eval( index, snapshot );
    matrix_outcome out;
    out.graph = eval.evaluate( r.ident(), menu );
    out.cache = cache.evaluate( r.ident(), menu );
    out.legacy = r.deduped_requirements().can_make_with_inventory(
                     nullptr, inv, r.get_component_filter( menu_flag ), 1, craft_flags::none );
    return out;
}

// Full failure context: scenario, inventory description, recipe, menu/filter
// mode, expected value, graph result, cache result, legacy result.
void check_matrix_case( const matrix_scenario &sc, const char *recipe_name,
                        const char *menu_name, scenario_expectation expected,
                        const matrix_outcome &out )
{
    INFO( "scenario: " << sc.name );
    INFO( "inventory: " << sc.inventory_description );
    INFO( "recipe: " << recipe_name );
    INFO( "menu/filter mode: " << menu_name );
    INFO( "expected: " << expectation_name( expected ) );
    INFO( "graph result: " << result_name( out.graph ) );
    INFO( "cache result: " << result_name( out.cache ) );
    INFO( "legacy craftable: " << out.legacy );
    if( expected == scenario_expectation::unknown_legacy_false ||
        expected == scenario_expectation::unknown_legacy_true ) {
        // Unknown graph result: the menu must fall back to the legacy
        // requirement check, whose outcome is asserted independently.
        const bool expected_legacy = expected == scenario_expectation::unknown_legacy_true;
        CHECK( out.graph == crafting_requirement_result::unknown );
        CHECK( out.cache == crafting_requirement_result::unknown );
        CHECK( out.legacy == expected_legacy );
    } else {
        // Exact graph result: graph matches the hand-authored expectation
        // and agrees with the legacy check.
        const crafting_requirement_result want =
            expected == scenario_expectation::satisfied
            ? crafting_requirement_result::satisfied
            : crafting_requirement_result::unsatisfied;
        CHECK( out.graph == want );
        CHECK( out.cache == out.graph );
        CHECK( out.legacy == ( want == crafting_requirement_result::satisfied ) );
    }
}

} // namespace

TEST_CASE( "crafting_inventory_scenario_matrix", "[crafting][inventory_matrix]" )
{
    const crafting_requirement_index &index = recipe_dict.requirement_index();
    REQUIRE( index.is_finalized() );

    const recipe &cudgel = recipe_cudgel_test_no_tools.obj();
    const recipe &tallow = recipe_test_tallow.obj();

    const crafting_requirement_fact_key key_2x4 = []() {
        crafting_requirement_fact_key k;
        k.kind = fact_kind::component_units;
        k.id = "2x4";
        return k;
    }
    ();
    const crafting_requirement_fact_key key_fat = []() {
        crafting_requirement_fact_key k;
        k.kind = fact_kind::component_units;
        k.id = "fat";
        return k;
    }
    ();

    for( const matrix_scenario &sc : matrix_scenarios() ) {
        INFO( "scenario: " << sc.name );
        INFO( "inventory: " << sc.inventory_description );

        inventory inv;
        sc.build( inv );

        // Representative snapshot facts, not just final craftability.
        const crafting_inventory_snapshot snap( index, inv );
        CHECK( snap.fact_count() == index.fact_count() );
        if( sc.min_2x4_count >= 0 ) {
            CAPTURE( sc.min_2x4_count );
            CHECK( snap.count_for( key_2x4 ) >= sc.min_2x4_count );
        }
        if( sc.min_fat_count >= 0 ) {
            CAPTURE( sc.min_fat_count );
            CHECK( snap.count_for( key_fat ) >= sc.min_fat_count );
        }

        for( std::size_t m = 0; m < matrix_menu_count; ++m ) {
            check_matrix_case( sc, "cudgel_test_no_tools", matrix_menu_names[m],
                               sc.cudgel[m],
                               run_matrix_case( cudgel, inv, matrix_menus[m],
                                                matrix_menu_flags[m] ) );
            check_matrix_case( sc, "test_tallow", matrix_menu_names[m],
                               sc.tallow[m],
                               run_matrix_case( tallow, inv, matrix_menus[m],
                                                matrix_menu_flags[m] ) );
        }
    }
}

// Exercise the same exact recipe from an empty inventory through 10,000
// concrete items in the routine suite.  The snapshot must scan the source
// inventory while retaining only the recipe-derived capped count; the
// 100,000-item stress case remains explicit below.
TEST_CASE( "crafting_inventory_scenario_volume_matrix",
           "[crafting][inventory_matrix][inventory_volume]" )
{
    const crafting_requirement_index &index = recipe_dict.requirement_index();
    const recipe &cudgel = recipe_cudgel_test_no_tools.obj();
    crafting_requirement_fact_key board;
    board.kind = fact_kind::component_units;
    board.id = "2x4";
    const int maximum = index.maximum_for( board );
    REQUIRE( maximum > 0 );

    for( int item_count : volume_item_counts ) {
        DYNAMIC_SECTION( item_count << " concrete 2x4 items" ) {
            const inventory inv = make_board_inventory( item_count );
            const crafting_inventory_snapshot snapshot( index, inv );
            const crafting_requirement_result_cache cache( index, snapshot );
            const crafting_requirement_evaluator evaluator( index, snapshot );
            const int expected_count = item_count < maximum ? item_count : maximum;
            const crafting_requirement_result expected_result = item_count == 0 ?
                    crafting_requirement_result::unsatisfied :
                    crafting_requirement_result::satisfied;

            INFO( "source item count: " << item_count );
            INFO( "2x4 ceiling: " << maximum );
            INFO( "stored 2x4 count: " << snapshot.count_for( board ) );
            CHECK( snapshot.count_for( board ) == expected_count );
            CHECK( snapshot.meets( board, 1 ) == ( item_count > 0 ) );
            if( item_count >= maximum ) {
                CHECK( snapshot.count_for( board ) == maximum );
            }

            for( menu_mode menu : matrix_menus ) {
                CAPTURE( menu );
                CHECK( evaluator.evaluate( cudgel.ident(), menu ) == expected_result );
                CHECK( cache.evaluate( cudgel.ident(), menu ) == expected_result );
            }

            const bool legacy = cudgel.deduped_requirements().can_make_with_inventory(
                                    nullptr, inv,
                                    cudgel.get_component_filter( recipe_filter_flags::none ),
                                    1, craft_flags::none );
            CHECK( legacy == ( item_count > 0 ) );
        }
    }
}

TEST_CASE( "crafting_inventory_scenario_extreme_volume",
           "[.][inventory_extreme]" )
{
    const crafting_requirement_index &index = recipe_dict.requirement_index();
    const recipe &cudgel = recipe_cudgel_test_no_tools.obj();
    crafting_requirement_fact_key board;
    board.kind = fact_kind::component_units;
    board.id = "2x4";
    const int maximum = index.maximum_for( board );
    REQUIRE( maximum > 0 );

    const inventory inv = make_board_inventory( extreme_item_count );
    const crafting_inventory_snapshot snapshot( index, inv );
    const crafting_requirement_result_cache cache( index, snapshot );
    const crafting_requirement_evaluator evaluator( index, snapshot );
    INFO( "source item count: " << extreme_item_count );
    INFO( "2x4 ceiling: " << maximum );
    CHECK( snapshot.count_for( board ) == maximum );
    CHECK( snapshot.meets( board, 1 ) );
    CHECK( snapshot.saturated_fact_count() > 0 );

    for( menu_mode menu : matrix_menus ) {
        CAPTURE( menu );
        CHECK( evaluator.evaluate( cudgel.ident(), menu ) ==
               crafting_requirement_result::satisfied );
        CHECK( cache.evaluate( cudgel.ident(), menu ) ==
               crafting_requirement_result::satisfied );
    }

    CHECK( cudgel.deduped_requirements().can_make_with_inventory(
               nullptr, inv,
               cudgel.get_component_filter( recipe_filter_flags::none ),
               1, craft_flags::none ) );
}

// Deterministic compact report over the same scenario table; hidden by
// default, run explicitly with the [inventory_matrix_report] tag. Prints
// per-menu outcome counts only; no timing, no wall-clock values.
TEST_CASE( "crafting_inventory_scenario_matrix_report",
           "[.][crafting][inventory_matrix_report]" )
{
    const recipe &cudgel = recipe_cudgel_test_no_tools.obj();
    const recipe &tallow = recipe_test_tallow.obj();

    std::array<std::size_t, matrix_menu_count> satisfied = {};
    std::array<std::size_t, matrix_menu_count> unsatisfied = {};
    std::array<std::size_t, matrix_menu_count> unknown = {};
    std::size_t comparisons = 0;

    for( const matrix_scenario &sc : matrix_scenarios() ) {
        inventory inv;
        sc.build( inv );
        for( std::size_t m = 0; m < matrix_menu_count; ++m ) {
            for( const recipe *r : {
                     &cudgel, &tallow
                 } ) {
                const matrix_outcome out = run_matrix_case( *r, inv, matrix_menus[m],
                                           matrix_menu_flags[m] );
                switch( out.graph ) {
                    case crafting_requirement_result::satisfied:
                        ++satisfied[m];
                        break;
                    case crafting_requirement_result::unsatisfied:
                        ++unsatisfied[m];
                        break;
                    case crafting_requirement_result::unknown:
                        ++unknown[m];
                        break;
                }
                ++comparisons;
                INFO( "scenario: " << sc.name << " recipe: " << r->ident().str()
                      << " menu: " << matrix_menu_names[m]
                      << " graph: " << result_name( out.graph )
                      << " legacy: " << out.legacy );
                CHECK( out.cache == out.graph );
            }
        }
    }

    CAPTURE( comparisons );
    CAPTURE( satisfied[0] );
    CAPTURE( unsatisfied[0] );
    CAPTURE( unknown[0] );
    CAPTURE( satisfied[1] );
    CAPTURE( unsatisfied[1] );
    CAPTURE( unknown[1] );
    CAPTURE( satisfied[2] );
    CAPTURE( unsatisfied[2] );
    CAPTURE( unknown[2] );
    REQUIRE( comparisons > 0 );
}

// Menu-facing availability over an arbitrary code-constructed crafting
// inventory, following the established crafting_gui_test pattern: the avatar
// is reset by the test harness and items are added in code.
namespace
{

Character &setup_matrix_character()
{
    clear_avatar();
    clear_map_without_vision();
    Character &guy = get_avatar();
    guy.worn.wear_item( guy, item( itype_id( "debug_backpack" ) ), false, false );
    guy.invalidate_crafting_inventory();
    return guy;
}

class recipe_ui_state_guard
{
    public:
        recipe_ui_state_guard() :
            hidden( uistate.hidden_recipes ),
            expanded( uistate.expanded_recipes ),
            read( uistate.read_recipes ) {
            uistate.hidden_recipes.clear();
            uistate.expanded_recipes.clear();
            uistate.read_recipes.clear();
        }

        ~recipe_ui_state_guard() {
            uistate.hidden_recipes = hidden;
            uistate.expanded_recipes = expanded;
            uistate.read_recipes = read;
        }

    private:
        decltype( uistate.hidden_recipes ) hidden;
        decltype( uistate.expanded_recipes ) expanded;
        decltype( uistate.read_recipes ) read;
};

double median_milliseconds( const std::function<void()> &operation, int samples )
{
    std::vector<double> timings;
    timings.reserve( samples );
    for( int i = 0; i < samples; ++i ) {
        const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        operation();
        const std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        timings.push_back( std::chrono::duration<double, std::milli>( end - start ).count() );
    }
    std::sort( timings.begin(), timings.end() );
    return timings[timings.size() / 2];
}

bool recipe_plan_has_tool_charges( const crafting_requirement_index &index,
                                   const recipe_id &id )
{
    const crafting_requirement_plan *plan = index.plan_for( id );
    if( plan == nullptr ) {
        return false;
    }
    for( const crafting_requirement_alternative &alternative : plan->alternatives ) {
        for( const crafting_requirement_group &group : alternative ) {
            for( const crafting_requirement_option &option : group.options ) {
                if( option.kind == fact_kind::tool_charges ) {
                    return true;
                }
            }
        }
    }
    return false;
}

} // namespace

TEST_CASE( "crafting_inventory_scenario_menu_availability",
           "[crafting][inventory_matrix]" )
{
    const recipe &cudgel = recipe_cudgel_test_no_tools.obj();

    // Empty inventory: missing component, dark gray, not craftable.
    SECTION( "empty inventory" ) {
        Character &guy = setup_matrix_character();
        guy.set_skill_level( skill_fabrication, 2 );
        guy.set_skill_level( skill_melee, 1 );
        guy.invalidate_crafting_inventory();

        const availability avail( guy, &cudgel );
        INFO( "scenario: empty inventory" );
        INFO( "recipe: cudgel_test_no_tools" );
        INFO( "expected: can_craft_recipe=false color=dark_gray" );
        INFO( "menu can_craft_recipe: " << avail.can_craft_recipe );
        CHECK_FALSE( avail.can_craft_recipe );
        CHECK( avail.has_all_skills );
        CHECK( avail.color() == c_dark_gray );
    }

    // Exact threshold: one 2x4 makes it craftable, white.
    SECTION( "exact threshold" ) {
        Character &guy = setup_matrix_character();
        guy.set_skill_level( skill_fabrication, 2 );
        guy.set_skill_level( skill_melee, 1 );
        guy.i_add( item( itype_2x4 ) );
        guy.invalidate_crafting_inventory();

        const availability avail( guy, &cudgel );
        INFO( "scenario: exact threshold" );
        INFO( "inventory: one 2x4" );
        INFO( "recipe: cudgel_test_no_tools" );
        INFO( "expected: can_craft_recipe=true color=white" );
        INFO( "menu can_craft_recipe: " << avail.can_craft_recipe );
        CHECK( avail.can_craft_recipe );
        // This diagnostic is only set when the deduplicated requirement
        // check fails but the simpler overlapping-component check passes.
        CHECK_FALSE( avail.is_apparently_craftable() );
        CHECK( avail.has_all_skills );
        CHECK_FALSE( avail.would_use_rotten );
        CHECK_FALSE( avail.would_use_favorite );
        CHECK( avail.color() == c_white );
    }

    // Nearby map source: crafting_inventory() includes reachable map items,
    // so the menu and graph cache must see the same board without carrying it.
    SECTION( "nearby map component" ) {
        Character &guy = setup_matrix_character();
        guy.set_skill_level( skill_fabrication, 2 );
        guy.set_skill_level( skill_melee, 1 );
        get_map().add_item_or_charges( guy.pos_bub(), item( itype_2x4 ) );
        guy.invalidate_crafting_inventory();

        const crafting_requirement_index &index = recipe_dict.requirement_index();
        const crafting_inventory_snapshot snapshot( index, guy.crafting_inventory() );
        const crafting_requirement_result_cache cache( index, snapshot );
        const availability legacy( guy, &cudgel );
        const availability cached( guy, &cudgel, 1, false, nullptr, &cache );
        INFO( "scenario: nearby map component" );
        INFO( "inventory: one 2x4 on the avatar's map tile" );
        INFO( "recipe: cudgel_test_no_tools" );
        CHECK( cache.evaluate( cudgel.ident(), menu_mode::normal ) ==
               crafting_requirement_result::satisfied );
        CHECK( legacy.can_craft_recipe );
        CHECK( cached.can_craft_recipe == legacy.can_craft_recipe );
        CHECK( cached.color() == legacy.color() );
        CHECK( legacy.color() == c_white );
    }

    SECTION( "adjacent map component among clutter" ) {
        Character &guy = setup_matrix_character();
        guy.set_skill_level( skill_fabrication, 2 );
        guy.set_skill_level( skill_melee, 1 );
        map &here = get_map();
        const tripoint_bub_ms adjacent = guy.pos_bub() + tripoint::east;
        for( int i = 0; i < 25; ++i ) {
            here.add_item_or_charges( guy.pos_bub(), item( itype_stick ) );
        }
        here.add_item_or_charges( adjacent, item( itype_2x4 ) );
        guy.invalidate_crafting_inventory();

        const crafting_requirement_index &index = recipe_dict.requirement_index();
        const crafting_inventory_snapshot snapshot( index, guy.crafting_inventory() );
        const crafting_requirement_result_cache cache( index, snapshot );
        const availability legacy( guy, &cudgel );
        const availability cached( guy, &cudgel, 1, false, nullptr, &cache );
        INFO( "scenario: adjacent map component among clutter" );
        INFO( "inventory: 25 sticks on the avatar tile and one 2x4 one tile east" );
        CHECK( cache.evaluate( cudgel.ident(), menu_mode::normal ) ==
               crafting_requirement_result::satisfied );
        CHECK( legacy.can_craft_recipe );
        CHECK( cached.can_craft_recipe == legacy.can_craft_recipe );
        CHECK( cached.color() == legacy.color() );
    }

    SECTION( "split carried and map requirements" ) {
        Character &guy = setup_matrix_character();
        guy.set_skill_level( skill_cooking, 3 );
        guy.i_add( item( itype_fat ) );
        guy.i_add( item( itype_fat ) );
        get_map().add_item_or_charges( guy.pos_bub() + tripoint::east,
                                       item( itype_knife_hunting ) );
        guy.invalidate_crafting_inventory();

        const recipe &tallow = recipe_test_tallow.obj();
        const crafting_requirement_index &index = recipe_dict.requirement_index();
        const crafting_inventory_snapshot snapshot( index, guy.crafting_inventory() );
        const crafting_requirement_result_cache cache( index, snapshot );
        const availability legacy( guy, &tallow );
        const availability cached( guy, &tallow, 1, false, nullptr, &cache );
        INFO( "scenario: split carried and map requirements" );
        INFO( "inventory: two carried fat and a hunting knife one tile east" );
        CHECK( cache.evaluate( tallow.ident(), menu_mode::normal ) ==
               crafting_requirement_result::unknown );
        CHECK( legacy.can_craft_recipe );
        CHECK( cached.can_craft_recipe == legacy.can_craft_recipe );
        CHECK( cached.has_all_skills == legacy.has_all_skills );
        CHECK( cached.color() == legacy.color() );
        CHECK( legacy.color() == c_white );
    }

    // Favorite filtering: a single favorite 2x4 satisfies the normal menu
    // but the cache knows the no-favorite menu would reject it.
    SECTION( "favorite component" ) {
        Character &guy = setup_matrix_character();
        guy.set_skill_level( skill_fabrication, 2 );
        guy.set_skill_level( skill_melee, 1 );
        item board( itype_2x4 );
        board.is_favorite = true;
        guy.i_add( board );
        guy.invalidate_crafting_inventory();

        const crafting_requirement_index &index = recipe_dict.requirement_index();
        const crafting_inventory_snapshot snapshot( index, guy.crafting_inventory() );
        const crafting_requirement_result_cache cache( index, snapshot );
        INFO( "scenario: favorite component" );
        INFO( "inventory: one favorite 2x4" );
        INFO( "recipe: cudgel_test_no_tools" );
        CAPTURE( cache.evaluate( cudgel.ident(), menu_mode::normal ) );
        CAPTURE( cache.evaluate( cudgel.ident(), menu_mode::no_favorite ) );
        REQUIRE( cache.evaluate( cudgel.ident(), menu_mode::normal ) ==
                 crafting_requirement_result::satisfied );
        REQUIRE( cache.evaluate( cudgel.ident(), menu_mode::no_favorite ) ==
                 crafting_requirement_result::unsatisfied );

        const availability legacy( guy, &cudgel );
        const availability cached( guy, &cudgel, 1, false, nullptr, &cache );
        INFO( "expected: cached menu fields equal legacy fields" );
        INFO( "menu can_craft_recipe: " << legacy.can_craft_recipe );
        INFO( "menu would_use_favorite: " << legacy.would_use_favorite );
        CHECK( legacy.can_craft_recipe );
        CHECK( legacy.would_use_favorite );
        CHECK( cached.can_craft_recipe == legacy.can_craft_recipe );
        CHECK( cached.would_use_rotten == legacy.would_use_rotten );
        CHECK( cached.would_use_favorite == legacy.would_use_favorite );
        CHECK( cached.is_apparently_craftable() == legacy.is_apparently_craftable() );
        CHECK( cached.has_proficiencies == legacy.has_proficiencies );
        CHECK( cached.has_all_skills == legacy.has_all_skills );
        CHECK( cached.is_nested_category == legacy.is_nested_category );
        CHECK( cached.color() == legacy.color() );
        CHECK( legacy.color() == c_pink );
    }

    // Broken exclusion: a broken 2x4 does not satisfy the menu.
    SECTION( "broken component" ) {
        Character &guy = setup_matrix_character();
        guy.set_skill_level( skill_fabrication, 2 );
        guy.set_skill_level( skill_melee, 1 );
        item board( itype_2x4 );
        board.set_flag( flag_ITEM_BROKEN );
        guy.i_add( board );
        guy.invalidate_crafting_inventory();

        const availability avail( guy, &cudgel );
        INFO( "scenario: broken component" );
        INFO( "inventory: one broken 2x4" );
        INFO( "recipe: cudgel_test_no_tools" );
        INFO( "expected: can_craft_recipe=false" );
        INFO( "menu can_craft_recipe: " << avail.can_craft_recipe );
        CHECK_FALSE( avail.can_craft_recipe );
        CHECK( avail.color() == c_dark_gray );
    }
}

TEST_CASE( "crafting_inventory_scenario_cudgel_skill_matrix",
           "[crafting][inventory_matrix][skill_matrix]" )
{
    struct skill_case {
        int fabrication;
        int melee;
        bool primary_skill;
        bool all_skills;
        nc_color color;
    };
    const std::array<skill_case, 5> cases = { {
            { 0, 0, false, false, c_light_red },
            { 1, 1, true, false, c_yellow },
            { 2, 0, true, false, c_yellow },
            { 2, 1, true, true, c_white },
            { 10, 10, true, true, c_white }
        }
    };
    const recipe &cudgel = recipe_cudgel_test_no_tools.obj();

    for( const skill_case &sc : cases ) {
        DYNAMIC_SECTION( "fabrication " << sc.fabrication << ", melee " << sc.melee ) {
            Character &guy = setup_matrix_character();
            guy.set_skill_level( skill_fabrication, sc.fabrication );
            guy.set_skill_level( skill_melee, sc.melee );
            guy.i_add( item( itype_2x4 ) );
            guy.invalidate_crafting_inventory();

            const crafting_requirement_index &index = recipe_dict.requirement_index();
            const crafting_inventory_snapshot snapshot( index, guy.crafting_inventory() );
            const crafting_requirement_result_cache cache( index, snapshot );
            const availability legacy( guy, &cudgel );
            const availability cached( guy, &cudgel, 1, false, nullptr, &cache );
            INFO( "inventory: one carried 2x4" );
            CHECK( cache.evaluate( cudgel.ident(), menu_mode::normal ) ==
                   crafting_requirement_result::satisfied );
            CHECK( legacy.can_craft_recipe );
            CHECK( legacy.crafter_has_primary_skill == sc.primary_skill );
            CHECK( legacy.has_all_skills == sc.all_skills );
            CHECK( legacy.color() == sc.color );
            CHECK( cached.can_craft_recipe == legacy.can_craft_recipe );
            CHECK( cached.crafter_has_primary_skill == legacy.crafter_has_primary_skill );
            CHECK( cached.has_all_skills == legacy.has_all_skills );
            CHECK( cached.color() == legacy.color() );
        }
    }
}

TEST_CASE( "crafting_inventory_scenario_tallow_skill_matrix",
           "[crafting][inventory_matrix][skill_matrix]" )
{
    struct skill_case {
        int cooking;
        bool primary_skill;
        bool all_skills;
        nc_color color;
    };
    const std::array<skill_case, 5> cases = { {
            { 0, false, false, c_light_red },
            { 1, false, false, c_light_red },
            { 2, true, false, c_yellow },
            { 3, true, true, c_white },
            { 10, true, true, c_white }
        }
    };
    const recipe &tallow = recipe_test_tallow.obj();

    for( const skill_case &sc : cases ) {
        DYNAMIC_SECTION( "cooking " << sc.cooking ) {
            Character &guy = setup_matrix_character();
            guy.set_skill_level( skill_cooking, sc.cooking );
            guy.i_add( item( itype_fat ) );
            guy.i_add( item( itype_fat ) );
            guy.i_add( item( itype_knife_hunting ) );
            guy.invalidate_crafting_inventory();

            const crafting_requirement_index &index = recipe_dict.requirement_index();
            const crafting_inventory_snapshot snapshot( index, guy.crafting_inventory() );
            const crafting_requirement_result_cache cache( index, snapshot );
            const availability legacy( guy, &tallow );
            const availability cached( guy, &tallow, 1, false, nullptr, &cache );
            INFO( "inventory: two carried fat and one hunting knife" );
            CHECK( cache.evaluate( tallow.ident(), menu_mode::normal ) ==
                   crafting_requirement_result::unknown );
            CHECK( legacy.can_craft_recipe );
            CHECK( legacy.crafter_has_primary_skill == sc.primary_skill );
            CHECK( legacy.has_all_skills == sc.all_skills );
            CHECK( legacy.color() == sc.color );
            CHECK( cached.can_craft_recipe == legacy.can_craft_recipe );
            CHECK( cached.crafter_has_primary_skill == legacy.crafter_has_primary_skill );
            CHECK( cached.has_all_skills == legacy.has_all_skills );
            CHECK( cached.color() == legacy.color() );
        }
    }
}

// Pure snapshot scenario semantics over a fully hand-built index, where the
// fact maxima are known exactly: threshold boundaries, saturation caps,
// count-by-charges facts, pseudo tools, and profile filtering.
TEST_CASE( "crafting_inventory_scenario_snapshot_semantics",
           "[crafting][inventory_matrix]" )
{
    crafting_requirement_index index;
    crafting_requirement_plan stick_plan;
    stick_plan.alternatives.push_back( { []()
    {
        crafting_requirement_group g;
        g.kind = requirement_group_kind::component;
        crafting_requirement_option o;
        o.kind = fact_kind::component_units;
        o.id = "stick";
        o.threshold = 3;
        g.options.push_back( o );
        return g;
    }
    () } );
    crafting_requirement_plan rock_plan;
    rock_plan.alternatives.push_back( { []()
    {
        crafting_requirement_group g;
        g.kind = requirement_group_kind::component;
        crafting_requirement_option o;
        o.kind = fact_kind::component_charges;
        o.id = "rock";
        o.threshold = 5;
        g.options.push_back( o );
        return g;
    }
    () } );
    crafting_requirement_plan tool_plan;
    tool_plan.alternatives.push_back( { []()
    {
        crafting_requirement_group g;
        g.kind = requirement_group_kind::tool;
        crafting_requirement_option o;
        o.kind = fact_kind::tool_instances;
        o.id = "stick";
        o.threshold = 1;
        g.options.push_back( o );
        return g;
    }
    () } );
    crafting_requirement_plan fav_plan;
    fav_plan.alternatives.push_back( { []()
    {
        crafting_requirement_group g;
        g.kind = requirement_group_kind::component;
        crafting_requirement_option o;
        o.kind = fact_kind::component_units;
        o.id = "stick";
        o.threshold = 2;
        g.options.push_back( o );
        return g;
    }
    () } );
    const std::array<int, menu_filter_mode_count> unfiltered = { {
            recipe_filter_none, recipe_filter_none, recipe_filter_none
        }
    };
    std::array<int, menu_filter_mode_count> no_favorite = { {
            recipe_filter_none, recipe_filter_none,
            recipe_filter_favorite_forbidden
        }
    };
    CHECK( index.add_recipe( recipe_id( std::string( "matrix_stick" ) ), stick_plan,
                             unfiltered ) );
    CHECK( index.add_recipe( recipe_id( std::string( "matrix_rock" ) ), rock_plan,
                             unfiltered ) );
    CHECK( index.add_recipe( recipe_id( std::string( "matrix_tool" ) ), tool_plan,
                             unfiltered ) );
    CHECK( index.add_recipe( recipe_id( std::string( "matrix_favorite" ) ), fav_plan,
                             no_favorite ) );
    index.finalize();

    const crafting_requirement_fact_key stick = []() {
        crafting_requirement_fact_key k;
        k.kind = fact_kind::component_units;
        k.id = "stick";
        return k;
    }
    ();
    const crafting_requirement_fact_key stick_charges = []() {
        crafting_requirement_fact_key k;
        k.kind = fact_kind::component_units;
        k.id = "stick";
        k.filter_profile = recipe_filter_favorite_forbidden;
        return k;
    }
    ();
    const crafting_requirement_fact_key rock = []() {
        crafting_requirement_fact_key k;
        k.kind = fact_kind::component_charges;
        k.id = "rock";
        return k;
    }
    ();
    const crafting_requirement_fact_key stick_tool = []() {
        crafting_requirement_fact_key k;
        k.kind = fact_kind::tool_instances;
        k.id = "stick";
        return k;
    }
    ();

    REQUIRE_FALSE( item::count_by_charges( itype_stick ) );
    REQUIRE( item::count_by_charges( itype_rock ) );
    REQUIRE( index.maximum_for( stick ) == 3 );
    REQUIRE( index.maximum_for( rock ) == 5 );

    // Empty: nothing counted, nothing saturated.
    {
        const crafting_inventory_snapshot snap( index, inventory() );
        INFO( "scenario: empty inventory" );
        INFO( "expected: stick=0 rock=0 saturated=0" );
        CHECK( snap.count_for( stick ) == 0 );
        CHECK( snap.count_for( rock ) == 0 );
        CHECK( snap.saturated_fact_count() == 0 );
        CHECK_FALSE( snap.meets( stick, 3 ) );
    }

    // Just below the threshold: 2 of 3 sticks.
    {
        inventory inv;
        add_sticks( inv, 2 );
        const crafting_inventory_snapshot snap( index, inv );
        INFO( "scenario: just below threshold" );
        INFO( "inventory: two sticks" );
        INFO( "expected: stick=2, threshold 3 unmet" );
        CHECK( snap.count_for( stick ) == 2 );
        CHECK_FALSE( snap.meets( stick, 3 ) );
    }

    // Exact threshold: 3 sticks and 5 rock charges, both saturated.
    {
        inventory inv;
        add_sticks( inv, 3 );
        item rock_item( itype_rock );
        rock_item.charges = 5;
        inv.add_item( rock_item );
        const crafting_inventory_snapshot snap( index, inv );
        INFO( "scenario: exact threshold" );
        INFO( "inventory: three sticks and one rock with 5 charges" );
        INFO( "expected: stick=3 rock=5; both representative facts saturated" );
        CHECK( snap.count_for( stick ) == 3 );
        CHECK( snap.count_for( rock ) == 5 );
        CHECK( snap.saturated_fact_count() >= 2 );
    }

    // Excess: counts cap exactly at the index maxima.
    {
        inventory inv;
        add_sticks( inv, 30 );
        item rock_item( itype_rock );
        rock_item.charges = 500;
        inv.add_item( rock_item );
        const crafting_inventory_snapshot snap( index, inv );
        INFO( "scenario: excess saturation" );
        INFO( "inventory: thirty sticks and one rock with 500 charges" );
        INFO( "expected: stick capped at 3 and rock capped at 5" );
        CHECK( snap.count_for( stick ) == 3 );
        CHECK( snap.count_for( rock ) == 5 );
        CHECK( snap.saturated_fact_count() >= 2 );
    }

    // Broken exclusion: broken sticks count as neither component nor tool.
    {
        inventory inv;
        add_sticks( inv, 3, false, true );
        const crafting_inventory_snapshot snap( index, inv );
        INFO( "scenario: broken exclusion" );
        INFO( "inventory: three broken sticks" );
        INFO( "expected: component=0 tool=0" );
        CHECK( snap.count_for( stick ) == 0 );
        CHECK( snap.count_for( stick_tool ) == 0 );
    }

    // Count-by-charges: one rock item with 4 charges is below the 5-charge
    // threshold; charges accumulate across items.
    {
        inventory inv;
        item low( itype_rock );
        low.charges = 4;
        inv.add_item( low );
        const crafting_inventory_snapshot snap( index, inv );
        INFO( "scenario: count by charges" );
        INFO( "inventory: one rock with 4 charges" );
        INFO( "expected: rock=4, threshold 5 unmet" );
        CHECK( snap.count_for( rock ) == 4 );
        CHECK_FALSE( snap.meets( rock, 5 ) );

        item top_up( itype_rock );
        top_up.charges = 1;
        inv.add_item( top_up );
        const crafting_inventory_snapshot combined( index, inv );
        INFO( "inventory: plus one rock with 1 charge" );
        INFO( "expected: rock=5, threshold met" );
        CHECK( combined.count_for( rock ) == 5 );
        CHECK( combined.meets( rock, 5 ) );
    }

    // Tool presence: a pseudo stick counts as a tool instance but never as
    // a component.
    {
        inventory inv;
        item pseudo( itype_stick );
        pseudo.set_flag( flag_PSEUDO );
        inv.add_item( pseudo );
        const crafting_inventory_snapshot snap( index, inv );
        INFO( "scenario: tool presence" );
        INFO( "inventory: one pseudo stick" );
        INFO( "expected: tool=1 component=0" );
        CHECK( snap.count_for( stick_tool ) == 1 );
        CHECK( snap.meets( stick_tool, 1 ) );
        CHECK( snap.count_for( stick ) == 0 );
    }

    // Favorite filtering: two favorite sticks count under the unfiltered
    // and no-rotten profiles but vanish under the no-favorite profile.
    {
        inventory inv;
        add_sticks( inv, 2, true );
        const crafting_inventory_snapshot snap( index, inv );
        INFO( "scenario: favorite filtering" );
        INFO( "inventory: two favorite sticks" );
        INFO( "expected: unfiltered=2, no-favorite=0" );
        CHECK( snap.count_for( stick ) == 2 );
        CHECK( snap.meets( stick, 2 ) );
        CHECK( snap.count_for( stick_charges ) == 0 );
        CHECK_FALSE( snap.meets( stick_charges, 2 ) );
    }
}

// Hidden wall-clock benchmark.  It deliberately has no timing assertion:
// machines and CI workers vary, while the reported samples remain useful for
// comparing branches and inventory scales with the same command.
TEST_CASE( "crafting_inventory_scenario_volume_benchmark",
           "[.][inventory_matrix_benchmark][benchmark]" )
{
    const crafting_requirement_index &index = recipe_dict.requirement_index();
    const recipe_id target_recipe = recipe_cudgel_test_no_tools;

    for( int item_count : benchmark_item_counts ) {
        DYNAMIC_SECTION( item_count << " concrete inventory items" ) {
            const inventory inv = make_board_inventory( item_count );
            const crafting_inventory_snapshot prepared_snapshot( index, inv );

            BENCHMARK( "snapshot construction" ) {
                const crafting_inventory_snapshot snapshot( index, inv );
                return snapshot.saturated_fact_count();
            };

            BENCHMARK( "result-cache construction from prepared snapshot" ) {
                const crafting_requirement_result_cache cache( index, prepared_snapshot );
                return cache.evaluate( target_recipe, menu_mode::normal );
            };

            BENCHMARK( "snapshot plus result-cache construction" ) {
                const crafting_inventory_snapshot snapshot( index, inv );
                const crafting_requirement_result_cache cache( index, snapshot );
                return cache.evaluate( target_recipe, menu_mode::normal );
            };
        }
    }
}

// Hidden end-to-end benchmark for the expensive path behind opening a
// populated crafting category.  Unlike the synthetic inventory benchmark
// above, this starts with concrete carried, map, and vehicle items and calls
// the same inventory assembly and recipe-list helper used by the menu.
TEST_CASE( "crafting_category_pipeline_benchmark",
           "[.][crafting_category_pipeline][benchmark]" )
{
    constexpr int carried_item_count = 40;
    constexpr int map_item_count = 5000;
    constexpr int vehicle_item_target = 200;
    constexpr int repeated_samples = 3;

    recipe_ui_state_guard ui_state_guard;
    Character &guy = setup_matrix_character();
    guy.set_all_skills( 10 );
    map &here = get_map();

    for( int i = 0; i < carried_item_count; ++i ) {
        guy.i_add( item( i % 2 == 0 ? itype_stick : itype_rock ) );
    }

    const std::array<tripoint_bub_ms, 4> clutter_tiles = { {
            guy.pos_bub(),
            guy.pos_bub() + tripoint::north,
            guy.pos_bub() + tripoint::south,
            guy.pos_bub() + tripoint::west
        }
    };
    for( int i = 0; i < map_item_count; ++i ) {
        item clutter;
        switch( i % 4 ) {
            case 0:
                clutter = item( itype_stick );
                clutter.is_favorite = i % 17 == 0;
                break;
            case 1:
                clutter = item( itype_rock );
                break;
            case 2:
                clutter = item( itype_fat );
                if( i % 19 == 0 ) {
                    clutter.set_rot( 1000_hours );
                }
                break;
            default:
                clutter = item( itype_2x4 );
                if( i % 23 == 0 ) {
                    clutter.set_flag( flag_ITEM_BROKEN );
                }
                break;
        }
        here.add_item( clutter_tiles[i % clutter_tiles.size()], clutter );
    }

    const tripoint_bub_ms vehicle_pos = guy.pos_bub() + tripoint::east;
    REQUIRE( here.add_vehicle( vehicle_prototype_test_shopping_cart, vehicle_pos,
                               0_degrees, 0, veh_spawn_status::UNDAMAGED ) );
    std::optional<vpart_reference> cargo = here.veh_at( here.get_abs( vehicle_pos ) ).cargo();
    REQUIRE( cargo );
    int vehicle_item_count = 0;
    for( int i = 0; i < vehicle_item_target; ++i ) {
        item cargo_item( i % 2 == 0 ? itype_stick : itype_rock );
        if( !cargo->vehicle().add_item( here, cargo->part(), cargo_item ) ) {
            break;
        }
        ++vehicle_item_count;
    }
    REQUIRE( vehicle_item_count > 0 );

    std::size_t timing_sink = 0;
    const double inventory_assembly_ms = median_milliseconds( [&]() {
        guy.invalidate_crafting_inventory();
        timing_sink += guy.crafting_inventory().size();
    }, repeated_samples );

    // test_mode deliberately rebuilds the available-recipe subset on every
    // call.  Keep the crafting inventory warm here so this line isolates
    // recipe discovery rather than measuring inventory assembly twice.
    const double recipe_discovery_ms = median_milliseconds( [&]() {
        timing_sink += guy.get_group_available_recipes().size();
    }, repeated_samples );
    recipe_subset &available_recipes = guy.get_group_available_recipes();
    REQUIRE( available_recipes.size() > 0 );

    std::vector<const recipe *> picking = available_recipes.in_category( crafting_category_armor );
    const bool used_armor_category = !picking.empty();
    if( picking.empty() ) {
        picking.assign( available_recipes.begin(), available_recipes.end() );
    }
    REQUIRE( picking.size() > 0 );

    std::vector<const recipe *> nested_picking = picking;
    std::size_t nested_root_count = std::count_if( nested_picking.begin(), nested_picking.end(),
    []( const recipe * rec ) {
        return rec->is_nested();
    } );
    bool added_nested_probe = false;
    if( nested_root_count == 0 ) {
        for( const recipe *rec : available_recipes ) {
            if( rec->is_nested() ) {
                nested_picking.push_back( rec );
                nested_root_count = 1;
                added_nested_probe = true;
                break;
            }
        }
    }

    const crafting_requirement_index &index = recipe_dict.requirement_index();
    const inventory &assembled_inventory = guy.crafting_inventory();
    CHECK( assembled_inventory.amount_of( itype_stick ) > 0 );
    CHECK( assembled_inventory.amount_of( itype_rock ) > 0 );

    const double snapshot_ms = median_milliseconds( [&]() {
        const crafting_inventory_snapshot snapshot( index, assembled_inventory );
        timing_sink += snapshot.saturated_fact_count();
    }, repeated_samples );
    const crafting_inventory_snapshot prepared_snapshot( index, assembled_inventory );

    const double result_cache_ms = median_milliseconds( [&]() {
        const crafting_requirement_result_cache cache( index, prepared_snapshot );
        timing_sink += cache.cached_recipe_count();
    }, repeated_samples );
    const crafting_requirement_result_cache requirement_cache( index, prepared_snapshot );

    std::array<std::size_t, matrix_menu_count> graph_satisfied = {};
    std::array<std::size_t, matrix_menu_count> graph_unsatisfied = {};
    std::array<std::size_t, matrix_menu_count> graph_unknown = {};
    std::size_t unsupported_recipe_count = 0;
    std::size_t nested_bypass_count = 0;
    std::size_t tool_charge_bypass_count = 0;
    std::size_t apparent_check_candidates = 0;
    std::size_t apparent_legacy_fallback_candidates = 0;
    for( const recipe *rec : picking ) {
        const crafting_recipe_support *support = index.support_for( rec->ident() );
        if( support == nullptr || support->state == recipe_support_state::unsupported ) {
            ++unsupported_recipe_count;
        }
        if( rec->is_nested() ) {
            ++nested_bypass_count;
        }
        if( recipe_plan_has_tool_charges( index, rec->ident() ) ) {
            ++tool_charge_bypass_count;
        }
        for( std::size_t mode = 0; mode < matrix_menu_count; ++mode ) {
            switch( requirement_cache.evaluate( rec->ident(), matrix_menus[mode] ) ) {
                case crafting_requirement_result::satisfied:
                    ++graph_satisfied[mode];
                    break;
                case crafting_requirement_result::unsatisfied:
                    ++graph_unsatisfied[mode];
                    break;
                case crafting_requirement_result::unknown:
                    ++graph_unknown[mode];
                    break;
            }
        }
        if( !rec->is_nested() &&
            requirement_cache.evaluate( rec->ident(), menu_mode::normal ) ==
            crafting_requirement_result::unsatisfied &&
            rec->character_has_required_proficiencies( guy ) &&
            rec->character_meets_requirements( guy ) ) {
            ++apparent_check_candidates;
            if( requirement_cache.apparent_craftability_needs_legacy_check( rec->ident() ) ) {
                ++apparent_legacy_fallback_candidates;
            }
        }
    }

    std::map<const recipe *, availability> legacy_availability;
    const recipe_list_data legacy_list = build_recipe_list(
            picking, false, false, guy, false, nullptr, false, false,
            legacy_availability, available_recipes, nullptr );
    std::map<const recipe *, availability> cached_availability;
    const recipe_list_data cached_list = build_recipe_list(
            picking, false, false, guy, false, nullptr, false, false,
            cached_availability, available_recipes, &requirement_cache );

    REQUIRE( cached_list.entries.size() == legacy_list.entries.size() );
    REQUIRE( cached_list.available.size() == legacy_list.available.size() );
    for( std::size_t i = 0; i < legacy_list.entries.size(); ++i ) {
        CAPTURE( i, legacy_list.entries[i]->ident(), cached_list.entries[i]->ident() );
        CHECK( cached_list.entries[i]->ident() == legacy_list.entries[i]->ident() );
        CHECK( cached_list.available[i].can_craft_recipe ==
               legacy_list.available[i].can_craft_recipe );
        CHECK( cached_list.available[i].would_use_rotten ==
               legacy_list.available[i].would_use_rotten );
        CHECK( cached_list.available[i].would_use_favorite ==
               legacy_list.available[i].would_use_favorite );
        CHECK( cached_list.available[i].is_apparently_craftable() ==
               legacy_list.available[i].is_apparently_craftable() );
        CHECK( cached_list.available[i].color() == legacy_list.available[i].color() );
    }

    const double legacy_cold_sorted_ms = median_milliseconds( [&]() {
        std::map<const recipe *, availability> cold_availability;
        const recipe_list_data result = build_recipe_list(
                                            picking, false, false, guy, false, nullptr, false, false,
                                            cold_availability, available_recipes, nullptr );
        timing_sink += result.entries.size();
    }, 1 );
    const double cached_cold_availability_ms = median_milliseconds( [&]() {
        std::map<const recipe *, availability> cold_availability;
        const recipe_list_data result = build_recipe_list(
                                            picking, false, true, guy, false, nullptr, false, false,
                                            cold_availability, available_recipes, &requirement_cache );
        timing_sink += result.entries.size();
    }, repeated_samples );
    const double cached_cold_sorted_ms = median_milliseconds( [&]() {
        std::map<const recipe *, availability> cold_availability;
        const recipe_list_data result = build_recipe_list(
                                            picking, false, false, guy, false, nullptr, false, false,
                                            cold_availability, available_recipes, &requirement_cache );
        timing_sink += result.entries.size();
    }, repeated_samples );
    const double warm_unsorted_ms = median_milliseconds( [&]() {
        const recipe_list_data result = build_recipe_list(
                                            picking, false, true, guy, false, nullptr, false, false,
                                            cached_availability, available_recipes, &requirement_cache );
        timing_sink += result.entries.size();
    }, repeated_samples );
    const double warm_sorted_ms = median_milliseconds( [&]() {
        const recipe_list_data result = build_recipe_list(
                                            picking, false, false, guy, false, nullptr, false, false,
                                            cached_availability, available_recipes, &requirement_cache );
        timing_sink += result.entries.size();
    }, repeated_samples );

    double warm_expanded_ms = 0.0;
    std::size_t expanded_entry_count = nested_picking.size();
    if( nested_root_count > 0 ) {
        for( const recipe *rec : nested_picking ) {
            if( rec->is_nested() ) {
                uistate.expanded_recipes.insert( rec->ident() );
            }
        }
        std::map<const recipe *, availability> expanded_availability = cached_availability;
        const recipe_list_data prepared_expanded = build_recipe_list(
                    nested_picking, false, false, guy, false, nullptr, false, false,
                    expanded_availability, available_recipes, &requirement_cache );
        expanded_entry_count = prepared_expanded.entries.size();
        warm_expanded_ms = median_milliseconds( [&]() {
            const recipe_list_data result = build_recipe_list(
                                                nested_picking, false, false, guy, false, nullptr, false, false,
                                                expanded_availability, available_recipes, &requirement_cache );
            timing_sink += result.entries.size();
        }, repeated_samples );
    }

    CHECK( timing_sink > 0 );
    CHECK( graph_satisfied[0] + graph_unsatisfied[0] + graph_unknown[0] == picking.size() );
    WARN( "crafting category pipeline benchmark (median unless noted)\n"
          << "source items: carried=" << carried_item_count
          << " map=" << map_item_count << " vehicle=" << vehicle_item_count << "\n"
          << "recipe source: " << ( used_armor_category ? "CC_ARMOR" : "all available fallback" )
          << " picking=" << picking.size() << " available=" << available_recipes.size() << "\n"
          << "inventory assembly: " << inventory_assembly_ms << " ms\n"
          << "available-recipe discovery (warm inventory): " << recipe_discovery_ms << " ms\n"
          << "snapshot construction: " << snapshot_ms << " ms"
          << " facts=" << prepared_snapshot.fact_count()
          << " saturated=" << prepared_snapshot.saturated_fact_count()
          << " inexact=" << prepared_snapshot.inexact_fact_count()
          << " unsupported=" << prepared_snapshot.unsupported_fact_count() << "\n"
          << "result-cache construction: " << result_cache_ms << " ms"
          << " recipes=" << requirement_cache.cached_recipe_count() << "\n"
          << "graph normal satisfied/unsatisfied/unknown: "
          << graph_satisfied[0] << "/" << graph_unsatisfied[0] << "/" << graph_unknown[0] << "\n"
          << "graph no-rotten satisfied/unsatisfied/unknown: "
          << graph_satisfied[1] << "/" << graph_unsatisfied[1] << "/" << graph_unknown[1] << "\n"
          << "graph no-favorite satisfied/unsatisfied/unknown: "
          << graph_satisfied[2] << "/" << graph_unsatisfied[2] << "/" << graph_unknown[2] << "\n"
          << "bypass indicators: unsupported_recipe=" << unsupported_recipe_count
          << " nested=" << nested_bypass_count
          << " tool_charges=" << tool_charge_bypass_count
          << " apparent_check_candidate=" << apparent_check_candidates
          << " apparent_legacy_fallback_candidate=" << apparent_legacy_fallback_candidates << "\n"
          << "legacy cold availability+sort (single sample): " << legacy_cold_sorted_ms << " ms\n"
          << "cached cold availability/list without sort: " << cached_cold_availability_ms << " ms\n"
          << "cached cold availability+sort: " << cached_cold_sorted_ms << " ms\n"
          << "cached warm list without sort: " << warm_unsorted_ms << " ms\n"
          << "cached warm list+sort: " << warm_sorted_ms << " ms"
          << " (sort delta " << warm_sorted_ms - warm_unsorted_ms << " ms)\n"
          << "cached warm expanded list+sort: " << warm_expanded_ms << " ms"
          << " roots=" << nested_root_count << " entries=" << expanded_entry_count
          << " added_probe=" << added_nested_probe );
}
