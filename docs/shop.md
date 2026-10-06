# Shops

Part of `Marx_Shop`.

## Setting up a shop

Any entity becomes a shop with:

- `MRX_ShopComponent`: shop ID (written to the ledger), catalog, sell percentage (default 50), whether the shop buys
  items back, trading distance (default 5 m)
- an enabled `RplComponent`
- an `ActionsManagerComponent` with `MRX_OpenShopAction` in one of its contexts

`Prefabs/Marx/Shop/MRX_ShopTable.et` is a ready example. A vanilla arsenal can be a shop too, see
[Arsenal shop](#arsenal-shop).

## Catalogs

A catalog is an `MRX_ShopCatalog` config with a list of `MRX_ShopItem`:

| Field | Meaning |
|---|---|
| ID | Unique in the catalog; sent in purchase requests and written to the ledger reason |
| Prefab | Item prefab |
| Name, Category | Display only; an empty name shows the prefab name |
| Currency | Default `cash` |
| Price | Purchase price; 0 or less = not for sale |
| Sell price | -1 = the shop's sell percentage of the price, 0 = not bought back, otherwise the amount paid |
| Stock | Reserved, -1 (stock is not enforced yet) |

Invalid entries (missing or duplicate ID, no prefab, unknown currency) are dropped with an error in the log.
`Configs/Marx/Shop/MRX_SampleCatalog.conf` lists seven vanilla items.

**Default contents.** Many prefabs come with other items: a rifle with a loaded magazine and optics, a vest with armor
plates or filled pouches, a medical kit with its contents. A price must cover them, or players earn money by buying the
item and selling it back (in one piece at an arsenal shop, or its parts one by one). `MRX_ShopContentsCheck` finds such
items, see [Script API](#script-api).

## Buying and selling

- **Buy:** the server checks the item, the price and the validators, checks that the player's inventory has room,
  debits the price, then spawns the item into the inventory. If the delivery fails, the price is refunded.
- **Sell:** only items of the shop's catalog, carried by the player, not locked, and empty (no attachments, loaded
  magazine or contents). The item is removed first, then the price is credited. If the credit fails, Marx tries to
  give the item back.
- One request per player at a time; requests closer than 250 ms are refused with `BUSY`.

Ledger source: `marx_shop`. Status codes: `MRX_EShopStatus`.

## Arsenal shop

The vanilla arsenal window can sell a Marx catalog for Marx money instead of supplies. Players use it as usual: drag an
item into a bag, vest or weapon attachment slot, or right-click it to buy; drop an item of their inventory on the arsenal
(or right-click it, "Deposit") to sell it.

Setup: on an entity with `SCR_ArsenalComponent` and `SCR_ResourceComponent` (every vanilla arsenal box has both), add
`MRX_ShopComponent` (shop ID, catalog, sell percentage) and `MRX_ArsenalShopComponent`. Disable saved loadouts on the
arsenal (`m_eArsenalSaveType` `SAVING_DISABLED`): a loadout saved there would respawn with unpaid gear.
`Prefabs/Marx/Shop/MRX_ArsenalBox.et` is a ready example with the sample catalog.

What changes on such an arsenal:

- It lists the catalog items that have a price, in catalog order, instead of the faction's arsenal items. Faction,
  item type and rank filters of the arsenal do not apply.
- It opens as its own panel the size of the vicinity panel, which is hidden meanwhile. Every item shows its price and
  is greyed out when the player cannot pay it; prices of one-cell items are short ("$14k"). Below the items, a balance
  panel shows the player's balance. After a trade it counts to the new value and briefly shows the change ("-$20",
  "+$510"); failures ("Not enough money") and the item count of a sale with contents show in a line below it.
- Above the items, a filter row narrows the list: a category button drops down the catalog's categories with their item
  counts (the choice is kept per shop for the session), and a search field keeps the items whose display name or item
  ID contains every word typed (the inventory's keys are blocked while typing; Enter, Escape or a click elsewhere ends
  it). Mouse and keyboard only.
- Buying debits the price and spawns the item where the player dropped it (a bag, a vest pouch, a weapon's attachment
  slot), or into the inventory. Selling buys back the item with everything it holds (attachments, magazines, stored
  items): the price is the sum of their sell prices. Contents the shop does not buy are removed with it and count 0; the
  result message says how many. Items that are not refundable (mission items) are never sold, alone or inside another
  item.
