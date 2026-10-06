//! Storage titles of a Marx arsenal show no supplies (its panel shows the balance instead).
modded class SCR_InventoryStorageBaseUI
{
	//------------------------------------------------------------------------------------------------
	override protected void UpdateContainerResources(notnull Widget targetTitle, notnull BaseInventoryStorageComponent targetStorage)
	{
		MRX_ArsenalShopComponent arsenal = MRX_ArsenalShopComponent.Find(targetStorage.GetOwner());
		if (!arsenal)
		{
			super.UpdateContainerResources(targetTitle, targetStorage);
			return;
		}

		MRX_ArsenalUI.HideSupplies(targetTitle);
	}

	//------------------------------------------------------------------------------------------------
	override void RefreshResources()
	{
		BaseInventoryStorageComponent storage = GetCurrentNavigationStorage();
		MRX_ArsenalShopComponent arsenal;
		if (storage)
			arsenal = MRX_ArsenalShopComponent.Find(storage.GetOwner());

		if (!arsenal)
		{
			super.RefreshResources();
			return;
		}

		MRX_ArsenalUI.HideSupplies(m_widget);
	}
}

//------------------------------------------------------------------------------------------------
//! Entering a Marx arsenal from the vicinity panel, by any input, opens its own panel instead.
modded class SCR_InventoryStorageLootUI
{
	//------------------------------------------------------------------------------------------------
	override void Traverse(BaseInventoryStorageComponent storage)
	{
		SCR_InventoryMenuUI menu = GetInventoryMenuHandler();
		if (menu && storage && MRX_ArsenalShopComponent.Find(storage.GetOwner()))
		{
			menu.MRX_OpenArsenalPanel(storage);
			return;
		}

		super.Traverse(storage);
	}
}
