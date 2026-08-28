# Advanced Inventory Responsibility Inventory

Status: research / behavior-preservation map

This document inventories the responsibilities currently served by the Advanced Inventory Manager (AIM) before any structural refactor begins.

The purpose is deliberately not to propose a replacement implementation first. The first goal is to establish what AIM is responsible for, which observable behaviors must remain stable, which game systems it depends on, and where those responsibilities are currently implemented. Refactoring should proceed by extracting one responsibility at a time behind characterization tests.

## Refactor objective

AIM currently acts as more than an inventory UI. It combines presentation, source enumeration, storage semantics, transfer planning, activity scheduling, persistence/re-entry, and game-rule special cases.

The long-term goal is to make AIM primarily a UI over explicit inventory-domain abstractions rather than an owner of storage and transfer semantics.

A useful target dependency direction is:

```text
Advanced Inventory UI
        |
        v
Inventory View Model
        |
        +---- ItemSource
        +---- ItemDestination
        +---- ItemQuery / Filter
        +---- TransferPlanner
                    |
                    v
             TransferExecutor
```

This document does not assume those exact types will survive implementation. They are labels for responsibilities that should become independently understandable and testable.

## Core rule for the refactor

Before moving code, answer four questions for every responsibility:

1. What user-visible outcome does it serve?
2. What state does it read?
3. What state does it mutate?
4. Which subsystem should ultimately own that behavior?

No responsibility should be extracted merely because a function is large. Extraction should reduce the number of places that need to understand storage type, source identity, destination capability, or transfer execution details.

---

# 1. User-facing responsibility inventory

## 1.1 Open, close, suspend, and resume AIM

**User outcome:** The player can enter AIM, perform actions that consume moves or start activities, and return to the same interface afterward when appropriate.

**Reads:** player activity state, saved AIM state, current moves.

**Mutates:** AIM lifetime, saved re-entry state, activity continuation state, UI adapter lifetime.

**Current ownership:** `create_advanced_inv`, `advanced_inventory::display`, `advanced_inventory::do_return_entry`, destructor/save-state logic, `uistate.transfer_save`, `cancel_aim_processing`.

**Important behavior:** AIM is sometimes closed only so another activity can run, then automatically reopened. This is not equivalent to a normal user-requested exit.

## 1.2 Maintain two independently configured panes

**User outcome:** The player can compare a source and destination, switch active panes, and retain independent area/filter/sort/selection state.

**Reads:** pane settings and saved UI state.

**Mutates:** active side, source/destination side, selected area, filter, sort, index, vehicle/ground mode, active container.

**Current ownership:** `advanced_inventory`, `advanced_inventory_pane`, saved AIM state.

## 1.3 Select a logical item source

**User outcome:** A pane can represent inventory, worn items, one adjacent map tile, vehicle cargo on a tile, dragged vehicle cargo, a container, or all surrounding squares.

**Reads:** player position, map, vehicles, grab state, container location.

**Mutates:** pane source identity and source-specific UI state.

**Current ownership:** `aim_location`, `advanced_inv_area`, `advanced_inventory_pane::set_area`, `advanced_inventory::change_square`, `advanced_inventory::recalc_pane`.

## 1.4 Distinguish ground storage from vehicle cargo on the same tile

**User outcome:** When a tile contains vehicle cargo, the player can choose whether the pane means the cargo part or the terrain below it.

**Reads:** vehicle-at-position and cargo-part state.

**Mutates:** pane `viewing_cargo` state.

**Current ownership:** `advanced_inventory_pane`, `advanced_inv_area`, `TOGGLE_VEH` handling.

## 1.5 Represent a dragged vehicle as a source/destination

**User outcome:** Cargo in the currently grabbed vehicle can be addressed directly even when its relative direction changes.

**Reads:** avatar grab type and grab point, vehicle cargo at the resulting tile.

**Mutates:** computed area offset/position and vehicle-part identity.

**Current ownership:** `advanced_inv_area::init`, `AIM_DRAGGED` special cases.

## 1.6 Represent a selected container and its parent relationship

**User outcome:** The player can enter a container, inspect or move its contents, return to its parent context, and preserve where the container originated.

**Reads:** `item_location` parentage and absolute position, player position, source area.

**Mutates:** pane container, `container_base_loc`, container-view navigation state.

