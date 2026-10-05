# Stash

Owned assets: items a player keeps in a per-owner stash and takes into the world. The service is in `Marx_Core`
(`MRX_Marx.GetStash()`, server), together with the stash point component and its RPCs; the stash point action,
dialog and sample prefab are in `Marx_Stash`.

## Asset states

| State | Meaning |
|---|---|
| `STASHED` | In the stash, not in the world |
| `DEPLOYED` | Spawned in the world and bound to its item |
| `LOST` | Destroyed, removed, or lost by the loss policy (final; the record leaves the stash) |
| `CONSUMED` | Used up, reported by a consumer mod (final) |

Allowed changes: `STASHED -> DEPLOYED`, `DEPLOYED -> STASHED | LOST | CONSUMED`. A deployed asset goes back to
`STASHED` only by depositing the very item bound to it.

## What is stored

An asset record holds the prefab and an `MRX_ItemSnapshot`: magazine ammo, damaged hit zones, fuel, and every item
stored in it (attachments, magazines, contents of backpacks and vests), recursively, with their storage and slot.
The snapshot is plain data in Marx's own format, independent of the storage backend. A granted asset without a
snapshot spawns with its prefab defaults.

## Operations

| Call | For | Effect |
|---|---|---|
| `Deposit(playerId, item, cb)` | players | Item in the player's inventory -> snapshot -> item removed -> `STASHED` (new asset, or the deployed asset bound to the item) |
| `Withdraw(playerId, assetId, cb)` | players | `STASHED` -> spawned into the player's inventory -> bound -> `DEPLOYED` |
| `Grant(ownerId, prefab, context, cb, snapshot)` | mods | New `STASHED` asset (e.g. a reward) |
| `Remove(ownerId, assetId, context, cb)` | mods | Deletes a `STASHED` asset |
| `MarkConsumed(item, context, cb)` | mods | Deployed asset of the item -> `CONSUMED` (the item itself is left to the caller) |
| `List(ownerId, cb)` | all | Copy of the stash |

Ordering and safety:

- Operations of the same owner run one at a time. Results arrive on a later frame (`MRX_StashResultCallback`,
  status `MRX_EStashStatus`).
- Withdraw locks through the owner queue, spawns, binds, then commits; if the commit fails, the spawned item is
  removed again.
- Deposit captures, removes the item, then commits; if the commit fails, the item is given back.
- Mod calls need a context (source, reason, idempotency key), like economy calls.
- Stash limit: 100 assets per owner by default (setting). A full stash refuses new assets but still accepts deposits
  of its own deployed assets.

`GetAssetId(item)` and `GetOwnerOf(item)` tell whether an item is an owned asset. Bindings live in server memory:
the engine cannot add components at runtime, so there is no ownership component on the item.

## Loss policy

`MRX_LossPolicy` (settings, or `SetLossPolicy()`):

| Event | Options |
|---|---|
| A player dies carrying deployed assets | `KEEP` (default): the items stay on the body. `LOSE`: lost at once, the items stay as ordinary loot. `RETURN`: taken off the body back into the stash. |
| Assets deployed in an earlier server session | `RESTORE` (default): back to the stash with the stored snapshot. `LOSE`. |

Independent of the policy, Marx checks bindings every 2 s: an item that disappeared while its owner is online is
`LOST`; one that disappeared while the owner is offline (e.g. deleted with the character on disconnect) goes back to
the stash. Items left on a body (`KEEP`) are lost even if the owner is offline when they disappear.

Subclass `MRX_LossPolicy` and override `GetDeathAction` / `GetRecoveryAction` for per-asset rules.

Note: `RESTORE` after a restart can duplicate items in scenarios whose world save restores dropped items. Use `LOSE`
there.

## Events and extension points

- `GetOnAssetStateChanged()` `(asset, oldState)`: every committed change; a new asset reports `oldState` equal to its
  state.
- `AddValidator(MRX_StashValidator)` with `CanDeposit` and `CanWithdraw`.
- World access sits behind `MRX_AssetWorld`; `MRX_EntityAssetWorld` is the engine implementation.

