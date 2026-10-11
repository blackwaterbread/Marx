//! The panel of a Marx arsenal lists the items of one category or matching a search (MRX_ArsenalFilterBar above its
//! items), shows the outcome of trades in the inventory's balance panel (SCR_InventoryMenuUI.MRX_GetBalancePanel; when
//! that is turned off, in its own below its items), and updates its slots when the balance changes.
modded class SCR_InventoryOpenedStorageArsenalUI
{
	protected MRX_ClientWallet m_MRX_Wallet;
	protected SCR_PlayerController m_MRX_Controller;
	protected ref MRX_BalancePanel m_MRX_BalanceBar;
	//! The balance bar is the panel's own, not the inventory's.
	protected bool m_bMRX_OwnBalanceBar;
	protected ref MRX_ArsenalFilter m_MRX_Filter;
	protected ref MRX_ArsenalFilterBar m_MRX_FilterBar;
	//! Weak: the controller owns the trade.
	protected MRX_ArsenalTradeClient m_MRX_Trade;
	//! The panel lists what the arsenal buys back (sale tab of a trade).
	protected bool m_bMRX_SaleList;

	//------------------------------------------------------------------------------------------------
	protected MRX_ArsenalShopComponent MRX_GetArsenal()
	{
		if (!m_Storage)
			return null;

		return MRX_ArsenalShopComponent.Find(m_Storage.GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	override void Init()
	{
		// The panel lists its items right after Init, already filtered.
		MRX_ArsenalShopComponent arsenal = MRX_GetArsenal();
		if (arsenal)
		{
			string shopId;
			if (arsenal.GetShop())
				shopId = arsenal.GetShop().m_sShopId;

			m_MRX_Filter = new MRX_ArsenalFilter(shopId, arsenal.GetCategories());
		}

		super.Init();
		if (!arsenal)
			return;

		m_MRX_FilterBar = MRX_ArsenalFilterBar.Create(m_widget, m_MRX_Filter, arsenal);
		if (m_MRX_FilterBar)
			m_MRX_FilterBar.GetOnChanged().Insert(MRX_OnFilterChanged);

		SCR_InventoryMenuUI menu = GetInventoryMenuHandler();
		if (menu)
			m_MRX_BalanceBar = menu.MRX_GetBalancePanel();

		if (!m_MRX_BalanceBar)
		{
			m_MRX_BalanceBar = MRX_BalancePanel.Create(m_widget, arsenal.GetCurrencies());
			m_bMRX_OwnBalanceBar = true;
		}

		m_MRX_Wallet = MRX_ClientWallet.GetLocal();
		if (m_MRX_Wallet)
			m_MRX_Wallet.GetOnBalanceChanged().Insert(MRX_OnBalanceChanged);

		m_MRX_Controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (m_MRX_Controller)
			m_MRX_Controller.MRX_GetOnShopResult().Insert(MRX_OnShopResult);

		m_MRX_Trade = MRX_ArsenalTradeClient.GetLocal();
		if (m_MRX_Trade)
			m_MRX_Trade.GetOnChanged().Insert(MRX_OnTradeChanged);

		RefreshResources();
	}

	//------------------------------------------------------------------------------------------------
	override event void HandlerDeattached(Widget w)
	{
		if (m_MRX_Wallet)
			m_MRX_Wallet.GetOnBalanceChanged().Remove(MRX_OnBalanceChanged);

		if (m_MRX_Controller)
			m_MRX_Controller.MRX_GetOnShopResult().Remove(MRX_OnShopResult);

		if (m_MRX_Trade)
			m_MRX_Trade.GetOnChanged().Remove(MRX_OnTradeChanged);

		if (m_MRX_BalanceBar && m_bMRX_OwnBalanceBar)
			m_MRX_BalanceBar.Stop();

		if (m_MRX_FilterBar)
			m_MRX_FilterBar.Stop();

		super.HandlerDeattached(w);
	}

	//------------------------------------------------------------------------------------------------
	//! The catalog items of the chosen category that match the search, in catalog order: the items for sale, or in the
	//! sale tab of a trade the items the arsenal buys back. The panel lists its slots with the arsenal's own storage as
	//! pStorage, other calls without one.
	override protected void GetAllItems(out notnull array<IEntity> pItemsInStorage, BaseInventoryStorageComponent pStorage = null)
	{
		MRX_ArsenalShopComponent arsenal = MRX_GetArsenal();
		ItemPreviewManagerEntity previews = MRX_ScriptedDialog.GetPreviewManager();
		if ((pStorage && pStorage != m_Storage) || !arsenal || !m_MRX_Filter || !previews)
		{
			super.GetAllItems(pItemsInStorage, pStorage);
			return;
		}

		// As the vanilla listing of an arsenal storage does.
		if (pStorage)
		{
			m_bIsArsenal = true;
			if (s_OnArsenalEnter)
				s_OnArsenalEnter.Invoke();
		}

		array<MRX_ShopItem> items = {};
		m_bMRX_SaleList = m_MRX_Trade && m_MRX_Trade.IsTradingAt(arsenal) && m_MRX_Trade.IsSaleTab();
		if (m_bMRX_SaleList)
			arsenal.GetSellListItems(items);
		else
			arsenal.GetListedItems(items);

		foreach (MRX_ShopItem item : items)
		{
			if (m_MRX_Filter.Matches(item))
				pItemsInStorage.Insert(previews.ResolvePreviewEntityForPrefab(item.m_sPrefab));
		}

		if (m_MRX_FilterBar)
			m_MRX_FilterBar.SetShownCount(pItemsInStorage.Count());
	}

	//------------------------------------------------------------------------------------------------
	//! Lists a category (empty for all) and the items matching a search, as if the user had chosen them.
	void MRX_SetFilter(string category, string text)
	{
		if (!m_MRX_Filter)
			return;

		m_MRX_Filter.SetCategory(category);
		m_MRX_Filter.SetText(text);
		if (m_MRX_FilterBar)
			m_MRX_FilterBar.Sync();

		MRX_OnFilterChanged();
	}

	//------------------------------------------------------------------------------------------------
	//! Number of item slots the panel shows, on all pages.
	int MRX_GetShownCount()
	{
		int count;
		foreach (SCR_InventorySlotUI slot : m_aSlots)
		{
			if (slot && slot.GetInventoryItemComponent())
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected void MRX_OnFilterChanged()
	{
		m_iLastShownPage = 0;
		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	//! \return True while the panel lists what the arsenal buys back (sale tab of a trade).
	bool MRX_IsSaleList()
	{
		return m_bMRX_SaleList;
	}

	//------------------------------------------------------------------------------------------------
	//! A switched tab changes the list; new totals change what the balance after the cart covers.
	protected void MRX_OnTradeChanged()
	{
		MRX_ArsenalShopComponent arsenal = MRX_GetArsenal();
		bool saleList = m_MRX_Trade && m_MRX_Trade.IsTradingAt(arsenal) && m_MRX_Trade.IsSaleTab();
		if (saleList != m_bMRX_SaleList)
		{
			MRX_OnFilterChanged();
			return;
		}

		foreach (SCR_InventorySlotUI slot : m_aSlots)
		{
			if (slot)
				slot.Refresh();
		}
	}

	//------------------------------------------------------------------------------------------------
	override void RefreshResources()
	{
		MRX_ArsenalShopComponent arsenal = MRX_GetArsenal();
		if (!arsenal)
		{
			super.RefreshResources();
			return;
		}

		MRX_ArsenalUI.HideSupplies(m_widget);
	}

	//------------------------------------------------------------------------------------------------
	//! Balance text of the panel, or null.
	TextWidget MRX_GetBalanceText()
	{
		if (!m_MRX_BalanceBar)
			return null;

		return m_MRX_BalanceBar.GetAmountText();
	}

	//------------------------------------------------------------------------------------------------
	protected void MRX_OnBalanceChanged(string currency, int balance)
	{
		// The inventory's balance panel gets the change from the inventory.
		if (m_MRX_BalanceBar && m_bMRX_OwnBalanceBar)
			m_MRX_BalanceBar.OnBalanceChanged(currency, balance);

		foreach (SCR_InventorySlotUI slot : m_aSlots)
		{
			if (slot)
				slot.Refresh();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Failures and details of a sale; the balance change itself shows from the wallet.
	protected void MRX_OnShopResult(MRX_EShopStatus status, MRX_ETxStatus txStatus, string itemId, int price, string currency, int itemCount, int unpaidCount)
	{
		if (!m_MRX_BalanceBar)
			return;

		if (status != MRX_EShopStatus.OK)
		{
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_INV_DROP_ERROR);
			m_MRX_BalanceBar.ShowInfo(MRX_ShopMenu.GetFailureText(status, txStatus), true);
			return;
		}

		if (itemCount <= 1 && unpaidCount == 0)
			return;

		string text = WidgetManager.Translate("#MRX-Arsenal_Sold", itemCount);
		if (unpaidCount > 0)
			text += WidgetManager.Translate("#MRX-Arsenal_NotBoughtBack", unpaidCount);

		m_MRX_BalanceBar.ShowInfo(text, false);
	}
}
