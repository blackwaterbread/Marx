#ifdef WORKBENCH
//! Workbench only: test hooks that give an input on a slot of the inventory as the player does (the game view takes no
//! synthesized keys).
modded class SCR_InventoryMenuUI
{
	//------------------------------------------------------------------------------------------------
	//! X (Inventory_Drop).
	void MRX_TestPressDrop(notnull SCR_InventorySlotUI slot)
	{
		m_pFocusedSlotUI = slot;
		Action_MoveBetween();
	}

	//------------------------------------------------------------------------------------------------
	//! Middle mouse (Inventory_OpenNewStorage).
	void MRX_TestOpenAsContainer(notnull SCR_InventorySlotUI slot)
	{
		m_pFocusedSlotUI = slot;
		Action_OpenContainer();
	}

	//------------------------------------------------------------------------------------------------
	//! Double click (Inventory_OpenStorage).
	void MRX_TestOpenStorage(notnull SCR_InventorySlotUI slot)
	{
		m_pFocusedSlotUI = slot;
		Action_UnfoldItem();
	}

	//------------------------------------------------------------------------------------------------
	//! Storages opened as containers on the left.
	int MRX_TestGetOpenedStorageCount()
	{
		return m_aOpenedStoragesUI.Count();
	}
}
#endif
