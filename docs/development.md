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

## Workbench plugins (Plugins > Marx)

- **Create Marx Reference Configs**: creates the reference persistence and systems configs in `Marx_Core`, and adds
  the stash collection to an existing persistence config.
- **Create Marx Sample Assets**: creates the sample catalog and the shop table prefab in `Marx_Shop` and the stash
  wardrobe prefab in `Marx_Stash`. It needs an open world and leaves it marked as modified; do not save that world.

Existing files are never overwritten. Resource GUIDs and object IDs come from Workbench.

## Conventions

See `AGENTS.md`: `MRX_` prefix, Enforce Script naming (`m_` + type letter), server-authoritative RPCs that carry IDs
only, every balance change through `MRX_EconomyService`, `int` money, and Workbench as the compiler of record.