**Current ownership:** `advanced_inventory_pane`, `advanced_inv_area::set_container_position`, container branches throughout AIM.

## 1.7 Aggregate all surrounding locations

**User outcome:** `AIM_ALL` presents items from the nine surrounding map squares as a single list while retaining enough source identity to move each item correctly.

**Reads:** nine map tiles, vehicle cargo on those tiles, opposite-pane destination identity.

**Mutates:** synthetic aggregate list and aggregate weight/volume totals.

**Current ownership:** `advanced_inventory::recalc_pane`, `advanced_inventory_pane::add_items_from_area`, `advanced_inv_area`.

**Important behavior:** The aggregate must exclude the destination representation when source and destination overlap, including the ground/vehicle distinction on the same tile.

## 1.8 Enumerate items from each storage kind

**User outcome:** The pane displays the items that logically belong to the selected source.

**Reads:** player inventory/pockets, worn items, wielded item/container, map stack, vehicle stack, selected container, corpse contents.

**Mutates:** pane display rows and area weight/volume accounting.

**Current ownership:** `advanced_inventory_pane::add_items_from_area`, `avatar::get_AIM_inventory`, `outfit::get_AIM_inventory`, `outfit::add_AIM_items_from_area`.

## 1.9 Convert raw items into display stacks

**User outcome:** Equivalent items appear as one AIM row where appropriate.

**Reads:** item identity and `display_stacked_with` behavior.

**Mutates:** transient row grouping only.

**Current ownership:** `advanced_inv_area::i_stacked`, local `item_list_to_stack` helper.

**Risk:** AIM currently has more than one stacking path. A refactor must first establish whether they are intentionally equivalent for all source kinds.

## 1.10 Build and cache row presentation data

**User outcome:** Each row has name, count, weight, volume, category, favorite/autopickup state, source marker, etc.

**Reads:** live `item_location` plus item presentation properties.

**Mutates:** `advanced_inv_listitem` snapshot fields.

**Current ownership:** `advanced_inv_listitem` constructors.

**Risk:** A row is partly a snapshot and partly a live proxy because sorting/rendering sometimes use cached fields and sometimes dereference `items.front()` again.

## 1.11 Filter visible items

**User outcome:** The player can restrict a pane using the shared item-filter grammar.

**Reads:** item properties and filter expression.

**Mutates:** pane filter predicate and displayed set.

**Current ownership:** generic `item_filter_from_string`; `advanced_inventory_pane::set_filter` and `is_filtered`; AIM filter UI.

**Boundary note:** The filter grammar is already largely outside AIM. New generic predicates should generally be implemented in the shared item-filter system, not as AIM-only branches.

## 1.12 Sort visible items

**User outcome:** The player can sort by name, weight, volume, density, charges, category, damage, ammo/charge type, spoilage, price, value density, or stack amount.

**Reads:** cached row data and live item properties.

**Mutates:** row order.

**Current ownership:** `advanced_inv_sorter`, sort menu, pane `sortby`.

## 1.13 Navigate rows, pages, and categories

**User outcome:** Normal scrolling, page scrolling, home/end, category jumps, and wraparound work consistently.

**Reads:** row order, category boundaries, lines-per-page.

**Mutates:** pane selection index.

**Current ownership:** `advanced_inventory_pane` scrolling methods, `advanced_inventory_pagination`.

## 1.14 Preserve selection across recalculation

**User outcome:** When a move or mutation rebuilds a pane, AIM attempts to remain on the same logical item rather than jumping arbitrarily.

**Reads:** `target_item_after_recalc`, rebuilt rows.

**Mutates:** pane index and target marker.

**Current ownership:** `advanced_inventory::recalc_pane`, pane state.

## 1.15 Render source identity and source metadata

**User outcome:** The player can see which area a pane represents, terrain/vehicle/container description, danger flags, capacity, source abbreviation for `AIM_ALL`, and mini-map selection.

**Reads:** `advanced_inv_area`, pane state, map/vehicle state.

**Mutates:** screen only.

**Current ownership:** `print_header`, `print_items`, `redraw_pane`, `redraw_sidebar`, minimap functions.

## 1.16 Display source/destination capacity

**User outcome:** The UI reports current and maximum weight/volume and can warn when a transfer exceeds destination capacity.

**Reads:** character capacity, container capacity, vehicle-stack free volume, map free volume, area aggregate totals.

