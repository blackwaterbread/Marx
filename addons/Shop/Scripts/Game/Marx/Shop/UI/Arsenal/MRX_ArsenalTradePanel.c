//! Panel of an arsenal trade (MRX_ArsenalTradeClient) in the middle of the inventory, where the character is shown
//! otherwise: the tabs (buy, sell), the vanilla storage panel of the cart or the sale box, the space below it, the
//! totals and the hold button of the checkout, as wide as the cart. The space below shows a storage of an item of the
//! trade (ToggleDetail). Mouse for the tabs and the button; the storage panels work like any other. Internal.
class MRX_ArsenalTradePanel : Managed
{
	protected static const ResourceName STORAGE_LAYOUT = "{E61475CBB560757F}UI/layouts/Menus/Inventory/InventoryContainerGrid.layout";
	//! Width of a cell of the vanilla storage panels.
	protected static const int CELL_SIZE = 66;
	//! Height of a vanilla storage panel without its cells: the header and the page buttons.
	protected static const int STORAGE_CHROME_HEIGHT = 105;
	protected static const int SIDE_PADDING = 10;
	//! The arsenal panel on the left ends in an 8 px margin of its own (vanilla InventoryContainerGrid.layout): the panel
	//! is centered in the rest, so the gaps on both sides look the same.
	protected static const int LEFT_MARGIN_OFFSET = 8;
	protected static const int TEXT_FONT_SIZE = 15;
	protected static const int TOTAL_FONT_SIZE = 16;
	protected static const int BALANCE_FONT_SIZE = 20;
	protected static const int STATUS_MS = 4000;

	//! Weak: the menu owns this panel.
	protected SCR_InventoryMenuUI m_Menu;
	//! Weak: the controller owns the trade.
	protected MRX_ArsenalTradeClient m_Trade;
	protected MRX_ArsenalShopComponent m_Arsenal;
	protected Widget m_wRoot;
	protected Widget m_wCartWidget;
	protected Widget m_wSaleWidget;
	protected ref MRX_ArsenalTradeStorageUI m_CartUI;
	protected ref MRX_ArsenalTradeStorageUI m_SaleUI;
	protected TextWidget m_wHint;
	protected Widget m_wFreeArea;
	protected Widget m_wDetailWidget;
	protected ref MRX_ArsenalTradeDetailStorageUI m_DetailUI;
	protected bool m_bShownSaleTab;
	protected Widget m_wTotals;
	protected TextWidget m_wStatus;
	protected ref MRX_FlatButton m_BuyTab;
	protected ref MRX_FlatButton m_SellTab;
	protected ref MRX_HoldButton m_CheckoutButton;
	//! Weak: the container's manager, whose item events refresh the panels.
	protected ScriptedInventoryStorageManagerComponent m_ContainerManager;
	//! Weak: the controller owns the wallet, whose changes update the balance after the cart.
	protected MRX_ClientWallet m_Wallet;
	protected string m_sStatus;
	protected bool m_bStatusError;

	//------------------------------------------------------------------------------------------------
	//! Adds the panel to an overlay (the menu's center area), filling it. \return Null when the trade is not ready.
	static MRX_ArsenalTradePanel Create(notnull Widget parent, notnull SCR_InventoryMenuUI menu, notnull MRX_ArsenalTradeClient trade, notnull MRX_ArsenalShopComponent arsenal)
	{
		if (!trade.IsReady())
			return null;

		MRX_ArsenalTradePanel panel = new MRX_ArsenalTradePanel();
		panel.m_Menu = menu;
		panel.m_Trade = trade;
		panel.m_Arsenal = arsenal;
		panel.Build(parent);
		trade.GetOnChanged().Insert(panel.Update);
		trade.GetOnResult().Insert(panel.OnResult);
		trade.GetOnNoAnswer().Insert(panel.OnNoAnswer);
		trade.GetOnFailure().Insert(panel.OnFailure);
		MRX_ArsenalTradeStorageComponent.GetOnItemMoved().Insert(panel.OnItemMoved);
		panel.m_ContainerManager = ScriptedInventoryStorageManagerComponent.Cast(trade.GetContainer().FindComponent(ScriptedInventoryStorageManagerComponent));
		if (panel.m_ContainerManager)
		{
			panel.m_ContainerManager.m_OnItemAddedInvoker.Insert(panel.OnContainerChanged);
			panel.m_ContainerManager.m_OnItemRemovedInvoker.Insert(panel.OnContainerChanged);
		}

		panel.m_Wallet = MRX_ClientWallet.GetLocal();
		if (panel.m_Wallet)
			panel.m_Wallet.GetOnBalanceChanged().Insert(panel.OnBalanceChanged);

		panel.Update();
		return panel;
	}

