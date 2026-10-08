# Getting started

## Requirements

- Arma Reforger 1.8 (Workbench from Arma Reforger Tools for development)
- Your mod depends on `Marx_Core`; add `Marx_Shop` for shops and `Marx_Stash` for stash points (both include the
  default UI from `Marx_UI`)

## What runs where

Marx registers its server system, `MRX_MarxSystem`, by overriding the vanilla `Configs/Systems/ChimeraSystemsConfig.conf`.
Every game mode that uses the vanilla systems configs gets it without setup. The system creates the services on the
server only:

| Service | Access | Purpose |
|---|---|---|
| `MRX_EconomyService` | `MRX_Marx.GetEconomy()` | Wallets and ledger |
| `MRX_StashService` | `MRX_Marx.GetStash()` | Owned assets |
| `MRX_IdentityService` | `MRX_Marx.GetIdentity()` | Player ID to owner ID |
| `MRX_ShopService` | `MRX_Shop.GetService()` (Marx_Shop) | Buying and selling |

On clients these getters return null. Clients see their balances through `MRX_ClientWallet.GetLocal()` and talk to the
server through RPCs on `SCR_PlayerController` (`MRX_RequestBuy`, `MRX_RequestStashWithdraw`, ...).

## Owners

An owner is a player's Bohemia identity UUID (`MRX_Marx.GetOwnerId(playerId)`). It is known once the player passed
the audit; before that, and for players without a backend identity, it is empty and economy calls fail with
`OWNER_NOT_READY`. Owner IDs are plain strings, so a mod can also keep wallets for things that are not players
(e.g. a faction treasury) under its own IDs.

## Settings

Marx reads `Configs/Marx/MRX_Settings.conf` from `Marx_Core`. To change it, use **Override in addon** on that file in
your mod (or on `ChimeraSystemsConfig.conf` to point `MRX_MarxSystem` at another settings file).

| Setting | Default | Meaning |
|---|---|---|
| Backend | `NATIVE` | `NATIVE` = game persistence (falls back to memory when the world has no Marx persistence config), `IN_MEMORY` = lost on restart |
| Max recent entries | 50 | Ledger entries kept per wallet |
| Max recent keys | 200 | Idempotency keys kept per wallet and per stash; older keys are no longer detected as duplicates |
| Max stash assets | 100 | Stashed and deployed assets per owner, 0 = no limit |
| Loadout slots | 3 | Loadout slots every player has unlocked at a stash point (`Marx_Stash`), 0 = off; see [Stash](stash.md#loadouts) |
| Max loadout slots | 10 | Loadout slots the loadout window shows; the ones beyond a player's unlocked slots are locked until added |
| Loss policy | keep on death, restore after restart | See [Stash](stash.md) |
| Currencies | one `cash` currency | ID, initial balance, maximum balance, negative balances allowed |

## Persistent storage

With the default `NATIVE` backend, the world must use a systems config whose persistence config contains the Marx
collections. Otherwise Marx logs an error and keeps everything in memory. See [Storage](storage.md).

## Placing a shop or a stash point

Two sample prefabs:

- `Marx_Shop: Prefabs/Marx/Shop/MRX_ShopTable.et`: a table with `MRX_ShopComponent` (sample catalog) and the "Trade"
  action
- `Marx_Stash: Prefabs/Marx/Stash/MRX_StashWardrobe.et`: a wardrobe with `MRX_StashPointComponent` and the "Stash"
  action

Place them in your world, or inherit from them and change the components. See [Shops](shop.md) and [Stash](stash.md)
to add the components to your own prefabs.
