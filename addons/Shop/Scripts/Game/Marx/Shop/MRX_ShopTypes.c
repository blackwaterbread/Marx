//! Outcome of a shop request (API v0).
enum MRX_EShopStatus
{
	OK,
	OWNER_NOT_READY,
	UNKNOWN_SHOP,
	UNKNOWN_ITEM,
	//! The item has no purchase price.
	NOT_FOR_SALE,
	//! The shop does not buy items back.
	SELL_DISABLED,
	//! The shop does not buy this item back (not in its catalog, or no sell price).
	NOT_BUYABLE,
	TOO_FAR,
	//! The player's inventory cannot take the item.
	NO_SPACE,
	NOT_IN_INVENTORY,
	//! Attachments, magazines or contents must be removed before selling.
	NOT_EMPTY,
	//! Refused by a registered MRX_ShopValidator.
	REJECTED,
	//! The economy refused or failed the payment; see MRX_ShopResult.m_eTxStatus.
	PAYMENT_FAILED,
	//! The item could not be given after payment; the payment was refunded.
	DELIVERY_FAILED,
	//! Another request of the same player is still running.
	BUSY
}

//! Result of a shop request (API v0).
class MRX_ShopResult : Managed
{
	MRX_EShopStatus m_eStatus;
	//! Economy status when m_eStatus is PAYMENT_FAILED (e.g. INSUFFICIENT_FUNDS).
	MRX_ETxStatus m_eTxStatus;
	string m_sItemId;
	string m_sCurrency;
	//! Amount paid (buy) or received (sell).
	int m_iPrice;
	string m_sRequestId;

	//------------------------------------------------------------------------------------------------
	static MRX_ShopResult Create(MRX_EShopStatus status, string requestId)
	{
		MRX_ShopResult result = new MRX_ShopResult();
		result.m_eStatus = status;
		result.m_sRequestId = requestId;
		return result;
	}
}

void MRX_ShopDelegate(MRX_ShopResult result);
typedef func MRX_ShopDelegate;

void MRX_ShopTradeDelegate(int playerId, string ownerId, MRX_ShopDefinition shop, MRX_ShopItem item, int price);
typedef func MRX_ShopTradeDelegate;

void MRX_ShopDeliveryDelegate(bool success);
typedef func MRX_ShopDeliveryDelegate;

//------------------------------------------------------------------------------------------------
//! Shop request callback: subscribe with GetOnResult().Insert() or override OnResult().
class MRX_ShopCallback : Managed
{
	protected ref ScriptInvokerBase<MRX_ShopDelegate> m_OnResult;

	//------------------------------------------------------------------------------------------------
	ScriptInvokerBase<MRX_ShopDelegate> GetOnResult()
	{
		if (!m_OnResult)
			m_OnResult = new ScriptInvokerBase<MRX_ShopDelegate>();

		return m_OnResult;
	}

	//------------------------------------------------------------------------------------------------
	void OnResult(MRX_ShopResult result)
	{
		if (m_OnResult)
			m_OnResult.Invoke(result);
	}
}

//------------------------------------------------------------------------------------------------
//! Delivery callback of MRX_ShopInventory.Give().
class MRX_ShopDeliveryCallback : Managed
{
	protected ref ScriptInvokerBase<MRX_ShopDeliveryDelegate> m_OnResult;

	//------------------------------------------------------------------------------------------------
	ScriptInvokerBase<MRX_ShopDeliveryDelegate> GetOnResult()
	{
		if (!m_OnResult)
			m_OnResult = new ScriptInvokerBase<MRX_ShopDeliveryDelegate>();

		return m_OnResult;
	}

	//------------------------------------------------------------------------------------------------
	void OnResult(bool success)
	{
		if (m_OnResult)
			m_OnResult.Invoke(success);
	}
}

//------------------------------------------------------------------------------------------------
//! Runs a shop callback on a later frame.
class MRX_ShopResultDelivery : MRX_DeferredCall
{
	protected ref MRX_ShopCallback m_Callback;
	protected ref MRX_ShopResult m_Result;

	//------------------------------------------------------------------------------------------------
	void MRX_ShopResultDelivery(MRX_ShopCallback callback, MRX_ShopResult result)
	{
		m_Callback = callback;
		m_Result = result;
	}

	//------------------------------------------------------------------------------------------------
	override void Run()
	{
		m_Callback.OnResult(m_Result);
	}
}

//------------------------------------------------------------------------------------------------
//! Extension point (API v0): refuse purchases or sales with REJECTED. Register with MRX_ShopService.AddValidator().
class MRX_ShopValidator : Managed
{
	//------------------------------------------------------------------------------------------------
	bool CanBuy(int playerId, string ownerId, MRX_ShopDefinition shop, MRX_ShopItem item)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	bool CanSell(int playerId, string ownerId, MRX_ShopDefinition shop, MRX_ShopItem item)
	{
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Inventory access used by the shop service (API v0). The engine implementation works on player characters;
//! tests use a fake. Items for sale are passed as Managed handles (an IEntity in the engine implementation).
class MRX_ShopInventory : Managed
{
	//------------------------------------------------------------------------------------------------
	//! True when the player's inventory can take one instance of the prefab now.
	bool CanGive(int playerId, ResourceName prefab)
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts one instance of the prefab into the player's inventory and reports success, possibly on a later frame.
	void Give(int playerId, ResourceName prefab, notnull MRX_ShopDeliveryCallback callback)
	{
		callback.OnResult(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Checks that the item is in the player's inventory and holds nothing.
	//! \param[out] prefab Prefab of the item when the status is OK.
	MRX_EShopStatus InspectForSale(int playerId, Managed item, out ResourceName prefab)
	{
		return MRX_EShopStatus.NOT_IN_INVENTORY;
	}

	//------------------------------------------------------------------------------------------------
	//! Deletes an item that passed InspectForSale(). True on success.
	bool Remove(Managed item)
	{
		return false;
	}
}