**Mutates:** display; transfer planning may change ordering based on capacity.

**Current ownership:** `advanced_inventory_pane::free_volume`, `free_weight_capacity`, `advanced_inv_area` totals, rendering, move-all planning.

## 1.17 Determine whether a source and destination are the same storage endpoint

**User outcome:** AIM must not move an item onto itself, duplicate it, or make it disappear when two UI representations resolve to the same underlying storage.

**Reads:** area IDs, map positions, vehicle pointers/part indexes, ground-vs-vehicle state, container identity.

**Mutates:** none directly; gates transfer operations.

**Current ownership:** `advanced_inv_area::is_same` plus additional transfer-specific equality checks in AIM.

**Refactor importance:** This should become a single explicit endpoint-identity concept rather than being reconstructed differently in multiple call sites.

## 1.18 Determine whether a destination can accept items at all

**User outcome:** Invalid destinations are disabled or rejected before a move begins.

**Reads:** terrain/furniture storage capability, vehicle cargo state, selected container validity, player inventory/worn semantics.

**Mutates:** none directly.

**Current ownership:** `advanced_inv_area::canputitems`, `canputitemsloc`, `advanced_inventory::query_destination`, transfer branches.

## 1.19 Determine whether a particular item may be moved to a destination

**User outcome:** AIM avoids impossible/unsafe moves and explains failures.

**Reads:** item phase, container compatibility, inventory capacity, corpse contents, bucket state, favorite/wielded state, NO_RELOAD/NO_UNLOAD flags, destination type.

**Mutates:** none until a move is accepted; may prompt the user.

**Current ownership:** `fill_lists_with_pane_items`, `query_charges`, `action_move_item`, `move_all_items`, underlying character/container APIs.

**Refactor importance:** AIM currently acts as a second transfer-rules engine here. The long-term owner should be below the UI.

## 1.20 Choose an amount for a move

**User outcome:** Single-item, variable-amount, whole-stack, and charge-counted items move the expected quantity subject to capacity.

**Reads:** stack size/charges, source/destination capacity, item count semantics.

**Mutates:** requested transfer quantity.

**Current ownership:** `query_charges`, action-specific transfer logic, downstream activity actors.

## 1.21 Move one item / partial amount / stack

**User outcome:** The selected item is transferred from source to destination with correct time/activity semantics.

**Reads:** source item locations, source type, destination type, requested quantity.

**Mutates:** player inventory, map items, vehicle cargo, containers, worn/wielded state as applicable.

**Current ownership:** `action_move_item`, `start_activity`, multiple activity actors and character APIs.

## 1.22 Move all eligible items

**User outcome:** AIM moves as many eligible items as requested while respecting capacity, favorites, buckets, wielded items, liquids/gases, corpses, and destination constraints.

**Reads:** effectively every transfer rule plus pane ordering and capacity.

**Mutates:** world/inventory state through scheduled activities; AIM processing/re-entry state.

**Current ownership:** `move_all_items`, `fill_lists_with_pane_items`, downstream activity actors.

**Risk:** This is the highest-coupling AIM responsibility and should not be an early extraction target without characterization coverage.

## 1.23 Choose transfer order when everything will not fit

**User outcome:** When destination capacity is insufficient, AIM attempts to move a useful subset in a deterministic order.

**Reads:** per-item weight/volume, destination type, downstream actor processing order.

**Mutates:** ordered transfer request.

**Current ownership:** `fill_lists_with_pane_items` and `move_all_items`.

**Coupling:** AIM currently knows that `pickup_activity_actor` consumes its list from the back and reverses ordering accordingly. This is an execution detail leaking into UI-side planning.

## 1.24 Protect favorite items

**User outcome:** Favorites are visually represented and move-all avoids dropping them unless necessary/confirmed.

**Reads:** item favorite state, source type.

**Mutates:** favorite state when toggled; transfer plan when moving all.

**Current ownership:** `process_action`, `advanced_inv_listitem`, `fill_lists_with_pane_items`, `move_all_items`.

## 1.25 Toggle autopickup rules

**User outcome:** The selected item can add/remove an autopickup rule directly from AIM.

**Reads:** autopickup rules and selected item.

**Mutates:** global autopickup configuration and row state.

**Current ownership:** AIM action handling + autopickup subsystem.

