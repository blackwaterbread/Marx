//! Loadout slots below the items of the stash panel, in the look of the balance panel: each slot shows the main weapon
//! and the number of items of its saved loadout and what putting it on costs now. Save and Load act only when held
//! (MRX_HoldButton): saving replaces the slot, loading replaces the gear. Client. Internal.
class MRX_LoadoutBar : Managed
{
	//! Vertical layout of a storage panel: header, title, item grid, pages.
	protected static const string PANEL_CONTAINER = "Container";
	protected static const ResourceName BOLD_FONT = "{EABA4FE9D014CCEF}UI/Fonts/RobotoCondensed/RobotoCondensed_Bold.fnt";
	protected static const ResourceName REGULAR_FONT = "{3E7733BAC8C831F6}UI/Fonts/RobotoCondensed/RobotoCondensed_Regular.fnt";
	protected static const int TITLE_FONT_SIZE = 15;
	protected static const int NAME_FONT_SIZE = 16;
	protected static const int DETAIL_FONT_SIZE = 13;
	protected static const int INDEX_FONT_SIZE = 20;
	protected static const float ACCENT_WIDTH = 4;
	protected static const float SAVE_WIDTH = 64;
	protected static const float LOAD_WIDTH = 124;
	protected static const int FEEDBACK_MS = 5000;
	protected static const int DECOR = WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS;
	//! Containers of buttons: a container that ignores the cursor hides its children from it too.
	protected static const int CONTAINER = WidgetFlags.VISIBLE;

	//! Weak.
	protected SCR_PlayerController m_Controller;
	//! Hidden until the server reports slots.
	protected Widget m_wBar;
	protected Widget m_wRows;
	protected TextWidget m_wInfo;
	protected ref array<ref MRX_HoldButton> m_aSaveButtons = {};
	protected ref array<ref MRX_HoldButton> m_aLoadButtons = {};
	protected ref array<TextWidget> m_aNames = {};
	protected ref array<TextWidget> m_aDetails = {};
	protected string m_sCurrency;
	//! The last request was a load (else a save).
	protected bool m_bLastLoad;

	//------------------------------------------------------------------------------------------------
	//! Adds the bar below the item grid of a storage panel and asks the server for the slots.
	//! \return Null when the panel has an unexpected layout or there is no local player controller.
	static MRX_LoadoutBar Create(Widget panelRoot)
	{
		if (!panelRoot)
			return null;

		Widget container = panelRoot.FindAnyWidget(PANEL_CONTAINER);
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!container || !controller)
			return null;

