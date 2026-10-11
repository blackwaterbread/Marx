//! Items dropped on an attachment slot of an inspected weapon. An item of a Marx arsenal (a local preview, which vanilla
//! cannot attach) goes into that slot only when the weapon is in the cart of the open trade (MRX_ArsenalTradeClient).
//! Items of the cart move only within the cart, and the player's own items do not go into it.
modded class SCR_InventoryGearInspectionPointUI
{
	//------------------------------------------------------------------------------------------------
	override bool OnDrop(SCR_InventorySlotUI slot)
	{
		if (MRX_OnDrop(slot))
			return true;

		return super.OnDrop(slot);
	}

	//------------------------------------------------------------------------------------------------
	//! \return False when vanilla handles the drop.
	protected bool MRX_OnDrop(SCR_InventorySlotUI slot)
	{
		if (!slot || !m_Storage)
			return false;

		IEntity weapon = m_Storage.GetOwner();
		bool weaponInCart = MRX_ArsenalTradeStorageComponent.IsInCart(weapon);
		MRX_ArsenalTradeClient trade = MRX_ArsenalTradeClient.GetLocal();
		int slotId = MRX_GetDropSlotId();

		SCR_ArsenalInventorySlotUI arsenalSlot = SCR_ArsenalInventorySlotUI.Cast(slot);
		if (arsenalSlot)
		{
			MRX_ArsenalShopComponent arsenal = arsenalSlot.MRX_FindArsenal();
			if (!arsenal)
				return false;

			// The sale tab lists what the arsenal buys back: nothing to take from there.
			bool trading = trade && trade.IsTradingAt(arsenal);
			MRX_ShopItem item = arsenalSlot.MRX_FindShopItem();
			if ((trading && trade.IsSaleTab()) || !item || !arsenalSlot.IsAvailable())
				return MRX_Refuse();

			if (!weaponInCart || !trading)
				return MRX_Refuse();

			trade.Add(item.m_sId, m_Storage, slotId);
			return true;
		}

		InventoryItemComponent itemComponent = slot.GetInventoryItemComponent();
		if (!itemComponent || !itemComponent.GetOwner())
			return false;

		if (MRX_ArsenalTradeStorageComponent.IsInCart(itemComponent.GetOwner()))
		{
			if (!weaponInCart || !trade)
				return MRX_Refuse();

			trade.MoveInCart(itemComponent.GetOwner(), m_Storage, slotId);
			return true;
		}

		if (weaponInCart)
			return MRX_Refuse();

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Places the attachment points on the preview again without rebuilding them, as vanilla UpdatePreviewSlotWidgets
	//! does (which also rebuilds every point). A point whose place is not known yet (the preview is not drawn) stays.
	void MRX_PlacePoints(notnull ItemPreviewWidget preview)
	{
		foreach (int i, SCR_InventorySlotUI slot : m_aSlots)
		{
			if (!slot || !slot.GetWidget() || !m_aStorageSlotsInStorage.IsIndexValid(i))
				continue;

			vector transform[4];
			transform[3] = m_aStorageSlotsInStorage[i].GetInspectionWidgetOffset();
			vector position;
			if (preview.TryGetItemNodePositionInWidgetSpace(-1, transform, position))
				FrameSlot.SetPos(slot.GetWidget(), position[0], position[1]);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Slot of the weapon's storage under the drop: the focused attachment point (its index is the slot ID), or -1.
	protected int MRX_GetDropSlotId()
	{
		SCR_InventorySlotUI point = m_pFocusedSlot;
		if (!point && m_MenuHandler)
			point = m_MenuHandler.GetCurrentlyFocusedSlot();

		if (!point)
			return -1;

		return m_aSlots.Find(point);
	}

	//------------------------------------------------------------------------------------------------
	//! Handled by doing nothing. \return True.
	protected static bool MRX_Refuse()
	{
		SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_INV_DROP_ERROR);
		return true;
	}
}
