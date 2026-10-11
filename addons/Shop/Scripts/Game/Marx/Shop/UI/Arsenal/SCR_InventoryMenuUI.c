//! A Marx arsenal opens as its own panel the size of the vicinity panel, which it replaces (like the Marx stash).
//! An arsenal with a cart (MRX_ArsenalShopComponent.UsesCart) opens a trade (MRX_ArsenalTradeClient) and shows it in the
//! middle instead of the character (MRX_ArsenalTradePanel): items taken from the arsenal go into the cart, items given
//! to it into the sale box. The cart or the sale box stays in view: an item of the trade opened (double click, middle
//! mouse) shows in the space below it. Items of the trade are not inspected; a weapon inspection of the player's own
//! takes the middle as without a trade.
modded class SCR_InventoryMenuUI
{
	//! Same size as the vicinity panel.
	protected static const int MRX_ARSENAL_COLUMNS = 6;
	protected static const int MRX_ARSENAL_ROWS = 8;
	//! Overlay in the middle with the character preview, which the trade panel takes over.
	protected static const string MRX_CENTER_AREA = "SoldierArea";

	protected ref MRX_ArsenalTradePanel m_MRX_TradePanel;
	//! Weak: the controller owns the trade.
	protected MRX_ArsenalTradeClient m_MRX_Trade;
	//! Weak: the arsenal of the trade.
	protected MRX_ArsenalShopComponent m_MRX_TradeArsenal;
	//! Children of the center area hidden for the trade panel.
	protected ref array<Widget> m_aMRX_HiddenCenter = {};

	//------------------------------------------------------------------------------------------------
	//! Opens the storage of a Marx arsenal as its own panel without a close button.
	void MRX_OpenArsenalPanel(notnull BaseInventoryStorageComponent storage)
	{
		// OpenStorageAsContainer closes a storage that is already open.
		if (GetOpenedStorage(storage))
			return;

		OpenStorageAsContainer(storage, false, true);
		MRX_UpdateVicinityVisibility();

		MRX_ArsenalShopComponent arsenal = MRX_ArsenalShopComponent.Find(storage.GetOwner());
		if (arsenal && arsenal.UsesCart())
			MRX_StartTrade(arsenal);
	}

	//------------------------------------------------------------------------------------------------
	//! The panel of the open trade in the middle, or null.
	MRX_ArsenalTradePanel MRX_GetTradePanel()
	{
		return m_MRX_TradePanel;
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

	//------------------------------------------------------------------------------------------------
	override void RemoveOpenStorage(SCR_InventoryOpenedStorageUI openedStorage)
	{
		super.RemoveOpenStorage(openedStorage);
		if (m_MRX_TradeArsenal && !MRX_FindArsenalPanel(m_MRX_TradeArsenal))
			MRX_StopTrade();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		MRX_StopTrade();
		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------------------------
	//! While trading, vanilla places the attachment points of an inspection while the trade panel still hides the
	//! preview: they follow it every frame.
	override void OnMenuUpdate(float tDelta)
	{
		super.OnMenuUpdate(tDelta);
		if (m_MRX_TradePanel && m_pGearInspectionPointUI && m_wPlayerRender)
			m_pGearInspectionPointUI.MRX_PlacePoints(m_wPlayerRender);
	}

	//------------------------------------------------------------------------------------------------
	//! Closes the item storage shown below the cart or the sale box.
	void MRX_CloseTradeDetail()
	{
		if (!m_MRX_TradePanel || !m_MRX_TradePanel.GetDetailStorageUI())
			return;

		SCR_InventoryStorageBaseUI detail = m_MRX_TradePanel.GetDetailStorageUI();
		if (m_pActiveHoveredStorageUI == detail)
			m_pActiveHoveredStorageUI = null;

		if (m_pActiveStorageUI == detail)
			m_pActiveStorageUI = null;

		m_MRX_TradePanel.CloseDetail();
		SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_INV_CONTAINER_CLOSE);
	}

	//------------------------------------------------------------------------------------------------
	//! Middle mouse on an item of the trade: its storage opens below the cart or the sale box, not on the left.
	override protected void OpenAsNewContainer()
	{
		if (!MRX_ShowTradeDetail(true))
			super.OpenAsNewContainer();
	}

	//------------------------------------------------------------------------------------------------
	//! Double click on an item of the cart or the sale box: its storage opens below them instead of in their place.
	//! Inside that storage, vanilla opens storages in place.
	override protected void TraverseActualSlot()
	{
		if (!MRX_ShowTradeDetail(false))
			super.TraverseActualSlot();
	}

	//------------------------------------------------------------------------------------------------
	//! Shows the storage of the focused item of the trade below the cart or the sale box, or closes it when shown.
	//! \param fromDetail Also for an item inside the storage shown there (it replaces that storage).
	//! \return False when vanilla handles it.
	protected bool MRX_ShowTradeDetail(bool fromDetail)
	{
		if (!m_MRX_TradePanel || !m_pFocusedSlotUI || !m_pFocusedSlotUI.GetInventoryItemComponent())
			return false;

		IEntity item = m_pFocusedSlotUI.GetInventoryItemComponent().GetOwner();
		BaseInventoryStorageComponent storage = m_pFocusedSlotUI.GetAsStorage();
		if (!item || !storage || !MRX_ArsenalTradeStorageComponent.FindHolder(item))
			return false;

		if (!fromDetail && m_pFocusedSlotUI.GetStorageUI() == m_MRX_TradePanel.GetDetailStorageUI())
			return false;

		m_pSelectedSlotUI = m_pFocusedSlotUI;
		if (m_pGearInspectionPointUI)
			InspectItem(null);

		if (m_MRX_TradePanel.GetDetailStorageUI() && m_MRX_TradePanel.GetDetailStorageUI().GetStorage() == storage)
		{
			MRX_CloseTradeDetail();
			return true;
		}

		m_MRX_TradePanel.ToggleDetail(storage);
		SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_INV_HOTKEY_OPEN);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Vanilla shows the character once the menu is set up.
	override protected bool ProcessInitQueue()
	{
		bool done = super.ProcessInitQueue();
		if (MRX_IsTradeShown())
			MRX_HideCenter();

		return done;
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnQueueProcessed()
	{
		super.OnQueueProcessed();
		if (MRX_IsTradeShown())
			MRX_HideCenter();
	}

	//------------------------------------------------------------------------------------------------
	//! A weapon inspection uses the middle: the trade panel (and an item storage below the cart) steps aside until it ends.
	override void InspectItem(SCR_InventorySlotUI itemSlot)
	{
		super.InspectItem(itemSlot);
		if (m_pGearInspectionPointUI)
			MRX_CloseTradeDetail();

		MRX_UpdateCenter();
	}

	//------------------------------------------------------------------------------------------------
	//! X on an item of the trade: an item of the cart leaves the cart, an item of the sale box comes back, an item of the
	//! player's goes into the sale box. Arsenal items go into the cart (MoveBetweenFromVicinity_VirtualArsenal).
	override protected void Action_MoveBetween()
	{
		IEntity item;
		if (m_pFocusedSlotUI && m_pFocusedSlotUI.GetInventoryItemComponent() && !SCR_ArsenalInventorySlotUI.Cast(m_pFocusedSlotUI))
			item = m_pFocusedSlotUI.GetInventoryItemComponent().GetOwner();

		if (!MRX_IsTrading() || !item || !GetCanInteract())
		{
			super.Action_MoveBetween();
			return;
		}

		if (m_pFocusedSlotUI == m_pInspectedSlot)
			InspectItem(null);

		MRX_ArsenalTradeStorageComponent holder = MRX_ArsenalTradeStorageComponent.FindHolder(item);
		bool done;
		if (holder && holder.GetRole() == MRX_EArsenalTradeRole.CART)
		{
			m_MRX_Trade.RemoveFromCart(item);
			done = true;
		}
		else if (holder)
		{
			done = MRX_ReturnFromSale(item, m_pFocusedSlotUI.GetStorageUI());
		}
		else if (item.GetRootParent() == m_Player)
		{
			done = m_MRX_Trade.PutUpForSale(item);
		}
		else
		{
			super.Action_MoveBetween();
			return;
		}

		if (!done)
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_INV_DROP_ERROR);

		m_pSelectedSlotUI = null;
		ResetHighlightsOnAvailableStorages();
		NavigationBarUpdate();
	}

	//------------------------------------------------------------------------------------------------
	//! Drops while trading. Items of the cart go only within the cart or back to the arsenal (taken out); the player's
	//! own items go into the sale box when dropped on it or on the arsenal, never into the cart. Arsenal items and drops on
	//! attachment points take the vanilla way, which ends in the trade (*_VirtualArsenal, SCR_InventoryGearInspectionPointUI).
	override protected void Action_Drop()
	{
		if (!MRX_IsTrading() || !m_pSelectedSlotUI || SCR_InventorySlotGearInspectionUI.Cast(m_pFocusedSlotUI) || !MRX_HandleTradeDrop())
		{
			super.Action_Drop();
			return;
		}

		DeselectSlot();
		NavigationBarUpdate();
		ResetHighlightsOnAvailableStorages();
	}

	//------------------------------------------------------------------------------------------------
	//! \return False when vanilla handles the drop.
	protected bool MRX_HandleTradeDrop()
	{
		if (SCR_ArsenalInventorySlotUI.Cast(m_pSelectedSlotUI) || !m_pSelectedSlotUI.GetInventoryItemComponent())
			return false;

		IEntity item = m_pSelectedSlotUI.GetInventoryItemComponent().GetOwner();
		if (!item)
			return false;

		if (m_pSelectedSlotUI == m_pInspectedSlot)
			InspectItem(null);

		bool toArsenal = MRX_IsDropOnArsenal();
		bool toCart = MRX_IsDropOnTrade(false);
		bool toSale = MRX_IsDropOnTrade(true);
		MRX_ArsenalTradeStorageComponent holder = MRX_ArsenalTradeStorageComponent.FindHolder(item);
		if (holder && holder.GetRole() == MRX_EArsenalTradeRole.CART)
		{
			if (toArsenal)
			{
				m_MRX_Trade.RemoveFromCart(item);
				MRX_PlayDrop(true);
				return true;
			}

			if (toCart)
			{
				m_MRX_Trade.MoveInCart(item, MRX_GetFocusedCartStorage(item));
				MRX_PlayDrop(true);
				return true;
			}

			MRX_PlayDrop(false);
			return true;
		}

		if (toCart)
		{
			MRX_PlayDrop(false);
			return true;
		}

		// Items of the sale box go back by the vanilla way.
		if (holder)
			return false;

		// Dropped on an item of the sale box or into the item storage shown below it: into that item, the vanilla way.
		bool intoSaleItem = toSale && ((m_pFocusedSlotUI && m_pFocusedSlotUI.GetInventoryItemComponent()) || MRX_IsOverStorageUI(m_MRX_TradePanel.GetDetailStorageUI()));
		if (toArsenal || (toSale && !intoSaleItem))
		{
			MRX_PlayDrop(m_MRX_Trade.PutUpForSale(item));
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts an arsenal item into the cart instead of buying it.
	override protected bool MoveBetweenFromVicinity_VirtualArsenal()
	{
		if (!MRX_IsTradeArsenalSlot(m_pSelectedSlotUI))
			return super.MoveBetweenFromVicinity_VirtualArsenal();

		MRX_PlayDrop(MRX_TakeFromArsenal(null));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Dropped on a storage: into the cart, into that storage when it belongs to an item in the cart.
	override protected bool MoveItem_VirtualArsenal(inout SCR_InventoryStorageBaseUI pStorageBaseUI = null, out bool operationFailed = false)
	{
		if (!MRX_IsTradeArsenalSlot(m_pSelectedSlotUI))
			return super.MoveItem_VirtualArsenal(pStorageBaseUI, operationFailed);

		// Dropped back onto the arsenal: nothing to do, as in vanilla.
		BaseInventoryStorageComponent storage;
		if (m_pActiveHoveredStorageUI)
			storage = m_pActiveHoveredStorageUI.GetCurrentNavigationStorage();

		if (IsStorageArsenal(storage))
			return true;

		operationFailed = !MRX_TakeFromArsenal(storage);
		if (!operationFailed)
			MRX_PlayDrop(true);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Dropped on an item: into the cart, into that item when it is in the cart.
	override protected bool MoveItemToStorageSlot_VirtualArsenal(out bool operationFailed = false)
	{
		if (!MRX_IsTradeArsenalSlot(m_pSelectedSlotUI))
			return super.MoveItemToStorageSlot_VirtualArsenal(operationFailed);

		// Dropped onto an arsenal item: nothing to do, as in vanilla.
		if (!m_pFocusedSlotUI || SCR_ArsenalInventorySlotUI.Cast(m_pFocusedSlotUI))
		{
			operationFailed = true;
			return true;
		}

		BaseInventoryStorageComponent storage = m_pFocusedSlotUI.GetAsStorage();
		if (!storage && m_pFocusedSlotUI.GetStorageUI())
			storage = m_pFocusedSlotUI.GetStorageUI().GetStorage();

		operationFailed = !MRX_TakeFromArsenal(storage);
		if (!operationFailed)
		{
			MRX_PlayDrop(true);
			DeselectSlot();
			NavigationBarUpdate();
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override protected bool EquipWeaponIntoWeaponSlot_VirtualArsenal()
	{
		if (!MRX_IsTradeArsenalSlot(m_pSelectedSlotUI))
			return super.EquipWeaponIntoWeaponSlot_VirtualArsenal();

		MRX_PlayDrop(MRX_TakeFromArsenal(null));
		DeselectSlot();
		NavigationBarUpdate();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override protected bool EquipDraggedItem_VirtualArsenal(bool forceEquip = false)
	{
		if (!MRX_IsTradeArsenalSlot(m_pSelectedSlotUI))
			return super.EquipDraggedItem_VirtualArsenal(forceEquip);

		MRX_PlayDrop(MRX_TakeFromArsenal(null));
		DeselectSlot();
		NavigationBarUpdate();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the selected arsenal item into the cart. \return False when it is not available or the trade refuses it.
	protected bool MRX_TakeFromArsenal(BaseInventoryStorageComponent storage)
	{
		SCR_ArsenalInventorySlotUI slot = SCR_ArsenalInventorySlotUI.Cast(m_pSelectedSlotUI);
		if (!slot || !slot.IsAvailable() || !m_MRX_Trade)
			return false;

		return m_MRX_Trade.TakeFromArsenal(m_MRX_TradeArsenal, slot.MRX_FindShopItem(), storage);
	}

	//------------------------------------------------------------------------------------------------
	//! An arsenal slot of the trade's arsenal.
	protected bool MRX_IsTradeArsenalSlot(SCR_InventorySlotUI slot)
	{
		SCR_ArsenalInventorySlotUI arsenalSlot = SCR_ArsenalInventorySlotUI.Cast(slot);
		return arsenalSlot && MRX_IsTrading() && arsenalSlot.MRX_FindArsenal() == m_MRX_TradeArsenal;
	}

	//------------------------------------------------------------------------------------------------
	//! Moves an item of the sale box back into the player's inventory: where it was before the sale box when it still
	//! takes it, else where vanilla puts an item picked up (weapon slot, worn, then any storage). A move like a drag, with
	//! its refresh of the panels: picking up does not reach the trade container, which is not in the vicinity.
	protected bool MRX_ReturnFromSale(notnull IEntity item, SCR_InventoryStorageBaseUI fromUI)
	{
		if (!m_InventoryManager || !m_InventoryManager.CanMoveItem(item))
			return false;

		BaseInventoryStorageComponent target = m_MRX_Trade.GetSaleOrigin(item);
		if (!target || !target.GetOwner() || target.GetOwner().GetRootParent() != m_Player || !m_InventoryManager.CanMoveItemToStorage(item, target))
		{
			target = null;
			array<EStoragePurpose> purposes = {EStoragePurpose.PURPOSE_WEAPON_PROXY, EStoragePurpose.PURPOSE_EQUIPMENT_ATTACHMENT, EStoragePurpose.PURPOSE_DEPOSIT, EStoragePurpose.PURPOSE_ANY};
			foreach (EStoragePurpose purpose : purposes)
			{
				BaseInventoryStorageComponent found = m_InventoryManager.FindStorageForItem(item, purpose);
				if (found && found.GetOwner().GetRootParent() == m_Player)
				{
					target = found;
					break;
				}
			}
		}

		if (!target)
			return false;

		SCR_InvCallBack callback = new SCR_InvCallBack();
		callback.m_pMenu = this;
		callback.m_pItem = item;
		callback.m_pStorageMan = m_InventoryManager;
		callback.m_pStorageFrom = fromUI;
		callback.m_pStorageTo = GetStorageUIByBaseStorageComponent(target);
		// Worn and weapon slots show in the storage list.
		if (!callback.m_pStorageTo)
			callback.m_pStorageTo = m_pStorageListUI;

		return m_InventoryManager.TryMoveItemToStorage(item, target, -1, callback);
	}

	//------------------------------------------------------------------------------------------------
	//! Storage under the cursor for an item moved within the cart: of the item of the cart there (e.g. a rifle a scope is
	//! dropped on), else of the item storage shown below the cart, else null for the cart's cells.
	protected BaseInventoryStorageComponent MRX_GetFocusedCartStorage(notnull IEntity moved)
	{
		if (m_pFocusedSlotUI && m_pFocusedSlotUI != m_pSelectedSlotUI && m_pFocusedSlotUI.GetInventoryItemComponent())
		{
			IEntity target = m_pFocusedSlotUI.GetInventoryItemComponent().GetOwner();
			if (target && target != moved && MRX_ArsenalTradeStorageComponent.IsInCart(target) && m_pFocusedSlotUI.GetAsStorage())
				return m_pFocusedSlotUI.GetAsStorage();
		}

		SCR_InventoryStorageBaseUI detail;
		if (m_MRX_TradePanel)
			detail = m_MRX_TradePanel.GetDetailStorageUI();

		if (detail && MRX_IsOverStorageUI(detail))
			return detail.GetCurrentNavigationStorage();

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The cursor is over the storage panel or one of its slots.
	protected bool MRX_IsOverStorageUI(SCR_InventoryStorageBaseUI storageUI)
	{
		if (!storageUI)
			return false;

		return m_pActiveHoveredStorageUI == storageUI || (m_pFocusedSlotUI && m_pFocusedSlotUI.GetStorageUI() == storageUI);
	}

	//------------------------------------------------------------------------------------------------
	//! The drop goes onto the panel of the trade's arsenal.
	protected bool MRX_IsDropOnArsenal()
	{
		SCR_InventoryOpenedStorageUI arsenalPanel = MRX_FindArsenalPanel(m_MRX_TradeArsenal);
		if (!arsenalPanel)
			return false;

		if (m_pActiveHoveredStorageUI == arsenalPanel)
			return true;

		return m_pFocusedSlotUI && m_pFocusedSlotUI.GetStorageUI() == arsenalPanel;
	}

	//------------------------------------------------------------------------------------------------
	//! The drop goes onto the shown storage panel of the trade or onto the item storage shown below it (one of the shown
	//! tab). \param sale True: the sale box, false: the cart.
	protected bool MRX_IsDropOnTrade(bool sale)
	{
		if (!m_MRX_TradePanel || !m_MRX_TradePanel.IsVisible())
			return false;

		MRX_ArsenalTradeStorageUI storageUI = m_MRX_TradePanel.GetStorageUI(sale);
		if (!storageUI || storageUI != m_MRX_TradePanel.GetShownStorageUI())
			return false;

		return MRX_IsOverStorageUI(storageUI) || MRX_IsOverStorageUI(m_MRX_TradePanel.GetDetailStorageUI());
	}

	//------------------------------------------------------------------------------------------------
	protected static void MRX_PlayDrop(bool success)
	{
		if (success)
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_INV_CONTAINER_DIFR_DROP);
		else
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_INV_DROP_ERROR);
	}

	//------------------------------------------------------------------------------------------------
	//! Open panel of an arsenal, or null.
	protected SCR_InventoryOpenedStorageUI MRX_FindArsenalPanel(MRX_ArsenalShopComponent arsenal)
	{
		if (!arsenal)
			return null;

		foreach (SCR_InventoryOpenedStorageUI panel : m_aOpenedStoragesUI)
		{
			if (panel && panel.GetStorage() && MRX_ArsenalShopComponent.Find(panel.GetStorage().GetOwner()) == arsenal)
				return panel;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The trade of this menu is open at its arsenal.
	protected bool MRX_IsTrading()
	{
		return m_MRX_Trade && m_MRX_TradeArsenal && m_MRX_Trade.IsTradingAt(m_MRX_TradeArsenal);
	}

	//------------------------------------------------------------------------------------------------
	protected void MRX_StartTrade(notnull MRX_ArsenalShopComponent arsenal)
	{
		if (m_MRX_TradeArsenal == arsenal && MRX_IsTrading())
			return;

		MRX_StopTrade();
		m_MRX_Trade = MRX_ArsenalTradeClient.GetLocal();
		if (!m_MRX_Trade)
			return;

		m_MRX_TradeArsenal = arsenal;
		m_MRX_Trade.GetOnChanged().Insert(MRX_OnTradeChanged);
		m_MRX_Trade.Open(arsenal.GetOwner());
		MRX_OnTradeChanged();
	}

	//------------------------------------------------------------------------------------------------
	protected void MRX_StopTrade()
	{
		MRX_RemoveTradePanel();
		if (m_MRX_Trade)
		{
			m_MRX_Trade.GetOnChanged().Remove(MRX_OnTradeChanged);
			if (m_MRX_TradeArsenal && m_MRX_Trade.IsTradingAt(m_MRX_TradeArsenal))
				m_MRX_Trade.Close(true);
		}

		m_MRX_Trade = null;
		m_MRX_TradeArsenal = null;
	}

	//------------------------------------------------------------------------------------------------
	//! The panel comes once the trade container has replicated, and goes when the trade closes (e.g. the server closed it).
	protected void MRX_OnTradeChanged()
	{
		if (!MRX_IsTrading())
		{
			MRX_RemoveTradePanel();
			return;
		}

		if (m_MRX_TradePanel || !m_MRX_Trade.IsReady())
			return;

		Widget root = GetRootWidget();
		Widget area;
		if (root)
			area = root.FindAnyWidget(MRX_CENTER_AREA);

		if (!area)
			return;

		m_MRX_TradePanel = MRX_ArsenalTradePanel.Create(area, this, m_MRX_Trade, m_MRX_TradeArsenal);
		MRX_UpdateCenter();
	}

	//------------------------------------------------------------------------------------------------
	protected void MRX_RemoveTradePanel()
	{
		if (m_MRX_TradePanel)
			m_MRX_TradePanel.Remove();

		m_MRX_TradePanel = null;
		MRX_RestoreCenter();
	}

	//------------------------------------------------------------------------------------------------
	protected bool MRX_IsTradeShown()
	{
		return m_MRX_TradePanel && m_MRX_TradePanel.IsVisible();
	}

	//------------------------------------------------------------------------------------------------
	//! The trade panel in the middle, or the vanilla content while a weapon is inspected.
	protected void MRX_UpdateCenter()
	{
		if (!m_MRX_TradePanel)
		{
			MRX_RestoreCenter();
			return;
		}

		bool showTrade = !m_pGearInspectionPointUI;
		m_MRX_TradePanel.SetVisible(showTrade);
		if (showTrade)
			MRX_HideCenter();
		else
			MRX_RestoreCenter();
	}

	//------------------------------------------------------------------------------------------------
	//! Hides the visible children of the center area except the trade panel, remembering them.
	protected void MRX_HideCenter()
	{
		Widget root = GetRootWidget();
		if (!root || !m_MRX_TradePanel)
			return;

		Widget area = root.FindAnyWidget(MRX_CENTER_AREA);
		if (!area)
			return;

		Widget panelRoot = m_MRX_TradePanel.GetRootWidget();
		Widget child = area.GetChildren();
		while (child)
		{
			if (child != panelRoot && child.IsVisible())
			{
				child.SetVisible(false);
				m_aMRX_HiddenCenter.Insert(child);
			}

			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void MRX_RestoreCenter()
	{
		foreach (Widget widget : m_aMRX_HiddenCenter)
		{
			if (widget)
				widget.SetVisible(true);
		}

		m_aMRX_HiddenCenter.Clear();
	}
}
