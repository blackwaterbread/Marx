void MRX_HoldButtonDelegate(MRX_HoldButton button);
typedef func MRX_HoldButtonDelegate;

//! Button that acts only when held (API v0), for actions that are costly to trigger by mistake: while the left mouse
//! button is held on it, a bar fills; releasing it or moving off before the bar is full cancels. Same look as the balance
//! panel (MRX_BalancePanel). Mouse only. Client.
class MRX_HoldButton : Managed
{
	static const int DEFAULT_HOLD_MS = 1200;
	protected static const string ACTION_MOUSE = "MouseLeft";
	protected static const int FONT_SIZE = 14;
	protected static const float HEIGHT = 30;
	protected static const int UPDATE_MS = 16;
	//! Images and texts: the cursor passes through them to the button.
	protected static const int DECOR = WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS;

	protected ButtonWidget m_wButton;
	protected Widget m_wFill;
	protected ImageWidget m_wProgress;
	protected TextWidget m_wLabel;
	protected ref MRX_HoldButtonHandler m_Handler;
	protected ref ScriptInvokerBase<MRX_HoldButtonDelegate> m_OnHeld;
	protected int m_iHoldMs;
	protected int m_iStartTick;
	protected bool m_bHolding;
	protected bool m_bHovered;
	protected bool m_bEnabled = true;

	//------------------------------------------------------------------------------------------------
	//! \param minWidth Width in reference pixels, 0 to fit the label.
	static MRX_HoldButton Create(notnull Widget parent, string label, float minWidth = 0, int holdMs = DEFAULT_HOLD_MS)
	{
		MRX_HoldButton button = new MRX_HoldButton();
		button.m_iHoldMs = Math.Max(100, holdMs);
		button.Build(parent, label, minWidth);
		return button;
	}

	//------------------------------------------------------------------------------------------------
	//! Invoked once the button was held long enough.
	ScriptInvokerBase<MRX_HoldButtonDelegate> GetOnHeld()
	{
		if (!m_OnHeld)
			m_OnHeld = new ScriptInvokerBase<MRX_HoldButtonDelegate>();

		return m_OnHeld;
	}

	//------------------------------------------------------------------------------------------------
	Widget GetRootWidget()
	{
		return m_wButton;
	}

	//------------------------------------------------------------------------------------------------
	void SetLabel(string label)
	{
		if (m_wLabel)
			m_wLabel.SetText(label);
	}

	//------------------------------------------------------------------------------------------------
	void SetEnabled(bool enabled)
	{
		m_bEnabled = enabled;
		if (!enabled)
			Cancel();

		if (m_wLabel && enabled)
			m_wLabel.SetOpacity(1);
		else if (m_wLabel)
			m_wLabel.SetOpacity(0.4);

		UpdateFill();
	}

	//------------------------------------------------------------------------------------------------
	bool IsEnabled()
	{
		return m_bEnabled;
	}

