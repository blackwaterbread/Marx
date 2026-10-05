//! Shop window (API v0): balance, Buy and Sell tabs, a category filter and the items with a preview, name and price.
//! Client side; every request is validated again by the server.
class MRX_ShopMenu : MRX_ScriptedDialog
{
	protected static const float WINDOW_WIDTH = 1040;
	//! Room for the title, header, tabs, filter and footer; the list takes the rest of the screen height.
	protected static const float RESERVED_HEIGHT = 380;
	protected static const float MIN_LIST_HEIGHT = 200;
	protected static const float MAX_LIST_HEIGHT = 600;
	protected static const float PREVIEW_SIZE = 72;
	protected static const int NAME_FONT_SIZE = 22;
	protected static const int DETAIL_FONT_SIZE = 16;
	//! Waits for the inventory to change after a trade before listing it again.
	protected static const int REFRESH_DELAY_MS = 300;
	protected static const int COLOR_REFUSED = 0xFFE06060;
	protected static const int COLOR_DONE = 0xFF80D080;

	//! Weak: the shop entity may stream out while the dialog is open.
	protected IEntity m_ShopEntity;
	protected ref MRX_ShopDefinition m_Definition;

	protected TextWidget m_wBalance;
	protected TextWidget m_wStatus;
	protected SCR_ButtonTextComponent m_BuyTab;
	protected SCR_ButtonTextComponent m_SellTab;
	protected Widget m_wCategories;
	protected VerticalLayoutWidget m_wList;
	protected bool m_bSelling;
	protected int m_iRowCount;
	//! Category filter entries and their buttons; the first one shows all.
	protected ref array<string> m_aCategories = {};
	protected ref array<SCR_ButtonTextComponent> m_aCategoryButtons = {};
	protected int m_iCategory;
	//! Items offered for sale, by index in the "sell:<index>" actions.
	protected ref array<IEntity> m_aSellItems = {};
	//! Name of the item of the running request, for its result.
	protected string m_sPendingName;
	protected bool m_bPendingSell;

