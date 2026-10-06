//! Shop window (API v0): the balance, Buy and Sell tabs, category filters and the items as cards with a preview, name,
//! details and price. Products (MRX_ShopProduct) also show their state for the player, e.g. "4 / 8 pages", which the
//! server sends on request. Client side; every request is validated again by the server.
class MRX_ShopMenu : MRX_ScriptedDialog
{
	protected static const float WINDOW_WIDTH = 1100;
	//! Room for the title, header, tabs, filter and footer; the list takes the rest of the screen height.
	protected static const float RESERVED_HEIGHT = 430;
	protected static const float MIN_LIST_HEIGHT = 200;
	protected static const float MAX_LIST_HEIGHT = 620;
	protected static const float PREVIEW_SIZE = 72;
	protected static const int TITLE_FONT_SIZE = 40;
	protected static const int NAME_FONT_SIZE = 20;
	protected static const int DETAIL_FONT_SIZE = 15;
	protected static const int PRICE_FONT_SIZE = 22;
	protected static const int BALANCE_FONT_SIZE = 28;
	protected static const int LABEL_FONT_SIZE = 14;
	protected static const float ROW_BUTTON_WIDTH = 96;
	//! Waits for the inventory to change after a trade before listing it again.
	protected static const int REFRESH_DELAY_MS = 300;
	//! Items per page of the Buy tab. Large catalogs would otherwise create hundreds of rows and item previews at once.
	static const int PAGE_SIZE = 20;
	protected static const int CATEGORIES_PER_ROW = 7;

	//! Weak: the shop entity may stream out while the dialog is open.
	protected IEntity m_ShopEntity;
	protected ref MRX_ShopDefinition m_Definition;

