# Development

## Opening the project

`tools/launch-workbench.ps1` starts Arma Reforger Workbench with the Marx addons registered, without the launcher:

```powershell
powershell -ExecutionPolicy Bypass -File tools/launch-workbench.ps1 [-Tests] [-AutoCloseTests] [-TestIdentity] [-PeerTest]
```

It opens `Marx_Example` (which loads all Marx addons), writes the logs to a new
`Documents\My Games\ArmaReforgerWorkbench\logs\logs_<timestamp>` folder and finds Workbench and the game through the
Steam library folders. `-DryRun` prints the command line only.

## Test harness

`addons/Example/Scripts/Game/Marx/Tests/` (Workbench only, `#ifdef WORKBENCH`) runs on server game start in Play mode
when Workbench was started with `-mrxTests`, and logs `[MRX_TEST] PASS|FAIL|SKIP <test>` and `[MRX_TEST] DONE passed= failed= skipped=`.

- Unit-style tests use the in-memory backend and fake worlds (economy, shop, stash rules and services).
- Native tests need the Marx persistence config: select `Configs/Marx/Systems/MRX_GameMasterSystems.conf` under
  Play > World Systems Config. Without it they are skipped.
- Entity tests (shop table, stash round trip, stash point) spawn real prefabs. In Game Master scenarios they spawn a
  character for the local player and possess it.

Command line switches (passed by the launch script):

| Switch | Effect |
|---|---|
| `-mrxTests` | runs the tests whenever Play mode starts (launch script: `-Tests`, implied by `-AutoCloseTests` and `-PeerTest`) |
| `-mrxTestsAutoClose` | leaves Play mode 2 s after the run |
| `-mrxTestIdentity` | players without a backend identity get a name-based test owner, so owner-dependent tests also run while the Bohemia backend is unreachable |
| `-mrxTestsPeer` | waits up to 120 s for a PeerTool client and checks the balance push to it (otherwise skipped) |

Typical loop: open `worlds/GameMaster/GM_Arland.ent`, press Play, read the `[MRX_TEST]` lines in the log.

## Debug actions

For trying things by hand, Marx has debug actions (`Scripts/Game/Marx/Debug/` in Core, `Debug/` folders in the other
addons). Each has an ID `<page>.<name>` and positional values; missing or empty values take the default. Three entry
points run the same actions:

- **Debug panel** (diag builds: Workbench, PeerTool peers, diag executables): open the diag menu (Win+Alt) and turn on
  **Marx > Debug panel**. One page per area, a button and value fields per action, the player, owner and wallet, and
  the latest results.
- **File command** (Workbench Play only): write one line `<action ID> [values...]` to `$profile:mrx_cmd.txt`
  (`Documents\My Games\ArmaReforgerWorkbench\profile\`); the host reads it every half second and deletes it. Meant for
  automation that cannot type in the game view.
- **Chat or RCON**: `#mrxdbg <action ID> [values...]` (administrators; `#mrxdbg` alone lists the IDs). Only actions
  without a client part; over RCON there is no player, so actions that need a character fail.

Every result is logged as `[MRX_DBG] <action ID> <STATUS> <text>`. Server parts run only in developer builds
(`Game.IsDev()`); elsewhere the server answers `REJECTED`. Consumer mods add their own actions in a
`modded class MRX_DebugRegistry` (`RegisterActions`, after `super.RegisterActions()`).

| ID | Values | Does |
|---|---|---|
| `marx.info` | | player, owner ID, identity, storage backend, economy state |
| `marx.char` | player | gives a player without a character (e.g. a PeerTool client in Game Master) one next to you |
| `marx.item` | prefab, count=1 | spawns items into your inventory |
| `marx.inv`, `marx.close`, `marx.pause` | | opens the inventory, closes the menus, opens the pause menu |
| `marx.equip`, `marx.view`, `marx.kill` | | takes the first weapon in hand, switches 1st/3rd person, kills your character |
| `marx.hands`, `marx.gear` | | what you hold, what you carry (issued items, magazine rounds), as the server sees it |
| `wallet.give`, `wallet.take` | amount, currency=default, player=me | changes a balance through the ledger (source `marx_debug`) |
| `wallet.history` | count=10, currency=default | your latest ledger entries (`all` for every currency) |
| `ui.lang` | code=ko_kr | switches the UI language |
| `ui.dialog` | kind=basic | a sample `MRX_ScriptedDialog` (`basic`, `scroll`, `items`) |
| `ui.gallery` | | the Marx widgets in their states |
| `shop.open` | | opens the nearest shop within 30 m, or spawns the sample shop table |
| `stash.open` | | opens the stash at the nearest stash point within 30 m, or spawns the sample stash wardrobe |
| `stash.kit` | | puts three backpacks and a few small items into your stash (reopen it to see them) |
| `stash.fromhands` | | moves the weapon in your hands into the open stash |
| `loadout.save` | slot=0 | saves your gear into a slot of the open stash (needs a price list) |
| `loadout.window` | | opens the loadout window |
| `loadout.load` | slot=0 | puts a slot on as the Load button of the open loadout window does |
| `loadout.addslot` | | unlocks one more loadout slot (shown from the next opening of the stash) |
| `sample.product` | | Example, Workbench only: adds a sample product to the nearest shop |
| `sample.prices` | | Example, Workbench only: sets the test prices as the price list |

## Workbench plugins (Plugins > Marx)

- **Create Marx Reference Configs**: creates the reference persistence and systems configs in `Marx_Core`, and adds
  the stash collection to an existing persistence config.
- **Create Marx Sample Assets**: creates the sample catalog and the shop table prefab in `Marx_Shop` and the stash
  wardrobe prefab in `Marx_Stash`. It needs an open world and leaves it marked as modified; do not save that world.

Existing files are never overwritten. Resource GUIDs and object IDs come from Workbench.

## Conventions

See `AGENTS.md`: `MRX_` prefix, Enforce Script naming (`m_` + type letter), server-authoritative RPCs that carry IDs
only, every balance change through `MRX_EconomyService`, `int` money, and Workbench as the compiler of record.
