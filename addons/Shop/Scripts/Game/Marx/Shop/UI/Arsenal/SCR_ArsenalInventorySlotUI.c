//! Slots of a Marx arsenal show the Marx price instead of the supply cost and can be taken when the local wallet covers
//! it. Vanilla rank locks and military supply allocation do not apply.
modded class SCR_ArsenalInventorySlotUI
{
	protected static const string MRX_SUPPLY_ICON_WIDGET_NAME = "SuppliesIcon";

	//------------------------------------------------------------------------------------------------
	//! Catalog entry of this slot when it belongs to a Marx arsenal, or null.
	MRX_ShopItem MRX_FindShopItem()
	{
		if (!m_pItem || !m_pItem.GetOwner() || !GetStorageUI())
			return null;

		BaseInventoryStorageComponent storage = GetStorageUI().GetCurrentNavigationStorage();
		if (!storage)
			return null;

		MRX_ArsenalShopComponent arsenal = MRX_ArsenalShopComponent.Find(storage.GetOwner());
		if (!arsenal)
			return null;

		return arsenal.FindItem(SCR_ResourceNameUtils.GetPrefabName(m_pItem.GetOwner()));
	}

	//------------------------------------------------------------------------------------------------
	override float GetTotalResources()
	{
		MRX_ShopItem item = MRX_FindShopItem();
		if (!item)
			return super.GetTotalResources();

		m_fSupplyCost = item.m_iPrice;
		return m_fSupplyCost;
	}

	//------------------------------------------------------------------------------------------------
	override void UpdateTotalResources(float totalResources)
	{
		MRX_ShopItem item = MRX_FindShopItem();
		if (!item || !m_CostResourceHolder || !m_CostResourceHolderText)
		{
			super.UpdateTotalResources(totalResources);
			return;
		}

		m_CostResourceHolder.SetVisible(true);
		Widget icon = m_CostResourceHolder.FindAnyWidget(MRX_SUPPLY_ICON_WIDGET_NAME);
		if (icon)
			icon.SetVisible(false);

		// One cell is too narrow for "$14,500".
		if (m_iSizeX <= 1)
			m_CostResourceHolderText.SetText(MRX_TextFormat.MoneyCompact(item.m_iPrice, item.m_sCurrency));
		else
			m_CostResourceHolderText.SetText(MRX_TextFormat.Money(item.m_iPrice, item.m_sCurrency));

		int balance;
		MRX_ClientWallet wallet = MRX_ClientWallet.GetLocal();
		SetItemAvailability(wallet && wallet.TryGetBalance(item.m_sCurrency, balance) && balance >= item.m_iPrice);
	}

	//------------------------------------------------------------------------------------------------
	override int GetPersonalResourceCost()
	{
		if (MRX_FindShopItem())
			return 0;

		return super.GetPersonalResourceCost();
	}

	//------------------------------------------------------------------------------------------------
	override protected void SetItemRank()
	{
		if (!MRX_FindShopItem())
		{
			super.SetItemRank();
			return;
		}

		m_eRequiredRank = -1;
	}
}