	//------------------------------------------------------------------------------------------------
	Widget GetRootWidget()
	{
		return m_wRoot;
	}

	//------------------------------------------------------------------------------------------------
	void SetVisible(bool visible)
	{
		if (m_wRoot)
			m_wRoot.SetVisible(visible);
	}

	//------------------------------------------------------------------------------------------------
	bool IsVisible()
	{
		return m_wRoot && m_wRoot.IsVisible();
	}

	//------------------------------------------------------------------------------------------------
	//! The space between the storage panel and the totals.
	Widget GetFreeArea()
	{
		return m_wFreeArea;
	}

	//------------------------------------------------------------------------------------------------
	//! Shows a storage of an item of the trade (e.g. a backpack in the cart, a weapon's attachments) in the space below
	//! the storage panel, or closes it when it is shown there. Paged to the rows that fit. \return True when shown.
	bool ToggleDetail(notnull BaseInventoryStorageComponent storage)
	{
		if (m_DetailUI && m_DetailUI.GetStorage() == storage)
		{
			CloseDetail();
			return false;
		}

		CloseDetail();
		float width, height;
		m_wFreeArea.GetScreenSize(width, height);
		float available = GetGame().GetWorkspace().DPIUnscale(height) - STORAGE_CHROME_HEIGHT;
		int rows = 1;
		while ((rows + 1) * CELL_SIZE <= available)
		{
			rows++;
		}

		m_wDetailWidget = GetGame().GetWorkspace().CreateWidgets(STORAGE_LAYOUT, m_wFreeArea);
		if (!m_wDetailWidget)
			return false;

		AlignableSlot.SetHorizontalAlign(m_wDetailWidget, LayoutHorizontalAlign.Center);
		AlignableSlot.SetVerticalAlign(m_wDetailWidget, LayoutVerticalAlign.Top);
		m_DetailUI = new MRX_ArsenalTradeDetailStorageUI(storage, null, m_Menu, 0, {storage}, m_Arsenal.GetCartColumns(), rows);
		m_wDetailWidget.AddHandler(m_DetailUI);
		m_DetailUI.ActivateCloseStorageButton();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void CloseDetail()
	{
		if (m_wDetailWidget)
		{
			if (m_DetailUI)
				m_wDetailWidget.RemoveHandler(m_DetailUI);

			m_wDetailWidget.RemoveFromHierarchy();
		}

		m_wDetailWidget = null;
		m_DetailUI = null;
	}

	//------------------------------------------------------------------------------------------------
	//! The storage panel in the space below, or null.
	MRX_ArsenalTradeDetailStorageUI GetDetailStorageUI()
	{
		return m_DetailUI;
	}

	//------------------------------------------------------------------------------------------------
	//! Storage panel of the cart (sale false) or of the sale box.
	MRX_ArsenalTradeStorageUI GetStorageUI(bool sale)
	{
		if (sale)
			return m_SaleUI;

		return m_CartUI;
	}

	//------------------------------------------------------------------------------------------------
	//! The storage panel of the shown tab.
	MRX_ArsenalTradeStorageUI GetShownStorageUI()
	{
		return GetStorageUI(m_Trade && m_Trade.IsSaleTab());
	}

	//------------------------------------------------------------------------------------------------
	//! Stops the timers and the event handlers and removes the widgets.
	void Remove()
	{
		CloseDetail();
		GetGame().GetCallqueue().Remove(RefreshStorages);
		GetGame().GetCallqueue().Remove(ClearStatus);
		MRX_ArsenalTradeStorageComponent.GetOnItemMoved().Remove(OnItemMoved);
		if (m_ContainerManager)
		{
			m_ContainerManager.m_OnItemAddedInvoker.Remove(OnContainerChanged);
			m_ContainerManager.m_OnItemRemovedInvoker.Remove(OnContainerChanged);
		}

		if (m_Wallet)
			m_Wallet.GetOnBalanceChanged().Remove(OnBalanceChanged);

		if (m_Trade)
		{
			m_Trade.GetOnChanged().Remove(Update);
			m_Trade.GetOnResult().Remove(OnResult);
			m_Trade.GetOnNoAnswer().Remove(OnNoAnswer);
			m_Trade.GetOnFailure().Remove(OnFailure);
		}

		if (m_CheckoutButton)
			m_CheckoutButton.Stop();

		if (m_wRoot)
			m_wRoot.RemoveFromHierarchy();
	}

	//------------------------------------------------------------------------------------------------
	protected void Build(notnull Widget parent)
	{
		SizeLayoutWidget size = SizeLayoutWidget.Cast(MRX_UIStyle.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, Color.FromInt(Color.WHITE), parent, MRX_UIStyle.CONTAINER));
		AlignableSlot.SetHorizontalAlign(size, LayoutHorizontalAlign.Center);
		AlignableSlot.SetVerticalAlign(size, LayoutVerticalAlign.Stretch);
		AlignableSlot.SetPadding(size, 0, 0, LEFT_MARGIN_OFFSET, 0);
		size.EnableWidthOverride(true);
		size.SetWidthOverride(m_Arsenal.GetCartColumns() * CELL_SIZE + 2 * SIDE_PADDING + 2);
		m_wRoot = size;

		Widget box = MRX_UIStyle.CreateBox(size, MRX_UIStyle.GetFillColor());
		MRX_UIStyle.Stretch(box);

		Widget column = MRX_UIStyle.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, Color.FromInt(Color.WHITE), box, MRX_UIStyle.CONTAINER);
		MRX_UIStyle.Stretch(column);
		AlignableSlot.SetPadding(column, SIDE_PADDING, 12, SIDE_PADDING, 14);

