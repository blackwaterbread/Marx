void MRX_ClientShopResultDelegate(MRX_EShopStatus status, MRX_ETxStatus txStatus, string itemId, int price, string currency);
typedef func MRX_ClientShopResultDelegate;

//! Shop requests: the client sends IDs only, the server resolves prices and items from the shop's catalog.
modded class SCR_PlayerController
{
	protected static const int MRX_SHOP_REQUEST_INTERVAL_MS = 250;

	protected int m_iMRX_LastShopRequestTick;
	protected ref ScriptInvokerBase<MRX_ClientShopResultDelegate> m_MRX_OnShopResult;

	//------------------------------------------------------------------------------------------------
	//! Client: invoked with the server's answer to a buy or sell request.
	ScriptInvokerBase<MRX_ClientShopResultDelegate> MRX_GetOnShopResult()
	{
		if (!m_MRX_OnShopResult)
			m_MRX_OnShopResult = new ScriptInvokerBase<MRX_ClientShopResultDelegate>();

		return m_MRX_OnShopResult;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: asks the server to buy an item of the shop.
	void MRX_RequestBuy(notnull IEntity shopEntity, string itemId)
	{
		Rpc(MRX_RpcAsk_ShopBuy, SCR_EntityHelper.EntityToRplId(shopEntity), itemId);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: asks the server to buy an item of the player's inventory back.
	void MRX_RequestSell(notnull IEntity shopEntity, notnull IEntity item)
	{
		InventoryItemComponent itemComponent = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
		if (!itemComponent)
			return;

		Rpc(MRX_RpcAsk_ShopSell, SCR_EntityHelper.EntityToRplId(shopEntity), Replication.FindItemId(itemComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sends a shop result to the player who owns this controller.
	void MRX_SendShopResult(notnull MRX_ShopResult result)
	{
		Rpc(MRX_RpcDo_ShopResult, result.m_eStatus, result.m_eTxStatus, result.m_sItemId, result.m_iPrice, result.m_sCurrency);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MRX_RpcAsk_ShopBuy(RplId shopId, string itemId)
	{
		MRX_ShopDefinition shop;
		MRX_EShopStatus status = MRX_ResolveShopRequest(shopId, shop);
		if (status != MRX_EShopStatus.OK)
		{
			MRX_ReplyShopStatus(status, itemId);
			return;
		}

		MRX_Shop.GetService().Buy(GetPlayerId(), shop, itemId, new MRX_ShopRpcReply(this));
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MRX_RpcAsk_ShopSell(RplId shopId, RplId itemId)
	{
		MRX_ShopDefinition shop;
		MRX_EShopStatus status = MRX_ResolveShopRequest(shopId, shop);
		if (status != MRX_EShopStatus.OK)
		{
			MRX_ReplyShopStatus(status, string.Empty);
			return;
		}

		IEntity item;
		InventoryItemComponent itemComponent = InventoryItemComponent.Cast(Replication.FindItem(itemId));
		if (itemComponent)
			item = itemComponent.GetOwner();

		MRX_Shop.GetService().Sell(GetPlayerId(), shop, item, new MRX_ShopRpcReply(this));
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void MRX_RpcDo_ShopResult(MRX_EShopStatus status, MRX_ETxStatus txStatus, string itemId, int price, string currency)
	{
		if (m_MRX_OnShopResult)
			m_MRX_OnShopResult.Invoke(status, txStatus, itemId, price, currency);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: rate limit, shop lookup and distance check shared by buy and sell requests.
	protected MRX_EShopStatus MRX_ResolveShopRequest(RplId shopId, out MRX_ShopDefinition shop)
	{
		int now = System.GetTickCount();
		if (m_iMRX_LastShopRequestTick != 0 && now - m_iMRX_LastShopRequestTick < MRX_SHOP_REQUEST_INTERVAL_MS)
			return MRX_EShopStatus.BUSY;

		m_iMRX_LastShopRequestTick = now;
		if (!MRX_Shop.GetService())
			return MRX_EShopStatus.UNKNOWN_SHOP;

		IEntity shopEntity = SCR_EntityHelper.RplIdToEntity(shopId);
		if (!shopEntity)
			return MRX_EShopStatus.UNKNOWN_SHOP;

		MRX_ShopComponent shopComponent = MRX_ShopComponent.Cast(shopEntity.FindComponent(MRX_ShopComponent));
		if (!shopComponent)
			return MRX_EShopStatus.UNKNOWN_SHOP;

		if (!shopComponent.IsInRange(GetControlledEntity()))
			return MRX_EShopStatus.TOO_FAR;

		shop = shopComponent.GetDefinition();
		if (!shop)
			return MRX_EShopStatus.UNKNOWN_SHOP;

		return MRX_EShopStatus.OK;
	}

	//------------------------------------------------------------------------------------------------
	protected void MRX_ReplyShopStatus(MRX_EShopStatus status, string itemId)
	{
		MRX_ShopResult result = MRX_ShopResult.Create(status, string.Empty);
		result.m_sItemId = itemId;
		MRX_SendShopResult(result);
	}
}

//------------------------------------------------------------------------------------------------
//! Sends a shop service result back to the requesting player. Internal.
class MRX_ShopRpcReply : MRX_ShopCallback
{
	//! Weak: the controller may be gone when the result arrives.
	protected SCR_PlayerController m_Controller;

	//------------------------------------------------------------------------------------------------
	void MRX_ShopRpcReply(SCR_PlayerController controller)
	{
		m_Controller = controller;
	}

	//------------------------------------------------------------------------------------------------
	override void OnResult(MRX_ShopResult result)
	{
		super.OnResult(result);
		if (m_Controller)
			m_Controller.MRX_SendShopResult(result);
	}
}
