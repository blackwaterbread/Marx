# Using Marx in your mod

This guide walks you through adding Marx to your own mod, step by step: paying players, charging them, giving
items, placing a shop and a stash, and adding your own rules. It assumes you know your way around Workbench and
Enforce Script, but not Marx. Every step says *why*, not only *what*, and the
[Troubleshooting](#troubleshooting) section collects the mistakes that cost the most time.

Reference pages: [Getting started](getting-started.md),
[Economy](economy.md), [Shops](shop.md), [Stash](stash.md), [Storage](storage.md).

## What Marx gives you

| Feature | What it is | You use it to |
|---|---|---|
| Wallets and ledger | Balances per player in one or more currencies; every change is a logged transaction | Pay rewards, charge fees, move money between players |
| Shops | A component that turns any entity into a shop with a catalog file | Sell gear at your base, buy loot back |
| Stash | A personal storage at stash points, opened in the vanilla inventory; items keep attachments, ammo and contents | Let players keep gear between sessions |
| Owned assets | Items that belong to a player (stashed or out in the world) | Give item rewards, decide what happens on death |
| Admin commands | `#marx balance/give/take` in the chat and over RCON | Testing and support |

## Five rules to read first

Marx is built so that money and items cannot be cheated or lost. That shapes the API, and knowing these five rules
saves you most of the surprises.

1. **Everything runs on the server.** `MRX_Marx.GetEconomy()`, `GetStash()` and `GetIdentity()` return `null` on
   clients. Call them from server code (game mode components, server-side callbacks). Clients only send requests;
   Marx's own UI already does that for shops and stashes.
2. **Players are identified by an owner ID, not a player ID.** A player ID changes every time someone connects. The
   owner ID (the player's Bohemia identity) stays the same, so wallets survive reconnects and restarts. Get it with
   `MRX_Marx.GetOwnerId(playerId)`. It is empty for a moment after a player joins (until the identity check passed).
3. **Every change carries a context: source, reason and idempotency key.** The source is your mod's name, the reason
   is free text for the ledger, and the key says *which event* this is. A second call with the same key does
   nothing and answers `DUPLICATE`. This is what keeps a reward from being paid twice when your code runs twice.
4. **Results arrive later.** Calls do not return a result; you get it on a later frame through a callback object.
   Calls for the same player run one after another, in the order you made them.
5. **Amounts are whole numbers.** Prices and balances are `int`. Clients never send amounts; the server looks
   everything up itself.

## Step 1: Add Marx to your project

Marx is split into addons, so you only take what you need:

| Addon | GUID | Take it when |
|---|---|---|
| `Marx_Core` | `6A885105A7EC72B9` | Always. Wallets, stash service, API. No UI. |
| `Marx_UI` | `6A8A0C05BCEEF932` | Comes with Shop and Stash. Balance panel in the inventory, dialog base. |
| `Marx_Shop` | `6A885683BA928BB5` | You want shops. |
| `Marx_Stash` | `6A8A0C37A19F762B` | You want stash points. |

Marx is not on the Workshop yet. Until it is:

1. Get the Marx repository and make its addon folders (`addons/Core`, `addons/UI`, `addons/Shop`, `addons/Stash`)
   known to Workbench, e.g. by starting Workbench with `-addonsDir` listing them (see `tools/launch-workbench.ps1`).
2. Add the GUIDs of the addons you need to the `Dependencies` of your `addon.gproj`, next to the game's own
   (`58D0FB3206B6F859`):

   ```
   Dependencies {
    "58D0FB3206B6F859" "6A885105A7EC72B9" "6A885683BA928BB5" "6A8A0C37A19F762B" "6A8A0C05BCEEF932"
   }
   ```

3. Restart Workbench. The Marx system registers itself; there is nothing to place in the world for the economy.

## Step 2: Make sure data is saved

By default Marx stores everything with the game's own persistence system. That needs a persistence config with the
Marx collections, which belongs to your scenario's **systems config**. Without it Marx still works but keeps data
only in memory, and everything is gone after a restart.

**How to check:** start the game and look for this line in the log:

```
[MRX] Wallets are stored in the 'MarxWallets' collection
```

If you see `NATIVE storage unavailable` instead, data is not saved. Two ways to fix it:

- **Use Marx's ready config** (Game Master based scenarios): set
  `{F0E75858E593A833}Configs/Marx/Systems/MRX_GameMasterSystems.conf` as the World Systems Config of your mission
  header.
- **Merge into your own persistence config** if your scenario has one. [Storage](storage.md) lists what to add.

**Workbench tip:** the Play button's World Systems Config menu only lists configs that your *own* `addon.gproj`
names under `SystemModuleSettings > Configs`. Configs in dependencies do not show up. Add the line there:

```
SystemModuleSettings SystemModuleSettings "{5F10A1570304557B}" {
 Configs + {
  "{F0E75858E593A833}Configs/Marx/Systems/MRX_GameMasterSystems.conf"
 }
}
```

`{5F10A1570304557B}` is the game's own `SystemModuleSettings` object, and `Configs +` adds to its list. Workbench
forgets the selection after a restart; pick it again from the Play menu.

## Step 3: Pay players from your code

The usual place for server logic is a game mode component. This one pays a reward when your mod reports a finished
contract:

```c
[ComponentEditorProps(category: "MyMod", description: "Pays players for finished contracts")]
class MYMOD_ContractPayComponentClass : SCR_BaseGameModeComponentClass
{
}

class MYMOD_ContractPayComponent : SCR_BaseGameModeComponent
{
	//! Your mod's name in the ledger. Keys are only compared within the same source.
	static const string SOURCE = "mymod";

	//! Call on the server when a player finished a contract.
	void PayContract(int playerId, string contractId, int amount)
	{
		MRX_EconomyService economy = MRX_Marx.GetEconomy();
		if (!economy)
			return; // a client, or Marx is not running

		string ownerId = MRX_Marx.GetOwnerId(playerId);
		if (ownerId.IsEmpty())
			return; // the player just joined; see Troubleshooting

		// One key per contract and player: a second call for the same contract pays nothing.
		string key = string.Format("contract:%1:%2", contractId, ownerId);
		MRX_TxContext context = MRX_TxContext.Create(SOURCE, "contract reward", key);

		MRX_TxCallback callback = new MRX_TxCallback();
		callback.GetOnResult().Insert(OnPaid);
		economy.Credit(ownerId, "cash", amount, context, callback);
	}

	protected void OnPaid(MRX_TxResult result)
	{
		// OK, or DUPLICATE when this contract was paid before.
		if (result.IsCommitted())
			return;

		Print("Contract reward failed: " + typename.EnumToString(MRX_ETxStatus, result.m_eStatus), LogLevel.WARNING);
	}
}
```

Add the component to your game mode entity and call `PayContract` from your contract logic.

**Choosing the key** is the one thing to get right:

| Event | Good key | Why |
|---|---|---|
| A contract finished | `contract:<contractId>:<ownerId>` | Each player is paid once per contract |
| A kill bounty | `kill:` + `MRX_Marx.NewId()` | Kills have no ID of their own, so make one per kill |
| A daily login bonus | `daily:<ownerId>:<date>` | Once per day |
| Retrying after `STORAGE_ERROR` | *the same key as the first try* | If the first try went through, the retry answers `DUPLICATE` instead of paying again |

Never build a new random key for a retry; that is how double payments happen. Also note that Marx remembers the last
200 keys per wallet (a setting): keys protect against retries and repeated events, not "once in a lifetime". For a
one-time bonus, remember yourself that it was given.

### Charging and moving money

`Debit` takes money; it fails with `INSUFFICIENT_FUNDS` instead of going negative (unless the currency allows
negative balances). If you need to know who the result belongs to, subclass the callback:

```c
class MYMOD_FeeCallback : MRX_TxCallback
{
	protected int m_iPlayerId;

	void MYMOD_FeeCallback(int playerId)
	{
		m_iPlayerId = playerId;
	}

	override void OnResult(MRX_TxResult result)
	{
		super.OnResult(result);
		if (result.m_eStatus == MRX_ETxStatus.INSUFFICIENT_FUNDS)
			Print(string.Format("Player %1 cannot pay the fee", m_iPlayerId));
		else if (result.IsCommitted())
			Print(string.Format("Player %1 paid the fee", m_iPlayerId));
	}
}

// ...
MRX_TxContext context = MRX_TxContext.Create("mymod", "operation fee", "fee:" + operationId + ":" + ownerId);
MRX_Marx.GetEconomy().Debit(ownerId, "cash", 200, context, new MYMOD_FeeCallback(playerId));
```

`Transfer(fromOwnerId, toOwnerId, currency, amount, context, callback)` moves money between two owners in one
transaction. Owners do not have to be players: any string works, e.g. `"faction:US"` for a faction treasury.

## Step 4: Give items

Items that belong to a player are *assets*. To give one as a reward, grant it into the player's stash; the player
takes it out at a stash point:

```c
ResourceName prefab = "{A81F501D3EF6F38E}Prefabs/Items/Medicine/FieldDressing_01/FieldDressing_US_01.et";
MRX_TxContext context = MRX_TxContext.Create("mymod", "contract loot", "loot:" + contractId + ":" + ownerId);
MRX_Marx.GetStash().Grant(ownerId, prefab, context);
```

Stashed and taken-out items are tracked: when a player dies with them, the loss policy in the settings decides
whether they stay on the body (default), are lost, or go back to the stash. When your mod uses an owned item up
(e.g. a deployable that is consumed), report it with `MarkConsumed(item, context)` so it does not come back.

## Step 5: Put a shop and a stash in your world

### The quick way

Place the two sample prefabs and try them in Play:

- `Marx_Shop: Prefabs/Marx/Shop/MRX_ShopTable.et`: a table with a "Trade" action and the sample catalog
- `Marx_Stash: Prefabs/Marx/Stash/MRX_StashWardrobe.et`: a wardrobe with a "Stash" action

### Your own shop

1. **Make a catalog.** In Workbench's Resource Browser, create a config of class `MRX_ShopCatalog` in your mod and
   add `MRX_ShopItem` entries:

   | Field | What to put |
   |---|---|
   | ID | A short unique name, e.g. `m16_mag`. It is written to the ledger. |
   | Prefab | The item prefab |
   | Name | Optional; empty shows the item's own name |
   | Category | Optional; becomes a filter tab in the shop window |
   | Currency | `cash` unless you added currencies |
   | Price | What the player pays; 0 = not for sale |
   | Sell price | -1 = the shop's percentage of the price, 0 = the shop does not buy it back |

2. **Make the shop entity.** Inherit from `MRX_ShopTable.et`, or add to any prefab:
   - `MRX_ShopComponent`: shop ID (for the ledger), display name (the window title), your catalog, sell percentage,
     whether it buys back, trading distance
   - an enabled `RplComponent`
   - an `ActionsManagerComponent` with a context and `MRX_OpenShopAction` in it

The client only asks; the server checks distance, price, room in the inventory and your validators, takes the
money, and hands over the item. If handing over fails, the money comes back.

### Your own stash point

Inherit from `MRX_StashWardrobe.et`, or add `MRX_StashPointComponent` (access distance), an enabled `RplComponent`
and `MRX_OpenStashAction` to any prefab. The stash opens as its own panel in the vanilla inventory: 3 pages of 6 x 8
cells by default, every item remembers its cell. Players drag items in and out like in any container.

## Step 6: React to what happens

Subscribe on the server when the game mode starts:

```c
override void OnGameModeStart()
{
	super.OnGameModeStart();

	MRX_ShopService shops = MRX_Shop.GetService();
	if (!shops)
		return; // not the server

	shops.GetOnPurchase().Insert(OnPurchase);
}

protected void OnPurchase(int playerId, string ownerId, MRX_ShopDefinition shop, MRX_ShopItem item, int price)
{
	Print(string.Format("%1 bought %2 for %3", playerId, item.m_sId, price));
}
```

| Event | Where | Fires |
|---|---|---|
| `GetOnBalanceChanged()` `(ownerId, currency, balance)` | `MRX_Marx.GetEconomy()` | After every balance change of a connected player |
| `GetOnTransactionCommitted()` `(MRX_LedgerEntry)` | `MRX_Marx.GetEconomy()` | For every ledger entry |
| `GetOnPurchase()`, `GetOnSale()` | `MRX_Shop.GetService()` | After a trade went through |
| `GetOnAssetStateChanged()` `(asset, oldState)` | `MRX_Marx.GetStash()` | When an owned item is stashed, taken out, lost or consumed |
| `GetOnOwnerReady()`, `GetOnOwnerLeft()` `(playerId, ownerId)` | `MRX_Marx.GetIdentity()` | When a player's owner ID becomes known, and when the player leaves |

## Step 7: Add your own rules

Validators let you refuse a trade or a transaction. Return `false` to refuse; the request then fails with
`REJECTED`.

```c
//! Only the US faction may buy from the "Weapons" category.
class MYMOD_FactionShopValidator : MRX_ShopValidator
{
	override bool CanBuy(int playerId, string ownerId, MRX_ShopDefinition shop, MRX_ShopItem item)
	{
		if (item.m_sCategory != "Weapons")
			return true;

		Faction faction = SCR_FactionManager.SGetPlayerFaction(playerId);
		return faction && faction.GetFactionKey() == "US";
	}
}

// In OnGameModeStart, on the server:
MRX_Shop.GetService().AddValidator(new MYMOD_FactionShopValidator());
```

The same pattern exists for money (`MRX_TxValidator.Validate`, added with `MRX_Marx.GetEconomy().AddValidator`) and
for the stash (`MRX_StashValidator.CanDeposit` / `CanWithdraw`, added with `MRX_Marx.GetStash().AddValidator`).

## Step 8: Show money in your own UI

Marx already shows the balance below the quick slots of the inventory (`MRX_BalancePanel`; turn it off with
`MRX_BalancePanel.SetShownInInventory(false)`). For your own UI, read the client-side copy of the local player's wallet:

```c
MRX_ClientWallet wallet = MRX_ClientWallet.GetLocal();
int cash;
if (wallet && wallet.TryGetBalance("cash", cash))
	m_wCashText.SetText(cash.ToString());

// Updates: (string currency, int balance)
wallet.GetOnBalanceChanged().Insert(OnBalanceChanged);
```

It is read-only. `GetLocal()` returns `null` until the player controller exists, so call it when your UI opens, not
at game start.

## Settings

Marx reads `Configs/Marx/MRX_Settings.conf` from `Marx_Core`. To change it, right-click the file in the Resource
Browser and choose **Override in addon** for your mod. What you can change:

- **Currencies:** ID, starting balance, maximum, negative balances allowed. The default is one `cash` currency.
- **Loss policy:** what happens to taken-out items when a player dies (keep on the body, lose, return to the stash)
  and after a server restart (back to the stash, or lost).
- **Limits:** assets per stash (100), ledger entries and idempotency keys kept per wallet.
- **Backend:** native persistence (default) or memory only.

Stash pages are set on the stash container (`m_iMaxPages` on `MRX_StashStorageComponent`), shop percentages on each
shop.

## Testing in Workbench

- Give yourself money with `#marx give <playerId> <amount>` in the chat (player ID 1 in Workbench); `#marx balance`
  shows your own balance. See [Admin commands](admin.md).
- Watch the log: Marx prints `[MRX]` lines about storage and errors.
- If `GetOwnerId` stays empty in Workbench, the Bohemia backend may be unreachable; the player then has no identity
  and Marx refuses to guess one.

## Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| `MRX_Marx.GetEconomy()` is `null` | Called on a client, or before the systems started | Call from server code; check `Replication.IsServer()` |
| Calls fail with `OWNER_NOT_READY` | The player just joined, or has no Bohemia identity | Wait for `GetOnOwnerReady()`, or try again later |
| Money or stash is empty after a restart | The world has no Marx persistence config | Step 2; look for the "stored in the 'MarxWallets' collection" log line |
| The Marx systems config is missing in the Play menu | It is only listed in Marx's own project | Add it to your `addon.gproj` (Step 2) |
| A reward was paid twice | A new key on every call, e.g. `NewId()` on retry | Build the key from the event (Step 3) |
| A reward was never paid for later events | The same key for different events | Put the event's ID into the key |
| Calls fail with `INVALID_CONTEXT` | Empty source or key | Always pass both |
| Items come back twice after a restart | Restoring taken-out items in a scenario that also saves the world | Set the restart loss policy to lose |
| The shop window is empty | The catalog failed to load or every price is 0 | Check the log for catalog errors |
| No "Trade"/"Stash" action | Missing `RplComponent`, action, or action context | Compare with the sample prefabs |

## Where to go next

- [Economy](economy.md): every call, status code and event
- [Shops](shop.md), [Stash](stash.md): details and the default UI
- [Storage](storage.md): the persistence configs, writing your own backend
- `addons/Example/Scripts/Game/Marx/Example/MRX_ExampleBountyComponent.c`: a complete small consumer
