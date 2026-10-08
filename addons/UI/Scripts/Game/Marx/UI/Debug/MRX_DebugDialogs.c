//! Windows of the "UI" debug actions: sample MRX_ScriptedDialog content and the Marx widget gallery. English only,
//! they are development tools. Client.

//------------------------------------------------------------------------------------------------
//! An MRX_ScriptedDialog with sample content of one kind (KINDS).
class MRX_DebugSampleDialog : MRX_ScriptedDialog
{
	static const string KIND_BASIC = "basic";
	static const string KIND_SCROLL = "scroll";
	static const string KIND_ITEMS = "items";
	static const string KINDS = "basic, scroll, items";
	protected static const int SCROLL_ROWS = 30;
	protected static const float SCROLL_HEIGHT = 300;
	protected static const float ITEM_SIZE = 96;

	protected string m_sKind;

	//------------------------------------------------------------------------------------------------
	static bool IsKind(string kind)
	{
		return kind == KIND_BASIC || kind == KIND_SCROLL || kind == KIND_ITEMS;
	}

	//------------------------------------------------------------------------------------------------
	static void Open(string kind)
	{
		MRX_DebugSampleDialog dialog = new MRX_DebugSampleDialog();
		dialog.m_sKind = kind;
		ResourceName layout = DIALOG_LAYOUT_MEDIUM;
		if (kind == KIND_BASIC)
			layout = DIALOG_LAYOUT;

		OpenDialog(dialog, "Sample dialog: " + kind, "MRX_DebugSample", layout);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnDialogOpened()
	{
		AddHeaderLine("Header line (AddHeaderLine)");
		if (!m_wRows)
			return;

		if (m_sKind == KIND_SCROLL)
		{
			BuildScroll();
			return;
		}

		if (m_sKind == KIND_ITEMS)
		{
			BuildItems();
			return;
		}

		AddRow("Row with a button (AddRow)", "Press", "press");
		AddRow("Row without a button");
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnRowAction(string action)
	{
		AddHeaderLine(string.Format("Row action '%1'", action));
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildScroll()
	{
		VerticalLayoutWidget list = CreateScrollList(m_wRows, SCROLL_HEIGHT);
		if (!list)
			return;

		for (int i = 1; i <= SCROLL_ROWS; i++)
		{
			CreateText(list, string.Format("Row %1 of %2 (CreateScrollList)", i, SCROLL_ROWS));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildItems()
	{
		array<ResourceName> prefabs = {
			"{61D4F80E49BF9B12}Prefabs/Items/Equipment/Compass/Compass_SY183.et",
			"{6FD6C96121905202}Prefabs/Items/Equipment/Watches/Watch_Vostok.et",
			"{0CF54B9A85D8E0D4}Prefabs/Items/Equipment/Binoculars/Binoculars_M22/Binoculars_M22.et",
			"{06B68C58B72EAAC6}Prefabs/Items/Equipment/Backpacks/Backpack_ALICE_Medium.et"
		};

		Widget row = CreateLayout(WidgetType.HorizontalLayoutWidgetTypeID, m_wRows);
		foreach (ResourceName prefab : prefabs)
		{
			ShowPrefabPreview(CreateItemPreview(row, ITEM_SIZE), prefab);
		}

		foreach (ResourceName prefab : prefabs)
		{
			CreateText(m_wRows, GetItemDisplayName(prefab));
		}
	}
}

//------------------------------------------------------------------------------------------------
//! The Marx widgets in their states: MRX_FlatButton, MRX_HoldButton, MRX_CheckBox and MRX_BalancePanel.
class MRX_DebugGalleryDialog : MRX_ScriptedDialog
{
	protected static const float WINDOW_WIDTH = 720;
	protected static const float BUTTON_WIDTH = 140;
	protected static const float GAP = 8;
	protected static const int RESTORE_MS = 2500;
	protected static const int FAKE_CHANGE = 510;
	protected static const string ACTION_CHANGE = "change";
	protected static const string ACTION_INFO = "info";
	protected static const string ACTION_ERROR = "error";

	protected ref array<ref MRX_FlatButton> m_aFlatButtons = {};
	protected ref array<ref MRX_HoldButton> m_aHoldButtons = {};
	protected ref array<ref MRX_CheckBox> m_aCheckBoxes = {};
	protected ref MRX_BalancePanel m_Balance;
	protected TextWidget m_wLog;

	//------------------------------------------------------------------------------------------------
	static void Open()
	{
		OpenDialog(new MRX_DebugGalleryDialog(), "Marx widgets", "MRX_DebugGallery", DIALOG_LAYOUT_MEDIUM);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnDialogOpened()
	{
		if (!m_wRows)
			return;

		SetDialogWidth(WINDOW_WIDTH);

		CreateText(m_wRows, "MRX_FlatButton: normal, selected, disabled");
		Widget row = CreateButtonRow();
		AddFlatButton(row, "Normal", string.Empty, false, true);
		AddFlatButton(row, "Selected", string.Empty, true, true);
		AddFlatButton(row, "Disabled", string.Empty, false, false);

		CreateText(m_wRows, "MRX_HoldButton: enabled (hold it), disabled");
		row = CreateButtonRow();
		AddHoldButton(row, "Hold me", true);
		AddHoldButton(row, "Disabled", false);

		CreateText(m_wRows, "MRX_CheckBox: checked, unchecked");
		row = CreateButtonRow();
		AddCheckBox(row, "Checked", true);
		AddCheckBox(row, "Unchecked", false);

		CreateText(m_wRows, "MRX_BalancePanel: the local wallet");
		m_Balance = MRX_BalancePanel.CreateIn(m_wRows, null);
		row = CreateButtonRow();
		AddFlatButton(row, "Show a change", ACTION_CHANGE, false, true);
		AddFlatButton(row, "Show info", ACTION_INFO, false, true);
		AddFlatButton(row, "Show error", ACTION_ERROR, false, true);

		m_wLog = CreateText(m_wRows, "Click, hold or toggle a widget");

		MRX_ClientWallet wallet = MRX_ClientWallet.GetLocal();
		if (wallet)
			wallet.GetOnBalanceChanged().Insert(OnBalanceChanged);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		GetGame().GetCallqueue().Remove(RestoreBalance);
		MRX_ClientWallet wallet = MRX_ClientWallet.GetLocal();
		if (wallet)
			wallet.GetOnBalanceChanged().Remove(OnBalanceChanged);

		foreach (MRX_HoldButton hold : m_aHoldButtons)
		{
			hold.Stop();
		}

		if (m_Balance)
			m_Balance.Stop();

		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------------------------
	protected Widget CreateButtonRow()
	{
		Widget row = MRX_UIStyle.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, Color.FromInt(Color.WHITE), m_wRows, MRX_UIStyle.CONTAINER);
		AlignableSlot.SetPadding(row, 0, 4, 0, 12);
		return row;
	}

	//------------------------------------------------------------------------------------------------
	protected void AddFlatButton(notnull Widget row, string label, string action, bool selected, bool enabled)
	{
		MRX_FlatButton button = MRX_FlatButton.Create(row, label, BUTTON_WIDTH);
		button.m_sAction = action;
		button.SetSelected(selected);
		button.SetEnabled(enabled);
		button.GetOnClicked().Insert(OnFlatButton);
		AlignableSlot.SetPadding(button.GetRootWidget(), 0, 0, GAP, 0);
		m_aFlatButtons.Insert(button);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddHoldButton(notnull Widget row, string label, bool enabled)
	{
		MRX_HoldButton button = MRX_HoldButton.Create(row, label, BUTTON_WIDTH);
		button.SetEnabled(enabled);
		button.GetOnHeld().Insert(OnHoldButton);
		AlignableSlot.SetPadding(button.GetRootWidget(), 0, 0, GAP, 0);
		m_aHoldButtons.Insert(button);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddCheckBox(notnull Widget row, string label, bool checked)
	{
		MRX_CheckBox box = MRX_CheckBox.Create(row, label, checked);
		box.GetOnChanged().Insert(OnCheckBox);
		AlignableSlot.SetPadding(box.GetRootWidget(), 0, 0, GAP * 2, 0);
		m_aCheckBoxes.Insert(box);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnFlatButton(MRX_FlatButton button)
	{
		Log("Clicked a flat button");
		if (!m_Balance)
			return;

		if (button.m_sAction == ACTION_CHANGE)
			ShowFakeChange();
		else if (button.m_sAction == ACTION_INFO)
			m_Balance.ShowInfo("An info line (ShowInfo)", false);
		else if (button.m_sAction == ACTION_ERROR)
			m_Balance.ShowInfo("An error line (ShowInfo)", true);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnHoldButton(MRX_HoldButton button)
	{
		Log("Held a hold button long enough");
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCheckBox(MRX_CheckBox box)
	{
		Log(string.Format("Check box is now %1", box.IsChecked()));
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBalanceChanged(string currency, int balance)
	{
		if (m_Balance)
			m_Balance.OnBalanceChanged(currency, balance);
	}

	//------------------------------------------------------------------------------------------------
	//! Shows the first currency FAKE_CHANGE higher, then the real balance again (display only).
	protected void ShowFakeChange()
	{
		string currency;
		int balance;
		if (!GetFirstBalance(currency, balance))
		{
			m_Balance.ShowInfo("No balance from the server yet", true);
			return;
		}

		m_Balance.OnBalanceChanged(currency, balance + FAKE_CHANGE);
		GetGame().GetCallqueue().Remove(RestoreBalance);
		GetGame().GetCallqueue().CallLater(RestoreBalance, RESTORE_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void RestoreBalance()
	{
		string currency;
		int balance;
		if (m_Balance && GetFirstBalance(currency, balance))
			m_Balance.OnBalanceChanged(currency, balance);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool GetFirstBalance(out string currency, out int balance)
	{
		MRX_ClientWallet wallet = MRX_ClientWallet.GetLocal();
		array<string> currencies = {};
		if (!wallet || wallet.GetCurrencies(currencies) == 0)
			return false;

		currency = currencies[0];
		return wallet.TryGetBalance(currency, balance);
	}

	//------------------------------------------------------------------------------------------------
	protected void Log(string text)
	{
		if (m_wLog)
			m_wLog.SetText(text);
	}
}
