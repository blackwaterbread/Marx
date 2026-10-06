//! Filter row between the title and the items of a Marx arsenal panel: a category button that drops down the
//! categories with their item counts, the vanilla search field and the number of items shown. Same look as the balance
//! panel. Typing ends with Enter, Escape or a click elsewhere; a click elsewhere also closes the categories. Mouse and
//! keyboard only. Internal.
class MRX_ArsenalFilterBar : Managed
{
	//! Vertical layout of a storage panel between its header and its items (vanilla traversal titles go there too).
	protected static const string TITLE_LAYOUT = "titleLayout";
	protected static const ResourceName SEARCH_LAYOUT = "{67B4F07635959DC7}UI/layouts/WidgetLibrary/EditBox/WLib_EditBoxSearch.layout";
	protected static const ResourceName ICONS = "{3262679C50EF4F01}UI/Textures/Icons/icons_wrapperUI.imageset";
	//! The chat's input context: while typing it keeps the inventory's keys (paging, navigation, closing) from acting.
	//! Its Enter and Escape actions end typing; the chat panel ignores them while it is closed.
	protected static const string TYPING_CONTEXT = "ChatContext";
	protected static const string ACTION_ENTER = "ChatSendMessage";
	protected static const string ACTION_ESCAPE = "ChatEscape";
	//! Left mouse button in the global context, active in every menu.
	protected static const string ACTION_CLICK = "MouseLeft";
	protected static const ResourceName BOLD_FONT = "{EABA4FE9D014CCEF}UI/Fonts/RobotoCondensed/RobotoCondensed_Bold.fnt";
	protected static const ResourceName REGULAR_FONT = "{3E7733BAC8C831F6}UI/Fonts/RobotoCondensed/RobotoCondensed_Regular.fnt";
	protected static const int FONT_SIZE = 15;
	protected static const float ROW_HEIGHT = 36;
	protected static const float ARROW_SIZE = 16;
	protected static const int SEARCH_DELAY_MS = 250;
	//! Images and texts: the cursor and the focus pass through them.
	protected static const int DECOR = WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS;
	//! Containers of the buttons and the search field: a container that ignores the cursor or the focus hides its children
	//! from it too.
	protected static const int CONTAINER = WidgetFlags.VISIBLE;
	protected static const int BUTTON = WidgetFlags.VISIBLE | WidgetFlags.NOFOCUS;

	protected MRX_ArsenalFilter m_Filter;
	protected ref MRX_ArsenalFilterHandler m_Handler;
	protected ref ScriptInvokerVoid m_OnChanged;
	protected Widget m_wPanelRoot;
	protected ButtonWidget m_wCategoryButton;
	protected TextWidget m_wCategoryText;
	protected Widget m_wSearch;
	protected SCR_EditBoxSearchComponent m_Search;
	protected bool m_bTyping;
	protected bool m_bListeningClicks;
	protected TextWidget m_wCount;
	protected SizeLayoutWidget m_wList;
	//! Category of each entry button of the list, empty for all.
	protected ref map<Widget, string> m_mEntries = new map<Widget, string>();
	protected ref map<Widget, Widget> m_mEntryBackgrounds = new map<Widget, Widget>();
	protected ref array<string> m_aCategories = {};
	protected ref map<string, int> m_mCounts = new map<string, int>();
	protected int m_iTotal;

	//------------------------------------------------------------------------------------------------
	//! Adds the row above the items of a storage panel.
	//! \param filter Kept by the caller; the bar changes it and then invokes GetOnChanged().
	//! \return Null when the panel has an unexpected layout.
	static MRX_ArsenalFilterBar Create(Widget panelRoot, notnull MRX_ArsenalFilter filter, notnull MRX_ArsenalShopComponent arsenal)
	{
		if (!panelRoot)
			return null;

		Widget titleLayout = panelRoot.FindAnyWidget(TITLE_LAYOUT);
		if (!titleLayout)
			return null;

		MRX_ArsenalFilterBar bar = new MRX_ArsenalFilterBar();
		bar.m_Filter = filter;
		bar.m_wPanelRoot = panelRoot;
		bar.CountItems(arsenal);
		bar.Build(titleLayout);
		return bar;
	}

	//------------------------------------------------------------------------------------------------
	//! Invoked after the user changed the category or the search text.
	ScriptInvokerVoid GetOnChanged()
	{
		if (!m_OnChanged)
			m_OnChanged = new ScriptInvokerVoid();

		return m_OnChanged;
	}

