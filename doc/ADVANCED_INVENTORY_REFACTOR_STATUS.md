# Advanced Inventory refactor status

This document tracks the incremental extraction work started from
`ADVANCED_INVENTORY_RESPONSIBILITY_INVENTORY.md`.

The rule for these tranches is behavior preservation first: name a responsibility,
characterize it, move it behind a seam, then make controller/UI callers consume that seam.

## Completed seams

### 1. Endpoint identity

Implemented in `advanced_inv_endpoint.h`.

AIM now has an explicit value for the storage/transfer boundary represented by a row or
direct pane:

- character inventory;
- worn equipment;
- wielded item;
- map ground at a coordinate;
- vehicle cargo at a specific vehicle part;
- a specific item container.

Important consequences:

- ground and cargo at one coordinate are different endpoints;
- dragged cargo and a directional square can represent the same endpoint;
- `AIM_ALL` is an aggregate view, not an endpoint;
- `AIM_WORN` is a view that can contain both worn and wielded row endpoints.

`advanced_inv_listitem` carries its source endpoint. The old `from_vehicle` flag is now a
compatibility/routing field derived from endpoint identity and asserted against the legacy
constructor hint.

### 2. Source enumeration

Implemented in `advanced_inv_source.{h,cpp}`.

Source enumeration now owns:

- item-location construction for ground storage;
- item-location construction for vehicle cargo;
- the legacy AIM area stacking algorithm;
- direct container-content stacking;
- carried-inventory enumeration;
- worn/equipment enumeration;
- corpse-content expansion;
- filtering at row construction time;
- source volume/weight aggregation;
- endpoint-aware `AIM_ALL` neighborhood aggregation.

`advanced_inventory_pane::add_items_from_area()` is now primarily a dispatcher: choose the
source, receive an `advanced_inv_source_snapshot`, assign metrics, and append rows.

This removed the AIM-specific enumeration implementations that previously lived in
`advanced_inv_pane.cpp` as `avatar`/`outfit` member definitions.

### 3. Read-only storage state

Implemented in `advanced_inv_storage.{h,cpp}`.

A concrete endpoint can now be inspected for:

- endpoint identity;
- raw item count;
- free volume;
- free weight capacity.

This matters because a ground pile and vehicle cargo can occupy the same coordinate while
having different counts and capacities.

The endpoint-aware pane APIs now include:

- `get_item_count( area )`;
- `free_volume( area )`;
- `free_weight_capacity( area )`.

The no-argument `free_weight_capacity()` remains temporarily because existing controller
call sites do not pass the destination area yet.

### 4. Destination assessment

Implemented in `advanced_inv_destination.{h,cpp}`.

The destination decision is split into two read-only questions.

**Capacity/quantity** mirrors the current `query_charges()` ordering and reports:

- requested amount;
- accepted amount;
- the limiting reason;
- inventory-pocket limits;
- generic volume/weight limits;
- map/vehicle item-count limits;
- pickup/overburden limits;
- worn-slot limits.

**Acceptance** handles item-policy rules independently of quantity:

- invalid destination;
- container `NO_RELOAD`;
- container `can_contain` / parent-container constraints;
- wear validity;
- the existing wear-to-wield fallback.

The capacity mirror intentionally preserves the current worn-slot behavior where the final
worn-slot calculation uses the original requested amount and can overwrite an earlier
pickup-weight limitation. That is recorded as a behavior question, not silently changed as
part of refactoring.

### 5. Transfer classification

Implemented in `advanced_inv_transfer.{h,cpp}`.

Endpoint-to-endpoint movement can now be classified without mutating game state or assigning
an activity:

- insert into container;
- drop from character;
- pick up to inventory;
- move between world endpoints;
- wear;
- wield;
- take off to inventory.

Same-endpoint movement is rejected at the planning layer.

## Characterization coverage added

The current branch adds focused coverage for:

