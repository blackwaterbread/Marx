//! Stash panel in the vanilla inventory: the stash container is opened as its own storage panel (OpenStorageAsContainer).
//! While it is open, the vicinity panel (items on the ground) is hidden (see Marx_UI MRX_ReplacesVicinity).
modded class SCR_InventoryMenuUI
{
	protected bool m_bMRX_QuickMove;

	//------------------------------------------------------------------------------------------------
	//! Opens the stash container as a storage panel without a close button and hides the vicinity panel.
	void MRX_OpenStashPanel(notnull MRX_StashStorageComponent storage)
	{
		// OpenStorageAsContainer closes a storage that is already open.
		if (GetOpenedStorage(storage))
			return;

		OpenStorageAsContainer(storage, false, true);

		// Vanilla fills the capacity bar of a new panel only on refresh; until then it shows the widget default.
		SCR_InventoryOpenedStorageUI panel = GetOpenedStorage(storage);
		if (panel)
			panel.Refresh();

		MRX_UpdateVicinityVisibility();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The open stash panel, or null.
	SCR_InventoryOpenedStorageUI MRX_FindStashPanel()
	{
		foreach (SCR_InventoryOpenedStorageUI panel : m_aOpenedStoragesUI)
		{
			if (panel && MRX_StashStorageComponent.Cast(panel.GetStorage()))
				return panel;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	override protected bool MRX_ReplacesVicinity(notnull SCR_InventoryOpenedStorageUI panel)
	{
		if (MRX_StashStorageComponent.Cast(panel.GetStorage()))
			return true;

		return super.MRX_ReplacesVicinity(panel);
	}

	//------------------------------------------------------------------------------------------------
	override protected SCR_InventoryOpenedStorageUI CreateOpenedStorageUI(BaseInventoryStorageComponent storage)
	{
		if (!MRX_StashStorageComponent.Cast(storage))
			return super.CreateOpenedStorageUI(storage);

		// Same size as the vicinity panel (SCR_InventoryStorageLootUI), which the stash panel replaces.
		return new MRX_StashPanelUI(storage, null, this, 0, {storage}, MRX_StashStorageComponent.PAGE_COLUMNS, MRX_StashStorageComponent.PAGE_ROWS);
	}

	//------------------------------------------------------------------------------------------------
	override void RefreshLootUIListener()
	{
		// The stash container has no bounds, which vanilla takes as out of reach. The stash session closes it by distance.
		array<SCR_InventoryOpenedStorageUI> stashPanels = {};
		for (int i = m_aOpenedStoragesUI.Count() - 1; i >= 0; i--)
		{
			SCR_InventoryOpenedStorageUI panel = m_aOpenedStoragesUI[i];
			if (panel && MRX_StashStorageComponent.Cast(panel.GetStorage()))
			{
				stashPanels.Insert(panel);
				m_aOpenedStoragesUI.Remove(i);
			}
		}

		super.RefreshLootUIListener();

		foreach (SCR_InventoryOpenedStorageUI stashPanel : stashPanels)
		{
			m_aOpenedStoragesUI.Insert(stashPanel);
		}

		// Other panels closed meanwhile may have shown the vicinity panel again.
		MRX_UpdateVicinityVisibility();
	}

	//------------------------------------------------------------------------------------------------
	//! Dropped on an item slot: on an item in the stash only if that item is a storage (a bag takes it in, as in
	//! vanilla). Stash cells are not swapped.
	override void MoveItemToStorageSlot()
	{
		if (m_pFocusedSlotUI && MRX_StashPanelUI.Cast(m_pFocusedSlotUI.GetStorageUI()) && !m_pFocusedSlotUI.GetAsStorage())
		{
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_INV_DROP_ERROR);
			return;
		}

		super.MoveItemToStorageSlot();
	}

	//------------------------------------------------------------------------------------------------
	override protected void MoveBetweenToVicinity()
	{
		m_bMRX_QuickMove = true;
		super.MoveBetweenToVicinity();
		m_bMRX_QuickMove = false;
	}

	//------------------------------------------------------------------------------------------------
	override protected void MoveToVicinity(IEntity pItem)
	{
		SCR_InventoryOpenedStorageUI stashPanel;
		if (m_bMRX_QuickMove)
			stashPanel = MRX_FindStashPanel();

		if (!stashPanel || !m_InventoryManager.CanMoveItem(pItem))
		{
			super.MoveToVicinity(pItem);
			return;
		}

		// Quick move while the stash is open: into the stash instead of onto the ground, at the first free cell of the
		// shown page (or another page).
		MRX_StashStorageComponent stash = MRX_StashStorageComponent.Cast(stashPanel.GetStorage());
		MRX_StashPanelUI stashPanelUI = MRX_StashPanelUI.Cast(stashPanel);
		int shownPage;
		if (stashPanelUI)
			shownPage = stashPanelUI.GetShownPage();

		string key = MRX_StashStorageComponent.GetItemKey(pItem);
		int width, height;
		MRX_StashStorageComponent.GetItemCellSize(pItem, width, height);
		MRX_StashPlacement placement = stash.GetGrid().FindFree(width, height, shownPage, key);
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!placement || !controller)
		{
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_INV_DROP_ERROR);
			return;
		}

		stash.SetPendingPlacement(key, placement);
		controller.MRX_RequestStashPlacement(pItem, placement);

		BaseInventoryStorageComponent storageFrom = m_pSelectedSlotUI.GetStorageUI().GetStorage();
		m_pCallBack.m_pStorageFrom = GetStorageUIByBaseStorageComponent(storageFrom);
		if (!m_pCallBack.m_pStorageFrom)
			m_pCallBack.m_pStorageFrom = m_pSelectedSlotUI.GetStorageUI();

		m_pCallBack.m_pStorageTo = stashPanel;
		m_InventoryManager.InsertItem(pItem, stashPanel.GetStorage(), storageFrom, m_pCallBack);
	}
}

//------------------------------------------------------------------------------------------------
//! The stash panel shows the stash point (set on the storage by the client) instead of the invisible container.
modded class SCR_InventoryStorageBaseUI
{
	//------------------------------------------------------------------------------------------------
	override protected void SetPreviewItem()
	{
		super.SetPreviewItem();

		MRX_StashStorageComponent stash = MRX_StashStorageComponent.Cast(m_Storage);
		ItemPreviewWidget renderPreview = ItemPreviewWidget.Cast(m_wPreviewImage);
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!stash || !stash.GetPreviewEntity() || !renderPreview || !world || !world.GetItemPreviewManager())
			return;

		world.GetItemPreviewManager().SetPreviewItem(renderPreview, stash.GetPreviewEntity(), null, true);
	}
}
