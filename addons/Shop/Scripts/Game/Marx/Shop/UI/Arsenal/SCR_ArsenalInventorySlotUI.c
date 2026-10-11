//! Slots of a Marx arsenal show the Marx price instead of the supply cost and can be taken when the local wallet covers
//! it (with a trade: the balance after the cart). In the sale tab of a trade they show what the arsenal pays for the
//! item. Vanilla rank locks and military supply allocation do not apply.
modded class SCR_ArsenalInventorySlotUI
{
	protected static const string MRX_SUPPLY_ICON_WIDGET_NAME = "SuppliesIcon";

	//------------------------------------------------------------------------------------------------
	//! Marx arsenal this slot belongs to, or null.
	MRX_ArsenalShopComponent MRX_FindArsenal()
	{
		if (!GetStorageUI())
			return null;

		BaseInventoryStorageComponent storage = GetStorageUI().GetCurrentNavigationStorage();
		if (!storage)
			return null;

		return MRX_ArsenalShopComponent.Find(storage.GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	//! The slot lists what the arsenal buys back (sale tab of a trade).
	bool MRX_IsSaleList()
	{
		SCR_InventoryOpenedStorageArsenalUI panel = SCR_InventoryOpenedStorageArsenalUI.Cast(GetStorageUI());
		return panel && panel.MRX_IsSaleList();
	}

	//------------------------------------------------------------------------------------------------
	//! Catalog entry of this slot when it belongs to a Marx arsenal, or null.
	MRX_ShopItem MRX_FindShopItem()
	{
		if (!m_pItem || !m_pItem.GetOwner())
			return null;

		MRX_ArsenalShopComponent arsenal = MRX_FindArsenal();
		if (!arsenal)
			return null;

		ResourceName prefab = SCR_ResourceNameUtils.GetPrefabName(m_pItem.GetOwner());
		if (MRX_IsSaleList())
			return arsenal.FindSellItem(prefab);

		return arsenal.FindItem(prefab);
	}

	//------------------------------------------------------------------------------------------------
	//! Purchase price, or in the sale list the price the arsenal pays.
	protected int MRX_GetShownPrice(notnull MRX_ShopItem item)
	{
		if (!MRX_IsSaleList())
			return item.m_iPrice;

		MRX_ArsenalShopComponent arsenal = MRX_FindArsenal();
		if (!arsenal || !arsenal.GetShop())
			return 0;

		return arsenal.GetShop().GetSellPrice(item);
	}

	//------------------------------------------------------------------------------------------------
	override float GetTotalResources()
	{
		MRX_ShopItem item = MRX_FindShopItem();
		if (!item)
			return super.GetTotalResources();

		m_fSupplyCost = MRX_GetShownPrice(item);
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
		int price = MRX_GetShownPrice(item);
		if (m_iSizeX <= 1)
			m_CostResourceHolderText.SetText(MRX_TextFormat.MoneyCompact(price, item.m_sCurrency));
		else
			m_CostResourceHolderText.SetText(MRX_TextFormat.Money(price, item.m_sCurrency));

		// The sale list only informs.
		if (MRX_IsSaleList())
		{
			SetItemAvailability(true);
			return;
		}

		int balance;
		bool known;
		MRX_ArsenalTradeClient trade = MRX_ArsenalTradeClient.GetLocal();
		if (trade && trade.IsTradingAt(MRX_FindArsenal()))
		{
			known = trade.GetBalanceAfter(item.m_sCurrency, balance);
		}
		else
		{
			MRX_ClientWallet wallet = MRX_ClientWallet.GetLocal();
			known = wallet && wallet.TryGetBalance(item.m_sCurrency, balance);
		}

		SetItemAvailability(known && balance >= item.m_iPrice);
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
