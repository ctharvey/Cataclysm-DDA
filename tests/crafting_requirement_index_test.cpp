#include <array>
#include <climits>
#include <map>
#include <string>
#include <vector>

#include "crafting_requirement_index.h"
#include "type_id.h"
#include "cata_catch.h"
#include "item.h"
#include "itype.h"
#include "recipe.h"
#include "recipe_dictionary.h"
#include "requirements.h"

using fact_kind = crafting_requirement_fact_kind;
using menu_mode = menu_filter_mode;
using group_kind = requirement_group_kind;

using profiles_array = std::array<int, menu_filter_mode_count>;

static recipe_id rid( const char *s )
{
    return recipe_id( std::string( s ) );
}

static profiles_array make_profiles( int normal, int no_rotten, int no_favorite )
{
    return { { normal, no_rotten, no_favorite } };
}

static profiles_array uniform_profiles( int profile )
{
    return make_profiles( profile, profile, profile );
}

static crafting_requirement_fact_key make_key(
    const std::string &id, fact_kind kind = fact_kind::component_units,
    int level = 0, int filter_profile = recipe_filter_none )
{
    crafting_requirement_fact_key key;
    key.kind = kind;
    key.id = id;
    key.level = level;
    key.filter_profile = filter_profile;
    return key;
}

static crafting_requirement_option make_option(
    const std::string &id, int threshold,
    fact_kind kind = fact_kind::component_units, int level = 0 )
{
    crafting_requirement_option opt;
    opt.kind = kind;
    opt.id = id;
    opt.level = level;
    opt.threshold = threshold;
    return opt;
}

static crafting_requirement_group make_group(
    group_kind kind, const std::vector<crafting_requirement_option> &opts )
{
    crafting_requirement_group grp;
    grp.kind = kind;
    grp.options = opts;
    return grp;
}

TEST_CASE( "crafting_requirement_fact_key_equality_and_ordering", "[crafting]" )
{
    const crafting_requirement_fact_key base = make_key( "steel" );
    crafting_requirement_fact_key same = make_key( "steel" );
    CHECK( base == same );
    CHECK_FALSE( base != same );
    CHECK_FALSE( base < same );
    CHECK_FALSE( same < base );

    // Each field participates in identity.
    CHECK( base != make_key( "steel", fact_kind::component_charges ) );
    CHECK( base != make_key( "iron" ) );
    CHECK( base != make_key( "steel", fact_kind::component_units, 1 ) );
    CHECK( base != make_key( "steel", fact_kind::component_units, 0,
                             recipe_filter_rotten_forbidden ) );

    // Strict lexicographic ordering: kind, then id, then level, then profile.
    CHECK( make_key( "a", fact_kind::component_units ) <
           make_key( "z", fact_kind::component_units ) );
    CHECK( make_key( "steel", fact_kind::component_units ) <
           make_key( "steel", fact_kind::component_charges ) );
    CHECK( make_key( "steel", fact_kind::component_units, 0 ) <
           make_key( "steel", fact_kind::component_units, 1 ) );
    CHECK( make_key( "steel", fact_kind::component_units, 1, 0 ) <
           make_key( "steel", fact_kind::component_units, 1,
                     recipe_filter_full_magazine_required ) );

    // Usable as a std::map key with strict weak ordering.
    std::map<crafting_requirement_fact_key, int> m;
    m[make_key( "steel" )] = 1;
    m[make_key( "steel" )] = 2;
    CHECK( m.size() == 1 );
    CHECK( m[make_key( "steel" )] == 2 );
}

TEST_CASE( "crafting_requirement_index_threshold_validation", "[crafting]" )
{
    crafting_requirement_index index;
    const crafting_requirement_fact_key steel = make_key( "steel" );

    crafting_requirement_plan plan;
    plan.alternatives.push_back( { make_group( group_kind::component,
                                  { make_option( "steel", 5 ) } ) } );
    CHECK( index.add_recipe( rid( "r1" ), plan, uniform_profiles( recipe_filter_none ) ) );
    // A plan with zero alternatives is malformed for a supported recipe.
    std::string err;
    CHECK_FALSE( index.add_recipe( rid( "r2" ), crafting_requirement_plan(),
                                   uniform_profiles( recipe_filter_none ),
                                   &err ) );
    CHECK_FALSE( err.empty() );

    index.finalize();
    CHECK( index.is_finalized() );

    // Mutations after finalization are rejected.
    crafting_requirement_plan more;
    more.alternatives.push_back( { make_group( group_kind::component,
                                    { make_option( "iron", 7 ) } ) } );
    CHECK_FALSE( index.add_recipe( rid( "r3" ), more,
                                   uniform_profiles( recipe_filter_none ), &err ) );
    CHECK_FALSE( index.add_unsupported_recipe( rid( "r4" ), "reason", &err ) );
    CHECK( index.thresholds_for( steel ) == std::vector<int> { 5 } );
}

TEST_CASE( "crafting_requirement_index_finalize_sorts_and_deduplicates", "[crafting]" )
{
    crafting_requirement_index index;
    const crafting_requirement_fact_key steel = make_key( "steel" );

    // Two recipes sharing one fact key, with a duplicated threshold.
    crafting_requirement_plan first;
    first.alternatives.push_back( { make_group( group_kind::component,
                                     { make_option( "steel", 10 ) } ) } );
    crafting_requirement_plan second;
    second.alternatives.push_back( { make_group( group_kind::component,
                                      { make_option( "steel", 10 ),
                                        make_option( "steel", 2 )
                                      } ) } );
    CHECK( index.add_recipe( rid( "r1" ), first, uniform_profiles( recipe_filter_none ) ) );
    CHECK( index.add_recipe( rid( "r2" ), second, uniform_profiles( recipe_filter_none ) ) );
    index.finalize();
    // Finalizing again is idempotent.
    index.finalize();

    CHECK( index.thresholds_for( steel ) == std::vector<int> { 2, 10 } );
    CHECK( index.maximum_for( steel ) == 10 );
}