## 1.26 Examine an item or its contents

**User outcome:** The player can inspect the selected item and nested contents from AIM.

**Reads:** selected `item_location` and item info.

**Mutates:** usually UI state; item examination may invoke other interaction paths.

**Current ownership:** `action_examine`, `inventory_examiner`, item-info UI.

## 1.27 Unload a selected item/container

**User outcome:** The player can unload into an appropriate destination and return to AIM afterward.

**Reads:** source/destination container state.

**Mutates:** item/container contents through character unload behavior; AIM re-entry state.

**Current ownership:** `action_unload`, avatar unload logic, re-entry machinery.

## 1.28 React to world mutations while AIM is open

**User outcome:** Activated/repaired/moved items, inventory restacks, changing vehicle/container state, and completed activities are reflected without stale or dangling presentation state.

**Reads:** live game state and invalidation flags.

**Mutates:** pane rows, indexes, cached totals, UI redraw state.

**Current ownership:** coarse `recalc`, `pane.recalc`, `always_recalc`, re-entry logic.

## 1.29 Persist and restore UI state

**User outcome:** AIM remembers layout, filters, sorts, selected areas, vehicle mode, and container context as intended across normal close/re-entry.

**Reads/writes:** `uistate.transfer_save` and pane save state.

**Current ownership:** `save_settings`, `load_settings`, pane save/load methods.

---

# 2. Storage/source kinds currently encoded by AIM

`aim_location` currently mixes multiple semantic categories in one enum. This is a major source of branching because callers must inspect the enum to rediscover what sort of thing a pane represents.

## Physical or storage-backed sources

- Player inventory
- Worn items
- Adjacent ground/map tile
- Vehicle cargo on an adjacent tile
- Dragged vehicle cargo
- Selected container contents

## Synthetic or navigation representations

- `AIM_ALL`: composite view over surrounding sources
- `AIM_PARENT`: container navigation concept
- `AIM_WIELD`: internal fallback destination, not a normal selectable area

These categories should remain conceptually separate even if the implementation does not immediately replace `aim_location`.

---

# 3. Current source enumeration matrix

| Source | Data authority | Special handling currently visible in AIM |
| --- | --- | --- |
| Player inventory | character inventory/pockets | exposes top-level contents via AIM-specific helpers |
| Worn | character attire | includes wielded item in the AIM worn view; worn-specific enumeration helper |
| Map tile | `map::i_at` | impassable-field check, corpse-content exposure, local stacking |
| Vehicle cargo | vehicle cargo part | separate ground/vehicle mode for same map position |
| Dragged vehicle | grab point + vehicle cargo | relative position can change dynamically |
| Container | `item_location` contents | parent/base location tracking, NO_RELOAD/NO_UNLOAD behavior |
| All surrounding | composite of map + vehicle sources | excludes destination-equivalent source; synthetic totals |

A first extraction should make this matrix explicit in code without changing transfer execution.

---

# 4. Current destination capability questions

Several AIM paths independently answer variants of the following questions. These should converge toward one authority over time.

1. Is this a real destination?
2. Is it the same endpoint as the source?
3. Can this endpoint store any items?
4. Can it store this particular item?
5. How much volume remains?
6. How much relevant weight capacity remains?
7. Does moving this item require a special operation (wear, wield, insert, unload, pickup, drop)?
8. Would the operation spill a bucket or violate liquid/gas rules?
9. Does the item require user confirmation because it is favorite/wielded?
10. Which game activity/API should execute the accepted transfer?

The UI should eventually consume answers to these questions instead of implementing them by branching on storage type.

---

# 5. Transfer execution matrix

AIM currently chooses execution strategy based on the source/destination combination.

Known execution mechanisms include:

- `pickup_activity_actor`
- `drop_activity_actor`
- `move_items_activity_actor`
- `insert_item_activity_actor`
- character unload/wear/wield/inventory APIs where applicable

The target boundary should eventually look closer to:

```text
UI intent
  -> transfer request
  -> validated transfer plan
  -> execution strategy
  -> activity/game mutation
```

rather than:

```text
UI action
  -> inspect aim_location
  -> inspect item special cases
  -> inspect destination capacity
  -> choose activity actor
  -> account for actor iteration semantics
  -> schedule activity
```

---

# 6. Behavioral invariants to preserve

