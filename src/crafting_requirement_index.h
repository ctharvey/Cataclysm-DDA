#pragma once
#ifndef CATA_SRC_CRAFTING_REQUIREMENT_INDEX_H
#define CATA_SRC_CRAFTING_REQUIREMENT_INDEX_H

#include <array>
#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "type_id.h"

class inventory;

// The kind of fact a requirement option ultimately counts against.
enum class crafting_requirement_fact_kind : int {
    component_units = 0,
    component_charges,
    tool_instances,
    tool_charges,
    quality_providers
};

// The menu filter mode a requirement is evaluated under. Item and tool
// options derive one fact key per mode using the recipe's effective filter
// profile for that mode; quality options share a single unfiltered key.
enum class menu_filter_mode : int {
    normal = 0,
    no_rotten,
    no_favorite
};

// Filter profile bits composing an effective profile. The effective profile
// for a menu mode is part of every item/tool fact key, so two recipes with
// different frozen/full-magazine/base-rotten rules never share an
// eligibility fact. Quality facts always use profile 0.
enum recipe_filter_profile : int {
    recipe_filter_none = 0,
    recipe_filter_rotten_forbidden = 1 << 0,
    recipe_filter_favorite_forbidden = 1 << 1,
    recipe_filter_frozen_forbidden = 1 << 2,
    recipe_filter_full_magazine_required = 1 << 3
};

// Number of menu filter modes; recipes carry one effective profile per mode.
constexpr int menu_filter_mode_count = 3;

// The kind of an AND group inside a recipe plan alternative. Options within
// a group must produce facts of a matching kind.
enum class requirement_group_kind : int {
    component = 0,
    tool,
    quality
};

// A recipe is either supported (it contributes a plan) or unsupported (it is
// retained for lookup only, with an explanatory string).
enum class recipe_support_state : int {
    supported = 0,
    unsupported
};

struct crafting_recipe_support {
    recipe_support_state state = recipe_support_state::supported;
    // Meaningful only when state is unsupported.
    std::string reason;
};

// Identity of a single countable fact: what is counted, at which level
// (e.g. quality level), under which effective filter profile.
struct crafting_requirement_fact_key {
    crafting_requirement_fact_kind kind = crafting_requirement_fact_kind::component_units;
    std::string id;
    int level = 0;
    int filter_profile = recipe_filter_none;

    bool operator==( const crafting_requirement_fact_key &rhs ) const;
    bool operator!=( const crafting_requirement_fact_key &rhs ) const;
    bool operator<( const crafting_requirement_fact_key &rhs ) const;
};

// One OR leaf inside a group: a fact coordinate plus the positive threshold
// that must be crossed. The three per-mode fact keys are derived from this
// at add_recipe time using the recipe's effective profiles.
struct crafting_requirement_option {
    crafting_requirement_fact_kind kind = crafting_requirement_fact_kind::component_units;
    std::string id;
    int level = 0;
    int threshold = 0;
};

// An OR set of options within a group.
using crafting_requirement_group_options = std::vector<crafting_requirement_option>;

// An AND group: group kind plus its OR options. An alternative containing
// zero groups is valid and means the inventory portion is trivially
// satisfied; a group containing zero options is invalid.
struct crafting_requirement_group {
    requirement_group_kind kind = requirement_group_kind::component;
    crafting_requirement_group_options options;
};

// An AND set of groups.
using crafting_requirement_alternative = std::vector<crafting_requirement_group>;

// A recipe plan: OR of alternatives, each an AND of groups, each an OR of
// options. A plan with zero alternatives is malformed for a supported
// recipe (it carries no requirement information at all).
struct crafting_requirement_plan {
    std::vector<crafting_requirement_alternative> alternatives;
};

// Reverse edge from a fact back to the recipe coordinate that registered a
// threshold. All fields together identify exactly one option threshold.
struct crafting_requirement_edge {
    recipe_id id;
    menu_filter_mode menu = menu_filter_mode::normal;
    int alternative = 0;
    requirement_group_kind group_kind = requirement_group_kind::component;
    int group = 0;
    int option = 0;
    int threshold = 0;