TEST_CASE( "crafting_requirement_index_plan_and_or_shape", "[crafting]" )
{
    crafting_requirement_index index;
    // OR of two alternatives:
    //   alt 0: (steel >= 2) AND (hammer >= 1)  -- two AND groups
    //   alt 1: (steel >= 5 OR iron >= 3)       -- one component group, OR options
    crafting_requirement_plan plan;
    plan.alternatives.emplace_back();
    plan.alternatives.back().push_back(
        make_group( group_kind::component, { make_option( "steel", 2 ) } ) );
    plan.alternatives.back().push_back(
        make_group( group_kind::tool,
        { make_option( "hammer", 1, fact_kind::tool_instances ) } ) );
    plan.alternatives.emplace_back();
    plan.alternatives.back().push_back(
        make_group( group_kind::component,
        { make_option( "steel", 5 ), make_option( "iron", 3 ) } ) );
    CHECK( index.add_recipe( rid( "or_recipe" ), plan,
                             uniform_profiles( recipe_filter_none ) ) );
    index.finalize();

    const std::vector<int> steel_normal = index.thresholds_for(
            make_key( "steel" ) );
    CHECK( steel_normal == std::vector<int> { 2, 5 } );
    CHECK( index.maximum_for( make_key( "steel" ) ) == 5 );
    CHECK( index.thresholds_for( make_key( "iron" ) ) == std::vector<int> { 3 } );
    CHECK( index.thresholds_for(
               make_key( "hammer", fact_kind::tool_instances ) ) ==
           std::vector<int> { 1 } );

    // The recipe appears in the edge lists of every fact it touches.
    CHECK_FALSE( index.edges_for( make_key( "steel" ) ).empty() );
    CHECK_FALSE( index.edges_for( make_key( "iron" ) ).empty() );
    CHECK_FALSE( index.edges_for(
                     make_key( "hammer", fact_kind::tool_instances ) ).empty() );
}

TEST_CASE( "crafting_requirement_index_effective_profiles_split_facts", "[crafting]" )
{
    crafting_requirement_index index;
    crafting_requirement_plan plan;
    plan.alternatives.push_back( { make_group( group_kind::component,
                                    { make_option( "meat", 4 ) } ) } );
    // Distinct effective profile per menu mode.
    const profiles_array profiles = make_profiles(
                                        recipe_filter_none,
                                        recipe_filter_rotten_forbidden,
                                        recipe_filter_favorite_forbidden );
    CHECK( index.add_recipe( rid( "meat_recipe" ), plan, profiles ) );
    index.finalize();

    // One fact key per menu mode, each carrying that mode's effective
    // profile; the modes do not bleed into each other.
    const crafting_requirement_fact_key normal = make_key( "meat",
            fact_kind::component_units, 0, recipe_filter_none );
    const crafting_requirement_fact_key no_rotten = make_key( "meat",
            fact_kind::component_units, 0, recipe_filter_rotten_forbidden );
    const crafting_requirement_fact_key no_favorite = make_key( "meat",
            fact_kind::component_units, 0, recipe_filter_favorite_forbidden );
    for( const crafting_requirement_fact_key &key : { normal, no_rotten,
        no_favorite } ) {
        CHECK( index.thresholds_for( key ) == std::vector<int> { 4 } );
        CHECK( index.maximum_for( key ) == 4 );
        REQUIRE( index.edges_for( key ).size() == 1 );
    }
    CHECK( index.effective_profile_for( rid( "meat_recipe" ),
                                        menu_mode::normal ) ==
           recipe_filter_none );
    CHECK( index.effective_profile_for( rid( "meat_recipe" ),
                                        menu_mode::no_rotten ) ==
           recipe_filter_rotten_forbidden );
    CHECK( index.effective_profile_for( rid( "meat_recipe" ),
                                        menu_mode::no_favorite ) ==
           recipe_filter_favorite_forbidden );
    CHECK( index.effective_profile_for( rid( "unknown_recipe" ),
                                        menu_mode::normal ) ==
           recipe_filter_none );

    // A tally on one profile fact does not satisfy a different profile.
    crafting_requirement_tally tally( index );
    CHECK( tally.add( normal, 4 ) == std::vector<int> { 4 } );
    CHECK_FALSE( tally.meets( no_rotten, 4 ) );
    CHECK( tally.count_for( no_rotten ) == 0 );
}

TEST_CASE( "crafting_requirement_index_frozen_magazine_rules_do_not_share_facts", "[crafting]" )
{
    crafting_requirement_index index;
    crafting_requirement_plan plan;
    plan.alternatives.push_back( { make_group( group_kind::component,
                                    { make_option( "steel", 3 ) } ) } );
    const int frozen = recipe_filter_frozen_forbidden;
    const int magazine = recipe_filter_full_magazine_required;
    // Both recipes use the normal menu mode but differ in extra rules.
    CHECK( index.add_recipe( rid( "frozen_recipe" ), plan,
                             make_profiles( frozen, frozen, frozen ) ) );
    CHECK( index.add_recipe( rid( "magazine_recipe" ), plan,
                             make_profiles( magazine, magazine, magazine ) ) );
    index.finalize();

    const crafting_requirement_fact_key steel_frozen = make_key( "steel",
            fact_kind::component_units, 0, frozen );
    const crafting_requirement_fact_key steel_magazine = make_key( "steel",
            fact_kind::component_units, 0, magazine );
    CHECK( index.thresholds_for( steel_frozen ) == std::vector<int> { 3 } );
    CHECK( index.thresholds_for( steel_magazine ) == std::vector<int> { 3 } );
    CHECK( index.thresholds_for( make_key( "steel" ) ).empty() );
    // All three menu modes share the frozen profile, so all three register
    // an edge onto the same fact key for the frozen recipe.
    REQUIRE( index.edges_for( steel_frozen ).size() == 3 );
    CHECK( index.edges_for( steel_frozen )[0].menu == menu_mode::normal );
    CHECK( index.edges_for( steel_frozen )[1].menu == menu_mode::no_rotten );
    CHECK( index.edges_for( steel_frozen )[2].menu == menu_mode::no_favorite );
    for( const crafting_requirement_edge &edge : index.edges_for( steel_frozen ) ) {
        CHECK( edge.id == rid( "frozen_recipe" ) );
        CHECK( edge.threshold == 3 );
    }
    REQUIRE( index.edges_for( steel_magazine ).size() == 3 );
    for( const crafting_requirement_edge &edge : index.edges_for( steel_magazine ) ) {
        CHECK( edge.id == rid( "magazine_recipe" ) );
        CHECK( edge.threshold == 3 );
    }
}

