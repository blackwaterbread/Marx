//! Every buy and sell of the vanilla arsenal window ends in these two requests. For a Marx arsenal they become Marx shop
//! requests (catalog item ID, target storage, item to sell); the vanilla supply requests are refused on the server.
modded class SCR_ResourcePlayerControllerInventoryComponent
{
	//------------------------------------------------------------------------------------------------
	override void RpcAsk_ArsenalRequestItem(RplId resourceComponentRplId, RplId storageComponentRplId, ResourceName resourceNameItem, EResourceType resourceType)
	{
		MRX_ArsenalShopComponent arsenal = MRX_ArsenalShopComponent.FindByResourceId(resourceComponentRplId);
		if (!arsenal)
		{
			super.RpcAsk_ArsenalRequestItem(resourceComponentRplId, storageComponentRplId, resourceNameItem, resourceType);
			return;
		}

		MRX_ShopItem item = arsenal.FindItem(resourceNameItem);
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetOwner());
		if (item && controller)
			controller.MRX_RequestArsenalBuy(arsenal.GetOwner(), storageComponentRplId, item.m_sId);
	}

	//------------------------------------------------------------------------------------------------
	override void RpcAsk_ArsenalRefundItem(RplId resourceComponentRplId, RplId inventoryItemRplId, EResourceType resourceType)
	{
		MRX_ArsenalShopComponent arsenal = MRX_ArsenalShopComponent.FindByResourceId(resourceComponentRplId);
		if (!arsenal)
		{
			super.RpcAsk_ArsenalRefundItem(resourceComponentRplId, inventoryItemRplId, resourceType);
			return;
		}

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetOwner());
		if (controller)
			controller.MRX_RequestArsenalSell(arsenal.GetOwner(), inventoryItemRplId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	override protected void RpcAsk_ArsenalRequestItem_(RplId resourceComponentRplId, RplId storageComponentRplId, ResourceName resourceNameItem, EResourceType resourceType)
	{
		if (MRX_ArsenalShopComponent.FindByResourceId(resourceComponentRplId))
		{
			MRX_LogRefused("buy");
			return;
		}

		super.RpcAsk_ArsenalRequestItem_(resourceComponentRplId, storageComponentRplId, resourceNameItem, resourceType);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	override protected void RpcAsk_ArsenalRefundItem_(RplId resourceComponentRplId, RplId inventoryItemRplId, EResourceType resourceType)
	{
		if (MRX_ArsenalShopComponent.FindByResourceId(resourceComponentRplId))
		{
			MRX_LogRefused("refund");
			return;
		}

		super.RpcAsk_ArsenalRefundItem_(resourceComponentRplId, inventoryItemRplId, resourceType);
	}

	//------------------------------------------------------------------------------------------------
	//! Only a modified client sends a supply request for a Marx arsenal.
	protected void MRX_LogRefused(string request)
	{
		int playerId;
		PlayerController controller = PlayerController.Cast(GetOwner());
		if (controller)
			playerId = controller.GetPlayerId();

		Print(string.Format("[MRX] Refused a vanilla arsenal %1 request for a Marx arsenal from player %2", request, playerId), LogLevel.WARNING);
	}
}