These are initial invariants gathered from the current implementation. They should become characterization tests before the relevant code is moved.

## Source/view invariants

- Ground and vehicle cargo on the same tile remain distinct endpoints.
- `AIM_ALL` retains the actual source of every displayed row.
- `AIM_ALL` does not include the destination endpoint when this would create a self-transfer.
- Container view becomes invalid if the container is no longer reachable/adjacent under the current rules.
- Filtering affects visibility without changing item ownership or source identity.
- Stacking never merges items that the existing display rules consider distinct.

## Selection/UI invariants

- Recalculation leaves the selected index valid.
- When possible, AIM returns to the same logical item after recalculation.
- Each pane retains independent filter, sort, area, and vehicle/ground state.
- Normal exit and temporary activity-driven re-entry remain distinguishable.

## Transfer invariants

- Source and destination resolving to the same endpoint never remove or duplicate items.
- Count-by-charges and count-by-item transfers preserve quantity.
- Capacity-limited transfers do not exceed the destination's supported capacity rules.
- Unsupported free liquids/gases are not moved by generic move-all behavior.
- Nonempty corpses retain their current move restrictions.
- Nonempty buckets retain spill-protection behavior.
- Favorites retain current move-all protection/confirmation behavior.
- Wielded-item special handling remains behaviorally equivalent.
- NO_RELOAD and NO_UNLOAD restrictions remain honored.
- Moving between ground, vehicle, inventory, and container endpoints retains current activity/time semantics.
- AIM correctly reopens after transfer activities when the current behavior requires it.

## Ordering invariants

- Existing visible sort modes remain stable.
- Capacity-limited move-all retains its current item-priority semantics until intentionally redesigned.
- Downstream actor list direction must not silently reverse user-visible transfer priority during refactoring.

---

# 7. Current ownership/coupling map

## `advanced_inventory`

Currently owns or coordinates:

- top-level UI lifecycle
- source/destination pane identity
- action dispatch
- rendering
- source recalculation
- aggregate `AIM_ALL` construction
- sorting invocation
- transfer validation/planning
- transfer execution selection
- move-all special cases
- activity re-entry
- saved-state coordination

This class is the primary convergence point of unrelated abstraction levels.

## `advanced_inventory_pane`

Currently owns or coordinates:

- selected area and previous area
- ground/vehicle view mode
- item row collection
- filtering
- scrolling/navigation
- container context
- free capacity queries
- pane persistence state
- source enumeration dispatch through `add_items_from_area`

The pane is therefore both UI state and a partial storage-domain object.

## `advanced_inv_area`

Currently represents:

- user-facing source metadata
- map-relative and absolute position
- vehicle cargo pointer/part
- storage capability
- total item weight/volume
- map danger metadata
- source identity comparison
- stacking for map/vehicle items

This object mixes presentation metadata, world-location resolution, storage capability, and transient aggregate accounting.

## `advanced_inv_listitem`

Currently represents a display row while retaining live `item_location` references. It snapshots some presentation/sort values but callers still reach through to live item state for other values.

## Generic item-filter subsystem

This is already a comparatively good boundary. AIM stores the expression/predicate and applies it, while the generic item-search code defines the grammar.

## Activity actors / character / map / vehicle / item APIs

These remain the actual mutation authorities, but AIM contains significant knowledge about which one to call and how each behaves.

---

# 8. Highest-risk coupling seams

## 8.1 `AIM_ALL`

`AIM_ALL` pretends to be an area while actually being a composite view of many endpoints. It also participates in move-all processing and destination exclusion logic.

**Direction:** eventually model it as a composite source over ordinary endpoints.

## 8.2 Move-all planning

Move-all combines UI prompts, item eligibility, destination compatibility, favorites, wielded items, buckets, corpses, capacity calculation, sort priority, and execution ordering.

**Direction:** freeze behavior with tests before attempting structural change.

## 8.3 Storage-type classification

Inventory/worn/container/map/vehicle distinctions are repeatedly reconstructed through `aim_location` branches.

**Direction:** introduce a read-only endpoint/source abstraction first, then migrate decisions one at a time.

## 8.4 Activity re-entry

AIM has a UI lifetime coupled to game activities and saved state.

**Direction:** do not alter this while extracting read-only source/query responsibilities.

## 8.5 Cached row state vs live item state

Rows cache some properties but read other properties live.