TEST_CASE( "crafting_requirement_index_equal_profiles_dedup_facts_keep_edges", "[crafting]" )
{
    crafting_requirement_index index;
    crafting_requirement_plan plan;
    plan.alternatives.push_back( { make_group( group_kind::component,
                                    { make_option( "steel", 6 ) } ) } );
    // All three modes share the same effective profile, so all three modes
    // map onto one fact key.
    CHECK( index.add_recipe( rid( "steel_recipe" ), plan, uniform_profiles(
                                 recipe_filter_rotten_forbidden ) ) );
    index.finalize();

    const crafting_requirement_fact_key steel = make_key( "steel",
            fact_kind::component_units, 0,
            recipe_filter_rotten_forbidden );
    CHECK( index.thresholds_for( steel ) == std::vector<int> { 6 } );
    // One edge per menu mode survives even though the fact is shared.
    REQUIRE( index.edges_for( steel ).size() == 3 );
    CHECK( index.edges_for( steel )[0].menu == menu_mode::normal );
    CHECK( index.edges_for( steel )[1].menu == menu_mode::no_rotten );
    CHECK( index.edges_for( steel )[2].menu == menu_mode::no_favorite );
    for( const crafting_requirement_edge &edge : index.edges_for( steel ) ) {
        CHECK( edge.id == rid( "steel_recipe" ) );
    }
}

TEST_CASE( "crafting_requirement_index_quality_shares_unfiltered_fact", "[crafting]" )
{
    crafting_requirement_index index;
    crafting_requirement_plan plan;
    plan.alternatives.push_back( { make_group( group_kind::quality,
                                    { make_option( "CUT", 2,
                                      fact_kind::quality_providers, 1 ) } ) } );
    // Even with nonzero effective profiles, quality facts use profile 0.
    CHECK( index.add_recipe( rid( "cut_recipe" ), plan, make_profiles(
                                 recipe_filter_rotten_forbidden,
                                 recipe_filter_frozen_forbidden,
                                 recipe_filter_full_magazine_required ) ) );
    index.finalize();

    const crafting_requirement_fact_key unfiltered = make_key( "CUT",
            fact_kind::quality_providers, 1, recipe_filter_none );
    CHECK( index.thresholds_for( unfiltered ) == std::vector<int> { 2 } );
    // Quality derives only one fact key, so only one edge exists.
    CHECK( index.edges_for( unfiltered ).size() == 1 );
    CHECK( index.edges_for( make_key( "CUT", fact_kind::quality_providers, 1,
                                      recipe_filter_rotten_forbidden ) ).empty() );
    CHECK( index.edges_for( make_key( "CUT", fact_kind::quality_providers, 1,
                                      recipe_filter_frozen_forbidden ) ).empty() );
}

TEST_CASE( "crafting_requirement_index_reverse_edge_coordinates", "[crafting]" )
{
    crafting_requirement_index index;
    crafting_requirement_plan plan;
    // alt 0: group 0 (component: steel OR iron), group 1 (tool: hammer)
    // alt 1: group 0 (quality: CUT)
    plan.alternatives.emplace_back();
    plan.alternatives.back().push_back(
        make_group( group_kind::component,
        { make_option( "steel", 2 ), make_option( "iron", 3 ) } ) );
    plan.alternatives.back().push_back(
        make_group( group_kind::tool,
        { make_option( "hammer", 1, fact_kind::tool_charges ) } ) );
    plan.alternatives.emplace_back();
    plan.alternatives.back().push_back(
        make_group( group_kind::quality,
        { make_option( "CUT", 2, fact_kind::quality_providers, 1 ) } ) );
    const recipe_id recipe = rid( "complex_recipe" );
    CHECK( index.add_recipe( recipe, plan, uniform_profiles( recipe_filter_none ) ) );
    index.finalize();

    // Item/tool options derive one edge per menu mode onto the shared
    // profile-0 key; all three edges carry identical coordinates apart from
    // the menu mode.
    const std::vector<crafting_requirement_edge> steel_edges =
        index.edges_for( make_key( "steel" ) );
    REQUIRE( steel_edges.size() == 3 );
    CHECK( steel_edges[0].menu == menu_mode::normal );
    CHECK( steel_edges[1].menu == menu_mode::no_rotten );
    CHECK( steel_edges[2].menu == menu_mode::no_favorite );
    for( const crafting_requirement_edge &edge : steel_edges ) {
        CHECK( edge.id == recipe );
        CHECK( edge.alternative == 0 );
        CHECK( edge.group_kind == group_kind::component );
        CHECK( edge.group == 0 );
        CHECK( edge.option == 0 );
        CHECK( edge.threshold == 2 );
    }

    const std::vector<crafting_requirement_edge> iron_edges =
        index.edges_for( make_key( "iron" ) );
    REQUIRE( iron_edges.size() == 3 );
    for( const crafting_requirement_edge &edge : iron_edges ) {
        CHECK( edge.id == recipe );
        CHECK( edge.alternative == 0 );
        CHECK( edge.group == 0 );
        CHECK( edge.option == 1 );
        CHECK( edge.threshold == 3 );
    }

    const std::vector<crafting_requirement_edge> hammer_edges =
        index.edges_for( make_key( "hammer", fact_kind::tool_charges ) );
    REQUIRE( hammer_edges.size() == 3 );
    for( const crafting_requirement_edge &edge : hammer_edges ) {
        CHECK( edge.id == recipe );
        CHECK( edge.group_kind == group_kind::tool );
        CHECK( edge.group == 1 );
        CHECK( edge.option == 0 );
        CHECK( edge.threshold == 1 );
    }

    // Quality option lives in alternative 1 with its single profile-0 fact,
    // so only one edge exists.
    const std::vector<crafting_requirement_edge> cut_edges =
        index.edges_for( make_key( "CUT", fact_kind::quality_providers, 1 ) );
    REQUIRE( cut_edges.size() == 1 );
    CHECK( cut_edges[0].id == recipe );
    CHECK( cut_edges[0].alternative == 1 );
    CHECK( cut_edges[0].group_kind == group_kind::quality );
    CHECK( cut_edges[0].group == 0 );
    CHECK( cut_edges[0].option == 0 );
    CHECK( cut_edges[0].threshold == 2 );
}

