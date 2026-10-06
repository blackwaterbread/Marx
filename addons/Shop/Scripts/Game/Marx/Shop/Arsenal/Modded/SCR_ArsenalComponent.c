//! Marx arsenal: lists the shop catalog instead of the faction's arsenal items.
modded class SCR_ArsenalComponent
{
	//------------------------------------------------------------------------------------------------
	override bool GetFilteredArsenalItems(out notnull array<SCR_ArsenalItem> filteredArsenalItems, EArsenalItemDisplayType requiresDisplayType = -1)
	{
		MRX_ArsenalShopComponent shop = MRX_ArsenalShopComponent.Find(GetOwner());
		if (!shop)
			return super.GetFilteredArsenalItems(filteredArsenalItems, requiresDisplayType);

		// Display racks (display types) never show shop items.
		array<SCR_ArsenalItem> items = {};
		if (requiresDisplayType == -1)
			shop.GetArsenalItems(items);

		filteredArsenalItems = items;
		return !items.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! A Marx arsenal stays enabled: the vanilla inventory refuses to sell items to a disabled arsenal.
	override void SetArsenalEnabled(bool enable, bool isOverwrite = true)
	{
		if (!enable && MRX_ArsenalShopComponent.Find(GetOwner()))
		{
			Print(string.Format("[MRX] Marx arsenal %1 stays enabled", GetOwner()), LogLevel.VERBOSE);
			return;
		}

		super.SetArsenalEnabled(enable, isOverwrite);
	}
}
