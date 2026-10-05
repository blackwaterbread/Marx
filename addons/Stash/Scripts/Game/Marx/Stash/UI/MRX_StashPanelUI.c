//! Stash panel of the vanilla inventory: shows every page the stash may fill from the start, also empty ones.
//! Items are still laid out from the first page on, as in any vanilla storage panel.
class MRX_StashPanelUI : SCR_InventoryOpenedStorageUI
{
	//------------------------------------------------------------------------------------------------
	override protected void SortSlots()
	{
		super.SortSlots();

		MRX_StashStorageComponent stash = MRX_StashStorageComponent.Cast(m_Storage);
		if (stash && stash.GetMaxPages() > m_iNrOfPages)
			m_iNrOfPages = stash.GetMaxPages();
	}

	//------------------------------------------------------------------------------------------------
	void MRX_StashPanelUI(
		BaseInventoryStorageComponent storage,
		LoadoutAreaType slotID = null,
		SCR_InventoryMenuUI menuManager = null,
		int iPage = 0,
		array<BaseInventoryStorageComponent> aTraverseStorage = null,
		int cols = 6,
		int rows = 3,
		bool fromVicinity = false)
	{
	}
}