TEST_CASE( "crafting_requirement_index_empty_alternative_is_valid", "[crafting]" )
{
    crafting_requirement_index index;
    // alt 0: a real component requirement; alt 1: trivially satisfied.
    crafting_requirement_plan plan;
    plan.alternatives.emplace_back();
    plan.alternatives.back().push_back(
        make_group( group_kind::component, { make_option( "steel", 2 ) } ) );
    plan.alternatives.emplace_back();
    CHECK( index.add_recipe( rid( "r1" ), plan, uniform_profiles( recipe_filter_none ) ) );
    CHECK( index.has_recipe( rid( "r1" ) ) );
    index.finalize();

    // The requirement still registers; the empty alternative adds nothing.
    CHECK( index.thresholds_for( make_key( "steel" ) ) ==
           std::vector<int> { 2 } );
    CHECK( index.recipe_count() == 1 );
}

TEST_CASE( "crafting_requirement_index_unsupported_recipes", "[crafting]" )
{
    crafting_requirement_index index;
    CHECK( index.add_unsupported_recipe( rid( "bad_recipe" ),
                                         "no matching components" ) );
    CHECK( index.recipe_count() == 1 );
    CHECK( index.has_recipe( rid( "bad_recipe" ) ) );
    CHECK_FALSE( index.has_recipe( rid( "missing_recipe" ) ) );

    // Supported lookup returns the record; unknown ids are unambiguously
    // distinct from supported recipes.
    const crafting_recipe_support *support = index.support_for( rid( "bad_recipe" ) );
    REQUIRE( support != nullptr );
    CHECK( support->state == recipe_support_state::unsupported );
    CHECK( support->reason == "no matching components" );
    CHECK( index.support_for( rid( "missing_recipe" ) ) == nullptr );

    // Unsupported recipes retain no plan; unknown ids have none either.
    CHECK( index.plan_for( rid( "bad_recipe" ) ) == nullptr );
    CHECK( index.plan_for( rid( "missing_recipe" ) ) == nullptr );

    index.finalize();
    // Unsupported recipes contribute no facts and no edges.
    CHECK( index.thresholds_for( make_key( "anything" ) ).empty() );
    CHECK( index.edges_for( make_key( "anything" ) ).empty() );
}

TEST_CASE( "crafting_requirement_index_malformed_plan_atomic_rejection", "[crafting]" )
{
    crafting_requirement_index index;
    // Pre-existing valid state that must survive every rejection untouched.
    crafting_requirement_plan good;
    good.alternatives.push_back( { make_group( group_kind::component,
                                    { make_option( "steel", 5 ) } ) } );
    CHECK( index.add_recipe( rid( "good_recipe" ), good,
                             uniform_profiles( recipe_filter_none ) ) );
    const std::size_t before_count = index.recipe_count();

    auto rejection_leaves_index_intact = [&]( const crafting_requirement_plan &bad,
    const char *label ) {
        INFO( label );
        std::string err;
        CHECK_FALSE( index.add_recipe( rid( "rejected_recipe" ), bad,
                                       uniform_profiles( recipe_filter_none ),
                                       &err ) );
        CHECK_FALSE( err.empty() );
        CHECK( index.recipe_count() == before_count );
        CHECK_FALSE( index.has_recipe( rid( "rejected_recipe" ) ) );
        // Raw thresholds are deduplicated only by finalize(); pre-finalize
        // accumulation may repeat a threshold, so only check the complete
        // set of registered values via the public API.
        for( int t : index.thresholds_for( make_key( "steel" ) ) ) {
            CHECK( t == 5 );
        }
        CHECK_FALSE( index.thresholds_for( make_key( "steel" ) ).empty() );
        CHECK( index.thresholds_for( make_key( "extra" ) ).empty() );
        CHECK( index.edges_for( make_key( "extra" ) ).empty() );
    };

    // Nonpositive threshold in a later alternative: nothing from the earlier
    // (valid) alternatives may be committed.
    crafting_requirement_plan bad_threshold;
    bad_threshold.alternatives.push_back( { make_group( group_kind::component,
                                            { make_option( "extra", 3 ) } ) } );
    bad_threshold.alternatives.push_back( { make_group( group_kind::component,
                                           { make_option( "extra", 0 ) } ) } );
    rejection_leaves_index_intact( bad_threshold, "nonpositive threshold" );

    crafting_requirement_plan bad_negative;
    bad_negative.alternatives.push_back( { make_group( group_kind::component,
                                           { make_option( "extra", -4 ) } ) } );
    rejection_leaves_index_intact( bad_negative, "negative threshold" );

    // Option kind does not match its group kind.
    crafting_requirement_plan bad_kind;
    bad_kind.alternatives.push_back( { make_group( group_kind::tool,
                                       { make_option( "steel", 2 ) } ) } );
    rejection_leaves_index_intact( bad_kind, "mismatched group kind" );

    // Group with no options remains invalid.
    crafting_requirement_plan bad_group;
    bad_group.alternatives.push_back( { make_group( group_kind::component, {} ) } );
    rejection_leaves_index_intact( bad_group, "empty group" );

    // Duplicate recipe id, including against an unsupported record.
    CHECK( index.add_unsupported_recipe( rid( "dup_recipe" ), "just because" ) );
    crafting_requirement_plan dup;
    dup.alternatives.push_back( { make_group( group_kind::component,
                                   { make_option( "extra", 1 ) } ) } );
    std::string err;
    CHECK_FALSE( index.add_recipe( rid( "dup_recipe" ), dup,
                                   uniform_profiles( recipe_filter_none ), &err ) );
    CHECK_FALSE( err.empty() );
    CHECK_FALSE( index.add_unsupported_recipe( rid( "good_recipe" ), "again", &err ) );
    CHECK_FALSE( err.empty() );
    CHECK( index.thresholds_for( make_key( "extra" ) ).empty() );
}

