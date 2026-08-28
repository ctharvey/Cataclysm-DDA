# Advanced Inventory endpoint identity

This note refines the responsibility inventory with one narrower question:

> What does it mean for two Advanced Inventory references to point at the same storage endpoint?

The current AIM code often reconstructs that answer from some combination of `aim_location`, map position, `in_vehicle`, cargo part, and active container state.  That works in most paths, but it makes storage identity implicit and causes screen/view concepts to leak into transfer logic.

## Endpoint identity is not screen identity

A map coordinate can contain multiple storage endpoints at once.

For example, one east tile may expose both:

- ground storage at the east coordinate;
- vehicle cargo at the east coordinate.

Those endpoints share a coordinate but are not interchangeable.  Conversely, two AIM screen locations can identify the same endpoint: `AIM_DRAGGED` and a directional square may both refer to the same vehicle cargo part.

The endpoint value object therefore identifies storage independently of `aim_location`.

## Endpoint kinds

The current model distinguishes:

| Kind | Identity |
| --- | --- |
| character inventory | singleton within AIM's player context |
| worn | singleton worn-item destination/view |
| wielded | singleton wielded-item endpoint |
| ground | bubble map coordinate |
| vehicle cargo | vehicle identity + cargo part |
| item container | `item_location` of the container |

The identities are intentionally ephemeral AIM/runtime identities.  They are not intended for persistence across map shifts, save/load, or vehicle reconstruction.

## Pane endpoints and row endpoints are different concepts

Not every pane represents exactly one storage endpoint.

### Direct panes

A direct ground, vehicle-cargo, inventory, container, or worn destination can resolve to a pane endpoint.

### `AIM_ALL`

`AIM_ALL` is an aggregate view over multiple ground and vehicle-cargo endpoints.  It has no single endpoint.  Each displayed row must retain the endpoint it came from.

### `AIM_WORN`

Despite its name, `AIM_WORN` displays both:

- the wielded item;
- worn items.

The pane itself still represents the worn/equipment view, but a wielded row has a distinct `wielded` endpoint.  This distinction already exists implicitly in transfer code, which special-cases the wielded item.

This means the architecture should not assume:

```text
pane endpoint == endpoint of every row in pane
```

Instead:

```text
direct pane -> optional pane endpoint
row         -> concrete source endpoint when resolvable
aggregate   -> no single pane endpoint
```

## Row endpoint derivation

`advanced_inv_listitem` now carries an optional `advanced_inv_endpoint` in addition to the legacy `area` and `from_vehicle` fields.

For map and vehicle rows, endpoint derivation walks to the top-level owning `item_location` before resolving storage.  This is required for rows that expose nested contents, such as corpse contents: the nested `item_location` may know that it recursively belongs to a vehicle while not itself retaining the vehicle cursor.

For container view, the immediate parent item is the endpoint.

For inventory/worn views, AIM's logical semantics determine the endpoint.  A wielded row inside `AIM_WORN` is explicitly identified as `wielded` rather than `worn`.

## Invariants

The endpoint layer should preserve these invariants:

1. Ground and vehicle cargo at the same coordinate are different endpoints.
2. The same vehicle cargo part is one endpoint regardless of whether AIM reached it through a directional square or `AIM_DRAGGED`.
3. Screen-area aliases do not change ground identity.
4. Two different item containers are different endpoints even if colocated.
5. `AIM_ALL` never becomes an endpoint itself.
6. A wielded row is not a worn endpoint merely because it is displayed in `AIM_WORN`.
7. Endpoint identity must not depend on presentation ordering, filtering, or sorting.

## Legacy identity fields during migration

`advanced_inv_listitem::area` and `advanced_inv_listitem::from_vehicle` remain for now because the controller and activity scheduling paths still consume them.

During the migration, `endpoint` is the identity source of truth while the legacy fields continue carrying routing/presentation information.  They should not be treated as a second independent definition of endpoint equality.

## Controller migration targets

The next replacements are:

1. `recalc_pane()` destination exclusion for `AIM_ALL`;
2. `action_move_item()` same-endpoint guard;
3. `move_all_items()` same-endpoint guard, including the dragged-vehicle special case;
4. `query_destination()` enable/disable logic, which currently excludes a whole screen square rather than a specific endpoint.

Once those sites use `advanced_inv_endpoint`, `advanced_inv_area::is_same()` should no longer be needed as a storage-identity surrogate.

## Architectural direction

Endpoint identity should remain a small value layer.  It should not become the new inventory subsystem.

The likely progression is:

```text
AIM view / pane
    |
    +-- zero or one direct endpoint
    +-- rows carrying concrete source endpoints
             |
             v
        ItemSource abstraction
             |
             v
       transfer planning
```

Defining identity first gives later `ItemSource` and transfer abstractions a stable answer to the basic question: "is this actually the same storage place?"
