//! Default wallet HUD (API v0): balances of the local player in the top right corner, with a short change hint.
//! Script-built widgets under the vanilla HUD root, so it follows the HUD visibility. Client side.
//! Mods with their own HUD call MRX_WalletHud.SetEnabled(false) and read MRX_ClientWallet instead.
class MRX_WalletHud
{
	static const ResourceName FONT = "{3E7733BAC8C831F6}UI/Fonts/RobotoCondensed/RobotoCondensed_Regular.fnt";
	protected static const int FONT_SIZE = 22;
	protected static const int CHANGE_DISPLAY_MS = 3000;
	protected static const float MARGIN = 24;

	protected static bool s_bDisabled;

	//! Weak: the wallet belongs to the player controller that owns this HUD.
	protected MRX_ClientWallet m_Wallet;
	protected Widget m_wRoot;
	protected ref map<string, TextWidget> m_mLines = new map<string, TextWidget>();
	protected ref map<string, int> m_mShownBalances = new map<string, int>();
	protected ref map<string, int> m_mChanges = new map<string, int>();
	protected ref map<string, int> m_mChangeExpiry = new map<string, int>();

	//------------------------------------------------------------------------------------------------
	static void SetEnabled(bool enabled)
	{
		s_bDisabled = !enabled;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsEnabled()
	{
		return !s_bDisabled;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Null when the HUD is disabled or the vanilla HUD root does not exist yet.
	static MRX_WalletHud Create(notnull MRX_ClientWallet wallet)
	{
		if (s_bDisabled)
			return null;

		SCR_HUDManagerComponent hudManager = SCR_HUDManagerComponent.GetHUDManager();
		if (!hudManager || !hudManager.GetHUDRootWidget())
			return null;

		MRX_WalletHud hud = new MRX_WalletHud();
		hud.Init(wallet, hudManager.GetHUDRootWidget());
		return hud;
	}

	//------------------------------------------------------------------------------------------------
	protected void Init(MRX_ClientWallet wallet, Widget parent)
	{
		m_Wallet = wallet;
		m_wRoot = GetGame().GetWorkspace().CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 0, parent);
		FrameSlot.SetAnchorMin(m_wRoot, 1, 0);
		FrameSlot.SetAnchorMax(m_wRoot, 1, 0);
		FrameSlot.SetAlignment(m_wRoot, 1, 0);
		FrameSlot.SetPos(m_wRoot, -MARGIN, MARGIN);
		FrameSlot.SetSizeToContent(m_wRoot, true);

		array<string> currencies = {};
		wallet.GetCurrencies(currencies);
		foreach (string currency : currencies)
		{
			int balance;
			wallet.TryGetBalance(currency, balance);
			m_mShownBalances.Set(currency, balance);
		}

		wallet.GetOnBalanceChanged().Insert(OnBalanceChanged);
		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	void ~MRX_WalletHud()
	{
		if (m_Wallet)
			m_Wallet.GetOnBalanceChanged().Remove(OnBalanceChanged);

		ScriptCallQueue callQueue = GetGame().GetCallqueue();
		if (callQueue)
			callQueue.Remove(Refresh);

		if (m_wRoot)
			m_wRoot.RemoveFromHierarchy();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBalanceChanged(string currency, int balance)
	{
		int previous;
		if (m_mShownBalances.Find(currency, previous) && previous != balance)
		{
			m_mChanges.Set(currency, balance - previous);
			m_mChangeExpiry.Set(currency, System.GetTickCount() + CHANGE_DISPLAY_MS);
			GetGame().GetCallqueue().CallLater(Refresh, CHANGE_DISPLAY_MS + 50);
		}

		m_mShownBalances.Set(currency, balance);
		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	protected void Refresh()
	{
		if (!m_wRoot)
			return;

		int now = System.GetTickCount();
		foreach (string currency, int balance : m_mShownBalances)
		{
			string text = MRX_TextFormat.Money(balance, currency);
			int expiry;
			if (m_mChangeExpiry.Find(currency, expiry) && now < expiry)
				text += FormatChange(m_mChanges.Get(currency), currency);

			GetLine(currency).SetText(text);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected TextWidget GetLine(string currency)
	{
		TextWidget line = m_mLines.Get(currency);
		if (line)
			return line;

		line = TextWidget.Cast(GetGame().GetWorkspace().CreateWidget(WidgetType.TextWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 0, m_wRoot));
		line.SetFont(FONT);
		line.SetExactFontSize(FONT_SIZE);
		line.SetShadow(2);
		m_mLines.Set(currency, line);
		return line;
	}

	//------------------------------------------------------------------------------------------------
	protected static string FormatChange(int change, string currency)
	{
		if (change > 0)
			return string.Format("  (+%1)", MRX_TextFormat.Money(change, currency));

		return string.Format("  (%1)", MRX_TextFormat.Money(change, currency));
	}
}