TEST_CASE( "crafting_requirement_index_edges_sorted_and_deduplicated", "[crafting]" )
{
    crafting_requirement_index index;
    // One recipe registers multiple steel thresholds under one profile.
    crafting_requirement_plan plan;
    plan.alternatives.push_back( { make_group( group_kind::component,
                                    { make_option( "steel", 9 ),
                                      make_option( "steel", 2 ),
                                      make_option( "steel", 5 )
                                    } ) } );
    CHECK( index.add_recipe( rid( "r1" ), plan, uniform_profiles( recipe_filter_none ) ) );
    index.finalize();

    // Each of the three options registers one edge per menu mode onto the
    // shared profile-0 key: nine edges in total, sorted and deduplicated.
    const std::vector<crafting_requirement_edge> edges =
        index.edges_for( make_key( "steel" ) );
    REQUIRE( edges.size() == 9 );
    const std::array<int, 3> threshold_by_option = { { 9, 2, 5 } };
    std::array<std::array<bool, 3>, 3> seen = {};
    for( const crafting_requirement_edge &edge : edges ) {
        REQUIRE( edge.option >= 0 );
        REQUIRE( edge.option < 3 );
        const int menu = static_cast<int>( edge.menu );
        REQUIRE( menu >= 0 );
        REQUIRE( menu < 3 );
        CHECK_FALSE( seen[edge.option][menu] );
        seen[edge.option][menu] = true;
        CHECK( edge.threshold == threshold_by_option[edge.option] );
    }
    for( std::size_t i = 1; i < edges.size(); ++i ) {
        CHECK( edges[i - 1] < edges[i] );
        CHECK_FALSE( edges[i - 1] == edges[i] );
    }
}

TEST_CASE( "crafting_requirement_index_retained_plan_preserves_and_or_shape", "[crafting]" )
{
    crafting_requirement_index index;
    // OR of two alternatives:
    //   alt 0: (steel >= 2) AND (hammer >= 1)  -- two AND groups
    //   alt 1: (steel >= 5 OR iron >= 3)       -- one component group, OR options
    crafting_requirement_plan plan;
    plan.alternatives.emplace_back();
    plan.alternatives.back().push_back(
        make_group( group_kind::component, { make_option( "steel", 2 ) } ) );
    plan.alternatives.back().push_back(
        make_group( group_kind::tool,
        { make_option( "hammer", 1, fact_kind::tool_instances ) } ) );
    plan.alternatives.emplace_back();
    plan.alternatives.back().push_back(
        make_group( group_kind::component,
        { make_option( "steel", 5 ), make_option( "iron", 3 ) } ) );
    const recipe_id recipe = rid( "shape_recipe" );
    CHECK( index.add_recipe( recipe, plan, uniform_profiles( recipe_filter_none ) ) );

    // The full plan is retained before and after finalize(), preserving the
    // AND/OR shape exactly as supplied.
    const crafting_requirement_plan *retained = index.plan_for( recipe );
    REQUIRE( retained != nullptr );
    REQUIRE( retained->alternatives.size() == 2 );

    REQUIRE( retained->alternatives[0].size() == 2 );
    CHECK( retained->alternatives[0][0].kind == group_kind::component );
    REQUIRE( retained->alternatives[0][0].options.size() == 1 );
    CHECK( retained->alternatives[0][0].options[0].id == "steel" );
    CHECK( retained->alternatives[0][0].options[0].threshold == 2 );
    CHECK( retained->alternatives[0][1].kind == group_kind::tool );
    REQUIRE( retained->alternatives[0][1].options.size() == 1 );
    CHECK( retained->alternatives[0][1].options[0].id == "hammer" );
    CHECK( retained->alternatives[0][1].options[0].kind ==
           fact_kind::tool_instances );
    CHECK( retained->alternatives[0][1].options[0].threshold == 1 );

    REQUIRE( retained->alternatives[1].size() == 1 );
    CHECK( retained->alternatives[1][0].kind == group_kind::component );
    REQUIRE( retained->alternatives[1][0].options.size() == 2 );
    CHECK( retained->alternatives[1][0].options[0].id == "steel" );
    CHECK( retained->alternatives[1][0].options[0].threshold == 5 );
    CHECK( retained->alternatives[1][0].options[1].id == "iron" );
    CHECK( retained->alternatives[1][0].options[1].threshold == 3 );

    index.finalize();
    const crafting_requirement_plan *retained_after = index.plan_for( recipe );
    REQUIRE( retained_after != nullptr );
    CHECK( retained_after->alternatives.size() == 2 );
    CHECK( retained_after->alternatives[0].size() == 2 );
    CHECK( retained_after->alternatives[1].size() == 1 );
    CHECK( retained_after->alternatives[1][0].options.size() == 2 );
}

TEST_CASE( "crafting_requirement_index_reverse_edge_recipe_ids_are_recipe_ids", "[crafting]" )
{
    crafting_requirement_index index;
    crafting_requirement_plan first;
    first.alternatives.push_back( { make_group( group_kind::component,
                                      { make_option( "steel", 2 ) } ) } );
    crafting_requirement_plan second;
    second.alternatives.push_back( { make_group( group_kind::component,
                                       { make_option( "steel", 7 ) } ) } );
    const recipe_id recipe_a = rid( "recipe_alpha" );
    const recipe_id recipe_b = rid( "recipe_beta" );
    CHECK( index.add_recipe( recipe_a, first, uniform_profiles( recipe_filter_none ) ) );
    CHECK( index.add_recipe( recipe_b, second, uniform_profiles( recipe_filter_none ) ) );
    index.finalize();

    // Reverse edges carry real recipe_id values, not integers: each recipe
    // is identified by its own id string on every edge it registered.
    const std::vector<crafting_requirement_edge> edges =
        index.edges_for( make_key( "steel" ) );
    REQUIRE( edges.size() == 6 );
    bool saw_a = false;
    bool saw_b = false;
    for( const crafting_requirement_edge &edge : edges ) {
        CHECK( ( edge.id == recipe_a || edge.id == recipe_b ) );
        saw_a = saw_a || edge.id == recipe_a;
        saw_b = saw_b || edge.id == recipe_b;
        CHECK( ( edge.id.str() == recipe_a.str() ||
                 edge.id.str() == recipe_b.str() ) );
    }
    CHECK( saw_a );
    CHECK( saw_b );
}

