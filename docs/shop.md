# Shops

Part of `Marx_Shop`.

## Setting up a shop

Any entity becomes a shop with:

- `MRX_ShopComponent`: shop ID (written to the ledger), catalog, sell percentage (default 50), whether the shop buys
  items back, trading distance (default 5 m)
- an enabled `RplComponent`
- an `ActionsManagerComponent` with `MRX_OpenShopAction` in one of its contexts

`Prefabs/Marx/Shop/MRX_ShopTable.et` is a ready example.

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

## Buying and selling

- **Buy:** the server checks the item, the price and the validators, checks that the player's inventory has room,
  debits the price, then spawns the item into the inventory. If the delivery fails, the price is refunded.
- **Sell:** only items of the shop's catalog, carried by the player, not locked, and empty (no attachments, loaded
  magazine or contents). The item is removed first, then the price is credited. If the credit fails, Marx tries to
  give the item back.
- One request per player at a time; requests closer than 250 ms are refused with `BUSY`.

Ledger source: `marx_shop`. Status codes: `MRX_EShopStatus`.

## Script API

`MRX_Shop.GetService()` (server) offers `Buy(playerId, shop, itemId, callback)` and
`Sell(playerId, shop, item, callback)`, the events `GetOnPurchase()` and `GetOnSale()`
`(playerId, ownerId, shop, item, price)`, and `AddValidator(MRX_ShopValidator)` with `CanBuy` and `CanSell` for custom
rules (faction, rank, ...). Inventory access sits behind `MRX_ShopInventory`; `MRX_EntityShopInventory` is the engine
implementation.

Clients call `SCR_PlayerController.MRX_RequestBuy(shopEntity, itemId)` and `MRX_RequestSell(shopEntity, item)` and get
the answer from `MRX_GetOnShopResult()`.

## Default UI

- `MRX_ShopMenu`: a shop window titled with the shop's display name (`m_sDisplayName` on `MRX_ShopComponent`). It
  shows the balance and the outcome of the last trade, a Buy tab with a category filter (the catalog items'
  `m_sCategory`) and a Sell tab with the sellable items the player carries. Every item has a 3D preview, its name (the
  catalog name, or the item's own inventory name), its price and a button; prices the player cannot pay are red and
  their button is disabled. One item per trade. The Buy tab shows 20 items per page (`MRX_ShopMenu.PAGE_SIZE`) with
  previous and next buttons, so large catalogs stay responsive; the category buttons wrap after six per row.
- Built on `MRX_ScriptedDialog` (`Marx_UI`) from vanilla parts in script (the wide configurable dialog, widget
  library buttons and toolbox, the dialog scroll area, inventory item slots), so no Marx layout asset is needed.
- `MRX_WalletHud` (`Marx_UI`): balances in the top right corner of the HUD with a short change hint. Mods with their
  own HUD call `MRX_WalletHud.SetEnabled(false)` and use `MRX_ClientWallet` instead.
- `MRX_TextFormat` (`Marx_UI`): amounts with thousands separators (`Amount(12500)` = "12,500", `Money(12500, "cash")` =
  "12,500 cash"), as the shop window and the wallet HUD show them. Mods can use it in their own UI.
