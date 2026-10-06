//! Storage panels that replace the vicinity panel (API v0): while one of them is open, the vicinity panel (items on the
//! ground) is hidden. Addons mark their panels by overriding MRX_ReplacesVicinity().
modded class SCR_InventoryMenuUI
{
	//------------------------------------------------------------------------------------------------
	//! Override (call super): true when the open storage panel takes the place of the vicinity panel.
	protected bool MRX_ReplacesVicinity(notnull SCR_InventoryOpenedStorageUI panel)
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	bool MRX_IsVicinityShown()
	{
		return m_wLootStorage && m_wLootStorage.IsVisible();
	}

	//------------------------------------------------------------------------------------------------
	//! The vicinity panel is only hidden, not removed: vanilla code uses it without null checks.
	protected void MRX_UpdateVicinityVisibility()
	{
		if (!m_wLootStorage)
			return;

		bool replaced;
		foreach (SCR_InventoryOpenedStorageUI panel : m_aOpenedStoragesUI)
		{
			if (panel && MRX_ReplacesVicinity(panel))
			{
				replaced = true;
				break;
			}
		}

		m_wLootStorage.SetVisible(!replaced);
	}

	//------------------------------------------------------------------------------------------------
	//! Vanilla rebuilds the vicinity panel when other storages are opened or the last one is closed.
	override void ShowVicinity(bool compact = false)
	{
		super.ShowVicinity(compact);
		MRX_UpdateVicinityVisibility();
	}

	//------------------------------------------------------------------------------------------------
	override void RemoveOpenStorage(SCR_InventoryOpenedStorageUI openedStorage)
	{
		super.RemoveOpenStorage(openedStorage);
		MRX_UpdateVicinityVisibility();
	}

	//------------------------------------------------------------------------------------------------
	//! Other panels closed meanwhile may have shown the vicinity panel again.
	override void RefreshLootUIListener()
	{
		super.RefreshLootUIListener();
		MRX_UpdateVicinityVisibility();
	}
}