TEST_CASE( "crafting_requirement_index_clear_and_rebuild", "[crafting]" )
{
    crafting_requirement_index index;
    crafting_requirement_plan plan;
    plan.alternatives.push_back( { make_group( group_kind::component,
                                    { make_option( "steel", 5 ) } ) } );
    CHECK( index.add_recipe( rid( "r1" ), plan, uniform_profiles( recipe_filter_none ) ) );
    index.finalize();
    CHECK( index.is_finalized() );
    CHECK( index.maximum_for( make_key( "steel" ) ) == 5 );
    CHECK( index.fact_count() == 1 );

    index.clear();
    CHECK_FALSE( index.is_finalized() );
    CHECK( index.recipe_count() == 0 );
    CHECK_FALSE( index.has_recipe( rid( "r1" ) ) );
    CHECK( index.thresholds_for( make_key( "steel" ) ).empty() );
    CHECK( index.maximum_for( make_key( "steel" ) ) == 0 );
    // Every retained id, plan, fact, and edge is gone.
    CHECK( index.fact_count() == 0 );
    CHECK( index.fact_keys().empty() );
    CHECK( index.support_for( rid( "r1" ) ) == nullptr );
    CHECK( index.plan_for( rid( "r1" ) ) == nullptr );

    // The index can be rebuilt from scratch after clear().
    crafting_requirement_plan rebuilt;
    rebuilt.alternatives.push_back( { make_group( group_kind::component,
                                       { make_option( "steel", 5 ) } ) } );
    CHECK( index.add_recipe( rid( "r1" ), rebuilt, uniform_profiles( recipe_filter_none ) ) );
    index.finalize();
    CHECK( index.is_finalized() );
    CHECK( index.thresholds_for( make_key( "steel" ) ) == std::vector<int> { 5 } );
}

TEST_CASE( "crafting_requirement_index_missing_facts", "[crafting]" )
{
    crafting_requirement_index index;
    const crafting_requirement_fact_key iron = make_key( "iron" );

    crafting_requirement_plan plan;
    plan.alternatives.push_back( { make_group( group_kind::component,
                                    { make_option( "steel", 4 ) } ) } );
    CHECK( index.add_recipe( rid( "r1" ), plan, uniform_profiles( recipe_filter_none ) ) );
    index.finalize();

    CHECK( index.maximum_for( iron ) == 0 );
    CHECK( index.thresholds_for( iron ).empty() );
    CHECK( index.edges_for( iron ).empty() );
}

TEST_CASE( "crafting_requirement_tally_incremental_crossing", "[crafting]" )
{
    crafting_requirement_index index;
    const crafting_requirement_fact_key steel = make_key( "steel" );
    crafting_requirement_plan plan;
    plan.alternatives.push_back( { make_group( group_kind::component,
                                    { make_option( "steel", 3 ),
                                      make_option( "steel", 6 ),
                                      make_option( "steel", 9 )
                                    } ) } );
    CHECK( index.add_recipe( rid( "r1" ), plan, uniform_profiles( recipe_filter_none ) ) );
    index.finalize();

    crafting_requirement_tally tally( index );
    CHECK( tally.count_for( steel ) == 0 );
    CHECK_FALSE( tally.meets( steel, 3 ) );

    CHECK( tally.add( steel, 3 ) == std::vector<int> { 3 } );
    CHECK( tally.count_for( steel ) == 3 );
    CHECK( tally.meets( steel, 3 ) );
    CHECK_FALSE( tally.meets( steel, 6 ) );

    CHECK( tally.add( steel, 2 ) == std::vector<int> {} );
    CHECK( tally.add( steel, 1 ) == std::vector<int> { 6 } );

    // One large addition crosses the remaining threshold.
    CHECK( tally.add( steel, 100 ) == std::vector<int> { 9 } );
    CHECK( tally.count_for( steel ) == 9 );
    CHECK( tally.meets( steel, 9 ) );
}

TEST_CASE( "crafting_requirement_tally_single_add_crosses_multiple_thresholds", "[crafting]" )
{
    crafting_requirement_index index;
    const crafting_requirement_fact_key steel = make_key( "steel" );
    crafting_requirement_plan plan;
    plan.alternatives.push_back( { make_group( group_kind::component,
                                    { make_option( "steel", 9 ),
                                      make_option( "steel", 2 ),
                                      make_option( "steel", 5 )
                                    } ) } );
    CHECK( index.add_recipe( rid( "r1" ), plan, uniform_profiles( recipe_filter_none ) ) );
    index.finalize();

    crafting_requirement_tally tally( index );
    // From a zero count, one addition crosses every registered threshold,
    // reported exactly once each, in ascending order.
    CHECK( tally.add( steel, 100 ) == std::vector<int> { 2, 5, 9 } );
    CHECK( tally.count_for( steel ) == 9 );
    CHECK( tally.add( steel, 100 ) == std::vector<int> {} );
}

TEST_CASE( "crafting_requirement_tally_no_duplicate_crossings", "[crafting]" )
{
    crafting_requirement_index index;
    const crafting_requirement_fact_key steel = make_key( "steel" );
    crafting_requirement_plan plan;
    plan.alternatives.push_back( { make_group( group_kind::component,
                                    { make_option( "steel", 5 ) } ) } );
    CHECK( index.add_recipe( rid( "r1" ), plan, uniform_profiles( recipe_filter_none ) ) );
    index.finalize();

    crafting_requirement_tally tally( index );
    CHECK( tally.add( steel, 5 ) == std::vector<int> { 5 } );
    CHECK( tally.add( steel, 5 ) == std::vector<int> {} );
    CHECK( tally.add( steel, 1 ) == std::vector<int> {} );
    CHECK( tally.count_for( steel ) == 5 );
}

TEST_CASE( "crafting_requirement_tally_saturates_at_maximum", "[crafting]" )
{
    crafting_requirement_index index;
    const crafting_requirement_fact_key steel = make_key( "steel" );
    crafting_requirement_plan plan;
    plan.alternatives.push_back( { make_group( group_kind::component,
                                    { make_option( "steel", 4 ) } ) } );
    CHECK( index.add_recipe( rid( "r1" ), plan, uniform_profiles( recipe_filter_none ) ) );
    index.finalize();

    crafting_requirement_tally tally( index );
    tally.add( steel, 2 );
    tally.add( steel, 2 );
    // Capped at the maximum: further additions neither grow the count nor
    // re-report the threshold.
    tally.add( steel, 100 );
    CHECK( tally.count_for( steel ) == 4 );
    CHECK( tally.add( steel, 10 ) == std::vector<int> {} );
    CHECK( tally.count_for( steel ) == 4 );
}

