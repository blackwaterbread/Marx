//! Loadout slots of the loadout window (MRX_LoadoutMenu), in the look of the balance panel: each slot shows the main
//! weapon and the number of items of its saved loadout and what putting it on costs now. Save and Load act only when
//! held (MRX_HoldButton): saving replaces the slot, loading replaces the gear. A check box chooses whether loading uses
//! the stash (MRX_LoadoutService.Load). Client. Internal.
class MRX_LoadoutBar : Managed
{
	protected static const ResourceName BOLD_FONT = "{EABA4FE9D014CCEF}UI/Fonts/RobotoCondensed/RobotoCondensed_Bold.fnt";
	protected static const ResourceName REGULAR_FONT = "{3E7733BAC8C831F6}UI/Fonts/RobotoCondensed/RobotoCondensed_Regular.fnt";
	protected static const int TITLE_FONT_SIZE = 15;
	protected static const int NAME_FONT_SIZE = 18;
	protected static const int DETAIL_FONT_SIZE = 14;
	protected static const int INDEX_FONT_SIZE = 24;
	protected static const float INDEX_WIDTH = 38;
	//! Opacity of the rows of locked slots.
	protected static const float LOCKED_OPACITY = 0.45;
	protected static const float ACCENT_WIDTH = 4;
	protected static const float SAVE_WIDTH = 80;
	protected static const float LOAD_WIDTH = 160;
	protected static const int FEEDBACK_MS = 5000;
	protected static const int DECOR = WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS;
	//! Containers of buttons: a container that ignores the cursor hides its children from it too.
	protected static const int CONTAINER = WidgetFlags.VISIBLE;

	//! Loading uses the stash; kept while the game runs.
	protected static bool s_bUseStash = true;

	//! Weak.
	protected SCR_PlayerController m_Controller;
	//! Hidden until the server reports slots.
	protected Widget m_wBar;
	protected Widget m_wRows;
	protected TextWidget m_wInfo;
	protected ref array<Widget> m_aSlotRows = {};
	protected ref array<TextWidget> m_aIndexes = {};
	protected ref MRX_CheckBox m_StashBox;
	protected ref array<ref MRX_HoldButton> m_aSaveButtons = {};
	protected ref array<ref MRX_HoldButton> m_aLoadButtons = {};
	protected ref array<TextWidget> m_aNames = {};
	protected ref array<TextWidget> m_aDetails = {};
	protected string m_sCurrency;
	//! The slots as last reported, see MRX_LoadoutInfo.
	protected ref array<string> m_aMainItems = {};
	protected ref array<int> m_aItemCounts = {};
	protected ref array<int> m_aNets = {};
	protected ref array<int> m_aUnavailable = {};
	protected ref array<int> m_aStashNets = {};
	protected ref array<int> m_aStashUnavailable = {};
	//! The last request was a load (else a save).
	protected bool m_bLastLoad;

	//------------------------------------------------------------------------------------------------
	//! Adds the bar to a layout and asks the server for the slots.
	//! \return Null when there is no local player controller.
	static MRX_LoadoutBar Create(notnull Widget parent)
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
			return null;

