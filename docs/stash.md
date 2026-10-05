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

Clients call `SCR_PlayerController.MRX_RequestStashList`, `MRX_RequestStashDeposit` and `MRX_RequestStashWithdraw`
(the server checks the distance) and get `MRX_GetOnStashList()` and `MRX_GetOnStashResult()`. `MRX_StashMenu` is the
default dialog.