    bool operator==( const crafting_requirement_edge &rhs ) const;
    bool operator!=( const crafting_requirement_edge &rhs ) const;
    bool operator<( const crafting_requirement_edge &rhs ) const;
};

// A retained recipe record, supported or not. Supported records retain the
// full plan for Phase 3 graph propagation; unsupported records leave it
// empty.
struct crafting_recipe_record {
    recipe_id id;
    crafting_recipe_support support;
    crafting_requirement_plan plan;
};

// Standalone immutable index of crafting requirement facts, built from
// recipe plans. Facts are registered as threshold lists; finalize() sorts
// and deduplicates them, computes maxima, sorts and deduplicates reverse
// edges, and freezes the index. Recipes are identified by stable recipe_id
// values only; no pointers are stored.
class crafting_requirement_index
{
    public:
        // Adds a supported recipe plan. effective_profiles holds one filter
        // profile bitmask per menu_filter_mode (indexed by the mode value).
        // Validates the whole plan first; on any malformation (nonpositive
        // threshold, option kind not matching its group kind, group with no
        // options, plan with no alternatives, duplicate recipe id, or use
        // after finalize) returns false, sets *error when provided, and
        // leaves the index completely unmodified. On success retains the
        // full plan in the recipe record, derives fact threshold lists and
        // reverse edges from every option (one fact key per menu mode using
        // that mode's effective profile; quality options share a single
        // profile-0 key), and records the effective profiles. Alternatives
        // with zero groups are valid and contribute no facts.
        bool add_recipe( const recipe_id &recipe_id,
                         const crafting_requirement_plan &plan,
                         const std::array<int, menu_filter_mode_count> &effective_profiles,
                         std::string *error = nullptr );

        // Records an unsupported recipe. It is retained for lookup but
        // contributes no facts, no edges, and no retained plan. Fails on
        // duplicate recipe id or use after finalize.
        bool add_unsupported_recipe( const recipe_id &recipe_id,
                                     const std::string &reason,
                                     std::string *error = nullptr );

        // Sorts and deduplicates every threshold list, computes maxima,
        // sorts and deduplicates reverse edges, and freezes the index.
        // Idempotent; subsequent calls have no effect.
        void finalize();

        bool is_finalized() const;

        // Drops all recipes, facts, edges, and state; the index is buildable
        // again from scratch.
        void clear();

        std::size_t recipe_count() const;
        bool has_recipe( const recipe_id &recipe_id ) const;

        // Number of distinct fact keys registered in the index.
        std::size_t fact_count() const;

        // All registered fact keys, in key order. Phase 2 enumerates these to
        // visit every fact (and, via edges_for, every dependent recipe)
        // without access to mutable state.
        std::vector<crafting_requirement_fact_key> fact_keys() const;

        // Support record for a recipe; unsupported recipes keep their
        // reason. Returns nullptr for unknown ids, so an unknown recipe is
        // never reported as supported.
        const crafting_recipe_support *support_for( const recipe_id &recipe_id ) const;

        // Retained full plan of a supported recipe; nullptr for unsupported
        // and unknown ids. Required for Phase 3 graph propagation.
        const crafting_requirement_plan *plan_for( const recipe_id &recipe_id ) const;

        // Effective filter profile bitmask recorded for a recipe under the
        // given menu mode, or recipe_filter_none.
        int effective_profile_for( const recipe_id &recipe_id,
                                   menu_filter_mode menu ) const;

        // Largest registered threshold for the fact, or 0 if unknown.
        int maximum_for( const crafting_requirement_fact_key &key ) const;

        // Registered thresholds for the fact (empty if unknown).
        const std::vector<int> &thresholds_for(
            const crafting_requirement_fact_key &key ) const;

        // Reverse edges registered for the fact (empty if unknown), sorted
        // and deduplicated after finalize(). Distinct menu modes pointing at
        // the same fact retain one edge each.
        const std::vector<crafting_requirement_edge> &edges_for(
            const crafting_requirement_fact_key &key ) const;