	//------------------------------------------------------------------------------------------------
	void SetShownCount(int count)
	{
		if (!m_wCount)
			return;

		if (count == 1)
			m_wCount.SetText("1 item");
		else
			m_wCount.SetText(string.Format("%1 items", count));
	}

	//------------------------------------------------------------------------------------------------
	//! Shows the filter's category and text after they were changed directly.
	void Sync()
	{
		UpdateCategoryText();
		if (m_Search && m_Search.GetValue() != m_Filter.GetText())
			m_Search.SetValue(m_Filter.GetText());
	}

	//------------------------------------------------------------------------------------------------
	//! Stops the timers and the typing context; call when the panel closes.
	void Stop()
	{
		GetGame().GetCallqueue().Remove(KeepTyping);
		GetGame().GetCallqueue().Remove(ApplyText);
		m_bTyping = false;
		SetTypingListeners(false);
		if (m_bListeningClicks)
			GetGame().GetInputManager().RemoveActionListener(ACTION_CLICK, EActionTrigger.DOWN, OnClickAnywhere);

		m_bListeningClicks = false;
		if (m_Search)
		{
			m_Search.m_OnTextChange.Remove(OnTextChange);
			m_Search.m_OnWriteModeEnter.Remove(OnWriteModeEnter);
			m_Search.m_OnWriteModeLeave.Remove(OnWriteModeLeave);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the widget was one of the bar's buttons.
	bool OnClick(Widget w)
	{
		if (w == m_wCategoryButton)
		{
			SetListVisible(!m_wList.IsVisible());
			return true;
		}

		string category;
		if (!m_mEntries.Find(w, category))
			return false;

		SetListVisible(false);
		if (category != m_Filter.GetCategory())
		{
			m_Filter.SetCategory(category);
			UpdateCategoryText();
			if (m_OnChanged)
				m_OnChanged.Invoke();
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	void OnHover(Widget w, bool hovered)
	{
		Widget background = m_mEntryBackgrounds.Get(w);
		if (!background && w == m_wCategoryButton)
			background = m_wCategoryButton.FindAnyWidget("Fill");

		if (!background)
			return;

		if (hovered)
			background.SetColor(GetHoverColor());
		else if (w == m_wCategoryButton)
			background.SetColor(GetFillColor());
		else
			background.SetColor(GetEntryColor(m_mEntries.Get(w)));
	}

	//------------------------------------------------------------------------------------------------
	protected void CountItems(notnull MRX_ArsenalShopComponent arsenal)
	{
		array<MRX_ShopItem> items = {};
		m_iTotal = arsenal.GetListedItems(items);
		foreach (MRX_ShopItem item : items)
		{
			m_mCounts.Set(item.m_sCategory, m_mCounts.Get(item.m_sCategory) + 1);
		}

		m_aCategories.Copy(arsenal.GetCategories());
	}

	//------------------------------------------------------------------------------------------------
	protected void Build(notnull Widget parent)
	{
		m_Handler = new MRX_ArsenalFilterHandler();
		m_Handler.m_Bar = this;

		Widget row = CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, Color.FromInt(Color.WHITE), parent, CONTAINER);
		AlignableSlot.SetHorizontalAlign(row, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetPadding(row, 0, 0, 0, 6);

		// A catalog without categories only gets the search field.
		if (!m_aCategories.IsEmpty())
			BuildCategoryButton(row);

		m_wSearch = GetGame().GetWorkspace().CreateWidgets(SEARCH_LAYOUT, row);
		if (m_wSearch)
		{
			LayoutSlot.SetSizeMode(m_wSearch, LayoutSizeMode.Fill);
			LayoutSlot.SetFillWeight(m_wSearch, 1.3);
			LayoutSlot.SetVerticalAlign(m_wSearch, LayoutVerticalAlign.Center);
			SizeLayoutWidget size = SizeLayoutWidget.Cast(m_wSearch.FindAnyWidget("SizeLayout"));
			if (size)
				size.SetHeightOverride(ROW_HEIGHT);

			m_Search = SCR_EditBoxSearchComponent.Cast(m_wSearch.FindHandler(SCR_EditBoxSearchComponent));
		}

		if (m_Search)
		{
			m_Search.SetValue(m_Filter.GetText());
			m_Search.m_OnTextChange.Insert(OnTextChange);
			m_Search.m_OnWriteModeEnter.Insert(OnWriteModeEnter);
			m_Search.m_OnWriteModeLeave.Insert(OnWriteModeLeave);
		}

		m_wCount = CreateText(row, REGULAR_FONT, GetCountColor());
		LayoutSlot.SetVerticalAlign(m_wCount, LayoutVerticalAlign.Center);
		AlignableSlot.SetPadding(m_wCount, 10, 0, 2, 0);
		SetShownCount(m_iTotal);

		if (!m_aCategories.IsEmpty())
			BuildList();
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildCategoryButton(notnull Widget row)
	{
		m_wCategoryButton = ButtonWidget.Cast(CreateWidget(WidgetType.ButtonWidgetTypeID, GetClearColor(), row, BUTTON));
		LayoutSlot.SetSizeMode(m_wCategoryButton, LayoutSizeMode.Fill);
		LayoutSlot.SetFillWeight(m_wCategoryButton, 1);
		LayoutSlot.SetVerticalAlign(m_wCategoryButton, LayoutVerticalAlign.Stretch);
		AlignableSlot.SetPadding(m_wCategoryButton, 0, 0, 8, 0);
		m_wCategoryButton.AddHandler(m_Handler);

		Widget box = CreateWidget(WidgetType.OverlayWidgetTypeID, Color.FromInt(Color.WHITE), m_wCategoryButton, DECOR);
		Stretch(box);
		Stretch(CreateWidget(WidgetType.ImageWidgetTypeID, GetFrameColor(), box, DECOR));
		Widget fill = CreateWidget(WidgetType.ImageWidgetTypeID, GetFillColor(), box, DECOR);
		fill.SetName("Fill");
		Stretch(fill);
		AlignableSlot.SetPadding(fill, 1, 1, 1, 1);

		Widget content = CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, Color.FromInt(Color.WHITE), box, DECOR);
		Stretch(content);
		AlignableSlot.SetPadding(content, 12, 0, 10, 0);
		m_wCategoryText = CreateText(content, BOLD_FONT, Color.FromInt(Color.WHITE));
		LayoutSlot.SetSizeMode(m_wCategoryText, LayoutSizeMode.Fill);
		LayoutSlot.SetVerticalAlign(m_wCategoryText, LayoutVerticalAlign.Center);

		ImageWidget arrow = ImageWidget.Cast(CreateWidget(WidgetType.ImageWidgetTypeID, GetAccentColor(), content, DECOR));
		arrow.LoadImageFromSet(0, ICONS, "sortArrowDown");
		arrow.SetSize(ARROW_SIZE, ARROW_SIZE);
		LayoutSlot.SetVerticalAlign(arrow, LayoutVerticalAlign.Center);
		UpdateCategoryText();
	}

	//------------------------------------------------------------------------------------------------
	//! The drop-down list, over the items below the category button.
	protected void BuildList()
	{
		m_wList = SizeLayoutWidget.Cast(CreateWidget(WidgetType.SizeLayoutWidgetTypeID, Color.FromInt(Color.WHITE), m_wPanelRoot, CONTAINER));
		AlignableSlot.SetHorizontalAlign(m_wList, LayoutHorizontalAlign.Left);
		AlignableSlot.SetVerticalAlign(m_wList, LayoutVerticalAlign.Top);
		m_wList.EnableMinDesiredWidth(true);

		Widget box = CreateWidget(WidgetType.OverlayWidgetTypeID, Color.FromInt(Color.WHITE), m_wList, CONTAINER);
		Stretch(CreateWidget(WidgetType.ImageWidgetTypeID, GetFrameColor(), box, DECOR));
		Widget fill = CreateWidget(WidgetType.ImageWidgetTypeID, GetFillColor(), box, DECOR);
		Stretch(fill);
		AlignableSlot.SetPadding(fill, 1, 1, 1, 1);

		Widget column = CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, Color.FromInt(Color.WHITE), box, CONTAINER);
		AlignableSlot.SetHorizontalAlign(column, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetPadding(column, 1, 1, 1, 1);

		AddEntry(column, string.Empty, "All", m_iTotal);
		foreach (string category : m_aCategories)
		{
			AddEntry(column, category, category, m_mCounts.Get(category));
		}

		m_wList.SetVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddEntry(notnull Widget column, string category, string label, int count)
	{
		Widget entry = CreateWidget(WidgetType.ButtonWidgetTypeID, GetClearColor(), column, BUTTON);
		AlignableSlot.SetHorizontalAlign(entry, LayoutHorizontalAlign.Stretch);
		entry.AddHandler(m_Handler);
		m_mEntries.Set(entry, category);

		Widget box = CreateWidget(WidgetType.OverlayWidgetTypeID, Color.FromInt(Color.WHITE), entry, DECOR);
		Stretch(box);
		Widget background = CreateWidget(WidgetType.ImageWidgetTypeID, GetEntryColor(category), box, DECOR);
		Stretch(background);
		m_mEntryBackgrounds.Set(entry, background);

		Widget content = CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, Color.FromInt(Color.WHITE), box, DECOR);
		Stretch(content);
		AlignableSlot.SetPadding(content, 12, 5, 12, 5);
		TextWidget name = CreateText(content, BOLD_FONT, Color.FromInt(Color.WHITE));
		name.SetText(label);
		LayoutSlot.SetSizeMode(name, LayoutSizeMode.Fill);
		TextWidget countText = CreateText(content, REGULAR_FONT, GetCountColor());
		countText.SetText(count.ToString());
		AlignableSlot.SetPadding(countText, 16, 0, 0, 0);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetListVisible(bool visible)
	{
		if (!m_wList)
			return;

		if (visible)
		{
			// Right below the category button and as wide; the panel root is an overlay.
			float buttonX, buttonY, buttonWidth, buttonHeight, rootX, rootY;
			m_wCategoryButton.GetScreenPos(buttonX, buttonY);
			m_wCategoryButton.GetScreenSize(buttonWidth, buttonHeight);
			m_wPanelRoot.GetScreenPos(rootX, rootY);
			WorkspaceWidget workspace = GetGame().GetWorkspace();
			AlignableSlot.SetPadding(m_wList, workspace.DPIUnscale(buttonX - rootX), workspace.DPIUnscale(buttonY + buttonHeight - rootY), 0, 0);
			m_wList.SetMinDesiredWidth(workspace.DPIUnscale(buttonWidth));
			foreach (Widget entry, string category : m_mEntries)
			{
				m_mEntryBackgrounds.Get(entry).SetColor(GetEntryColor(category));
			}
		}

		m_wList.SetVisible(visible);
		UpdateClickListener();
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateCategoryText()
	{
		if (!m_wCategoryText)
			return;

		string category = m_Filter.GetCategory();
		if (category.IsEmpty())
			m_wCategoryText.SetText("All");
		else
			m_wCategoryText.SetText(category);
	}

	//------------------------------------------------------------------------------------------------
	//! The search applies once typing pauses: every change rebuilds the item slots.
	protected void OnTextChange(string text)
	{
		GetGame().GetCallqueue().Remove(ApplyText);
		GetGame().GetCallqueue().CallLater(ApplyText, SEARCH_DELAY_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyText()
	{
		if (!m_Search || m_Search.GetValue() == m_Filter.GetText())
			return;

		m_Filter.SetText(m_Search.GetValue());
		if (m_OnChanged)
			m_OnChanged.Invoke();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnWriteModeEnter()
	{
		SetListVisible(false);
		m_bTyping = true;
		SetTypingListeners(true);
		UpdateClickListener();
		GetGame().GetCallqueue().Remove(KeepTyping);
		GetGame().GetCallqueue().CallLater(KeepTyping, 0, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnWriteModeLeave(string text)
	{
		m_bTyping = false;
		SetTypingListeners(false);
		UpdateClickListener();
		GetGame().GetCallqueue().Remove(KeepTyping);
		GetGame().GetCallqueue().Remove(ApplyText);
		ApplyText();
	}

	//------------------------------------------------------------------------------------------------
	//! Taking the focus away from the search field ends its write mode.
	protected void EndTyping(float value = 0, EActionTrigger reason = 0)
	{
		GetGame().GetWorkspace().SetFocusedWidget(null);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetTypingListeners(bool listen)
	{
		InputManager input = GetGame().GetInputManager();
		input.RemoveActionListener(ACTION_ENTER, EActionTrigger.DOWN, EndTyping);
		input.RemoveActionListener(ACTION_ESCAPE, EActionTrigger.DOWN, EndTyping);
		if (!listen)
			return;

		input.AddActionListener(ACTION_ENTER, EActionTrigger.DOWN, EndTyping);
		input.AddActionListener(ACTION_ESCAPE, EActionTrigger.DOWN, EndTyping);
	}

	//------------------------------------------------------------------------------------------------
	//! Clicks are watched while typing or while the categories are shown.
	protected void UpdateClickListener()
	{
		bool listen = m_bTyping || (m_wList && m_wList.IsVisible());
		if (listen == m_bListeningClicks)
			return;

		m_bListeningClicks = listen;
		if (listen)
			GetGame().GetInputManager().AddActionListener(ACTION_CLICK, EActionTrigger.DOWN, OnClickAnywhere);
		else
			GetGame().GetInputManager().RemoveActionListener(ACTION_CLICK, EActionTrigger.DOWN, OnClickAnywhere);
	}

	//------------------------------------------------------------------------------------------------
	//! A click outside the search field ends typing, one outside the categories and their button closes them.
	protected void OnClickAnywhere(float value = 0, EActionTrigger reason = 0)
	{
		if (m_bTyping && !IsUnderCursor(m_wSearch))
			EndTyping();

		if (m_wList && m_wList.IsVisible() && !IsUnderCursor(m_wList) && !IsUnderCursor(m_wCategoryButton))
			SetListVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsUnderCursor(Widget widget)
	{
		if (!widget)
			return false;

		Widget hovered = WidgetManager.GetWidgetUnderCursor();
		while (hovered)
		{
			if (hovered == widget)
				return true;

			hovered = hovered.GetParent();
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The context is active for one frame at a time.
	protected void KeepTyping()
	{
		GetGame().GetInputManager().ActivateContext(TYPING_CONTEXT);
	}

	//------------------------------------------------------------------------------------------------
	//! \param flags DECOR, CONTAINER or BUTTON.
	protected static Widget CreateWidget(WidgetType type, Color color, Widget parent, int flags)
	{
		return GetGame().GetWorkspace().CreateWidget(type, flags, color, 0, parent);
	}

	//------------------------------------------------------------------------------------------------
	protected static TextWidget CreateText(Widget parent, ResourceName font, Color color)
	{
		TextWidget text = TextWidget.Cast(CreateWidget(WidgetType.TextWidgetTypeID, color, parent, DECOR));
		text.SetFont(font);
		text.SetExactFontSize(FONT_SIZE);
		return text;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Stretch(notnull Widget widget)
	{
		AlignableSlot.SetHorizontalAlign(widget, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetVerticalAlign(widget, LayoutVerticalAlign.Stretch);
	}

	//------------------------------------------------------------------------------------------------
	//! The chosen category stands out in the list.
	protected Color GetEntryColor(string category)
	{
		if (category == m_Filter.GetCategory())
			return Color.FromSRGBA(62, 66, 72, 255);

		return GetClearColor();
	}

	//------------------------------------------------------------------------------------------------
	//! Buttons draw nothing themselves, their children do.
	protected static Color GetClearColor()
	{
		return Color.FromSRGBA(0, 0, 0, 0);
	}

	//------------------------------------------------------------------------------------------------
	protected static Color GetAccentColor()
	{
		return Color.FromSRGBA(226, 167, 79, 255);
	}

	//------------------------------------------------------------------------------------------------
	protected static Color GetFrameColor()
	{
		return Color.FromSRGBA(62, 66, 72, 255);
	}

	//------------------------------------------------------------------------------------------------
	protected static Color GetFillColor()
	{
		return Color.FromSRGBA(20, 22, 25, 240);
	}

	//------------------------------------------------------------------------------------------------
	protected static Color GetHoverColor()
	{
		return Color.FromSRGBA(90, 72, 44, 255);
	}

	//------------------------------------------------------------------------------------------------
	protected static Color GetCountColor()
	{
		return Color.FromSRGBA(160, 164, 170, 255);
	}
}

//------------------------------------------------------------------------------------------------
//! Passes the clicks and hovers of the filter bar's buttons to the bar. Internal.
class MRX_ArsenalFilterHandler : ScriptedWidgetEventHandler
{
	//! Weak: the bar owns this handler.
	MRX_ArsenalFilterBar m_Bar;

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		return m_Bar && m_Bar.OnClick(w);
	}

	//------------------------------------------------------------------------------------------------
	override bool OnMouseEnter(Widget w, int x, int y)
	{
		if (m_Bar)
			m_Bar.OnHover(w, true);

		return false;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
	{
		if (m_Bar)
			m_Bar.OnHover(w, false);

		return false;
	}
}
