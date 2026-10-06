# Marx

Private-property framework for Arma Reforger: wallets with a ledger, a shop, a stash of owned items, and a script API
that other mods build on. Everything is server-authoritative; clients only send requests by ID.

Status: **API v0** (breaking changes are still possible). Target game version: 1.8.

## Addons

| Addon | Project ID | Contents |
|---|---|---|
| `addons/Core` | `Marx_Core` | Economy (wallets, ledger), identity, stash and owned assets, storage backends, client wallet mirror, stash point component and RPCs, admin command, public API (`MRX_Marx`) |
| `addons/UI` | `Marx_UI` | Default UI building blocks: script-built dialog base, wallet HUD |
| `addons/Shop` | `Marx_Shop` | Shops (component, catalogs, buy/sell, RPCs), shop dialog, arsenal shop, sample catalog, shop table and arsenal box prefabs |
| `addons/Stash` | `Marx_Stash` | Stash in the vanilla inventory: stash point action, personal container, stash wardrobe prefab |
| `addons/Example` | `Marx_Example` | Example consumer code and the Workbench test harness |

Depend on `Marx_Core` for the API only (no UI). Add `Marx_Shop` and/or `Marx_Stash` for the ready-made shop and
stash point; both bring `Marx_UI`, which also shows the wallet HUD.

## Features

- **Wallets:** several currencies (`int` amounts), credit/debit/transfer, limits, per-owner ordering, idempotency keys,
  recent ledger entries per wallet. Balances are pushed to the owning client.
- **Shops:** a component that turns any entity into a shop, `.conf` catalogs, purchase into the player's inventory,
  sell-back at a configurable percentage, validators for custom rules. A vanilla arsenal can sell a catalog for Marx
  money in its own inventory window.
- **Stash:** a personal stash that opens as its own panel of the vanilla inventory at stash points (pages of cells;
  every item keeps its cell). Stored items keep their attachments, magazines, contents, damage and fuel. Assets move through `STASHED`, `DEPLOYED`, `LOST` and `CONSUMED`; a loss policy decides what happens on death
  and after a server restart.
- **Storage:** the game's own persistence system (`GamemodeStorage`, committed after every change), with an in-memory
  fallback. A REST backend for shared databases is planned.
- **Admin:** `#marx balance|give|take` in the chat for logged-in administrators, and over RCON.

## Documentation

- **[Using Marx in your mod](docs/guide.md)**: step-by-step guide for mod authors
- [Getting started](docs/getting-started.md): dependencies, server setup, settings
- [Storage](docs/storage.md): persistent storage, the reference configs, merging them into your scenario
- [Economy API](docs/economy.md): owners, currencies, transactions, callbacks, events
- [Shops](docs/shop.md): shop component, catalogs, UI
- [Stash](docs/stash.md): owned assets, deposit and withdraw, loss policy
- [Admin commands](docs/admin.md)
- [Development](docs/development.md): Workbench setup and the test harness

## Quick example

```c
// Server side: pay a player 50 cash once per mission completion.
string ownerId = MRX_Marx.GetOwnerId(playerId);
MRX_TxContext context = MRX_TxContext.Create("my_mod", "mission_complete", "mission:" + missionId + ":" + ownerId);
MRX_Marx.GetEconomy().Credit(ownerId, "cash", 50, context);
```

See `addons/Example/Scripts/Game/Marx/Example/MRX_ExampleBountyComponent.c` for a complete consumer.
