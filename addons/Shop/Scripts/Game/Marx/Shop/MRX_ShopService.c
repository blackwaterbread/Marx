//! Server-side buying and selling (API v0). Prices and items always come from the shop's catalog.
//! A player can have one request running at a time (others get BUSY). Callbacks run on a later frame.
class MRX_ShopService : Managed
{
	static const string LEDGER_SOURCE = "marx_shop";

	protected ref MRX_EconomyService m_Economy;
	protected ref MRX_IdentityService m_Identity;
	protected ref MRX_ShopInventory m_Inventory;
	protected ref MRX_CallQueue m_CallQueue = new MRX_CallQueue();
	protected ref array<ref MRX_ShopValidator> m_aValidators = {};
	protected ref array<ref MRX_ShopOp> m_aOps = {};
	protected ref set<int> m_aBusyPlayerIds = new set<int>();

	protected ref ScriptInvokerBase<MRX_ShopTradeDelegate> m_OnPurchase;
	protected ref ScriptInvokerBase<MRX_ShopTradeDelegate> m_OnSale;

	//------------------------------------------------------------------------------------------------
	void MRX_ShopService(notnull MRX_EconomyService economy, notnull MRX_IdentityService identity, notnull MRX_ShopInventory inventory)
	{
		m_Economy = economy;
		m_Identity = identity;
		m_Inventory = inventory;
	}

	//------------------------------------------------------------------------------------------------
	//! Buys one item for the player and puts it into the player's inventory.
	//! \param target Storage the item goes to (a BaseInventoryStorageComponent for the engine inventory, e.g. a weapon's
	//! attachment slots); null: any free place in the inventory. The caller checks that the player may use it.
	void Buy(int playerId, MRX_ShopDefinition shop, string itemId, MRX_ShopCallback callback = null, Managed target = null)
	{
		StartOp(new MRX_ShopBuyOp(this, playerId, shop, itemId, callback, target));
	}

	//------------------------------------------------------------------------------------------------
	//! Sells an item from the player's inventory to the shop.
	//! \param item Item to sell (an IEntity for the engine inventory).
	//! \param withContents False: the item must be empty (NOT_EMPTY). True: everything it holds is sold with it; the price
	//! is the sum of the sell prices of the item and its contents (contents the shop does not buy, or in another currency
	//! than the item, count 0 and are removed too).
	void Sell(int playerId, MRX_ShopDefinition shop, Managed item, MRX_ShopCallback callback = null, bool withContents = false)
	{
		StartOp(new MRX_ShopSellOp(this, playerId, shop, item, callback, withContents));
	}