    private:
        // Mutable accumulation state; meaningful only before finalize().
        std::map<recipe_id, crafting_recipe_record> recipes_;
        std::map<recipe_id, std::array<int, menu_filter_mode_count>> recipe_profiles_;
        std::map<crafting_requirement_fact_key, std::vector<int>> thresholds_;
        std::map<crafting_requirement_fact_key, int> maxima_;
        std::map<crafting_requirement_fact_key, std::vector<crafting_requirement_edge>> edges_;
        bool finalized_ = false;
};

// Accumulates amounts against the facts registered in an index. The tally
// holds a const reference to its index: the index must outlive the tally.
class crafting_requirement_tally
{
    public:
        explicit crafting_requirement_tally( const crafting_requirement_index &index );

        // Adds amount to the fact's count. Nonpositive amounts and facts
        // unknown to the index are ignored. The count saturates at the index
        // maximum without integer overflow. Returns the registered thresholds
        // newly crossed by this addition, in ascending order; each threshold
        // is reported at most once over the tally's lifetime.
        std::vector<int> add( const crafting_requirement_fact_key &key, int amount );

        // Current (capped) count for the fact, or 0 if unknown.
        int count_for( const crafting_requirement_fact_key &key ) const;

        // True only when threshold is positive and the capped count for the
        // fact has reached it.
        bool meets( const crafting_requirement_fact_key &key, int threshold ) const;

    private:
        const crafting_requirement_index &index_;
        std::map<crafting_requirement_fact_key, int> counts_;
        std::map<crafting_requirement_fact_key, int> crossed_; // number of thresholds already crossed
};

class inventory;

// A pointer-free snapshot of inventory counts against the facts registered
// in a finalized index. Constructed from an index and an inventory, it
// visits the concrete items once and stores only per-fact integer counts;
// no item pointers survive construction. Only facts registered in the
// index are ever incremented.
class crafting_inventory_snapshot
{
    public:
        // Builds the snapshot. The index and inventory are read during
        // construction only; neither is referenced afterwards.
        crafting_inventory_snapshot( const crafting_requirement_index &index,
                                     const inventory &inv );

        // Capped count for the fact, or 0 if unknown to the index.
        int count_for( const crafting_requirement_fact_key &key ) const;

        // True only when threshold is positive and the capped count for the
        // fact has reached it.
        bool meets( const crafting_requirement_fact_key &key, int threshold ) const;

        // Number of fact keys tracked (the index allowlist size).
        std::size_t fact_count() const;

        // Number of tracked facts whose capped count has reached the index
        // maximum for that fact.
        std::size_t saturated_fact_count() const;

        // True when the count for the fact is believed to exactly match
        // legacy crafting behavior. False (inexact) for quality facts,
        // whose legacy resolution can depend on charged/contained quality
        // semantics and player state, and for tool_charges facts whose
        // contributing tools can consult external pools (UPS, bionic
        // power, multimag firing requirements); those retain only the
        // local charge sum.
        bool is_exact( const crafting_requirement_fact_key &key ) const;

        // Number of tracked facts marked inexact.
        std::size_t inexact_fact_count() const;

        // Number of tracked tool_charges facts whose exact semantics
        // require external pools that this snapshot does not consult.
        // Phase 3 can treat these as unknown.
        std::size_t unsupported_fact_count() const;

        // Rough size of the snapshot's own bookkeeping in bytes, including
        // this object and its per-fact maps.
        std::size_t approximate_memory_bytes() const;

    private:
        std::size_t fact_total_ = 0;
        std::map<crafting_requirement_fact_key, int> counts_;
        std::set<crafting_requirement_fact_key> saturated_keys_;
        std::set<crafting_requirement_fact_key> inexact_keys_;
        std::set<crafting_requirement_fact_key> unsupported_keys_;
};

#endif // CATA_SRC_CRAFTING_REQUIREMENT_INDEX_H
