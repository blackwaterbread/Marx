//! Balance panel below the items of a Marx arsenal: a dark box with a thin frame and an accent stripe, a small coloured
//! "BALANCE" title and the balance in large bold letters. Trades show here too: the balance counts to its new value, the
//! change ("-$20", "+$510") lights up next to it, and failures or details of a sale show in a line below. Internal.
class MRX_ArsenalBalanceBar : Managed
{
	//! Vertical layout of a storage panel: header, title, item grid, pages.
	protected static const string PANEL_CONTAINER = "Container";
	protected static const ResourceName BOLD_FONT = "{EABA4FE9D014CCEF}UI/Fonts/RobotoCondensed/RobotoCondensed_Bold.fnt";
	protected static const int TITLE_FONT_SIZE = 15;
	protected static const int AMOUNT_FONT_SIZE = 30;
	protected static const int CHANGE_FONT_SIZE = 20;
	protected static const int INFO_FONT_SIZE = 15;
	protected static const float ACCENT_WIDTH = 4;
	protected static const int COUNT_STEPS = 12;
	protected static const int COUNT_STEP_MS = 25;
	protected static const int FEEDBACK_MS = 3000;
	protected static const float FADE_SPEED = 2;

	protected MRX_ArsenalShopComponent m_Arsenal;
	protected TextWidget m_wAmount;
	protected TextWidget m_wChange;
	protected TextWidget m_wInfo;
	//! Balances as last received, by currency.
	protected ref map<string, int> m_mBalances = new map<string, int>();
	//! Balances as currently shown while counting.
	protected ref map<string, int> m_mShown = new map<string, int>();
	protected ref map<string, int> m_mCountFrom = new map<string, int>();
	protected int m_iCountStep = -1;

	//------------------------------------------------------------------------------------------------
	//! Adds the panel below the item grid and pages of a storage panel.
	//! \return Null when the panel has an unexpected layout.
	static MRX_ArsenalBalanceBar Create(Widget panelRoot, notnull MRX_ArsenalShopComponent arsenal)
	{
		if (!panelRoot)
			return null;

		Widget container = panelRoot.FindAnyWidget(PANEL_CONTAINER);
		if (!container)
			return null;

		MRX_ArsenalBalanceBar bar = new MRX_ArsenalBalanceBar();
		bar.m_Arsenal = arsenal;
		bar.Build(container);
		bar.ReadBalances();
		bar.UpdateAmount();
		return bar;
	}

	//------------------------------------------------------------------------------------------------
	TextWidget GetAmountText()
	{
		return m_wAmount;
	}

	//------------------------------------------------------------------------------------------------
	//! A balance changed: count to the new value and show the change.
	void OnBalanceChanged(string currency, int balance)
	{
		int previous;
		bool known = m_mBalances.Find(currency, previous);
		m_mBalances.Set(currency, balance);
		if (!known || balance == previous || !m_Arsenal.GetCurrencies().Contains(currency))
		{
			m_mShown.Set(currency, balance);
			UpdateAmount();
			return;
		}

		ShowChange(balance - previous, currency);
		StartCount();
	}

	//------------------------------------------------------------------------------------------------
	//! A line below the balance for a few seconds, red for errors.
	void ShowInfo(string text, bool error)
	{
		if (!m_wInfo)
			return;

		m_wInfo.SetText(text);
		if (error)
			m_wInfo.SetColor(GetDecreaseColor());
		else
			m_wInfo.SetColor(GetInfoColor());

		Flash(m_wInfo);
	}

	//------------------------------------------------------------------------------------------------
	//! Stops the timers; call when the panel closes.
	void Stop()
	{
		GetGame().GetCallqueue().Remove(CountStep);
		GetGame().GetCallqueue().Remove(FadeOut);
	}