TEST_CASE( "crafting_requirement_tally_ignores_nonpositive_and_unknown", "[crafting]" )
{
    crafting_requirement_index index;
    const crafting_requirement_fact_key steel = make_key( "steel" );
    const crafting_requirement_fact_key unknown = make_key( "unobtainium" );
    crafting_requirement_plan plan;
    plan.alternatives.push_back( { make_group( group_kind::component,
                                    { make_option( "steel", 5 ) } ) } );
    CHECK( index.add_recipe( rid( "r1" ), plan, uniform_profiles( recipe_filter_none ) ) );
    index.finalize();

    crafting_requirement_tally tally( index );
    CHECK( tally.add( steel, 0 ).empty() );
    CHECK( tally.add( steel, -7 ).empty() );
    CHECK( tally.count_for( steel ) == 0 );

    CHECK( tally.add( unknown, 5 ).empty() );
    CHECK( tally.count_for( unknown ) == 0 );
    CHECK_FALSE( tally.meets( unknown, 5 ) );
}

TEST_CASE( "crafting_requirement_tally_meets_requires_positive_threshold", "[crafting]" )
{
    crafting_requirement_index index;
    const crafting_requirement_fact_key steel = make_key( "steel" );
    crafting_requirement_plan plan;
    plan.alternatives.push_back( { make_group( group_kind::component,
                                    { make_option( "steel", 5 ) } ) } );
    CHECK( index.add_recipe( rid( "r1" ), plan, uniform_profiles( recipe_filter_none ) ) );
    index.finalize();

    crafting_requirement_tally tally( index );
    tally.add( steel, 5 );
    CHECK_FALSE( tally.meets( steel, 0 ) );
    CHECK_FALSE( tally.meets( steel, -1 ) );
    CHECK( tally.meets( steel, 5 ) );
    CHECK_FALSE( tally.meets( steel, 6 ) );
}

TEST_CASE( "crafting_requirement_tally_overflow_safe_near_int_max", "[crafting]" )
{
    crafting_requirement_index index;
    const crafting_requirement_fact_key steel = make_key( "steel" );
    crafting_requirement_plan plan;
    plan.alternatives.push_back( { make_group( group_kind::component,
                                    { make_option( "steel", INT_MAX / 2 ),
                                      make_option( "steel", INT_MAX )
                                    } ) } );
    CHECK( index.add_recipe( rid( "r1" ), plan, uniform_profiles( recipe_filter_none ) ) );
    index.finalize();

    crafting_requirement_tally tally( index );
    // Establish a count near INT_MAX.
    CHECK( tally.add( steel, INT_MAX / 2 ) == std::vector<int> { INT_MAX / 2 } );
    CHECK( tally.count_for( steel ) == INT_MAX / 2 );
    // Naive previous + amount would overflow; this must saturate instead and
    // report the final threshold exactly once.
    CHECK( tally.add( steel, INT_MAX ) == std::vector<int> { INT_MAX } );
    CHECK( tally.add( steel, INT_MAX ).empty() );
    CHECK( tally.count_for( steel ) == INT_MAX );
    CHECK( tally.meets( steel, INT_MAX ) );
}

// ---------------------------------------------------------------------------
// Phase 1B: loaded recipe_dictionary wiring and translation of finalized
// recipes into the capped requirement index.
// ---------------------------------------------------------------------------

namespace
{
constexpr int recipe_filter_valid_bits =
    recipe_filter_rotten_forbidden | recipe_filter_favorite_forbidden |
    recipe_filter_frozen_forbidden | recipe_filter_full_magazine_required;

// Recomputes the effective profile the same way recipe::get_component_filter
// derives its rules, for cross-checking the retained profiles.
int expected_base_profile( const recipe &r )
{
    const item result( r.result() );
    int base = recipe_filter_none;
    if( result.is_food() && !result.goes_bad() && !r.has_flag( "ALLOW_ROTTEN" ) ) {
        base |= recipe_filter_rotten_forbidden;
    }
    if( result.has_temperature() && !r.hot_result() ) {
        base |= recipe_filter_frozen_forbidden;
    }
    if( r.has_flag( "NEED_FULL_MAGAZINE" ) ) {
        base |= recipe_filter_full_magazine_required;
    }
    return base;
}
} // namespace

TEST_CASE( "requirement_index_loaded_dictionary_one_explicit_record_per_recipe", "[crafting]" )
{
    const crafting_requirement_index &index = recipe_dict.requirement_index();
    REQUIRE( index.is_finalized() );
    REQUIRE( index.recipe_count() == recipe_dict.size() );

    for( const auto &e : recipe_dict ) {
        const recipe_id &id = e.first;
        INFO( "recipe: " << id.str() );
        // IDs retained by the index are the stable recipe_id values.
        CHECK( id.str() == e.second.ident().str() );
        const crafting_recipe_support *support = index.support_for( id );
        REQUIRE( support != nullptr );

        if( support->state == recipe_support_state::unsupported ) {
            CHECK_FALSE( support->reason.empty() );
            CHECK( index.plan_for( id ) == nullptr );
            continue;
        }

        const crafting_requirement_plan *plan = index.plan_for( id );
        REQUIRE( plan != nullptr );
        REQUIRE_FALSE( plan->alternatives.empty() );

        for( std::size_t a = 0; a < plan->alternatives.size(); ++a ) {
            for( std::size_t g = 0; g < plan->alternatives[a].size(); ++g ) {
                const crafting_requirement_group &grp = plan->alternatives[a][g];
                REQUIRE_FALSE( grp.options.empty() );
                for( std::size_t o = 0; o < grp.options.size(); ++o ) {
                    const crafting_requirement_option &opt = grp.options[o];
                    REQUIRE( opt.threshold > 0 );
                    const bool is_quality = opt.kind == fact_kind::quality_providers;
                    const int modes = is_quality ? 1 : menu_filter_mode_count;
                    for( int m = 0; m < modes; ++m ) {
                        const int profile = is_quality
                                            ? recipe_filter_none
                                            : index.effective_profile_for( id,
                                                    static_cast<menu_mode>( m ) );
                        // Effective profile bits must be a valid combination.
                        REQUIRE( ( profile & ~recipe_filter_valid_bits ) == 0 );
                        crafting_requirement_fact_key key;
                        key.kind = opt.kind;
                        key.id = opt.id;
                        key.level = opt.level;
                        key.filter_profile = profile;
                        CHECK( index.maximum_for( key ) >= opt.threshold );
                        // A live reverse edge must target these exact
                        // stored-plan coordinates.
                        bool found = false;
                        for( const crafting_requirement_edge &edge :
                            index.edges_for( key ) ) {
                            if( edge.id == id &&
                                edge.alternative == static_cast<int>( a ) &&
                                edge.group_kind == grp.kind &&
                                edge.group == static_cast<int>( g ) &&
                                edge.option == static_cast<int>( o ) &&
                                edge.threshold == opt.threshold &&
                                ( is_quality ||
                                  edge.menu == static_cast<menu_mode>( m ) ) ) {
                                found = true;
                                break;
                            }
                        }
                        CHECK( found );
                    }
                }
            }
        }
    }
}