**Direction:** decide explicitly whether the future row model is a snapshot projection or a live facade. Do not accidentally change refresh semantics while extracting enumeration.

---

# 9. Proposed extraction sequence

The ordering below is chosen to minimize behavior change and avoid starting in move-all.

## Tranche 0: Characterization coverage

Add tests around the existing behavior before changing abstractions.

Priority cases:

1. map source enumeration
2. vehicle source enumeration
3. ground vs vehicle on the same tile
4. container enumeration
5. inventory/worn enumeration
6. `AIM_ALL` source provenance and destination exclusion
7. filtering across multiple source kinds
8. display stacking equivalence across source kinds
9. endpoint identity/self-transfer protection
10. basic container/vehicle/map/inventory transfer matrix
11. save/re-entry around at least one transfer activity
12. move-all special cases before move-all itself is touched

## Tranche 1: Read-only source interface

Extract the question:

> What items are exposed by this source?

Do not change transfer behavior.

A provisional conceptual interface:

```cpp
class item_source_view {
    public:
        source_identity identity() const;
        std::vector<item_location> items() const;
};
```

The actual API should account for display stacking/provenance without forcing callers back into `aim_location` branching.

## Tranche 2: Canonical display-stack builder

Unify the duplicated stack-building paths behind one tested operation.

The result should preserve existing display behavior before any stacking policy changes are considered.

## Tranche 3: Composite source for `AIM_ALL`

Make the aggregate explicitly compose ordinary sources rather than acting like another physical area.

This should simplify:

- provenance retention
- destination exclusion
- aggregate totals
- future range/area expansion

## Tranche 4: Endpoint identity and capability queries

Centralize:

- endpoint equality
- can-store-anything
- can-accept-item
- free volume
- relevant weight capacity

This is where repeated source/destination classification should begin disappearing from AIM.

## Tranche 5: Transfer request / validation plan

Introduce a representation of transfer intent that is independent of UI action dispatch.

For example conceptually:

```text
transfer_request
- source endpoint
- destination endpoint
- item locations
- requested quantity
- policy flags / user intent
```

Validation should return either a plan or an explanation of why the request cannot execute.

## Tranche 6: Execution strategy

Move knowledge of `pickup_activity_actor`, `drop_activity_actor`, `move_items_activity_actor`, and `insert_item_activity_actor` behind a transfer-execution boundary.

AIM should no longer care which actor consumes its vector from which direction.

## Tranche 7: Move-all migration

Only after ordinary endpoint capabilities and transfer planning are stable should move-all be rewritten to build a sequence of ordinary transfer plans.

## Tranche 8: UI-state simplification

After storage and execution semantics leave AIM, revisit:

- `recalc` / `always_recalc` / `pane.recalc`
- selection preservation
- action dispatch size
- rendering-only pane state
- re-entry state machine

---

# 10. Things not to redesign during the first extraction

To keep the first tranches reviewable and behavior-preserving, initially avoid changing:

- item-filter syntax
- visible sort semantics
- move-all favorite behavior
- liquid/gas behavior
- corpse-content behavior
- bucket spill behavior
- worn/wield destination semantics
- activity costs/timing
- save/re-entry behavior
- `AIM_ALL` user-visible behavior
- container reachability rules

These can be redesigned later, but combining redesign with abstraction extraction would make regressions difficult to localize.

---

# 11. Separate QoL work: accepted-ammo filtering

The desired `a:battery` / `a:9mm` style filter is intentionally not part of the AIM refactor.

Because AIM already delegates filter parsing to the generic item-filter subsystem, an accepted-ammo predicate should be implemented and tested there. AIM should gain the feature automatically.

Keeping this separate is useful because it gives us a small functionality change without entangling the storage/transfer refactor.

---

# 12. Definition of success for the responsibility-inventory phase

This phase is complete when:

- every AIM user action can be mapped to one or more responsibilities above;
- every storage/source kind has an explicit enumeration path;
- every source/destination combination can be traced to its validation and execution authority;
- the major behavior invariants have characterization tests or explicit test gaps;
- we can select the first extraction tranche without changing user-visible behavior;
- new AIM work can answer "which responsibility owns this?" without defaulting to another branch in `advanced_inv.cpp`.

The intended first code change after this document is not a rewrite. It is characterization coverage followed by a read-only source abstraction.