	//------------------------------------------------------------------------------------------------
	//! Price the shop pays for an item and its contents (see Sell() with contents).
	//! \param[out] unpaidCount Contents worth nothing to the shop.
	//! \return 0 when the shop does not buy the item itself.
	static int GetSellPriceWithContents(notnull MRX_ShopDefinition shop, ResourceName prefab, notnull array<ResourceName> contents, out int unpaidCount = 0)
	{
		unpaidCount = 0;
		MRX_ShopItem root = shop.m_Catalog.FindByPrefab(prefab);
		if (!root)
			return 0;

		int total = shop.GetSellPrice(root);
		if (total <= 0)
			return 0;

		foreach (ResourceName content : contents)
		{
			MRX_ShopItem item = shop.m_Catalog.FindByPrefab(content);
			int price;
			if (item && item.m_sCurrency == root.m_sCurrency)
				price = shop.GetSellPrice(item);

			if (price <= 0)
			{
				unpaidCount++;
				continue;
			}

			// No int overflow: cap at int.MAX.
			if (total > int.MAX - price)
				total = int.MAX;
			else
				total += price;
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! States of the shop's products for a player (MRX_ShopProduct.GetState), e.g. for the shop window. The callback
	//! gets them all at once, in catalog order; without an owner it gets none.
	void GetProductStates(int playerId, notnull MRX_ShopDefinition shop, notnull MRX_ShopStatesCallback callback)
	{
		MRX_ShopStatesRequest request = new MRX_ShopStatesRequest(callback, m_CallQueue);
		string ownerId = m_Identity.GetOwnerId(playerId);
		if (!ownerId.IsEmpty())
		{
			foreach (MRX_ShopItem item : shop.m_Catalog.m_aItems)
			{
				if (item.m_Product)
					request.Add(playerId, ownerId, item);
			}
		}

		request.Close();
	}

	//------------------------------------------------------------------------------------------------
	void AddValidator(notnull MRX_ShopValidator validator)
	{
		if (!m_aValidators.Contains(validator))
			m_aValidators.Insert(validator);
	}

	//------------------------------------------------------------------------------------------------
	void RemoveValidator(MRX_ShopValidator validator)
	{
		m_aValidators.RemoveItem(validator);
	}

	//------------------------------------------------------------------------------------------------
	//! Invoked after a completed purchase (price = amount paid).
	ScriptInvokerBase<MRX_ShopTradeDelegate> GetOnPurchase()
	{
		if (!m_OnPurchase)
			m_OnPurchase = new ScriptInvokerBase<MRX_ShopTradeDelegate>();

		return m_OnPurchase;
	}

	//------------------------------------------------------------------------------------------------
	//! Invoked after a completed sale (price = amount received).
	ScriptInvokerBase<MRX_ShopTradeDelegate> GetOnSale()
	{
		if (!m_OnSale)
			m_OnSale = new ScriptInvokerBase<MRX_ShopTradeDelegate>();

		return m_OnSale;
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, used by MRX_ShopOp.
	MRX_EconomyService GetEconomy()
	{
		return m_Economy;
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, used by MRX_ShopOp.
	MRX_ShopInventory GetInventory()
	{
		return m_Inventory;
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, used by MRX_ShopOp.
	bool IsAllowed(bool buying, int playerId, string ownerId, MRX_ShopDefinition shop, MRX_ShopItem item)
	{
		foreach (MRX_ShopValidator validator : m_aValidators)
		{
			if (buying && !validator.CanBuy(playerId, ownerId, shop, item))
				return false;

			if (!buying && !validator.CanSell(playerId, ownerId, shop, item))
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by MRX_ShopOp once with its final result.
	//! \param releasePlayer False for a BUSY answer, whose player still has another request running.
	void OnOpFinished(notnull MRX_ShopOp op, notnull MRX_ShopResult result, MRX_ShopItem item, bool buying, bool releasePlayer = true)
	{
		if (releasePlayer)
			m_aBusyPlayerIds.RemoveItem(op.GetPlayerId());

		if (result.m_eStatus == MRX_EShopStatus.OK && item)
		{
			if (buying && m_OnPurchase)
				m_OnPurchase.Invoke(op.GetPlayerId(), op.GetOwnerId(), op.GetShop(), item, result.m_iPrice);

			if (!buying && m_OnSale)
				m_OnSale.Invoke(op.GetPlayerId(), op.GetOwnerId(), op.GetShop(), item, result.m_iPrice);
		}

		MRX_ShopCallback callback = op.GetCallback();
		if (callback)
			m_CallQueue.Post(new MRX_ShopResultDelivery(callback, result));
	}

	//------------------------------------------------------------------------------------------------
	protected void StartOp(notnull MRX_ShopOp op)
	{
		for (int i = m_aOps.Count() - 1; i >= 0; i--)
		{
			if (m_aOps[i].IsDone())
				m_aOps.Remove(i);
		}

		m_aOps.Insert(op);

		int playerId = op.GetPlayerId();
		if (m_aBusyPlayerIds.Contains(playerId))
		{
			op.FinishEarly(MRX_EShopStatus.BUSY, false);
			return;
		}

		if (!op.GetShop())
		{
			op.FinishEarly(MRX_EShopStatus.UNKNOWN_SHOP, true);
			return;
		}

		string ownerId = m_Identity.GetOwnerId(playerId);
		if (ownerId.IsEmpty())
		{
			op.FinishEarly(MRX_EShopStatus.OWNER_NOT_READY, true);
			return;
		}

		m_aBusyPlayerIds.Insert(playerId);
		op.Start(ownerId);
	}
}

//------------------------------------------------------------------------------------------------
//! One running shop request. Internal.
class MRX_ShopOp : Managed
{
	//! Weak, the service owns its ops.
	protected MRX_ShopService m_Service;
	protected int m_iPlayerId;
	protected string m_sOwnerId;
	protected ref MRX_ShopDefinition m_Shop;
	protected ref MRX_ShopCallback m_Callback;
	protected ref MRX_ShopResult m_Result;
	protected bool m_bDone;

	//------------------------------------------------------------------------------------------------
	void Start(string ownerId)
	{
	}

	//------------------------------------------------------------------------------------------------
	int GetPlayerId()
	{
		return m_iPlayerId;
	}

	//------------------------------------------------------------------------------------------------
	string GetOwnerId()
	{
		return m_sOwnerId;
	}

	//------------------------------------------------------------------------------------------------
	MRX_ShopDefinition GetShop()
	{
		return m_Shop;
	}

	//------------------------------------------------------------------------------------------------
	MRX_ShopCallback GetCallback()
	{
		return m_Callback;
	}

	//------------------------------------------------------------------------------------------------
	bool IsDone()
	{
		return m_bDone;
	}

	//------------------------------------------------------------------------------------------------
	//! Finishes before Start(). \param releasePlayer False when the player's running request must stay marked busy.
	void FinishEarly(MRX_EShopStatus status, bool releasePlayer)
	{
		m_Result.m_eStatus = status;
		m_bDone = true;
		m_Service.OnOpFinished(this, m_Result, null, false, releasePlayer);
	}

	//------------------------------------------------------------------------------------------------
	protected void Init(MRX_ShopService service, int playerId, MRX_ShopDefinition shop, MRX_ShopCallback callback)
	{
		m_Service = service;
		m_iPlayerId = playerId;
		m_Shop = shop;
		m_Callback = callback;
		string requestId = PersistenceIdUtils.Generate();
		m_Result = MRX_ShopResult.Create(MRX_EShopStatus.OK, requestId);
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_TxContext CreateContext(string action, string itemId, string keyPrefix)
	{
		string reason = string.Format("%1:%2:%3", action, m_Shop.m_sShopId, itemId);
		return MRX_TxContext.Create(MRX_ShopService.LEDGER_SOURCE, reason, keyPrefix + ":" + m_Result.m_sRequestId);
	}

	//------------------------------------------------------------------------------------------------
	protected void Finish(MRX_EShopStatus status, MRX_ShopItem item, bool buying)
	{
		m_Result.m_eStatus = status;
		m_bDone = true;
		m_Service.OnOpFinished(this, m_Result, item, buying);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_ShopBuyOp : MRX_ShopOp
{
	protected string m_sItemId;
	protected MRX_ShopItem m_Item;
	//! Kept until the delivery, which follows the payment on a later frame.
	protected ref Managed m_Target;
	//! Reported after the refund of a failed delivery.
	protected MRX_EShopStatus m_eDeliveryFailure = MRX_EShopStatus.DELIVERY_FAILED;

	//------------------------------------------------------------------------------------------------
	void MRX_ShopBuyOp(MRX_ShopService service, int playerId, MRX_ShopDefinition shop, string itemId, MRX_ShopCallback callback, Managed target)
	{
		Init(service, playerId, shop, callback);
		m_sItemId = itemId;
		m_Result.m_sItemId = itemId;
		m_Target = target;
	}

	//------------------------------------------------------------------------------------------------
	override void Start(string ownerId)
	{
		m_sOwnerId = ownerId;
		m_Item = m_Shop.m_Catalog.FindItem(m_sItemId);
		if (!m_Item)
		{
			Finish(MRX_EShopStatus.UNKNOWN_ITEM, null, true);
			return;
		}

		m_Result.m_sCurrency = m_Item.m_sCurrency;
		m_Result.m_iPrice = m_Item.m_iPrice;
		if (m_Item.m_iPrice <= 0)
		{
			Finish(MRX_EShopStatus.NOT_FOR_SALE, null, true);
			return;
		}

		if (!m_Service.IsAllowed(true, m_iPlayerId, m_sOwnerId, m_Shop, m_Item))
		{
			Finish(MRX_EShopStatus.REJECTED, null, true);
			return;
		}

		if (m_Item.m_Product)
		{
			MRX_ShopProductCallback checkCallback = new MRX_ShopProductCallback();
			checkCallback.GetOnResult().Insert(OnProductChecked);
			m_Item.m_Product.Check(m_iPlayerId, m_sOwnerId, m_Item, checkCallback);
			return;
		}

		if (!m_Service.GetInventory().CanGive(m_iPlayerId, m_Item.m_sPrefab, m_Target))
		{
			Finish(MRX_EShopStatus.NO_SPACE, null, true);
			return;
		}

		Pay();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnProductChecked(MRX_EShopStatus status)
	{
		if (status != MRX_EShopStatus.OK)
		{
			Finish(status, null, true);
			return;
		}

		Pay();
	}

	//------------------------------------------------------------------------------------------------
	protected void Pay()
	{
		MRX_TxCallback callback = new MRX_TxCallback();
		callback.GetOnResult().Insert(OnPaid);
		m_Service.GetEconomy().Debit(m_sOwnerId, m_Item.m_sCurrency, m_Item.m_iPrice, CreateContext("buy", m_Item.m_sId, "buy"), callback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPaid(MRX_TxResult result)
	{
		if (result.m_eStatus != MRX_ETxStatus.OK)
		{
			m_Result.m_eTxStatus = result.m_eStatus;
			Finish(MRX_EShopStatus.PAYMENT_FAILED, null, true);
			return;
		}

		if (m_Item.m_Product)
		{
			MRX_ShopProductCallback deliveryCallback = new MRX_ShopProductCallback();
			deliveryCallback.GetOnResult().Insert(OnProductDelivered);
			m_Item.m_Product.Deliver(m_iPlayerId, m_sOwnerId, m_Item, deliveryCallback);
			return;
		}

		MRX_ShopDeliveryCallback callback = new MRX_ShopDeliveryCallback();
		callback.GetOnResult().Insert(OnDelivered);
		m_Service.GetInventory().Give(m_iPlayerId, m_Item.m_sPrefab, callback, m_Target);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnProductDelivered(MRX_EShopStatus status)
	{
		if (status == MRX_EShopStatus.OK)
		{
			Finish(MRX_EShopStatus.OK, m_Item, true);
			return;
		}

		m_eDeliveryFailure = status;
		Refund();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnDelivered(bool success)
	{
		if (success)
		{
			Finish(MRX_EShopStatus.OK, m_Item, true);
			return;
		}

		Refund();
	}

	//------------------------------------------------------------------------------------------------
	protected void Refund()
	{
		MRX_TxCallback callback = new MRX_TxCallback();
		callback.GetOnResult().Insert(OnRefunded);
		m_Service.GetEconomy().Credit(m_sOwnerId, m_Item.m_sCurrency, m_Item.m_iPrice, CreateContext("refund", m_Item.m_sId, "refund"), callback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnRefunded(MRX_TxResult result)
	{
		if (result.m_eStatus != MRX_ETxStatus.OK)
		{
			Print(string.Format("[MRX] Refund of shop request %1 failed (%2): owner %3 paid %4 %5 without getting '%6'",
				m_Result.m_sRequestId, typename.EnumToString(MRX_ETxStatus, result.m_eStatus), m_sOwnerId, m_Item.m_iPrice, m_Item.m_sCurrency, m_Item.m_sId), LogLevel.ERROR);
		}

		Finish(m_eDeliveryFailure, null, true);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_ShopSellOp : MRX_ShopOp
{
	//! Weak: an entity in the engine implementation.
	protected Managed m_ItemHandle;
	protected bool m_bWithContents;
	protected MRX_ShopItem m_Item;
	protected ResourceName m_sPrefab;
	protected ref Managed m_ReturnCapture;

	//------------------------------------------------------------------------------------------------
	void MRX_ShopSellOp(MRX_ShopService service, int playerId, MRX_ShopDefinition shop, Managed item, MRX_ShopCallback callback, bool withContents)
	{
		Init(service, playerId, shop, callback);
		m_ItemHandle = item;
		m_bWithContents = withContents;
	}

	//------------------------------------------------------------------------------------------------
	override void Start(string ownerId)
	{
		m_sOwnerId = ownerId;
		if (!m_Shop.m_bAllowSell)
		{
			Finish(MRX_EShopStatus.SELL_DISABLED, null, false);
			return;
		}

		MRX_ShopInventory inventory = m_Service.GetInventory();
		array<ResourceName> contents;
		if (m_bWithContents)
			contents = {};

		MRX_EShopStatus status = inventory.InspectForSale(m_iPlayerId, m_ItemHandle, m_sPrefab, contents);
		if (status != MRX_EShopStatus.OK)
		{
			Finish(status, null, false);
			return;
		}

		m_Item = m_Shop.m_Catalog.FindByPrefab(m_sPrefab);
		if (!m_Item)
		{
			Finish(MRX_EShopStatus.NOT_BUYABLE, null, false);
			return;
		}

		m_Result.m_sItemId = m_Item.m_sId;
		m_Result.m_sCurrency = m_Item.m_sCurrency;
		m_Result.m_iItemCount = 1;
		if (contents)
		{
			int unpaid;
			m_Result.m_iPrice = MRX_ShopService.GetSellPriceWithContents(m_Shop, m_sPrefab, contents, unpaid);
			m_Result.m_iItemCount += contents.Count();
			m_Result.m_iUnpaidCount = unpaid;
		}
		else
		{
			m_Result.m_iPrice = m_Shop.GetSellPrice(m_Item);
		}

		if (m_Result.m_iPrice <= 0)
		{
			Finish(MRX_EShopStatus.NOT_BUYABLE, null, false);
			return;
		}

		if (!m_Service.IsAllowed(false, m_iPlayerId, m_sOwnerId, m_Shop, m_Item))
		{
			Finish(MRX_EShopStatus.REJECTED, null, false);
			return;
		}

		// Remove first: losing a payment is recoverable by admins, a duplicated item is not.
		m_ReturnCapture = inventory.CaptureForReturn(m_iPlayerId, m_ItemHandle);
		if (!inventory.Remove(m_ItemHandle))
		{
			Finish(MRX_EShopStatus.NOT_IN_INVENTORY, null, false);
			return;
		}

		MRX_TxCallback callback = new MRX_TxCallback();
		callback.GetOnResult().Insert(OnPaid);
		m_Service.GetEconomy().Credit(m_sOwnerId, m_Item.m_sCurrency, m_Result.m_iPrice, CreateContext("sell", m_Item.m_sId, "sell"), callback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPaid(MRX_TxResult result)
	{
		if (result.m_eStatus == MRX_ETxStatus.OK)
		{
			Finish(MRX_EShopStatus.OK, m_Item, false);
			return;
		}

		Print(string.Format("[MRX] Payment for sold item failed (%1), request %2: returning '%3' to player %4",
			typename.EnumToString(MRX_ETxStatus, result.m_eStatus), m_Result.m_sRequestId, m_Item.m_sId, m_iPlayerId), LogLevel.ERROR);
		m_Result.m_eTxStatus = result.m_eStatus;

		MRX_ShopDeliveryCallback callback = new MRX_ShopDeliveryCallback();
		callback.GetOnResult().Insert(OnReturned);
		m_Service.GetInventory().GiveBack(m_iPlayerId, m_sPrefab, m_ReturnCapture, callback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnReturned(bool success)
	{
		if (!success)
			Print(string.Format("[MRX] Could not return '%1' to player %2 after a failed sale (request %3)", m_Item.m_sId, m_iPlayerId, m_Result.m_sRequestId), LogLevel.ERROR);

		Finish(MRX_EShopStatus.PAYMENT_FAILED, null, false);
	}
}