	//------------------------------------------------------------------------------------------------
	//! Opens the window for a shop. \return Null when the shop has no valid catalog.
	static MRX_ShopMenu Open(notnull MRX_ShopComponent shop)
	{
		MRX_ShopDefinition definition = shop.GetDefinition();
		if (!definition)
			return null;

		MRX_ShopMenu menu = new MRX_ShopMenu();
		menu.m_ShopEntity = shop.GetOwner();
		menu.m_Definition = definition;
		OpenDialog(menu, shop.GetDisplayName(), "MRX_Shop", DIALOG_LAYOUT_MEDIUM);
		return menu;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Item rows of the shown tab and category.
	int GetRowCount()
	{
		return m_iRowCount;
	}

	//------------------------------------------------------------------------------------------------
	//! Shows the Sell tab (true) or the Buy tab.
	void ShowTab(bool selling)
	{
		m_bSelling = selling && m_SellTab;
		m_BuyTab.SetToggled(!m_bSelling, false, false);
		if (m_SellTab)
			m_SellTab.SetToggled(m_bSelling, false, false);

		if (m_wCategories)
			m_wCategories.SetVisible(!m_bSelling);

		BuildList();
	}

	//------------------------------------------------------------------------------------------------
	//! Filters the Buy tab. \param index 0 = all categories, then the catalog's categories in order.
	void ShowCategory(int index)
	{
		if (index < 0 || index >= m_aCategories.Count())
			index = 0;

		m_iCategory = index;
		foreach (int i, SCR_ButtonTextComponent button : m_aCategoryButtons)
		{
			button.SetToggled(i == m_iCategory, false, false);
		}

		BuildList();
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnDialogOpened()
	{
		SetDialogWidth(WINDOW_WIDTH);

		// Balance on the left, the outcome of the last trade on the right.
		Widget header = CreateLayout(WidgetType.HorizontalLayoutWidgetTypeID, m_wHeader);
		m_wBalance = CreateText(header, string.Empty);
		LayoutSlot.SetSizeMode(m_wBalance, LayoutSizeMode.Fill);
		m_wStatus = CreateText(header, string.Empty);

		Widget tabs = CreateLayout(WidgetType.HorizontalLayoutWidgetTypeID, m_wHeader);
		AlignableSlot.SetPadding(tabs, 0, 8, 0, 4);
		m_BuyTab = CreateTab(tabs, "Buy");
		if (m_Definition.m_bAllowSell)
			m_SellTab = CreateTab(tabs, "Sell");

		CreateCategoryFilter();
		m_wList = CreateScrollList(m_wRows, Math.Clamp(GetScreenHeight() - RESERVED_HEIGHT, MIN_LIST_HEIGHT, MAX_LIST_HEIGHT));

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (controller)
		{
			controller.MRX_GetOnShopResult().Insert(OnShopResult);
			controller.MRX_GetWallet().GetOnBalanceChanged().Insert(OnBalanceChanged);
		}

		UpdateBalance();
		ShowTab(false);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		GetGame().GetCallqueue().Remove(BuildList);
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (controller)
		{
			controller.MRX_GetOnShopResult().Remove(OnShopResult);
			controller.MRX_GetWallet().GetOnBalanceChanged().Remove(OnBalanceChanged);
		}

		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_ButtonTextComponent CreateTab(notnull Widget parent, string text)
	{
		Widget root = GetGame().GetWorkspace().CreateWidgets(BUTTON_LAYOUT, parent);
		if (!root)
			return null;

		SCR_ButtonTextComponent tab = SCR_ButtonTextComponent.FindButtonTextComponent(root);
		if (!tab)
			return null;

		tab.SetText(text);
		tab.m_OnClicked.Insert(OnTabClicked);
		return tab;
	}

	//------------------------------------------------------------------------------------------------
	//! "All" and the catalog's categories, when it has more than one.
	protected void CreateCategoryFilter()
	{
		m_aCategories.Insert("All");
		foreach (MRX_ShopItem item : m_Definition.m_Catalog.m_aItems)
		{
			if (item.m_iPrice > 0 && !item.m_sCategory.IsEmpty() && !m_aCategories.Contains(item.m_sCategory))
				m_aCategories.Insert(item.m_sCategory);
		}

		if (m_aCategories.Count() < 3)
			return;

		m_wCategories = CreateLayout(WidgetType.HorizontalLayoutWidgetTypeID, m_wHeader);
		AlignableSlot.SetPadding(m_wCategories, 0, 0, 0, 8);
		foreach (string category : m_aCategories)
		{
			SCR_ButtonTextComponent button = CreateTab(m_wCategories, category);
			if (!button)
				continue;

			button.m_OnClicked.Remove(OnTabClicked);
			button.m_OnClicked.Insert(OnCategoryClicked);
			button.SetToggled(m_aCategoryButtons.IsEmpty(), false, false);
			m_aCategoryButtons.Insert(button);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildList()
	{
		if (!m_wList)
			return;

		ClearChildren(m_wList);
		ClearRowButtons();
		m_aSellItems.Clear();
		m_iRowCount = 0;
		if (m_bSelling)
			BuildSellList();
		else
			BuildBuyList();
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildBuyList()
	{
		string category;
		if (m_iCategory > 0 && m_iCategory < m_aCategories.Count())
			category = m_aCategories[m_iCategory];

		foreach (MRX_ShopItem item : m_Definition.m_Catalog.m_aItems)
		{
			if (item.m_iPrice <= 0 || (!category.IsEmpty() && item.m_sCategory != category))
				continue;

			bool affordable = CanAfford(item.m_sCurrency, item.m_iPrice);
			ItemPreviewWidget preview = AddItemRow(GetItemName(item), item.m_sCategory, FormatPrice(item.m_iPrice, item.m_sCurrency), affordable, "Buy", "buy:" + item.m_sId);
			ShowPrefabPreview(preview, item.m_sPrefab);
		}

		if (m_iRowCount == 0)
			CreateText(m_wList, "Nothing for sale here.");
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildSellList()
	{
		array<IEntity> carried = {};
		InventoryStorageManagerComponent manager = GetLocalStorageManager();
		if (manager)
			manager.GetItems(carried);

		foreach (IEntity entity : carried)
		{
			MRX_ShopItem sellable = m_Definition.m_Catalog.FindByPrefab(SCR_ResourceNameUtils.GetPrefabName(entity));
			if (!sellable)
				continue;

			int sellPrice = m_Definition.GetSellPrice(sellable);
			if (sellPrice <= 0)
				continue;

			string name = GetEntityDisplayName(entity);
			if (name.IsEmpty())
				name = GetItemName(sellable);

			int index = m_aSellItems.Insert(entity);
			ItemPreviewWidget preview = AddItemRow(name, sellable.m_sCategory, "+" + FormatPrice(sellPrice, sellable.m_sCurrency), true, "Sell", "sell:" + index.ToString());
			ShowItemPreview(preview, entity);
		}

		if (m_iRowCount == 0)
			CreateText(m_wList, "You carry nothing this shop buys.");
	}

	//------------------------------------------------------------------------------------------------
	//! Row with a preview slot, the name and a detail line, the price and a button. \return The preview widget.
	protected ItemPreviewWidget AddItemRow(string name, string detail, string price, bool enabled, string buttonText, string action)
	{
		Widget row = CreateLayout(WidgetType.HorizontalLayoutWidgetTypeID, m_wList);
		AlignableSlot.SetHorizontalAlign(row, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetPadding(row, 0, 4, 16, 4);
		ItemPreviewWidget preview = CreateItemPreview(row, PREVIEW_SIZE);

		Widget texts = CreateLayout(WidgetType.VerticalLayoutWidgetTypeID, row);
		LayoutSlot.SetSizeMode(texts, LayoutSizeMode.Fill);
		AlignableSlot.SetVerticalAlign(texts, LayoutVerticalAlign.Center);
		AlignableSlot.SetPadding(texts, 12, 0, 12, 0);
		TextWidget nameText = CreateText(texts, name);
		nameText.SetExactFontSize(NAME_FONT_SIZE);
		if (!detail.IsEmpty())
		{
			TextWidget detailText = CreateText(texts, detail);
			detailText.SetExactFontSize(DETAIL_FONT_SIZE);
			detailText.SetOpacity(0.6);
		}

		TextWidget priceText = CreateText(row, price);
		priceText.SetExactFontSize(NAME_FONT_SIZE);
		AlignableSlot.SetVerticalAlign(priceText, LayoutVerticalAlign.Center);
		AlignableSlot.SetPadding(priceText, 0, 0, 16, 0);
		if (!enabled)
			priceText.SetColor(Color.FromInt(COLOR_REFUSED));

		SCR_ButtonTextComponent button = AddButton(row, buttonText, action);
		if (button)
		{
			AlignableSlot.SetVerticalAlign(button.GetRootWidget(), LayoutVerticalAlign.Center);
			button.SetEnabled(enabled, false);
		}

		m_iRowCount++;
		return preview;
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnRowAction(string action)
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller || !m_ShopEntity)
			return;

		if (action.StartsWith("buy:"))
		{
			string itemId = action.Substring(4, action.Length() - 4);
			MRX_ShopItem item = m_Definition.m_Catalog.FindItem(itemId);
			if (item)
				m_sPendingName = GetItemName(item);

			m_bPendingSell = false;
			controller.MRX_RequestBuy(m_ShopEntity, itemId);
			ShowStatus("...", true);
			return;
		}

		int sellIndex = action.Substring(5, action.Length() - 5).ToInt();
		if (sellIndex < 0 || sellIndex >= m_aSellItems.Count() || !m_aSellItems[sellIndex])
			return;

		m_sPendingName = GetEntityDisplayName(m_aSellItems[sellIndex]);
		m_bPendingSell = true;
		controller.MRX_RequestSell(m_ShopEntity, m_aSellItems[sellIndex]);
		ShowStatus("...", true);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnTabClicked(SCR_ButtonBaseComponent tab)
	{
		ShowTab(tab == m_SellTab);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCategoryClicked(SCR_ButtonBaseComponent button)
	{
		int index = m_aCategoryButtons.Find(SCR_ButtonTextComponent.Cast(button));
		if (index >= 0)
			ShowCategory(index);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnShopResult(MRX_EShopStatus status, MRX_ETxStatus txStatus, string itemId, int price, string currency)
	{
		if (status == MRX_EShopStatus.OK)
		{
			if (m_bPendingSell)
				ShowStatus(string.Format("Sold %1 for %2", m_sPendingName, FormatPrice(price, currency)), true);
			else
				ShowStatus(string.Format("Bought %1 for %2", m_sPendingName, FormatPrice(price, currency)), true);
		}
		else
		{
			ShowStatus(GetFailureText(status, txStatus), false);
		}

		GetGame().GetCallqueue().Remove(BuildList);
		GetGame().GetCallqueue().CallLater(BuildList, REFRESH_DELAY_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBalanceChanged(string currency, int balance)
	{
		UpdateBalance();
		if (!m_bSelling)
			BuildList();
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowStatus(string text, bool success)
	{
		if (!m_wStatus)
			return;

		m_wStatus.SetText(text);
		if (success)
			m_wStatus.SetColor(Color.FromInt(COLOR_DONE));
		else
			m_wStatus.SetColor(Color.FromInt(COLOR_REFUSED));
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateBalance()
	{
		MRX_ClientWallet wallet = MRX_ClientWallet.GetLocal();
		if (!wallet || !m_wBalance)
			return;

		array<string> currencies = {};
		wallet.GetCurrencies(currencies);
		array<string> balances = {};
		foreach (string currency : currencies)
		{
			int balance;
			wallet.TryGetBalance(currency, balance);
			balances.Insert(FormatPrice(balance, currency));
		}

		m_wBalance.SetText("Balance: " + SCR_StringHelper.Join(", ", balances));
	}

	//------------------------------------------------------------------------------------------------
	protected static bool CanAfford(string currency, int price)
	{
		MRX_ClientWallet wallet = MRX_ClientWallet.GetLocal();
		int balance;
		return wallet && wallet.TryGetBalance(currency, balance) && balance >= price;
	}

	//------------------------------------------------------------------------------------------------
	protected static string FormatPrice(int amount, string currency)
	{
		return string.Format("%1 %2", amount, currency);
	}

	//------------------------------------------------------------------------------------------------
	protected static string GetFailureText(MRX_EShopStatus status, MRX_ETxStatus txStatus)
	{
		switch (status)
		{
			case MRX_EShopStatus.PAYMENT_FAILED:
			{
				if (txStatus == MRX_ETxStatus.INSUFFICIENT_FUNDS)
					return "Not enough money";

				return "Payment failed";
			}

			case MRX_EShopStatus.NO_SPACE: return "No room in your inventory";
			case MRX_EShopStatus.TOO_FAR: return "Too far from the shop";
			case MRX_EShopStatus.NOT_EMPTY: return "Empty it first (attachments, magazines, contents)";
			case MRX_EShopStatus.BUSY: return "Please wait";
			case MRX_EShopStatus.NOT_BUYABLE: return "The shop does not buy this";
			case MRX_EShopStatus.DELIVERY_FAILED: return "Could not hand the item over; refunded";
		}

		return "Failed: " + typename.EnumToString(MRX_EShopStatus, status);
	}

	//------------------------------------------------------------------------------------------------
	protected static string GetItemName(notnull MRX_ShopItem item)
	{
		if (!item.m_sName.IsEmpty())
			return WidgetManager.Translate(item.m_sName);

		return GetItemDisplayName(item.m_sPrefab);
	}
}