## Stash points

An entity with `MRX_StashPointComponent` (access distance, default 5 m), an enabled `RplComponent`, and
`MRX_OpenStashAction` (`Marx_Stash`) in its `ActionsManagerComponent`. `Marx_Stash: Prefabs/Marx/Stash/MRX_StashWardrobe.et`
is a ready example.

### Stash in the vanilla inventory (Marx_Stash)

Using the "Stash" action opens the vanilla inventory with the player's stash as its own panel:

1. The server spawns a personal container (`Prefabs/Marx/Stash/MRX_StashContainer.et`, no model) at the stash point and
   restores the player's `STASHED` assets into it. Their records stay `STASHED` while they lie in the container.
2. The client opens the vanilla inventory and shows the container as a storage panel (`OpenStorageAsContainer`) with
   the stash point as its preview. The container is never listed in the vicinity, and the vicinity panel is hidden
   while the stash is open. Items are moved by drag and drop as usual; the quick move from the character's inventory
   goes into the stash instead of onto the ground.
3. Every move is committed right away (one frame later, so intermediate moves do not count):
   - an item moved out of the container: `TakeWorldItem` (`DEPLOYED`, bound to the item)
   - an item moved in: `StoreWorldItem` (a bound asset of the player goes back to `STASHED`, any other item becomes a
     new asset)
   - changed contents of a stashed bag or weapon inside the container: its snapshot is updated
   - an item of the player's assets that went into a bag in the container (a stashed one moved into it, or a deployed
     one put into it): merged into the bag (`MergeIntoStashed`, one request: the bag's snapshot holds it, its own
     asset is removed)
   - a refused move (stash full, validator, ...) is undone and shown as a hint
4. Closing the inventory, walking away from the stash point, dying or leaving closes the stash: the container and the
   items still inside are removed (they stay `STASHED`).

Only the player who opened the container may take items out (`MRX_StashContainerManagerComponent`), and items carried
by other characters cannot be put in (`MRX_StashStorageComponent.CanStoreItem`). The container
holds up to 200000 cm3 and items up to 300 cm per side (`MaxCumulativeVolume`, `MaxItemSize` with
`UseCapacityCoefficient` off, because the container has no model); the asset limit of the settings applies as well.
The items lie on a grid of at most 3 pages of 6 x 8 cells (`m_iMaxPages` on `MRX_StashStorageComponent`, 0 = no
limit; `MRX_StashGrid`), each item on its own cells (1x1, 2x1, 2x2 or 3x3 as in the vanilla inventory; identical items
are not stacked). The panel (`MRX_StashPanelUI`) shows all pages from the start and every item at its cell:

- An item dropped on an empty cell goes there, whether it is moved inside the stash or put in from elsewhere. A drop
  on cells that are taken is refused; a drop on a bag in the stash puts the item into the bag, as in vanilla.
- The quick move puts an item at the first free cell of the shown page, or of another page. Other ways in (e.g. a
  gamepad) take the first free cell.
- The cell is stored with the asset (`MRX_AssetRecord.m_sPlacement`, "page,column,row") and used when the stash opens
  again. Items without a stored cell, or whose cells are taken, go to the first free cells; items already stashed are
  always shown, behind the page limit if needed.
- The server keeps the grid; the client asks for cells (`SCR_PlayerController.MRX_RequestStashPlacement`) and shows
  the grid the server sends.

Server side, `MRX_StashSessions.Get()` returns the open containers (`MRX_StashSessionManager`).

### Other front-ends

Core also offers request RPCs for custom UIs: `SCR_PlayerController.MRX_RequestStashList`, `MRX_RequestStashDeposit`
and `MRX_RequestStashWithdraw` (the server checks the distance), answered through `MRX_GetOnStashList()` and
`MRX_GetOnStashResult()`. Front-ends that keep stashed items in the world can use the service calls
`StoreWorldItem`, `TakeWorldItem`, `UpdateStashedSnapshot` and `RemoveVanished`.