		MRX_LoadoutBar bar = new MRX_LoadoutBar();
		bar.m_Controller = controller;
		bar.Build(parent);
		controller.MRX_GetOnLoadoutInfo().Insert(bar.OnInfo);
		controller.MRX_GetOnLoadoutResult().Insert(bar.OnResult);
		controller.MRX_RequestLoadoutInfo();
		return bar;
	}

	//------------------------------------------------------------------------------------------------
	//! Stops listening and the timers; call when the window closes.
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
		AlignableSlot.SetPadding(column, 16 + ACCENT_WIDTH, 12, 12, 14);

		m_wRows = CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, Color.FromInt(Color.WHITE), column, CONTAINER);
		AlignableSlot.SetHorizontalAlign(m_wRows, LayoutHorizontalAlign.Stretch);

		ImageWidget divider = ImageWidget.Cast(CreateWidget(WidgetType.ImageWidgetTypeID, MRX_UIStyle.GetFrameColor(), column, DECOR));
		AlignableSlot.SetHorizontalAlign(divider, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetPadding(divider, 0, 14, 0, 12);
		// One unit is less than a pixel at lower resolutions.
		divider.SetSize(1, 2);

		Widget option = CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, Color.FromInt(Color.WHITE), column, CONTAINER);
		AlignableSlot.SetHorizontalAlign(option, LayoutHorizontalAlign.Left);
		m_StashBox = MRX_CheckBox.Create(option, "#MRX-Loadout_UseStash", s_bUseStash);
		m_StashBox.GetOnChanged().Insert(OnStashBoxChanged);
		TextWidget optionHint = CreateText(option, REGULAR_FONT, DETAIL_FONT_SIZE, MRX_UIStyle.GetMutedColor());
		optionHint.SetText("#MRX-Loadout_UseStashHint");
		LayoutSlot.SetVerticalAlign(optionHint, LayoutVerticalAlign.Center);
		AlignableSlot.SetPadding(optionHint, 10, 0, 0, 0);

		// Results show here for a while; the line keeps its height, so the window does not move.
		m_wInfo = CreateText(column, BOLD_FONT, TITLE_FONT_SIZE, MRX_UIStyle.GetMutedColor());
		AlignableSlot.SetHorizontalAlign(m_wInfo, LayoutHorizontalAlign.Left);
		AlignableSlot.SetPadding(m_wInfo, 0, 10, 0, 0);
		m_wInfo.SetText(" ");
		m_wInfo.SetOpacity(0);
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
		m_aSlotRows.Clear();
		m_aIndexes.Clear();
		m_aNames.Clear();
		m_aDetails.Clear();
		for (int slot = 0; slot < count; slot++)
		{
			Widget row = CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, Color.FromInt(Color.WHITE), m_wRows, CONTAINER);
			AlignableSlot.SetHorizontalAlign(row, LayoutHorizontalAlign.Stretch);
			if (slot > 0)
				AlignableSlot.SetPadding(row, 0, 10, 0, 0);

			m_aSlotRows.Insert(row);

			// Wide enough for two digits, so the names line up.
			SizeLayoutWidget indexSize = SizeLayoutWidget.Cast(CreateWidget(WidgetType.SizeLayoutWidgetTypeID, Color.FromInt(Color.WHITE), row, DECOR));
			indexSize.EnableWidthOverride(true);
			indexSize.SetWidthOverride(INDEX_WIDTH);
			LayoutSlot.SetVerticalAlign(indexSize, LayoutVerticalAlign.Center);
			TextWidget index = CreateText(indexSize, BOLD_FONT, INDEX_FONT_SIZE, MRX_UIStyle.GetAccentColor());
			index.SetText((slot + 1).ToString());
			AlignableSlot.SetVerticalAlign(index, LayoutVerticalAlign.Center);
			m_aIndexes.Insert(index);

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
	protected void OnInfo(MRX_ELoadoutStatus status, string currency, array<string> mainItems, array<int> itemCounts, array<int> nets, array<int> unavailable, array<int> stashNets, array<int> stashUnavailable)
	{
		if (!m_wRows)
			return;

		m_wBar.SetVisible(status == MRX_ELoadoutStatus.OK && !mainItems.IsEmpty());
		if (status != MRX_ELoadoutStatus.OK)
			return;

		m_sCurrency = currency;
		m_aMainItems.Copy(mainItems);
		m_aItemCounts.Copy(itemCounts);
		m_aNets.Copy(nets);
		m_aUnavailable.Copy(unavailable);
		m_aStashNets.Copy(stashNets);
		m_aStashUnavailable.Copy(stashUnavailable);
		if (m_aMainItems.Count() != m_aSaveButtons.Count())
			BuildRows(m_aMainItems.Count());

		UpdateSlots();
	}

	//------------------------------------------------------------------------------------------------
	//! Shows the slots as last reported, priced with or without the stash as the check box says. Locked slots are grey
	//! and their buttons off.
	protected void UpdateSlots()
	{
		for (int slot = 0, count = m_aMainItems.Count(); slot < count; slot++)
		{
			bool locked = m_aItemCounts[slot] == MRX_LoadoutInfo.LOCKED;
			// The whole row, buttons included.
			if (locked)
				m_aSlotRows[slot].SetOpacity(LOCKED_OPACITY);
			else
				m_aSlotRows[slot].SetOpacity(1);

			m_aSaveButtons[slot].SetEnabled(!locked);
			if (locked)
			{
				m_aIndexes[slot].SetColor(MRX_UIStyle.GetMutedColor());
				m_aNames[slot].SetText("#MRX-Loadout_Locked");
				m_aNames[slot].SetColor(MRX_UIStyle.GetMutedColor());
				m_aDetails[slot].SetText("#MRX-Loadout_LockedHint");
				m_aLoadButtons[slot].SetLabel("#MRX-Loadout_Load");
				m_aLoadButtons[slot].SetEnabled(false);
				continue;
			}

			m_aIndexes[slot].SetColor(MRX_UIStyle.GetAccentColor());
			bool saved = !m_aMainItems[slot].IsEmpty();
			if (!saved)
			{
				m_aNames[slot].SetText("#MRX-Loadout_Empty");
				m_aNames[slot].SetColor(MRX_UIStyle.GetMutedColor());
				m_aDetails[slot].SetText("#MRX-Loadout_EmptyHint");
				m_aLoadButtons[slot].SetLabel("#MRX-Loadout_Load");
				m_aLoadButtons[slot].SetEnabled(false);
				continue;
			}

			int net = m_aNets[slot];
			int unavailable = m_aUnavailable[slot];
			if (s_bUseStash && slot < m_aStashNets.Count() && slot < m_aStashUnavailable.Count())
			{
				net = m_aStashNets[slot];
				unavailable = m_aStashUnavailable[slot];
			}

			m_aNames[slot].SetText(MRX_ScriptedDialog.GetItemDisplayName(m_aMainItems[slot]));
			m_aNames[slot].SetColor(Color.FromInt(Color.WHITE));
			string detail = WidgetManager.Translate("#MRX-Loadout_Items", m_aItemCounts[slot]);
			if (unavailable > 0)
				detail += WidgetManager.Translate("#MRX-Loadout_NotForSale", unavailable);

			m_aDetails[slot].SetText(detail);
			m_aLoadButtons[slot].SetLabel(GetLoadLabel(net));
			m_aLoadButtons[slot].SetEnabled(true);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnStashBoxChanged(MRX_CheckBox box)
	{
		s_bUseStash = box.IsChecked();
		UpdateSlots();
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
	//! Asks the server to save the gear into the slot (0-based), as holding its Save button does.
	void RequestSave(int slot)
	{
		if (!m_Controller)
			return;

		m_bLastLoad = false;
		m_Controller.MRX_RequestLoadoutSave(slot);
		ShowInfo(WidgetManager.Translate("#MRX-Loadout_Saving", slot + 1), false);
	}

	//------------------------------------------------------------------------------------------------
	//! Asks the server to put on the slot's loadout (0-based), as holding its Load button does.
	void RequestLoad(int slot)
	{
		if (!m_Controller)
			return;

		m_bLastLoad = true;
		m_Controller.MRX_RequestLoadoutLoad(slot, s_bUseStash);
		ShowInfo(WidgetManager.Translate("#MRX-Loadout_PuttingOn", slot + 1), false);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnHeld(MRX_HoldButton button)
	{
		int slot = m_aSaveButtons.Find(button);
		if (slot >= 0)
		{
			RequestSave(slot);
			return;
		}

		slot = m_aLoadButtons.Find(button);
		if (slot >= 0)
			RequestLoad(slot);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnResult(MRX_ELoadoutStatus status, MRX_ETxStatus txStatus, int slot, int net, string currency, int unavailable, int fromStash, int stored)
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

				// Items left out (not for sale) show in the slot's line.
				if (fromStash > 0 && stored > 0)
					text += " " + WidgetManager.Translate("#MRX-Loadout_StashUse", fromStash, stored);
				else if (fromStash > 0)
					text += " " + WidgetManager.Translate("#MRX-Loadout_StashTaken", fromStash);
				else if (stored > 0)
					text += " " + WidgetManager.Translate("#MRX-Loadout_StashStored", stored);

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
	//! Shows the text for FEEDBACK_MS.
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
		GetGame().GetCallqueue().Remove(FadeOut);
		GetGame().GetCallqueue().CallLater(FadeOut, FEEDBACK_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void FadeOut()
	{
		if (m_wInfo)
			AnimateWidget.Opacity(m_wInfo, 0, 2);
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