- same-coordinate ground vs cargo identity;
- directional vs dragged aliases for one cargo endpoint;
- `AIM_ALL` excluding only the matching endpoint;
- direct source snapshots for inventory/worn/container;
- wielded rows having a distinct endpoint inside the worn view;
- endpoint-aware ground/cargo counts and free volume;
- aggregate source metrics after filtering;
- destination capacity using the actual ground/cargo endpoint;
- inventory pocket availability;
- worn acceptance and wield fallback;
- direct container acceptance;
- endpoint-driven transfer classification.

## Responsibilities still embedded in legacy owners

### `advanced_inv_area`

Still owns:

- world-position initialization;
- vehicle/cargo discovery;
- terrain/vehicle descriptions;
- danger/field/trap/water display flags;
- destination-location validity;
- legacy `get_item_count()`;
- legacy area-level `is_same()`;
- vehicle-stack lookup.

The custom stacking responsibility has been removed.

### `advanced_inventory_pane`

Still owns:

- pane selection and scroll state;
- filter text/function;
- source dispatch;
- display rows;
- target-after-recalc selection recovery;
- container-view state;
- a legacy no-area weight-capacity method.

It no longer owns the mechanics of enumerating map, vehicle, inventory, worn, or container
sources.

### `advanced_inventory`

Still owns the orchestration and mutation-heavy behavior:

- legacy `AIM_ALL` controller orchestration;
- same-source/destination guards;
- destination-selection UI;
- legacy transfer quantity calculation and prompts;
- move-one orchestration;
- move-all policy;
- favorite/bucket/liquid/corpse/wield special cases;
- activity actor construction/assignment;
- activity re-entry state;
- UI action dispatch.

The important difference is that endpoint identity, source enumeration, destination
assessment, and transfer classification now exist outside this controller and can replace
those reconstructions mechanically.

## Next extraction order

### A. Move-all policy

Move-all still combines source eligibility, destination eligibility, favorites, buckets,
corpses, wielded items, sorting for partial capacity, user confirmation, and activity
selection.

The next seam should separate per-row policy from orchestration. At minimum it should answer:

```text
row + destination
        |
        v
 move-all disposition
        |
        +-- eligible normally
        +-- favorite
        +-- liquid/gas: skip
        +-- non-empty corpse: skip
        +-- bucket would spill: defer/special handling
        +-- wielded: defer/special handling
        +-- destination rejects item: skip
```

This makes the remaining move-all function a coordinator instead of the owner of every rule.

### B. Controller migration

The seams now exist for the first mechanical controller replacements:

1. `recalc_pane()` `AIM_ALL` enumeration -> `enumerate_advanced_inv_around_sources()`;
2. move-one same-endpoint guard -> row endpoint vs destination endpoint;
3. move-all same-endpoint guards -> pane endpoint equality;
4. item-count/capacity reads -> endpoint-aware pane/storage APIs;
5. `query_charges()` quantity math -> destination capacity assessment;
6. wear/container policy -> destination acceptance;
7. activity-routing branches -> transfer classification;
8. `query_destination()` square exclusion -> endpoint exclusion.

After those migrations, `advanced_inv_area::is_same()` should be removable as an inventory
identity concept.

### C. Execution adapter

Once controller routing consumes transfer plans, extract activity construction/assignment
behind a small execution adapter around:

- `insert_item_activity_actor`;
- `drop_activity_actor`;
- `pickup_activity_actor`;
- `move_items_activity_actor`;
- wear/wield activity actors.

### D. Remove legacy APIs

After controller migration:

- remove `advanced_inv_area::get_item_count()`;
- remove `advanced_inv_area::is_same()`;
- remove no-area `advanced_inventory_pane::free_weight_capacity()`;
- remove stale AIM-specific declarations from `avatar` / `outfit`;
- decide whether `from_vehicle` can be replaced entirely by row endpoint kind.

## Deliberately not tackled yet

The following remain deferred until their dependencies are characterized:

- move-all resumability across save/load;
- coarse `recalc` invalidation;
- UI rendering cleanup;
- source-column presentation;
- unifying the two existing stacking semantics;
- redesigning activity actor behavior;
- changing actual movement rules.

The current goal is still a strangler refactor, not a rewrite.