- The server checks requests like vanilla arsenal requests (an alive character within 30 m that may use the arsenal,
  a target storage of its own or a nearby one of nobody's) and refuses vanilla supply requests for it.
- Its support station actions (the arsenal box's resupply actions) are hidden: they would hand out listed magazines and
  medical items for free.
- It cannot be disabled (`SCR_ArsenalComponent.SetArsenalEnabled(false)` is ignored), since the vanilla inventory
  refuses to sell items to a disabled arsenal.

Large catalogs take a moment to open the first time: the window creates a preview of every item (about 2 ms each). Marx
creates them a few per frame while the local player is within 30 m of the arsenal.

## Script API

`MRX_Shop.GetService()` (server) offers `Buy(playerId, shop, itemId, callback, target)` and
`Sell(playerId, shop, item, callback, withContents)`, the events `GetOnPurchase()` and `GetOnSale()`
`(playerId, ownerId, shop, item, price)`, and `AddValidator(MRX_ShopValidator)` with `CanBuy` and `CanSell` for custom
rules (faction, rank, ...). `target` (optional, `MRX_ShopStorageTarget.Create(storage)`) is the storage the item goes
to; `withContents` sells the item with everything it holds (`GetSellPriceWithContents` computes the price). Inventory
access sits behind `MRX_ShopInventory`; `MRX_EntityShopInventory` is the engine implementation.
`MRX_ArsenalRequests.Buy`/`Sell` add the arsenal checks. `MRX_ArsenalShopComponent.GetListedItems(items)` and
`GetCategories()` return the catalog entries and categories an arsenal lists.

`MRX_ShopComponent.SetDefinition(definition)` replaces a shop's catalog and settings at runtime (call it with the same
definition on the server and every client).

Clients call `SCR_PlayerController.MRX_RequestBuy(shopEntity, itemId)` and `MRX_RequestSell(shopEntity, item)` (arsenal:
`MRX_RequestArsenalBuy(arsenal, storageId, itemId)`, `MRX_RequestArsenalSell(arsenal, itemId)`) and get the answer from
`MRX_GetOnShopResult()`, with the counts of a sale with contents.

`MRX_ShopContentsCheck` (server, development tool) spawns every item of a shop once, locally, and reports what each
prefab holds by default and what it sells back for; `IsProfitable()` marks items whose price does not cover their
default contents. Each entry also lists the content prefabs (`m_aContents`), so a price generator can price them
itself. `WriteReport(path, entries)` writes the result as CSV, e.g. for a price generator. It takes about 15
ms per item, spread over frames; run it in Workbench or a test, not on a live server.

## Default UI

- `MRX_ShopMenu`: a shop window titled with the shop's display name (`m_sDisplayName` on `MRX_ShopComponent`). It
  shows the balance and the outcome of the last trade, a Buy tab with a category filter (the catalog items'
  `m_sCategory`) and a Sell tab with the sellable items the player carries. Every item has a 3D preview, its name (the
  catalog name, or the item's own inventory name), its price and a button; prices the player cannot pay are red and
  their button is disabled. One item per trade. The Buy tab shows 20 items per page (`MRX_ShopMenu.PAGE_SIZE`) with
  previous and next buttons, so large catalogs stay responsive; the category buttons wrap after six per row.
- Built on `MRX_ScriptedDialog` (`Marx_UI`) from vanilla parts in script (the wide configurable dialog, widget
  library buttons and toolbox, the dialog scroll area, inventory item slots), so no Marx layout asset is needed.
- `MRX_BalancePanel` (`Marx_UI`): the player's balances in large letters below the items of the inventory's vicinity
  panel (every currency of the wallet) and of a Marx arsenal (its currencies). A change counts to the new value and
  briefly shows the difference ("+$6,000"). Mods with their own balance display call
  `MRX_BalancePanel.SetShownInInventory(false)` on every machine and use `MRX_ClientWallet` instead.
- `MRX_TextFormat` (`Marx_UI`): amounts with thousands separators (`Amount(12500)` = "12,500", `Money(12500, "cash")` =
  "12,500 cash"), as the shop window and the balance panel show them. Mods can use it in their own UI.
  `MRX_TextFormat.SetCurrencyFormat("cash", "$%1")` changes how a currency is shown ("$12,500") on the machine that
  calls it; storage and the API keep the currency ID. Call it on every machine, e.g. in your game mode's `EOnInit`.
  `AmountCompact` and `MoneyCompact` fit amounts in four characters for narrow places ("14k", "$1.5k").
