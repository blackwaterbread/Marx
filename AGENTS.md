# AGENTS.md — Marx

Marx: generic private-property framework mod for Arma Reforger (wallets, ledger, stash, owned items/vehicles, shops, public API). Other mods depend on it. First consumer: a separate PvE random-operations mod (not in this repo).

## Platform
- Arma Reforger, Enfusion engine, Enforce Script (`.c`). Target: stable 1.8, experimental 1.9.
- Workbench (Arma Reforger Tools) is the compiler of record. Code is not "done" until it compiles in Workbench.
- Your Enfusion API knowledge may be outdated. Verify classes/methods against current game scripts (Workbench / MCP API search) before using them. Never invent APIs. BI wiki pages may lag behind 1.8.
- MCP API search indexes can be older than the installed game. Confirm signatures and `[Obsolete]` attributes by reading the actual game script files.

## 1.8 API notes
- Player identity: call `SCR_PlayerIdentityUtils.GetPlayerIdentityId` only inside `MRX_IdentityService`. Server-only, valid only after `OnPlayerAuditSuccess`. Reject empty/null IDs.
- REST: `RestContext` async methods only, with `RestCallback.SetOnSuccess` / `SetOnError`. Never use `*_now` methods or the obsolete `OnSuccess`/`OnError`/`OnTimeout` overrides.
  - 4xx/5xx arrive in OnError (`EREST_ERROR_UNKNOWN`); read `GetHttpCode()` / `GetData()` there. `SetTimeout` is in seconds and counts queue time, so a TIMEOUT means "outcome unknown" — retry with the same idempotency key.
- JSON: use `JsonSaveContext` / `JsonLoadContext`. `SCR_JsonSaveContext` / `SCR_JsonLoadContext` are obsolete.
- Persistence: Marx data lives in its own `GamemodeStorage` collection; commit it with `CommitStorage(GamemodeStorage)`. Never call `CommitStorage(PersistenceSessionStorage)` from Marx (it flushes world save points). `BackendApi.GetStorage()` / `SessionStorage` are obsolete.
- Persistent scripted states (per-player records): `new` → `SetId(obj, deterministicUuid)` → `RequestLoad` → modify → `Save`. Never load them with `RequestSpawn` and re-track: the instance comes back untracked and re-tracking saves a duplicate record under a new UUID.
- Never call `PersistenceSystem.DeserializeSpawn` / `DeserializeLoad` with script-built contexts: it hard-crashes the engine. Stash snapshots use Marx's own JSON format (spawn prefab, then restore state through component APIs).

## Repo layout
```
addons/Core/     gproj ID Marx_Core     services, models, storage, entity bridge, RPC, admin, public API
addons/Shop/     gproj ID Marx_Shop     shop logic, prefabs, catalogs, UI layouts (depends on Core)
addons/Example/  gproj ID Marx_Example  minimal consumer sample
assets-src/      raw sources (outside gproj dirs = not packed)
tools/           build/validation scripts
```
- Dependency direction: Example → Shop → Core. Core must never reference Shop/Example.
- Everything inside an addon dir gets packed. Keep raw sources in `assets-src/`.

## Architecture rules (non-negotiable)
- Server-authoritative. Clients only send requests via RPC; RPCs carry IDs, never prices/amounts/permissions. Server resolves everything from config/state.
- All storage access goes through `MRX_IStorageBackend`. All calls are async (callback-based), even for local backends.
- Backends: InMemory (tests), Native (Reforger PersistenceSystem), REST (external, via `RestContext`). EPF is deprecated — do not use or reference it.
- Every balance change goes through `MRX_EconomyService` with an `MRX_TxContext` (source, reason, idempotency key). Never mutate balances directly. Ledger is the source of truth.
- Currency amounts are `int`. No floats for money.
- Asset state machine: `STASHED` / `DEPLOYED` / `LOST` / `CONSUMED`. `DEPLOYED → STASHED` only via the entity bound to that record. Withdraw = lock record → spawn → commit; rollback on spawn failure.
- No consumer-specific logic (e.g. PvE ops) in Marx. Generalize into the API or leave it out.
- Keep domain logic separable from engine entity APIs where practical.

## Conventions
- Script class prefix `MRX_`. Script path: `Scripts/Game/Marx/...`.
- Follow BI Enforce Script conventions: `m_` member prefix + type letter (`m_iCount`, `m_sName`, `m_bActive`, `m_aItems`, `m_mLookup`), `s_` statics, `UPPER_CASE` constants, PascalCase methods.
- Use `modded class` to extend vanilla. Never copy-paste or edit base game files.
- Mind `ref` ownership on `Managed` members; avoid strong ref cycles.
- Public API surface lives in `addons/Core/.../API/`. Mark it `v0` until stabilized; breaking changes allowed until `v1`.
- Code, comments, identifiers: English.

## Resources & GUIDs
- Never invent, copy, or hand-edit resource GUIDs or `.meta` files. Let Workbench generate them.
- Prefabs (`.et`), layouts (`.layout`), configs (`.conf`): prefer authoring in Workbench. Text edits must keep `{GUID}path` references intact.
- Commit `.meta` files. Do not commit `resourceDatabase.rdb`.

## Don'ts
- Don't add features outside the current task scope.
- Don't trust client data. Don't put secrets/tokens in addon files.
- Don't assume an API exists because it existed in Arma 3 / DayZ / older Reforger.