	//------------------------------------------------------------------------------------------------
	protected void Build(notnull Widget container)
	{
		Widget bar = CreateWidget(WidgetType.OverlayWidgetTypeID, Color.FromInt(Color.WHITE), container);
		AlignableSlot.SetHorizontalAlign(bar, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetPadding(bar, 0, 8, 0, 0);

		Stretch(CreateWidget(WidgetType.ImageWidgetTypeID, GetFrameColor(), bar));
		Widget fill = CreateWidget(WidgetType.ImageWidgetTypeID, GetFillColor(), bar);
		Stretch(fill);
		AlignableSlot.SetPadding(fill, 1, 1, 1, 1);
		ImageWidget accent = ImageWidget.Cast(CreateWidget(WidgetType.ImageWidgetTypeID, GetAccentColor(), bar));
		AlignableSlot.SetHorizontalAlign(accent, LayoutHorizontalAlign.Left);
		AlignableSlot.SetVerticalAlign(accent, LayoutVerticalAlign.Stretch);
		accent.SetSize(ACCENT_WIDTH, 1);

		Widget column = CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, Color.FromInt(Color.WHITE), bar);
		AlignableSlot.SetHorizontalAlign(column, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetPadding(column, 16 + ACCENT_WIDTH, 8, 16, 10);

		CreateText(column, TITLE_FONT_SIZE, GetAccentColor()).SetText("BALANCE");

		Widget row = CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, Color.FromInt(Color.WHITE), column);
		m_wAmount = CreateText(row, AMOUNT_FONT_SIZE, Color.FromInt(Color.WHITE));
		m_wChange = CreateText(row, CHANGE_FONT_SIZE, GetIncreaseColor());
		AlignableSlot.SetVerticalAlign(m_wChange, LayoutVerticalAlign.Center);
		AlignableSlot.SetPadding(m_wChange, 12, 0, 0, 0);
		m_wChange.SetVisible(false);

		m_wInfo = CreateText(column, INFO_FONT_SIZE, GetInfoColor());
		m_wInfo.SetVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	protected void ReadBalances()
	{
		MRX_ClientWallet wallet = MRX_ClientWallet.GetLocal();
		foreach (string currency : m_Arsenal.GetCurrencies())
		{
			int balance;
			if (wallet && wallet.TryGetBalance(currency, balance))
				m_mBalances.Set(currency, balance);

			m_mShown.Set(currency, balance);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowChange(int change, string currency)
	{
		if (!m_wChange)
			return;

		string text = MRX_TextFormat.Money(change, currency);
		if (change > 0)
		{
			text = "+" + text;
			m_wChange.SetColor(GetIncreaseColor());
		}
		else
		{
			m_wChange.SetColor(GetDecreaseColor());
		}

		m_wChange.SetText(text);
		Flash(m_wChange);
	}

	//------------------------------------------------------------------------------------------------
	//! Shows the widget fully, then fades it out after FEEDBACK_MS (a new call restarts the time).
	protected void Flash(notnull Widget widget)
	{
		AnimateWidget.StopAllAnimations(widget);
		widget.SetOpacity(1);
		widget.SetVisible(true);
		GetGame().GetCallqueue().Remove(FadeOut);
		GetGame().GetCallqueue().CallLater(FadeOut, FEEDBACK_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void FadeOut()
	{
		if (m_wChange && m_wChange.IsVisible())
			AnimateWidget.Opacity(m_wChange, 0, FADE_SPEED, true);

		if (m_wInfo && m_wInfo.IsVisible())
			AnimateWidget.Opacity(m_wInfo, 0, FADE_SPEED, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void StartCount()
	{
		m_mCountFrom.Clear();
		foreach (string currency, int shown : m_mShown)
		{
			m_mCountFrom.Set(currency, shown);
		}

		m_iCountStep = 0;
		GetGame().GetCallqueue().Remove(CountStep);
		GetGame().GetCallqueue().CallLater(CountStep, COUNT_STEP_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void CountStep()
	{
		m_iCountStep++;
		float progress = Math.Min(1, m_iCountStep * 1.0 / COUNT_STEPS);
		foreach (string currency, int target : m_mBalances)
		{
			int from = target;
			m_mCountFrom.Find(currency, from);
			int counted = Math.Round((target - from) * progress);
			m_mShown.Set(currency, from + counted);
		}

		UpdateAmount();
		if (progress >= 1)
			GetGame().GetCallqueue().Remove(CountStep);
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateAmount()
	{
		if (!m_wAmount)
			return;

		string text;
		foreach (string currency : m_Arsenal.GetCurrencies())
		{
			int shown;
			m_mShown.Find(currency, shown);
			if (!text.IsEmpty())
				text += "    ";

			text += MRX_TextFormat.Money(shown, currency);
		}

		m_wAmount.SetText(text);
	}

	//------------------------------------------------------------------------------------------------
	protected static Widget CreateWidget(WidgetType type, Color color, Widget parent)
	{
		int flags = WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS;
		return GetGame().GetWorkspace().CreateWidget(type, flags, color, 0, parent);
	}

	//------------------------------------------------------------------------------------------------
	protected static TextWidget CreateText(Widget parent, int size, Color color)
	{
		TextWidget text = TextWidget.Cast(CreateWidget(WidgetType.TextWidgetTypeID, color, parent));
		text.SetFont(BOLD_FONT);
		text.SetExactFontSize(size);
		return text;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Stretch(notnull Widget widget)
	{
		AlignableSlot.SetHorizontalAlign(widget, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetVerticalAlign(widget, LayoutVerticalAlign.Stretch);
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
	protected static Color GetIncreaseColor()
	{
		return Color.FromSRGBA(120, 205, 100, 255);
	}

	//------------------------------------------------------------------------------------------------
	protected static Color GetDecreaseColor()
	{
		return Color.FromSRGBA(235, 90, 75, 255);
	}

	//------------------------------------------------------------------------------------------------
	protected static Color GetInfoColor()
	{
		return Color.FromSRGBA(200, 204, 210, 255);
	}
}