TEST_CASE( "requirement_index_loaded_dictionary_fact_table_invariants", "[crafting]" )
{
    const crafting_requirement_index &index = recipe_dict.requirement_index();
    REQUIRE( index.is_finalized() );
    REQUIRE( index.fact_count() == index.fact_keys().size() );
    CHECK( index.fact_count() > 0 );

    for( const crafting_requirement_fact_key &key : index.fact_keys() ) {
        INFO( "fact: kind=" << static_cast<int>( key.kind ) << " id=" << key.id );
        REQUIRE( ( key.filter_profile & ~recipe_filter_valid_bits ) == 0 );
        const std::vector<int> &thresholds = index.thresholds_for( key );
        CHECK_FALSE( thresholds.empty() );
        for( std::size_t i = 0; i < thresholds.size(); ++i ) {
            CHECK( thresholds[i] > 0 );
            if( i > 0 ) {
                // Sorted ascending and deduplicated.
                CHECK( thresholds[i - 1] < thresholds[i] );
            }
        }
        CHECK( index.maximum_for( key ) == thresholds.back() );

        const std::vector<crafting_requirement_edge> &edges = index.edges_for( key );
        for( std::size_t i = 1; i < edges.size(); ++i ) {
            CHECK( edges[i - 1] < edges[i] );
            CHECK_FALSE( edges[i - 1] == edges[i] );
        }
    }
}

TEST_CASE( "requirement_index_loaded_dictionary_unsupported_records", "[crafting]" )
{
    const crafting_requirement_index &index = recipe_dict.requirement_index();
    REQUIRE( index.is_finalized() );
    bool saw_unsupported = false;

    for( const auto &e : recipe_dict ) {
        const recipe &r = e.second;
        INFO( "recipe: " << e.first.str() );
        const bool should_be_unsupported =
            r.obsolete || r.is_nested() || r.is_blueprint() ||
            r.deduped_requirements().is_too_complex();
        const crafting_recipe_support *support = index.support_for( e.first );
        REQUIRE( support != nullptr );
        if( should_be_unsupported ) {
            REQUIRE( support->state == recipe_support_state::unsupported );
            CHECK_FALSE( support->reason.empty() );
            CHECK( index.plan_for( e.first ) == nullptr );
            saw_unsupported = true;
        }
    }
    // Obsolete, nested, and blueprint recipes all exist in loaded data.
    CHECK( saw_unsupported );
}

TEST_CASE( "requirement_index_loaded_dictionary_translation_normalization", "[crafting]" )
{
    const crafting_requirement_index &index = recipe_dict.requirement_index();
    REQUIRE( index.is_finalized() );

    bool saw_units = false;
    bool saw_charges = false;
    bool saw_tool_instances = false;
    bool saw_tool_charges = false;
    bool saw_quality = false;
    bool saw_multi_group_alternative = false;
    bool saw_multi_option_group = false;
    bool saw_rotten_base = false;
    bool saw_frozen_base = false;
    bool saw_magazine_base = false;

    for( const auto &e : recipe_dict ) {
        const recipe &r = e.second;
        const recipe_id &id = e.first;
        const crafting_requirement_plan *plan = index.plan_for( id );
        if( plan == nullptr ) {
            continue;
        }
        INFO( "recipe: " << id.str() );

        // Effective profiles match the component filter rules exactly.
        const int base = expected_base_profile( r );
        CHECK( index.effective_profile_for( id, menu_mode::normal ) == base );
        CHECK( index.effective_profile_for( id, menu_mode::no_rotten ) ==
               ( base | recipe_filter_rotten_forbidden ) );
        CHECK( index.effective_profile_for( id, menu_mode::no_favorite ) ==
               ( base | recipe_filter_favorite_forbidden ) );
        saw_rotten_base |= ( base & recipe_filter_rotten_forbidden ) != 0;
        saw_frozen_base |= ( base & recipe_filter_frozen_forbidden ) != 0;
        saw_magazine_base |= ( base & recipe_filter_full_magazine_required ) != 0;

        for( const crafting_requirement_alternative &alt : plan->alternatives ) {
            saw_multi_group_alternative |= alt.size() > 1;
            for( const crafting_requirement_group &grp : alt ) {
                saw_multi_option_group |= grp.options.size() > 1;
                for( const crafting_requirement_option &opt : grp.options ) {
                    const itype_id type( opt.id );
                    INFO( "option: " << opt.id << " threshold=" << opt.threshold );
                    switch( opt.kind ) {
                        case fact_kind::component_units:
                            CHECK_FALSE( item::count_by_charges( type ) );
                            saw_units = true;
                            break;
                        case fact_kind::component_charges:
                            CHECK( item::count_by_charges( type ) );
                            saw_charges = true;
                            break;
                        case fact_kind::tool_instances:
                            saw_tool_instances = true;
                            break;
                        case fact_kind::tool_charges: {
                            const itype *tool_type = item::find_type( type );
                            REQUIRE( tool_type != nullptr );
                            // Threshold is count * charge_factor, so it is a
                            // positive multiple of the tool's charge factor.
                            CHECK( opt.threshold > 0 );
                            CHECK( opt.threshold %
                                   tool_type->charge_factor() == 0 );
                            saw_tool_charges = true;
                            break;
                        }
                        case fact_kind::quality_providers:
                            CHECK( opt.level > 0 );
                            CHECK( opt.threshold > 0 );
                            saw_quality = true;
                            break;
                    }
                }
            }
        }
    }

    // Loaded data exercises every normalization path.
    CHECK( saw_units );
    CHECK( saw_charges );
    CHECK( saw_tool_instances );
    CHECK( saw_tool_charges );
    CHECK( saw_quality );
    CHECK( saw_multi_group_alternative );
    CHECK( saw_multi_option_group );
    CHECK( saw_rotten_base );
    CHECK( saw_frozen_base );
    CHECK( saw_magazine_base );
}
