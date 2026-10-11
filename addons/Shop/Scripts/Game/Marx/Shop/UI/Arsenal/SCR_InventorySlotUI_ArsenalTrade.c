//! Items in the cart of an arsenal trade are not paid for: they cannot be used or equipped from the inventory (the
//! server refuses taking them out of the cart too).
modded class SCR_InventorySlotUI
{
	//------------------------------------------------------------------------------------------------
	override void UseItem(IEntity player, SCR_EUseContext context)
	{
		if (MRX_IsUnpaid())
		{
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_INV_DROP_ERROR);
			return;
		}

		super.UseItem(player, context);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanUseItem(IEntity player)
	{
		if (MRX_IsUnpaid())
			return false;

		return super.CanUseItem(player);
	}

	//------------------------------------------------------------------------------------------------
	//! The slot's item lies in the cart of an arsenal trade.
	bool MRX_IsUnpaid()
	{
		return m_pItem && MRX_ArsenalTradeStorageComponent.IsInCart(m_pItem.GetOwner());
	}
}
