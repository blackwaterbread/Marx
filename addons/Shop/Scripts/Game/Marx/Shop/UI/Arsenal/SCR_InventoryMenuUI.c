//! A Marx arsenal opens as its own panel the size of the vicinity panel, which it replaces (like the Marx stash).
modded class SCR_InventoryMenuUI
{
	//! Same size as the vicinity panel.
	protected static const int MRX_ARSENAL_COLUMNS = 6;
	protected static const int MRX_ARSENAL_ROWS = 8;

	//------------------------------------------------------------------------------------------------
	//! Opens the storage of a Marx arsenal as its own panel without a close button.
	void MRX_OpenArsenalPanel(notnull BaseInventoryStorageComponent storage)
	{
		// OpenStorageAsContainer closes a storage that is already open.
		if (GetOpenedStorage(storage))
			return;

		OpenStorageAsContainer(storage, false, true);
		MRX_UpdateVicinityVisibility();
	}

	//------------------------------------------------------------------------------------------------
	protected static bool MRX_IsArsenalStorage(BaseInventoryStorageComponent storage)
	{
		return storage && MRX_ArsenalShopComponent.Find(storage.GetOwner()) != null;
	}

	//------------------------------------------------------------------------------------------------
	override protected bool MRX_ReplacesVicinity(notnull SCR_InventoryOpenedStorageUI panel)
	{
		if (MRX_IsArsenalStorage(panel.GetStorage()))
			return true;

		return super.MRX_ReplacesVicinity(panel);
	}

	//------------------------------------------------------------------------------------------------
	override protected SCR_InventoryOpenedStorageUI CreateOpenedStorageUI(BaseInventoryStorageComponent storage)
	{
		if (!MRX_IsArsenalStorage(storage))
			return super.CreateOpenedStorageUI(storage);

		return new SCR_InventoryOpenedStorageArsenalUI(storage, null, this, 0, {storage}, MRX_ARSENAL_COLUMNS, MRX_ARSENAL_ROWS);
	}

	//------------------------------------------------------------------------------------------------
	//! Opened through the arsenal's open action.
	override bool SetOpenStorage()
	{
		SCR_InventoryStorageManagerComponent manager = GetInventoryStorageManager();
		if (!manager)
			return super.SetOpenStorage();

		// GetStorageToOpen() also clears it: put it back for vanilla when it is not a Marx arsenal.
		IEntity storageToOpen = manager.GetStorageToOpen();
		BaseInventoryStorageComponent storage;
		if (storageToOpen)
			storage = BaseInventoryStorageComponent.Cast(storageToOpen.FindComponent(BaseInventoryStorageComponent));

		if (!MRX_IsArsenalStorage(storage))
		{
			manager.SetStorageToOpen(storageToOpen);
			return super.SetOpenStorage();
		}

		MRX_OpenArsenalPanel(storage);
		return true;
	}
}
