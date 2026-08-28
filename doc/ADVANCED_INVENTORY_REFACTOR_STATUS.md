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

## Characterization coverage added

The current branch adds focused coverage for:

- same-coordinate ground vs cargo identity;
- directional vs dragged aliases for one cargo endpoint;
- `AIM_ALL` excluding only the matching endpoint;
- direct source snapshots for inventory/worn/container;
- wielded rows having a distinct endpoint inside the worn view;
- endpoint-aware ground/cargo counts and free volume;
- aggregate source metrics after filtering.

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

Still owns most high-risk behavior:

- `AIM_ALL` controller orchestration;
- same-source/destination guards;
- destination selection;
- transfer quantity calculation;
- move-one policy;
- move-all policy;
- favorite/bucket/liquid/corpse/wield special cases;
- activity actor selection;
- activity re-entry state;
- UI action dispatch.

This is now the main concentration of domain behavior.

## Next extraction order

### A. Destination acceptance

Extract the answer to:

> Can this concrete destination accept this item, and what limits the amount?

The current answer is distributed across:

- `advanced_inv_area::canputitems()`;
- `advanced_inventory_pane::free_volume()`;
- `free_weight_capacity()`;
- `Character::can_stash_partial()`;
- container `can_contain*()` checks;
- map/vehicle item-count limits;
- worn/wield checks.

Target shape:

```text
endpoint + item + requested count
              |
              v
      destination assessment
              |
      +-------+-------+
      |               |
   allowed         rejected
      |
 max count + limiting reason
```

This should be read-only and reusable by move-one and move-all.

### B. Transfer request / plan

Separate deciding *what should happen* from assigning a `player_activity`.

A transfer request should describe at least:

- concrete source endpoint / row;
- concrete destination endpoint;
- requested quantity;
- resulting accepted quantity;
- special handling requirement (drop, pickup, vehicle move, container insert, wear, wield).

The first version should mirror current activity selection rather than redesign it.

### C. Controller migration

Once endpoint/source/destination helpers exist, replace controller reconstruction sites:

1. `recalc_pane()` `AIM_ALL` enumeration with `enumerate_advanced_inv_around_sources()`;
2. move-one same-endpoint guard with row endpoint vs destination endpoint;
3. move-all same-endpoint guards with pane endpoint equality;
4. item-count/capacity reads with endpoint-aware pane/storage APIs;
5. `query_destination()` square exclusion with endpoint exclusion.

After those migrations, `advanced_inv_area::is_same()` should be removable as an inventory
identity concept.

### D. Execution adapter

Only after planning is separated should activity assignment be extracted into a small
execution adapter around:

- `insert_item_activity_actor`;
- `drop_activity_actor`;
- `pickup_activity_actor`;
- `move_items_activity_actor`;
- wear/wield activity actors.

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
