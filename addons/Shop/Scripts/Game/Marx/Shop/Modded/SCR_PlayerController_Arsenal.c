//! Marx arsenal requests: the client sends the arsenal, the catalog item ID and the target storage (buy) or the item
//! (sell); the server checks them like vanilla arsenal requests and resolves prices from the shop's catalog.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Client: asks the server to buy a catalog item of a Marx arsenal into a storage.
	void MRX_RequestArsenalBuy(notnull IEntity arsenal, RplId storageId, string itemId)
	{
		Rpc(MRX_RpcAsk_ArsenalBuy, SCR_EntityHelper.EntityToRplId(arsenal), storageId, itemId);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: asks the server to buy an item of the player's inventory back, with everything it holds.
	void MRX_RequestArsenalSell(notnull IEntity arsenal, RplId itemId)
	{
		Rpc(MRX_RpcAsk_ArsenalSell, SCR_EntityHelper.EntityToRplId(arsenal), itemId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MRX_RpcAsk_ArsenalBuy(RplId arsenalId, RplId storageId, string itemId)
	{
		if (!MRX_CheckShopRequestRate())
		{
			MRX_ReplyShopStatus(MRX_EShopStatus.BUSY, itemId);
			return;
		}

		BaseInventoryStorageComponent storage;
		if (storageId.IsValid())
			storage = BaseInventoryStorageComponent.Cast(Replication.FindItem(storageId));

		IEntity arsenal = SCR_EntityHelper.RplIdToEntity(arsenalId);
		MRX_EShopStatus status = MRX_ArsenalRequests.Buy(GetPlayerId(), arsenal, storage, itemId, new MRX_ShopRpcReply(this));
		if (status != MRX_EShopStatus.OK)
			MRX_ReplyShopStatus(status, itemId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MRX_RpcAsk_ArsenalSell(RplId arsenalId, RplId itemId)
	{
		if (!MRX_CheckShopRequestRate())
		{
			MRX_ReplyShopStatus(MRX_EShopStatus.BUSY, string.Empty);
			return;
		}

		IEntity item;
		InventoryItemComponent itemComponent;
		if (itemId.IsValid())
			itemComponent = InventoryItemComponent.Cast(Replication.FindItem(itemId));

		if (itemComponent)
			item = itemComponent.GetOwner();

		IEntity arsenal = SCR_EntityHelper.RplIdToEntity(arsenalId);
		MRX_EShopStatus status = MRX_ArsenalRequests.Sell(GetPlayerId(), arsenal, item, new MRX_ShopRpcReply(this));
		if (status != MRX_EShopStatus.OK)
			MRX_ReplyShopStatus(status, string.Empty);
	}
}
