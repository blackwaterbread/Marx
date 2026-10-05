//! "Trade" interaction (API v0): opens the shop UI on the client of the player who uses it.
//! Add it to the ActionsManagerComponent of an entity that has MRX_ShopComponent.
class MRX_OpenShopAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (pUserEntity != SCR_PlayerController.GetLocalControlledEntity())
			return;

		MRX_ShopComponent shop = MRX_ShopComponent.Cast(pOwnerEntity.FindComponent(MRX_ShopComponent));
		if (shop)
			MRX_ShopMenu.Open(shop);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return GetOwner().FindComponent(MRX_ShopComponent) != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBroadcastScript()
	{
		return false;
	}
}