	//------------------------------------------------------------------------------------------------
	//! Stops the timer; call when the widgets go.
	void Stop()
	{
		Cancel();
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by MRX_HoldButtonHandler.
	void OnPressed()
	{
		if (!m_bEnabled || m_bHolding)
			return;

		m_bHolding = true;
		m_iStartTick = System.GetTickCount();
		GetGame().GetCallqueue().Remove(Update);
		GetGame().GetCallqueue().CallLater(Update, UPDATE_MS, true);
		SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_FE_BUTTON_HOVER);
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by MRX_HoldButtonHandler.
	void OnReleased()
	{
		Cancel();
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by MRX_HoldButtonHandler.
	void OnHover(bool hovered)
	{
		m_bHovered = hovered;
		if (!hovered)
			Cancel();

		UpdateFill();
	}

	//------------------------------------------------------------------------------------------------
	protected void Build(notnull Widget parent, string label, float minWidth)
	{
		m_Handler = new MRX_HoldButtonHandler();
		m_Handler.m_Button = this;

		m_wButton = ButtonWidget.Cast(GetGame().GetWorkspace().CreateWidget(WidgetType.ButtonWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.NOFOCUS, Color.FromSRGBA(0, 0, 0, 0), 0, parent));
		m_wButton.AddHandler(m_Handler);

		SizeLayoutWidget size = SizeLayoutWidget.Cast(CreateWidget(WidgetType.SizeLayoutWidgetTypeID, Color.FromInt(Color.WHITE), m_wButton));
		Stretch(size);
		size.EnableHeightOverride(true);
		size.SetHeightOverride(HEIGHT);
		if (minWidth > 0)
		{
			size.EnableMinDesiredWidth(true);
			size.SetMinDesiredWidth(minWidth);
		}

		Widget box = CreateWidget(WidgetType.OverlayWidgetTypeID, Color.FromInt(Color.WHITE), size);
		Stretch(box);
		Stretch(CreateWidget(WidgetType.ImageWidgetTypeID, MRX_UIStyle.GetFrameColor(), box));
		m_wFill = CreateWidget(WidgetType.ImageWidgetTypeID, MRX_UIStyle.GetFillColor(), box);
		Stretch(m_wFill);
		AlignableSlot.SetPadding(m_wFill, 1, 1, 1, 1);

		m_wProgress = ImageWidget.Cast(CreateWidget(WidgetType.ImageWidgetTypeID, GetProgressColor(), box));
		AlignableSlot.SetHorizontalAlign(m_wProgress, LayoutHorizontalAlign.Left);
		AlignableSlot.SetVerticalAlign(m_wProgress, LayoutVerticalAlign.Stretch);
		AlignableSlot.SetPadding(m_wProgress, 1, 1, 1, 1);
		m_wProgress.SetSize(0, 1);

		m_wLabel = TextWidget.Cast(CreateWidget(WidgetType.TextWidgetTypeID, Color.FromInt(Color.WHITE), box));
		m_wLabel.SetFont(MRX_UIStyle.BOLD_FONT);
		m_wLabel.SetExactFontSize(FONT_SIZE);
		m_wLabel.SetText(label);
		AlignableSlot.SetHorizontalAlign(m_wLabel, LayoutHorizontalAlign.Center);
		AlignableSlot.SetVerticalAlign(m_wLabel, LayoutVerticalAlign.Center);
		AlignableSlot.SetPadding(m_wLabel, 12, 0, 12, 0);
	}

	//------------------------------------------------------------------------------------------------
	protected void Update()
	{
		if (!m_wButton || !m_bEnabled || GetGame().GetInputManager().GetActionValue(ACTION_MOUSE) <= 0)
		{
			Cancel();
			return;
		}

		float elapsed = System.GetTickCount() - m_iStartTick;
		float progress = elapsed / m_iHoldMs;
		SetProgress(progress);
		if (progress < 1)
			return;

		Cancel();
		SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.CLICK);
		if (m_OnHeld)
			m_OnHeld.Invoke(this);
	}

	//------------------------------------------------------------------------------------------------
	protected void Cancel()
	{
		m_bHolding = false;
		GetGame().GetCallqueue().Remove(Update);
		SetProgress(0);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetProgress(float progress)
	{
		if (!m_wProgress || !m_wButton)
			return;

		float width, height;
		m_wButton.GetScreenSize(width, height);
		width = GetGame().GetWorkspace().DPIUnscale(width) - 2;
		m_wProgress.SetSize(Math.Clamp(progress, 0, 1) * Math.Max(0, width), 1);
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateFill()
	{
		if (!m_wFill)
			return;

		if (m_bHovered && m_bEnabled)
			m_wFill.SetColor(GetHoverColor());
		else
			m_wFill.SetColor(MRX_UIStyle.GetFillColor());
	}

	//------------------------------------------------------------------------------------------------
	protected static Widget CreateWidget(WidgetType type, Color color, Widget parent)
	{
		return GetGame().GetWorkspace().CreateWidget(type, DECOR, color, 0, parent);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Stretch(notnull Widget widget)
	{
		AlignableSlot.SetHorizontalAlign(widget, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetVerticalAlign(widget, LayoutVerticalAlign.Stretch);
	}

	//------------------------------------------------------------------------------------------------
	protected static Color GetHoverColor()
	{
		return Color.FromSRGBA(48, 44, 38, 250);
	}

	//------------------------------------------------------------------------------------------------
	protected static Color GetProgressColor()
	{
		return Color.FromSRGBA(226, 167, 79, 190);
	}
}

//------------------------------------------------------------------------------------------------
//! Passes the mouse events of a hold button to it. Internal.
class MRX_HoldButtonHandler : ScriptedWidgetEventHandler
{
	//! Weak: the button owns this handler.
	MRX_HoldButton m_Button;

	//------------------------------------------------------------------------------------------------
	override bool OnMouseButtonDown(Widget w, int x, int y, int button)
	{
		if (button != 0 || !m_Button)
			return false;

		m_Button.OnPressed();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnMouseButtonUp(Widget w, int x, int y, int button)
	{
		if (button == 0 && m_Button)
			m_Button.OnReleased();

		return false;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnMouseEnter(Widget w, int x, int y)
	{
		if (m_Button)
			m_Button.OnHover(true);

		return false;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
	{
		if (m_Button)
			m_Button.OnHover(false);

		return false;
	}
}
