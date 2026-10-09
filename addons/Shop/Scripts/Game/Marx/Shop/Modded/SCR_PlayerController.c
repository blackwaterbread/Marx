//! itemCount and unpaidCount: sales with contents only (see MRX_ShopResult).
void MRX_ClientShopResultDelegate(MRX_EShopStatus status, MRX_ETxStatus txStatus, string itemId, int price, string currency, int itemCount, int unpaidCount);
typedef func MRX_ClientShopResultDelegate;

//! Product states of a shop (MRX_ShopProduct.GetState) as parallel arrays; available holds 1 or 0.
void MRX_ClientShopStatesDelegate(IEntity shopEntity, array<string> itemIds, array<int> available, array<string> texts);
typedef func MRX_ClientShopStatesDelegate;

//! Shop requests: the client sends IDs only, the server resolves prices and items from the shop's catalog.
modded class SCR_PlayerController
{
	protected ref ScriptInvokerBase<MRX_ClientShopResultDelegate> m_MRX_OnShopResult;
	protected ref ScriptInvokerBase<MRX_ClientShopStatesDelegate> m_MRX_OnShopStates;

	//------------------------------------------------------------------------------------------------
	//! Client: invoked with the server's answer to a buy or sell request.
	ScriptInvokerBase<MRX_ClientShopResultDelegate> MRX_GetOnShopResult()
	{
		if (!m_MRX_OnShopResult)
			m_MRX_OnShopResult = new ScriptInvokerBase<MRX_ClientShopResultDelegate>();

		return m_MRX_OnShopResult;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: invoked with the product states of a shop (see MRX_RequestShopStates).
	ScriptInvokerBase<MRX_ClientShopStatesDelegate> MRX_GetOnShopStates()
	{
		if (!m_MRX_OnShopStates)
			m_MRX_OnShopStates = new ScriptInvokerBase<MRX_ClientShopStatesDelegate>();

		return m_MRX_OnShopStates;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: asks for the states of the shop's products (MRX_ShopProduct.GetState) for this player.
	void MRX_RequestShopStates(notnull IEntity shopEntity)
	{
		Rpc(MRX_RpcAsk_ShopStates, SCR_EntityHelper.EntityToRplId(shopEntity));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sends product states to the player who owns this controller.
	void MRX_SendShopStates(RplId shopId, array<string> itemIds, array<int> available, array<string> texts)
	{
		Rpc(MRX_RpcDo_ShopStates, shopId, itemIds, available, texts);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MRX_RpcAsk_ShopStates(RplId shopId)
	{
		IEntity shopEntity = SCR_EntityHelper.RplIdToEntity(shopId);
		MRX_ShopComponent shopComponent;
		if (shopEntity)
			shopComponent = MRX_ShopComponent.Cast(shopEntity.FindComponent(MRX_ShopComponent));

		MRX_ShopDefinition shop;
		if (shopComponent && shopComponent.IsInRange(GetControlledEntity()))
			shop = shopComponent.GetDefinition();

		if (!shop || !MRX_Shop.GetService())
		{
			MRX_SendShopStates(shopId, new array<string>(), new array<int>(), new array<string>());
			return;
		}

		MRX_Shop.GetService().GetProductStates(GetPlayerId(), shop, new MRX_ShopStatesRpcReply(this, shopId));
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void MRX_RpcDo_ShopStates(RplId shopId, array<string> itemIds, array<int> available, array<string> texts)
	{
		if (m_MRX_OnShopStates)
			m_MRX_OnShopStates.Invoke(SCR_EntityHelper.RplIdToEntity(shopId), itemIds, available, texts);
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
		Rpc(MRX_RpcDo_ShopResult, result.m_eStatus, result.m_eTxStatus, result.m_sItemId, result.m_iPrice, result.m_sCurrency, result.m_iItemCount, result.m_iUnpaidCount);
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
	protected void MRX_RpcDo_ShopResult(MRX_EShopStatus status, MRX_ETxStatus txStatus, string itemId, int price, string currency, int itemCount, int unpaidCount)
	{
		if (m_MRX_OnShopResult)
			m_MRX_OnShopResult.Invoke(status, txStatus, itemId, price, currency, itemCount, unpaidCount);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: shop lookup and distance check shared by buy and sell requests. The shop service limits how many requests
	//! of a player may wait.
	protected MRX_EShopStatus MRX_ResolveShopRequest(RplId shopId, out MRX_ShopDefinition shop)
	{
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
//! Sends product states back to the requesting player. Internal.
class MRX_ShopStatesRpcReply : MRX_ShopStatesCallback
{
	//! Weak: the controller may be gone when the states arrive.
	protected SCR_PlayerController m_Controller;
	protected RplId m_ShopId;

	//------------------------------------------------------------------------------------------------
	void MRX_ShopStatesRpcReply(SCR_PlayerController controller, RplId shopId)
	{
		m_Controller = controller;
		m_ShopId = shopId;
	}

	//------------------------------------------------------------------------------------------------
	override void OnResult(array<string> itemIds, array<bool> available, array<string> texts)
	{
		if (!m_Controller)
			return;

		array<int> flags = {};
		foreach (bool isAvailable : available)
		{
			if (isAvailable)
				flags.Insert(1);
			else
				flags.Insert(0);
		}

		m_Controller.MRX_SendShopStates(m_ShopId, itemIds, flags, texts);
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