		Widget tabs = MRX_UIStyle.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, Color.FromInt(Color.WHITE), column, MRX_UIStyle.CONTAINER);
		AlignableSlot.SetHorizontalAlign(tabs, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetPadding(tabs, 0, 0, 0, 10);
		m_BuyTab = MRX_FlatButton.Create(tabs, "#MRX-Cart_TabBuy", 120);
		m_BuyTab.GetOnClicked().Insert(OnTabClicked);
		m_SellTab = MRX_FlatButton.Create(tabs, "#MRX-Cart_TabSell", 120);
		AlignableSlot.SetPadding(m_SellTab.GetRootWidget(), 6, 0, 0, 0);
		m_SellTab.GetOnClicked().Insert(OnTabClicked);
		MRX_ShopDefinition shop = m_Arsenal.GetShop();
		m_SellTab.SetEnabled(shop && shop.m_bAllowSell);

		// Both storage panels stay; the tab shows one of them.
		Widget host = MRX_UIStyle.CreateWidget(WidgetType.OverlayWidgetTypeID, Color.FromInt(Color.WHITE), column, MRX_UIStyle.CONTAINER);
		AlignableSlot.SetHorizontalAlign(host, LayoutHorizontalAlign.Center);
		m_CartUI = CreateStorage(host, m_Trade.GetCartStorage(), m_wCartWidget);
		m_SaleUI = CreateStorage(host, m_Trade.GetSaleStorage(), m_wSaleWidget);

		m_wHint = MRX_UIStyle.CreateText(column, string.Empty, TEXT_FONT_SIZE, MRX_UIStyle.GetMutedColor());
		m_wHint.SetTextWrapping(true);
		AlignableSlot.SetHorizontalAlign(m_wHint, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetPadding(m_wHint, 2, 8, 2, 0);

		// Pushes the totals and the button to the bottom.
		m_wFreeArea = MRX_UIStyle.CreateWidget(WidgetType.OverlayWidgetTypeID, Color.FromInt(Color.WHITE), column, MRX_UIStyle.CONTAINER);
		AlignableSlot.SetHorizontalAlign(m_wFreeArea, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetPadding(m_wFreeArea, 0, 8, 0, 0);
		LayoutSlot.SetSizeMode(m_wFreeArea, LayoutSizeMode.Fill);

		Widget line = MRX_UIStyle.CreateWidget(WidgetType.ImageWidgetTypeID, MRX_UIStyle.GetFrameColor(), column);
		AlignableSlot.SetHorizontalAlign(line, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetPadding(line, 0, 8, 0, 8);
		ImageWidget.Cast(line).SetSize(1, 1);

		m_wTotals = MRX_UIStyle.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, Color.FromInt(Color.WHITE), column);
		AlignableSlot.SetHorizontalAlign(m_wTotals, LayoutHorizontalAlign.Stretch);

		m_wStatus = MRX_UIStyle.CreateText(column, string.Empty, TEXT_FONT_SIZE, MRX_UIStyle.GetMutedColor(), true);
		m_wStatus.SetTextWrapping(true);
		AlignableSlot.SetHorizontalAlign(m_wStatus, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetPadding(m_wStatus, 0, 6, 0, 0);

		m_CheckoutButton = MRX_HoldButton.Create(column, "#MRX-Cart_HoldBuy");
		AlignableSlot.SetHorizontalAlign(m_CheckoutButton.GetRootWidget(), LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetPadding(m_CheckoutButton.GetRootWidget(), 0, 8, 0, 0);
		m_CheckoutButton.GetOnHeld().Insert(OnCheckoutHeld);
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_ArsenalTradeStorageUI CreateStorage(notnull Widget host, MRX_ArsenalTradeStorageComponent storage, out Widget widget)
	{
		if (!storage)
			return null;

		widget = GetGame().GetWorkspace().CreateWidgets(STORAGE_LAYOUT, host);
		if (!widget)
			return null;

		AlignableSlot.SetHorizontalAlign(widget, LayoutHorizontalAlign.Center);
		AlignableSlot.SetVerticalAlign(widget, LayoutVerticalAlign.Top);
		BaseInventoryStorageComponent baseStorage = storage;
		MRX_ArsenalTradeStorageUI handler = new MRX_ArsenalTradeStorageUI(baseStorage, null, m_Menu, 0, {baseStorage}, m_Arsenal.GetCartColumns(), m_Arsenal.GetCartRows());
		widget.AddHandler(handler);
		return handler;
	}

	//------------------------------------------------------------------------------------------------
	protected void Update()
	{
		if (!m_Trade || !m_wRoot)
			return;

		bool sale = m_Trade.IsSaleTab();
		// An item storage of the other tab goes with it.
		if (sale != m_bShownSaleTab)
		{
			m_bShownSaleTab = sale;
			CloseDetail();
		}

		m_BuyTab.SetSelected(!sale);
		m_SellTab.SetSelected(sale);
		if (m_wCartWidget)
			m_wCartWidget.SetVisible(!sale);

		if (m_wSaleWidget)
			m_wSaleWidget.SetVisible(sale);

		if (m_SaleUI && sale)
			m_SaleUI.MRX_UpdatePrices();

		MRX_ArsenalTradeStorageComponent storage;
		if (sale)
			storage = m_Trade.GetSaleStorage();
		else
			storage = m_Trade.GetCartStorage();

		array<IEntity> items = {};
		bool empty = !storage || storage.GetRootItems(items) == 0;
		if (!empty)
			m_wHint.SetText(string.Empty);
		else if (sale)
			m_wHint.SetText("#MRX-Cart_EmptySell");
		else
			m_wHint.SetText("#MRX-Cart_EmptyBuy");

		m_wHint.SetVisible(empty);
		bool covered = UpdateTotals(sale);

		bool busy = m_Trade.IsCheckingOut();
		if (busy)
			m_CheckoutButton.SetLabel("#MRX-Cart_Paying");
		else if (sale)
			m_CheckoutButton.SetLabel("#MRX-Cart_HoldSell");
		else
			m_CheckoutButton.SetLabel("#MRX-Cart_HoldBuy");

		m_CheckoutButton.SetEnabled(!busy && !empty && covered);
		m_wStatus.SetText(m_sStatus);
		if (m_bStatusError)
			m_wStatus.SetColor(MRX_UIStyle.GetDecreaseColor());
		else
			m_wStatus.SetColor(MRX_UIStyle.GetMutedColor());

		m_wStatus.SetVisible(!m_sStatus.IsEmpty());
	}

	//------------------------------------------------------------------------------------------------
	//! Buy tab: the cart's total and the balance after it, by currency. Sale tab: the sale box's total.
	//! \return False when a balance after the cart would be negative.
	protected bool UpdateTotals(bool sale)
	{
		Widget child = m_wTotals.GetChildren();
		while (child)
		{
			Widget next = child.GetSibling();
			child.RemoveFromHierarchy();
			child = next;
		}

		array<string> currencies = {};
		array<int> amounts = {};
		if (sale)
		{
			m_Trade.GetSaleTotals(currencies, amounts);
			foreach (int i, string saleCurrency : currencies)
			{
				AddTotal("#MRX-Cart_SellTotal", "+" + MRX_TextFormat.Money(amounts[i], saleCurrency), MRX_UIStyle.GetIncreaseColor(), BALANCE_FONT_SIZE);
			}

			return true;
		}

		bool covered = true;
		m_Trade.GetCartTotals(currencies, amounts);
		foreach (int index, string currency : currencies)
		{
			AddTotal("#MRX-Cart_BuyTotal", "-" + MRX_TextFormat.Money(amounts[index], currency), Color.FromInt(Color.WHITE), TOTAL_FONT_SIZE);
			int after;
			if (!m_Trade.GetBalanceAfter(currency, after))
				continue;

			Color afterColor = Color.FromInt(Color.WHITE);
			if (after < 0)
			{
				afterColor = MRX_UIStyle.GetDecreaseColor();
				covered = false;
			}

			AddTotal("#MRX-Cart_BalanceAfter", MRX_TextFormat.Money(after, currency), afterColor, BALANCE_FONT_SIZE);
		}

		return covered;
	}

	//------------------------------------------------------------------------------------------------
	protected void AddTotal(string label, string value, Color valueColor, int fontSize)
	{
		Widget row = MRX_UIStyle.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, Color.FromInt(Color.WHITE), m_wTotals);
		AlignableSlot.SetHorizontalAlign(row, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetPadding(row, 0, 1, 0, 1);
		TextWidget labelText = MRX_UIStyle.CreateText(row, label, TOTAL_FONT_SIZE, MRX_UIStyle.GetMutedColor(), true);
		LayoutSlot.SetSizeMode(labelText, LayoutSizeMode.Fill);
		LayoutSlot.SetVerticalAlign(labelText, LayoutVerticalAlign.Center);
		TextWidget valueText = MRX_UIStyle.CreateText(row, value, fontSize, valueColor, true);
		LayoutSlot.SetVerticalAlign(valueText, LayoutVerticalAlign.Center);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnTabClicked(MRX_FlatButton button)
	{
		if (m_Trade)
			m_Trade.SetSaleTab(button == m_SellTab);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCheckoutHeld(MRX_HoldButton button)
	{
		if (!m_Trade)
			return;

		SetStatus(string.Empty, false);
		m_Trade.Checkout();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnResult(bool sell, MRX_EShopStatus status, MRX_ETxStatus txStatus, array<string> currencies, array<int> amounts, int doneCount, int leftCount)
	{
		if (status != MRX_EShopStatus.OK)
		{
			SetStatus(MRX_ShopMenu.GetFailureText(status, txStatus), true);
		}
		else if (leftCount > 0)
		{
			SetStatus(WidgetManager.Translate("#MRX-Cart_Left", leftCount), true);
		}
		else
		{
			SetStatus("#MRX-Cart_Done", false);
			GetGame().GetCallqueue().Remove(ClearStatus);
			GetGame().GetCallqueue().CallLater(ClearStatus, STATUS_MS);
		}

		Update();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnNoAnswer()
	{
		SetStatus("#MRX-Cart_NoAnswer", true);
		Update();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnFailure(MRX_EShopStatus status)
	{
		if (status == MRX_EShopStatus.NO_SPACE && m_Trade && !m_Trade.IsSaleTab())
			SetStatus("#MRX-Cart_Full", true);
		else
			SetStatus(MRX_ShopMenu.GetFailureText(status, MRX_ETxStatus.OK), true);

		GetGame().GetCallqueue().Remove(ClearStatus);
		GetGame().GetCallqueue().CallLater(ClearStatus, STATUS_MS);
		Update();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBalanceChanged(string currency, int balance)
	{
		Update();
	}

	//------------------------------------------------------------------------------------------------
	protected void SetStatus(string text, bool error)
	{
		m_sStatus = text;
		m_bStatusError = error;
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearStatus()
	{
		SetStatus(string.Empty, false);
		Update();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnItemMoved(MRX_ArsenalTradeStorageComponent storage, IEntity item, bool added)
	{
		if (m_Trade && (storage == m_Trade.GetCartStorage() || storage == m_Trade.GetSaleStorage()))
			ScheduleRefresh();
	}

	//------------------------------------------------------------------------------------------------
	//! Items moved inside items of the container (e.g. a scope onto a rifle in the cart).
	protected void OnContainerChanged(IEntity item, BaseInventoryStorageComponent storage)
	{
		ScheduleRefresh();
	}

	//------------------------------------------------------------------------------------------------
	protected void ScheduleRefresh()
	{
		GetGame().GetCallqueue().Remove(RefreshStorages);
		GetGame().GetCallqueue().CallLater(RefreshStorages);
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshStorages()
	{
		if (m_CartUI)
			m_CartUI.Refresh();

		if (m_SaleUI)
			m_SaleUI.Refresh();

		// The item of the storage below left the trade (taken out, paid, sold, returned).
		if (m_DetailUI)
		{
			BaseInventoryStorageComponent detail = m_DetailUI.GetStorage();
			if (!detail || !detail.GetOwner() || !MRX_ArsenalTradeStorageComponent.FindHolder(detail.GetOwner()))
				CloseDetail();
			else
				m_DetailUI.Refresh();
		}

		Update();
	}
}

//------------------------------------------------------------------------------------------------
//! Vanilla storage panel of an item of the trade in the space below the cart or the sale box (MRX_ArsenalTradePanel).
//! Its close button closes it there. Internal.
class MRX_ArsenalTradeDetailStorageUI : SCR_InventoryOpenedStorageUI
{
	//------------------------------------------------------------------------------------------------
	override void CloseStorage()
	{
		if (m_MenuHandler)
			m_MenuHandler.MRX_CloseTradeDetail();
	}

	//------------------------------------------------------------------------------------------------
	void MRX_ArsenalTradeDetailStorageUI(
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

//------------------------------------------------------------------------------------------------
//! Vanilla storage panel of the cart or the sale box in the middle of the inventory. Its title names the tab, its
//! preview shows the arsenal, and the items of the sale box show their sale price. Internal.
class MRX_ArsenalTradeStorageUI : SCR_InventoryOpenedStorageUI
{
	protected static const string PRICE_WIDGET = "MRX_SalePrice";
	protected static const string SLOT_CONTENT = "Overlay";
	protected static const int PRICE_FONT_SIZE = 12;

	//------------------------------------------------------------------------------------------------
	MRX_ArsenalTradeStorageComponent GetTradeStorage()
	{
		return MRX_ArsenalTradeStorageComponent.Cast(m_Storage);
	}

	//------------------------------------------------------------------------------------------------
	override void Init()
	{
		super.Init();
		MRX_ArsenalTradeStorageComponent storage = GetTradeStorage();
		if (m_wStorageName && storage)
		{
			if (storage.GetRole() == MRX_EArsenalTradeRole.SALE)
				m_wStorageName.SetText("#MRX-Cart_TabSell");
			else
				m_wStorageName.SetText("#MRX-Cart_Title");
		}

		Widget closeButton;
		if (m_widget)
			closeButton = m_widget.FindAnyWidget("CloseStorageBtn");

		if (closeButton)
			closeButton.SetVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	//! The arsenal instead of the invisible container.
	override protected void SetPreviewItem()
	{
		super.SetPreviewItem();
		MRX_ArsenalTradeClient trade = MRX_ArsenalTradeClient.GetLocal();
		ItemPreviewWidget renderPreview = ItemPreviewWidget.Cast(m_wPreviewImage);
		ItemPreviewManagerEntity previews = MRX_ScriptedDialog.GetPreviewManager();
		if (trade && trade.GetArsenal() && renderPreview && previews)
			previews.SetPreviewItem(renderPreview, trade.GetArsenal(), null, true);
	}

	//------------------------------------------------------------------------------------------------
	override void Refresh()
	{
		super.Refresh();
		MRX_UpdatePrices();
	}

	//------------------------------------------------------------------------------------------------
	//! Sale box: each item shows the server's sale price (with what it holds) at the bottom of its slot.
	void MRX_UpdatePrices()
	{
		MRX_ArsenalTradeStorageComponent storage = GetTradeStorage();
		MRX_ArsenalTradeClient trade = MRX_ArsenalTradeClient.GetLocal();
		if (!storage || storage.GetRole() != MRX_EArsenalTradeRole.SALE || !trade)
			return;

		foreach (SCR_InventorySlotUI slot : m_aSlots)
		{
			if (!slot || !slot.GetWidget() || !slot.GetInventoryItemComponent())
				continue;

			int price;
			string currency;
			string text;
			if (trade.GetSalePrice(slot.GetInventoryItemComponent().GetOwner(), price, currency))
				text = MRX_TextFormat.MoneyCompact(price, currency);

			SetPriceText(slot.GetWidget(), text);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void SetPriceText(notnull Widget slotWidget, string text)
	{
		TextWidget label = TextWidget.Cast(slotWidget.FindAnyWidget(PRICE_WIDGET));
		if (!label)
		{
			Widget content = slotWidget.FindAnyWidget(SLOT_CONTENT);
			if (!content)
				return;

			label = MRX_UIStyle.CreateText(content, string.Empty, PRICE_FONT_SIZE, Color.FromInt(Color.WHITE), true);
			label.SetName(PRICE_WIDGET);
			AlignableSlot.SetHorizontalAlign(label, LayoutHorizontalAlign.Center);
			AlignableSlot.SetVerticalAlign(label, LayoutVerticalAlign.Bottom);
			AlignableSlot.SetPadding(label, 2, 0, 2, 2);
		}

		label.SetText(text);
		label.SetVisible(!text.IsEmpty());
	}

	//------------------------------------------------------------------------------------------------
	void MRX_ArsenalTradeStorageUI(
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