	protected TextWidget m_wBalance;
	protected TextWidget m_wStatus;
	protected ref MRX_FlatButton m_BuyTab;
	protected ref MRX_FlatButton m_SellTab;
	protected Widget m_wCategories;
	protected VerticalLayoutWidget m_wList;
	protected Widget m_wPager;
	protected bool m_bSelling;
	protected int m_iRowCount;
	protected int m_iPage;
	protected int m_iPageCount = 1;
	//! Category filter entries and their buttons; the first one shows all.
	protected ref array<string> m_aCategories = {};
	protected ref array<ref MRX_FlatButton> m_aCategoryButtons = {};
	protected int m_iCategory;
	//! Buttons of the current list and pager.
	protected ref array<ref MRX_FlatButton> m_aListButtons = {};
	//! Items offered for sale, by index in the "sell:<index>" actions.
	protected ref array<IEntity> m_aSellItems = {};
	//! Product states from the server, by item ID.
	protected ref map<string, bool> m_mAvailable = new map<string, bool>();
	protected ref map<string, string> m_mStateTexts = new map<string, string>();
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
	//! \return Item rows of the shown tab, category and page.
	int GetRowCount()
	{
		return m_iRowCount;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Pages of the shown tab and category (at least 1).
	int GetPageCount()
	{
		return m_iPageCount;
	}

	//------------------------------------------------------------------------------------------------
	//! Shows a page of the Buy tab, 0-based; clamped to the existing pages.
	void ShowPage(int page)
	{
		m_iPage = page;
		BuildList();

		ScrollLayoutWidget scroll;
		if (m_wList)
			scroll = ScrollLayoutWidget.Cast(m_wList.GetParent());

		if (scroll)
			scroll.SetSliderPos(0, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Shows the Sell tab (true) or the Buy tab.
	void ShowTab(bool selling)
	{
		m_iPage = 0;
		m_bSelling = selling && m_SellTab;
		m_BuyTab.SetSelected(!m_bSelling);
		if (m_SellTab)
			m_SellTab.SetSelected(m_bSelling);

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

		m_iPage = 0;
		m_iCategory = index;
		foreach (int i, MRX_FlatButton button : m_aCategoryButtons)
		{
			button.SetSelected(i == m_iCategory);
		}

		BuildList();
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnDialogOpened()
	{
		SetDialogWidth(WINDOW_WIDTH);
		StyleTitle();
		BuildHeader();

		// Tabs on the left, the page buttons of the Buy tab on the right.
		Widget tabs = CreateLayout(WidgetType.HorizontalLayoutWidgetTypeID, m_wHeader);
		AlignableSlot.SetHorizontalAlign(tabs, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetPadding(tabs, 0, 10, 0, 6);
		m_BuyTab = CreateTab(tabs, "BUY");
		if (m_Definition.m_bAllowSell)
		{
			m_SellTab = CreateTab(tabs, "SELL");
			AlignableSlot.SetPadding(m_SellTab.GetRootWidget(), 6, 0, 0, 0);
		}

		Widget spacer = CreateLayout(WidgetType.HorizontalLayoutWidgetTypeID, tabs);
		LayoutSlot.SetSizeMode(spacer, LayoutSizeMode.Fill);
		m_wPager = MRX_UIStyle.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, Color.FromInt(Color.WHITE), tabs, MRX_UIStyle.CONTAINER);

		CreateCategoryFilter();
		m_wList = CreateScrollList(m_wRows, Math.Clamp(GetScreenHeight() - RESERVED_HEIGHT, MIN_LIST_HEIGHT, MAX_LIST_HEIGHT));

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (controller)
		{
			controller.MRX_GetOnShopResult().Insert(OnShopResult);
			controller.MRX_GetOnShopStates().Insert(OnShopStates);
			controller.MRX_GetWallet().GetOnBalanceChanged().Insert(OnBalanceChanged);
		}

		UpdateBalance();
		ShowTab(false);
		RequestStates();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		GetGame().GetCallqueue().Remove(BuildList);
		GetGame().GetCallqueue().Remove(ShowPage);
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (controller)
		{
			controller.MRX_GetOnShopResult().Remove(OnShopResult);
			controller.MRX_GetOnShopStates().Remove(OnShopStates);
			controller.MRX_GetWallet().GetOnBalanceChanged().Remove(OnBalanceChanged);
		}

		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------------------------
	//! A large bold title with an accent line under it. The vanilla title (rich text fitted to a narrow box) keeps its
	//! own font, so it is replaced by a text of the window's own.
	protected void StyleTitle()
	{
		Widget header = GetRootWidget().FindAnyWidget("Header");
		Widget titleSize = GetRootWidget().FindAnyWidget("TitleSize");
		if (m_wTitle && header && titleSize)
		{
			titleSize.SetVisible(false);
			TextWidget title = MRX_UIStyle.CreateText(header, m_wTitle.GetText(), TITLE_FONT_SIZE, Color.FromInt(Color.WHITE), true);
			AlignableSlot.SetVerticalAlign(title, LayoutVerticalAlign.Center);
			AlignableSlot.SetPadding(title, 8, 0, 0, 0);
		}

		if (m_wImgTopLine)
			m_wImgTopLine.SetColor(MRX_UIStyle.GetAccentColor());
	}

	//------------------------------------------------------------------------------------------------
	//! Balance box (like the inventory's balance panel) with the outcome of the last trade on its right.
	protected void BuildHeader()
	{
		Widget box = MRX_UIStyle.CreateBox(m_wHeader, MRX_UIStyle.GetFillColor(), true);
		AlignableSlot.SetHorizontalAlign(box, LayoutHorizontalAlign.Stretch);

		Widget row = MRX_UIStyle.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, Color.FromInt(Color.WHITE), box);
		AlignableSlot.SetHorizontalAlign(row, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetPadding(row, 16 + MRX_UIStyle.ACCENT_WIDTH, 8, 16, 10);

		Widget column = MRX_UIStyle.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, Color.FromInt(Color.WHITE), row);
		LayoutSlot.SetSizeMode(column, LayoutSizeMode.Fill);
		MRX_UIStyle.CreateText(column, "BALANCE", LABEL_FONT_SIZE, MRX_UIStyle.GetAccentColor(), true);
		m_wBalance = MRX_UIStyle.CreateText(column, string.Empty, BALANCE_FONT_SIZE, Color.FromInt(Color.WHITE), true);

		m_wStatus = MRX_UIStyle.CreateText(row, string.Empty, DETAIL_FONT_SIZE + 1, MRX_UIStyle.GetMutedColor(), true);
		LayoutSlot.SetVerticalAlign(m_wStatus, LayoutVerticalAlign.Center);
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_FlatButton CreateTab(notnull Widget parent, string text)
	{
		MRX_FlatButton tab = MRX_FlatButton.Create(parent, text, 110, 34, 16);
		tab.GetOnClicked().Insert(OnTabClicked);
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

		// Rows of a few buttons each, so long category lists stay inside the window.
		m_wCategories = MRX_UIStyle.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, Color.FromInt(Color.WHITE), m_wHeader, MRX_UIStyle.CONTAINER);
		AlignableSlot.SetPadding(m_wCategories, 0, 0, 0, 8);
		Widget categoryRow;
		foreach (int i, string category : m_aCategories)
		{
			if (i % CATEGORIES_PER_ROW == 0)
			{
				categoryRow = MRX_UIStyle.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, Color.FromInt(Color.WHITE), m_wCategories, MRX_UIStyle.CONTAINER);
				AlignableSlot.SetPadding(categoryRow, 0, 4, 0, 0);
			}

			MRX_FlatButton button = MRX_FlatButton.Create(categoryRow, category, 0, 28, 14);
			AlignableSlot.SetPadding(button.GetRootWidget(), 0, 0, 6, 0);
			button.GetOnClicked().Insert(OnCategoryClicked);
			button.SetSelected(m_aCategoryButtons.IsEmpty());
			m_aCategoryButtons.Insert(button);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildList()
	{
		if (!m_wList)
			return;

		ClearChildren(m_wList);
		if (m_wPager)
			ClearChildren(m_wPager);

		ClearRowButtons();
		m_aListButtons.Clear();
		m_aSellItems.Clear();
		m_iRowCount = 0;
		m_iPageCount = 1;
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

		array<MRX_ShopItem> items = {};
		foreach (MRX_ShopItem item : m_Definition.m_Catalog.m_aItems)
		{
			if (item.m_iPrice > 0 && (category.IsEmpty() || item.m_sCategory == category))
				items.Insert(item);
		}

		m_iPageCount = Math.Max(1, (items.Count() + PAGE_SIZE - 1) / PAGE_SIZE);
		m_iPage = Math.ClampInt(m_iPage, 0, m_iPageCount - 1);
		int end = Math.Min(items.Count(), (m_iPage + 1) * PAGE_SIZE);
		for (int i = m_iPage * PAGE_SIZE; i < end; i++)
		{
			MRX_ShopItem shown = items[i];
			bool affordable = CanAfford(shown.m_sCurrency, shown.m_iPrice);
			// Items without a state from the server (all but products) are always available.
			bool available = !m_mAvailable.Contains(shown.m_sId) || m_mAvailable.Get(shown.m_sId);
			string state = m_mStateTexts.Get(shown.m_sId);
			string detail = shown.m_sCategory;
			if (!shown.m_sDescription.IsEmpty())
				detail = WidgetManager.Translate(shown.m_sDescription);

			string buttonText = "BUY";
			if (!available)
				buttonText = "MAX";

			ItemPreviewWidget preview = AddItemRow(GetItemName(shown), detail, state, FormatPrice(shown.m_iPrice, shown.m_sCurrency), GetPriceColor(affordable), affordable && available, buttonText, "buy:" + shown.m_sId);
			ShowPrefabPreview(preview, shown.m_sPrefab);
		}

		if (m_iRowCount == 0)
			MRX_UIStyle.CreateText(m_wList, "Nothing for sale here.", NAME_FONT_SIZE, MRX_UIStyle.GetMutedColor());

		BuildPager();
	}

	//------------------------------------------------------------------------------------------------
	//! Previous / page number / next, when there is more than one page.
	protected void BuildPager()
	{
		if (!m_wPager || m_iPageCount <= 1)
			return;

		MRX_FlatButton previous = MRX_FlatButton.Create(m_wPager, "<", 40, 34, 16);
		previous.m_sAction = "page:previous";
		previous.GetOnClicked().Insert(OnListButton);
		previous.SetEnabled(m_iPage > 0);
		m_aListButtons.Insert(previous);

		TextWidget pageText = MRX_UIStyle.CreateText(m_wPager, string.Format("PAGE %1 / %2", m_iPage + 1, m_iPageCount), LABEL_FONT_SIZE + 1, MRX_UIStyle.GetMutedColor(), true);
		AlignableSlot.SetVerticalAlign(pageText, LayoutVerticalAlign.Center);
		AlignableSlot.SetPadding(pageText, 14, 0, 14, 0);

		MRX_FlatButton next = MRX_FlatButton.Create(m_wPager, ">", 40, 34, 16);
		next.m_sAction = "page:next";
		next.GetOnClicked().Insert(OnListButton);
		next.SetEnabled(m_iPage < m_iPageCount - 1);
		m_aListButtons.Insert(next);
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
			ItemPreviewWidget preview = AddItemRow(name, sellable.m_sCategory, string.Empty, "+" + FormatPrice(sellPrice, sellable.m_sCurrency), MRX_UIStyle.GetIncreaseColor(), true, "SELL", "sell:" + index.ToString());
			ShowItemPreview(preview, entity);
		}

		if (m_iRowCount == 0)
			MRX_UIStyle.CreateText(m_wList, "You carry nothing this shop buys.", NAME_FONT_SIZE, MRX_UIStyle.GetMutedColor());
	}

	//------------------------------------------------------------------------------------------------
	//! Card with a preview slot, the name, a detail line and a state, the price and a button. \return The preview widget.
	protected ItemPreviewWidget AddItemRow(string name, string detail, string state, string price, Color priceColor, bool enabled, string buttonText, string action)
	{
		Color fill = MRX_UIStyle.GetRowColor();
		if (m_iRowCount % 2 == 1)
			fill = MRX_UIStyle.GetFillColor();

		Widget card = MRX_UIStyle.CreateBox(m_wList, fill, !state.IsEmpty());
		AlignableSlot.SetHorizontalAlign(card, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetPadding(card, 0, 0, 12, 4);

		Widget row = MRX_UIStyle.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, Color.FromInt(Color.WHITE), card, MRX_UIStyle.CONTAINER);
		AlignableSlot.SetHorizontalAlign(row, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetPadding(row, 8 + MRX_UIStyle.ACCENT_WIDTH, 6, 12, 6);
		ItemPreviewWidget preview = CreateItemPreview(row, PREVIEW_SIZE);

		Widget texts = MRX_UIStyle.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, Color.FromInt(Color.WHITE), row);
		LayoutSlot.SetSizeMode(texts, LayoutSizeMode.Fill);
		AlignableSlot.SetVerticalAlign(texts, LayoutVerticalAlign.Center);
		AlignableSlot.SetPadding(texts, 14, 0, 12, 0);
		MRX_UIStyle.CreateText(texts, name, NAME_FONT_SIZE, Color.FromInt(Color.WHITE), true);
		if (!detail.IsEmpty())
			MRX_UIStyle.CreateText(texts, detail, DETAIL_FONT_SIZE, MRX_UIStyle.GetMutedColor());

		if (!state.IsEmpty())
		{
			TextWidget stateText = MRX_UIStyle.CreateText(texts, state, DETAIL_FONT_SIZE, MRX_UIStyle.GetAccentColor(), true);
			AlignableSlot.SetPadding(stateText, 0, 2, 0, 0);
		}

		TextWidget priceText = MRX_UIStyle.CreateText(row, price, PRICE_FONT_SIZE, priceColor, true);
		AlignableSlot.SetVerticalAlign(priceText, LayoutVerticalAlign.Center);
		AlignableSlot.SetPadding(priceText, 0, 0, 16, 0);

		MRX_FlatButton button = MRX_FlatButton.Create(row, buttonText, ROW_BUTTON_WIDTH, 36, 16);
		AlignableSlot.SetVerticalAlign(button.GetRootWidget(), LayoutVerticalAlign.Center);
		button.m_sAction = action;
		button.SetEnabled(enabled);
		button.GetOnClicked().Insert(OnListButton);
		m_aListButtons.Insert(button);

		m_iRowCount++;
		return preview;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnListButton(MRX_FlatButton button)
	{
		OnRowAction(button.m_sAction);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnRowAction(string action)
	{
		if (action == "page:previous" || action == "page:next")
		{
			int step = 1;
			if (action == "page:previous")
				step = -1;

			// Later: the pressed button is removed with the list it belongs to.
			GetGame().GetCallqueue().CallLater(ShowPage, 0, false, m_iPage + step);
			return;
		}

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
	protected void OnTabClicked(MRX_FlatButton tab)
	{
		ShowTab(tab == m_SellTab);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCategoryClicked(MRX_FlatButton button)
	{
		int index = m_aCategoryButtons.Find(button);
		if (index >= 0)
			ShowCategory(index);
	}

	//------------------------------------------------------------------------------------------------
	//! Asks the server for the products' states, when the catalog has products.
	protected void RequestStates()
	{
		bool hasProducts;
		foreach (MRX_ShopItem item : m_Definition.m_Catalog.m_aItems)
		{
			if (item.m_Product)
				hasProducts = true;
		}

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (hasProducts && controller && m_ShopEntity)
			controller.MRX_RequestShopStates(m_ShopEntity);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnShopStates(IEntity shopEntity, array<string> itemIds, array<int> available, array<string> texts)
	{
		if (shopEntity != m_ShopEntity)
			return;

		foreach (int i, string itemId : itemIds)
		{
			m_mAvailable.Set(itemId, available[i] != 0);
			m_mStateTexts.Set(itemId, texts[i]);
		}

		if (!m_bSelling)
			BuildList();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnShopResult(MRX_EShopStatus status, MRX_ETxStatus txStatus, string itemId, int price, string currency, int itemCount, int unpaidCount)
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

		RequestStates();
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
			m_wStatus.SetColor(MRX_UIStyle.GetIncreaseColor());
		else
			m_wStatus.SetColor(MRX_UIStyle.GetDecreaseColor());
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

		m_wBalance.SetText(SCR_StringHelper.Join("    ", balances));
	}

	//------------------------------------------------------------------------------------------------
	protected static Color GetPriceColor(bool affordable)
	{
		if (affordable)
			return MRX_UIStyle.GetAccentColor();

		return MRX_UIStyle.GetDecreaseColor();
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
		return MRX_TextFormat.Money(amount, currency);
	}

	//------------------------------------------------------------------------------------------------
	//! Short text for a failed shop request (API v0).
	static string GetFailureText(MRX_EShopStatus status, MRX_ETxStatus txStatus)
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
			case MRX_EShopStatus.ISSUED: return "Issued gear is not bought back";
			case MRX_EShopStatus.LIMIT_REACHED: return "Already at the maximum";
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
