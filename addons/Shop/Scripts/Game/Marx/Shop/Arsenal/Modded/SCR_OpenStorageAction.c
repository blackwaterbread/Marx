//! The open action of a Marx arsenal shows the shop's display name instead of the vanilla arsenal text.
modded class SCR_OpenStorageAction
{
	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		IEntity owner = GetOwner();
		if (owner && MRX_ArsenalShopComponent.Find(owner))
		{
			MRX_ShopComponent shop = MRX_ShopComponent.Cast(owner.FindComponent(MRX_ShopComponent));
			if (shop)
			{
				outName = shop.GetDisplayName();
				return true;
			}
		}

		return super.GetActionNameScript(outName);
	}
}