		MRX_LoadoutBar bar = new MRX_LoadoutBar();
		bar.m_Controller = controller;
		bar.Build(container);
		controller.MRX_GetOnLoadoutInfo().Insert(bar.OnInfo);
		controller.MRX_GetOnLoadoutResult().Insert(bar.OnResult);
		controller.MRX_RequestLoadoutInfo();
		return bar;
	}

	//------------------------------------------------------------------------------------------------
	//! Stops listening and the timers; call when the panel closes.
	void Stop()
	{
		GetGame().GetCallqueue().Remove(FadeOut);
		if (m_Controller)
		{
			m_Controller.MRX_GetOnLoadoutInfo().Remove(OnInfo);
			m_Controller.MRX_GetOnLoadoutResult().Remove(OnResult);
		}

		foreach (MRX_HoldButton button : m_aSaveButtons)
		{
			button.Stop();
		}

		foreach (MRX_HoldButton loadButton : m_aLoadButtons)
		{
			loadButton.Stop();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Slots shown (0 until the server answered).
	int GetSlotCount()
	{
		return m_aSaveButtons.Count();
	}

	//------------------------------------------------------------------------------------------------
	protected void Build(notnull Widget container)
	{
		Widget bar = CreateWidget(WidgetType.OverlayWidgetTypeID, Color.FromInt(Color.WHITE), container, CONTAINER);
		AlignableSlot.SetHorizontalAlign(bar, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetPadding(bar, 0, 8, 0, 0);
		bar.SetVisible(false);
		m_wBar = bar;

		MRX_UIStyle.Stretch(CreateWidget(WidgetType.ImageWidgetTypeID, MRX_UIStyle.GetFrameColor(), bar, DECOR));
		Widget fill = CreateWidget(WidgetType.ImageWidgetTypeID, MRX_UIStyle.GetFillColor(), bar, DECOR);
		MRX_UIStyle.Stretch(fill);
		AlignableSlot.SetPadding(fill, 1, 1, 1, 1);
		ImageWidget accent = ImageWidget.Cast(CreateWidget(WidgetType.ImageWidgetTypeID, MRX_UIStyle.GetAccentColor(), bar, DECOR));
		AlignableSlot.SetHorizontalAlign(accent, LayoutHorizontalAlign.Left);
		AlignableSlot.SetVerticalAlign(accent, LayoutVerticalAlign.Stretch);
		accent.SetSize(ACCENT_WIDTH, 1);

		Widget column = CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, Color.FromInt(Color.WHITE), bar, CONTAINER);
		AlignableSlot.SetHorizontalAlign(column, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetPadding(column, 16 + ACCENT_WIDTH, 8, 12, 10);

		Widget header = CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, Color.FromInt(Color.WHITE), column, DECOR);
		AlignableSlot.SetHorizontalAlign(header, LayoutHorizontalAlign.Stretch);
		TextWidget title = CreateText(header, BOLD_FONT, TITLE_FONT_SIZE, MRX_UIStyle.GetAccentColor());
		title.SetText("#MRX-Loadout_Title");
		LayoutSlot.SetSizeMode(title, LayoutSizeMode.Fill);
		TextWidget hint = CreateText(header, REGULAR_FONT, DETAIL_FONT_SIZE, MRX_UIStyle.GetMutedColor());
		hint.SetText("#MRX-Loadout_HoldHint");
		LayoutSlot.SetVerticalAlign(hint, LayoutVerticalAlign.Center);

		m_wRows = CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, Color.FromInt(Color.WHITE), column, CONTAINER);
		AlignableSlot.SetHorizontalAlign(m_wRows, LayoutHorizontalAlign.Stretch);

		m_wInfo = CreateText(column, BOLD_FONT, DETAIL_FONT_SIZE, MRX_UIStyle.GetMutedColor());
		AlignableSlot.SetPadding(m_wInfo, 0, 6, 0, 0);
		m_wInfo.SetVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildRows(int count)
	{
		foreach (MRX_HoldButton oldSave : m_aSaveButtons)
		{
			oldSave.Stop();
		}

		foreach (MRX_HoldButton oldLoad : m_aLoadButtons)
		{
			oldLoad.Stop();
		}

		Widget child = m_wRows.GetChildren();
		while (child)
		{
			Widget next = child.GetSibling();
			child.RemoveFromHierarchy();
			child = next;
		}

		m_aSaveButtons.Clear();
		m_aLoadButtons.Clear();
		m_aNames.Clear();
		m_aDetails.Clear();
		for (int slot = 0; slot < count; slot++)
		{
			Widget row = CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, Color.FromInt(Color.WHITE), m_wRows, CONTAINER);
			AlignableSlot.SetHorizontalAlign(row, LayoutHorizontalAlign.Stretch);
			AlignableSlot.SetPadding(row, 0, 6, 0, 0);

			TextWidget index = CreateText(row, BOLD_FONT, INDEX_FONT_SIZE, MRX_UIStyle.GetAccentColor());
			index.SetText((slot + 1).ToString());
			LayoutSlot.SetVerticalAlign(index, LayoutVerticalAlign.Center);
			AlignableSlot.SetPadding(index, 0, 0, 10, 0);

			Widget texts = CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, Color.FromInt(Color.WHITE), row, DECOR);
			LayoutSlot.SetSizeMode(texts, LayoutSizeMode.Fill);
			LayoutSlot.SetVerticalAlign(texts, LayoutVerticalAlign.Center);
			m_aNames.Insert(CreateText(texts, BOLD_FONT, NAME_FONT_SIZE, Color.FromInt(Color.WHITE)));
			m_aDetails.Insert(CreateText(texts, REGULAR_FONT, DETAIL_FONT_SIZE, MRX_UIStyle.GetMutedColor()));

			MRX_HoldButton save = MRX_HoldButton.Create(row, "#MRX-Loadout_Save", SAVE_WIDTH);
			LayoutSlot.SetVerticalAlign(save.GetRootWidget(), LayoutVerticalAlign.Center);
			AlignableSlot.SetPadding(save.GetRootWidget(), 8, 0, 0, 0);
			save.GetOnHeld().Insert(OnHeld);
			m_aSaveButtons.Insert(save);

			MRX_HoldButton load = MRX_HoldButton.Create(row, "#MRX-Loadout_Load", LOAD_WIDTH);
			LayoutSlot.SetVerticalAlign(load.GetRootWidget(), LayoutVerticalAlign.Center);
			AlignableSlot.SetPadding(load.GetRootWidget(), 6, 0, 0, 0);
			load.GetOnHeld().Insert(OnHeld);
			m_aLoadButtons.Insert(load);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnInfo(MRX_ELoadoutStatus status, string currency, array<string> mainItems, array<int> itemCounts, array<int> nets, array<int> unavailable)
	{
		if (!m_wRows)
			return;

		m_wBar.SetVisible(status == MRX_ELoadoutStatus.OK && !mainItems.IsEmpty());
		if (status != MRX_ELoadoutStatus.OK)
			return;

		m_sCurrency = currency;
		int count = mainItems.Count();
		if (count != m_aSaveButtons.Count())
			BuildRows(count);

		for (int slot = 0; slot < count; slot++)
		{
			bool saved = !mainItems[slot].IsEmpty();
			if (!saved)
			{
				m_aNames[slot].SetText("#MRX-Loadout_Empty");
				m_aNames[slot].SetColor(MRX_UIStyle.GetMutedColor());
				m_aDetails[slot].SetText("#MRX-Loadout_EmptyHint");
				m_aLoadButtons[slot].SetLabel("#MRX-Loadout_Load");
				m_aLoadButtons[slot].SetEnabled(false);
				continue;
			}

			m_aNames[slot].SetText(MRX_ScriptedDialog.GetItemDisplayName(mainItems[slot]));
			m_aNames[slot].SetColor(Color.FromInt(Color.WHITE));
			string detail = WidgetManager.Translate("#MRX-Loadout_Items", itemCounts[slot]);
			if (unavailable[slot] > 0)
				detail += WidgetManager.Translate("#MRX-Loadout_NotForSale", unavailable[slot]);

			m_aDetails[slot].SetText(detail);
			m_aLoadButtons[slot].SetLabel(GetLoadLabel(nets[slot]));
			m_aLoadButtons[slot].SetEnabled(true);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected string GetLoadLabel(int net)
	{
		if (net > 0)
			return WidgetManager.Translate("#MRX-Loadout_LoadCost", MRX_TextFormat.Money(net, m_sCurrency));

		if (net < 0)
			return WidgetManager.Translate("#MRX-Loadout_LoadRefund", MRX_TextFormat.Money(-net, m_sCurrency));

		return "#MRX-Loadout_LoadFree";
	}

	//------------------------------------------------------------------------------------------------
	protected void OnHeld(MRX_HoldButton button)
	{
		if (!m_Controller)
			return;

		int slot = m_aSaveButtons.Find(button);
		if (slot >= 0)
		{
			m_bLastLoad = false;
			m_Controller.MRX_RequestLoadoutSave(slot);
			ShowInfo(WidgetManager.Translate("#MRX-Loadout_Saving", slot + 1), false);
			return;
		}

		slot = m_aLoadButtons.Find(button);
		if (slot >= 0)
		{
			m_bLastLoad = true;
			m_Controller.MRX_RequestLoadoutLoad(slot);
			ShowInfo(WidgetManager.Translate("#MRX-Loadout_PuttingOn", slot + 1), false);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnResult(MRX_ELoadoutStatus status, MRX_ETxStatus txStatus, int slot, int net, string currency, int unavailable)
	{
		string text;
		bool error = status != MRX_ELoadoutStatus.OK;
		switch (status)
		{
			case MRX_ELoadoutStatus.OK:
			{
				if (!m_bLastLoad)
					text = WidgetManager.Translate("#MRX-Loadout_Saved", slot + 1);
				else
					text = WidgetManager.Translate("#MRX-Loadout_PutOn", slot + 1, GetChangeText(net, currency));

				if (unavailable > 0)
					text += " " + WidgetManager.Translate("#MRX-Loadout_LeftOut", unavailable);

				break;
			}

			case MRX_ELoadoutStatus.INCOMPLETE: text = WidgetManager.Translate("#MRX-Loadout_Incomplete", slot + 1, GetChangeText(net, currency)); break;
			case MRX_ELoadoutStatus.PAYMENT_FAILED:
			{
				if (txStatus == MRX_ETxStatus.INSUFFICIENT_FUNDS)
					text = "#MRX-Common_NotEnoughMoney";
				else
					text = "#MRX-Common_PaymentFailed";

				break;
			}

			case MRX_ELoadoutStatus.CHANGED: text = "#MRX-Loadout_Changed"; break;
			case MRX_ELoadoutStatus.EMPTY_SLOT: text = "#MRX-Loadout_EmptySlot"; break;
			case MRX_ELoadoutStatus.NO_STASH: text = "#MRX-Loadout_NoStash"; break;
			case MRX_ELoadoutStatus.BUSY: text = "#MRX-Common_PleaseWait"; break;
			case MRX_ELoadoutStatus.NOT_AVAILABLE: text = "#MRX-Loadout_NotAvailable"; break;
			case MRX_ELoadoutStatus.OWNER_NOT_READY: text = "#MRX-Common_OwnerNotReady"; break;
			default: text = WidgetManager.Translate("#MRX-Common_Failed", typename.EnumToString(MRX_ELoadoutStatus, status));
		}

		if (error)
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_INV_DROP_ERROR);

		ShowInfo(text, error);
	}

	//------------------------------------------------------------------------------------------------
	protected static string GetChangeText(int net, string currency)
	{
		if (net > 0)
			return WidgetManager.Translate("#MRX-Loadout_Paid", MRX_TextFormat.Money(net, currency));

		if (net < 0)
			return WidgetManager.Translate("#MRX-Loadout_Received", MRX_TextFormat.Money(-net, currency));

		return WidgetManager.Translate("#MRX-Loadout_NothingToPay");
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowInfo(string text, bool error)
	{
		if (!m_wInfo)
			return;

		m_wInfo.SetText(text);
		if (error)
			m_wInfo.SetColor(MRX_UIStyle.GetDecreaseColor());
		else
			m_wInfo.SetColor(MRX_UIStyle.GetIncreaseColor());

		AnimateWidget.StopAllAnimations(m_wInfo);
		m_wInfo.SetOpacity(1);
		m_wInfo.SetVisible(true);
		GetGame().GetCallqueue().Remove(FadeOut);
		GetGame().GetCallqueue().CallLater(FadeOut, FEEDBACK_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void FadeOut()
	{
		if (m_wInfo && m_wInfo.IsVisible())
			AnimateWidget.Opacity(m_wInfo, 0, 2, true);
	}

	//------------------------------------------------------------------------------------------------
	protected static Widget CreateWidget(WidgetType type, Color color, Widget parent, int flags)
	{
		return GetGame().GetWorkspace().CreateWidget(type, flags, color, 0, parent);
	}

	//------------------------------------------------------------------------------------------------
	protected static TextWidget CreateText(Widget parent, ResourceName font, int size, Color color)
	{
		TextWidget text = TextWidget.Cast(CreateWidget(WidgetType.TextWidgetTypeID, color, parent, DECOR));
		text.SetFont(font);
		text.SetExactFontSize(size);
		return text;
	}

}
