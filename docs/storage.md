# Storage

Marx keeps wallets and stashes behind `MRX_StorageBackend`. Two backends exist today:

| Backend | Durable | Notes |
|---|---|---|
| `MRX_NativeBackend` (`NATIVE`, default) | Yes | Game persistence system, Marx's own `GamemodeStorage` collections, committed after every change |
| `MRX_InMemoryBackend` (`IN_MEMORY`) | No | Tests, and the fallback when native storage is not configured |

## How native storage works

Each owner has one persistent scripted state per collection, identified by an ID derived from the owner ID:

| Collection | State | Serializer |
|---|---|---|
| `MarxWallets` | `MRX_WalletState` | `MRX_WalletStateSerializer` |
| `MarxStashes` | `MRX_StashState` | `MRX_StashStateSerializer` |

After every change Marx saves the state and calls `CommitStorage(GamemodeStorage)`, so a change is on disk before the
caller's callback reports OK. A failed commit restores the previous state; a commit that does not answer within 10 s
reports `STORAGE_ERROR` without restoring, because it may still succeed (retry with the same idempotency key).

Data is stored per mission: `profile/.save/<app>_<user>/game/<mission>/gamemode/<collection>/<id>.bin`.

## Selecting the Marx persistence config

The persistence config is part of the world's **systems config**, which belongs to the scenario. Marx does not change
the vanilla persistence configs; it ships reference configs instead:

- `Marx_Core: Configs/Marx/Persistence/MRX_GameMasterPersistence.conf`: inherits the vanilla Game Master persistence
  config and adds a `GamemodeStorage`, the two Marx collections and their state configs
- `Marx_Core: Configs/Marx/Systems/MRX_GameMasterSystems.conf`: inherits the vanilla Game Master systems config and
  points `SCR_PersistenceSystem` at the persistence config above

Choose one of these ways:

1. **Use the reference systems config.** Set `MRX_GameMasterSystems.conf` as the World Systems Config of your mission
   header (in Workbench Play: the Play button menu > World Systems Config). Suitable for Game Master based scenarios.
2. **Merge into your own configs.** If your scenario has its own persistence config, add to it:
   - a `GamemodeStorage` in `Storages` (database: the vanilla "Main" database)
   - two `PersistenceCollection` entries named `MarxWallets` and `MarxStashes` that use that storage
   - under `Configurations > ScriptedStates > Gameplay`, a `StatePersistenceConfig` per collection with the serializer
     from the table above

   Open `MRX_GameMasterPersistence.conf` in Workbench as a template. Collection names may only contain letters,
   digits, hyphens and dots.

When the world has no persistence system, or the persistence config has no `MarxWallets` collection, Marx logs an
error and uses memory for everything. Without the `MarxStashes` collection only the stash falls back to memory.
The log line `Wallets are stored in the 'MarxWallets' collection` confirms native storage.

## Custom backends

Subclass `MRX_StorageBackend` and implement every method. Callbacks must run on a later frame, also for local work.
Backends that keep records themselves can reuse `MRX_WalletMath.Apply` and `MRX_StashMath.Apply`, which implement the
transaction and stash rules (limits, idempotency, state transitions) without engine dependencies. To use a custom
backend, mod `MRX_MarxSystem.CreateBackend()`:

```c
modded class MRX_MarxSystem
{
	override protected MRX_StorageBackend CreateBackend()
	{
		return new MyDatabaseBackend();
	}
}
```
