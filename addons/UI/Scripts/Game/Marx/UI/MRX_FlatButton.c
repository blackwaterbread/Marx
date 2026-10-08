void MRX_FlatButtonDelegate(MRX_FlatButton button);
typedef func MRX_FlatButtonDelegate;

//! Clickable box in the Marx look (API v0): frame, dark fill that lights up under the cursor, bold label. A selected
//! button (tabs, filters) is filled with the accent colour. Mouse only. Client.
class MRX_FlatButton : Managed
{
	protected ButtonWidget m_wButton;
	protected Widget m_wFrame;
	protected Widget m_wFill;
	//! Label and icon, centred in the button.
	protected Widget m_wContent;
	protected TextWidget m_wLabel;
	protected ImageWidget m_wIcon;
	protected ref MRX_FlatButtonHandler m_Handler;
	protected ref ScriptInvokerBase<MRX_FlatButtonDelegate> m_OnClicked;
	protected bool m_bEnabled = true;
	protected bool m_bSelected;
	protected bool m_bHovered;
	//! Free use by the owner, e.g. the action the button stands for.
	string m_sAction;

	//------------------------------------------------------------------------------------------------
	//! \param minWidth In reference pixels, 0 to fit the label.
	static MRX_FlatButton Create(notnull Widget parent, string label, float minWidth = 0, float height = 32, int fontSize = 15)
	{
		MRX_FlatButton button = new MRX_FlatButton();
		button.Build(parent, label, minWidth, height, fontSize);
		return button;
	}

	//------------------------------------------------------------------------------------------------
	ScriptInvokerBase<MRX_FlatButtonDelegate> GetOnClicked()
	{
		if (!m_OnClicked)
			m_OnClicked = new ScriptInvokerBase<MRX_FlatButtonDelegate>();

		return m_OnClicked;
	}

	//------------------------------------------------------------------------------------------------
	Widget GetRootWidget()
	{
		return m_wButton;
	}

	//------------------------------------------------------------------------------------------------
	void SetLabel(string label)
	{
		m_wLabel.SetText(label);
	}

	//------------------------------------------------------------------------------------------------
	//! Shows an icon after the label, e.g. an arrow for a button that opens something.
	void SetTrailingIcon(ResourceName imageset, string image, float size = 16)
	{
		if (!m_wContent)
			return;

		if (!m_wIcon)
		{
			// Blended, so the icon's transparent parts stay transparent.
			int flags = MRX_UIStyle.DECOR | WidgetFlags.BLEND | WidgetFlags.STRETCH;
			m_wIcon = ImageWidget.Cast(MRX_UIStyle.CreateWidget(WidgetType.ImageWidgetTypeID, Color.FromInt(Color.WHITE), m_wContent, flags));
			LayoutSlot.SetVerticalAlign(m_wIcon, LayoutVerticalAlign.Center);
			AlignableSlot.SetPadding(m_wIcon, 8, 0, 0, 0);
		}

		m_wIcon.LoadImageFromSet(0, imageset, image);
		m_wIcon.SetSize(size, size);
		UpdateLook();
	}

	//------------------------------------------------------------------------------------------------
	void SetEnabled(bool enabled)
	{
		m_bEnabled = enabled;
		UpdateLook();
	}

	//------------------------------------------------------------------------------------------------
	bool IsEnabled()
	{
		return m_bEnabled;
	}

	//------------------------------------------------------------------------------------------------
	void SetSelected(bool selected)
	{
		m_bSelected = selected;
		UpdateLook();
	}

	//------------------------------------------------------------------------------------------------
	bool IsSelected()
	{
		return m_bSelected;
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by MRX_FlatButtonHandler.
	bool OnClicked()
	{
		if (!m_bEnabled)
			return false;

		SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.CLICK);
		if (m_OnClicked)
			m_OnClicked.Invoke(this);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by MRX_FlatButtonHandler.
	void OnHover(bool hovered)
	{
		m_bHovered = hovered;
		UpdateLook();
	}

	//------------------------------------------------------------------------------------------------
	protected void Build(notnull Widget parent, string label, float minWidth, float height, int fontSize)
	{
		m_Handler = new MRX_FlatButtonHandler();
		m_Handler.m_Button = this;

		m_wButton = ButtonWidget.Cast(MRX_UIStyle.CreateWidget(WidgetType.ButtonWidgetTypeID, Color.FromSRGBA(0, 0, 0, 0), parent, WidgetFlags.VISIBLE | WidgetFlags.NOFOCUS | WidgetFlags.INHERIT_CLIPPING));
		m_wButton.AddHandler(m_Handler);

		SizeLayoutWidget size = SizeLayoutWidget.Cast(MRX_UIStyle.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, Color.FromInt(Color.WHITE), m_wButton));
		MRX_UIStyle.Stretch(size);
		size.EnableHeightOverride(true);
		size.SetHeightOverride(height);
		if (minWidth > 0)
		{
			size.EnableMinDesiredWidth(true);
			size.SetMinDesiredWidth(minWidth);
		}

		Widget box = MRX_UIStyle.CreateWidget(WidgetType.OverlayWidgetTypeID, Color.FromInt(Color.WHITE), size);
		MRX_UIStyle.Stretch(box);
		m_wFrame = MRX_UIStyle.CreateWidget(WidgetType.ImageWidgetTypeID, MRX_UIStyle.GetFrameColor(), box);
		MRX_UIStyle.Stretch(m_wFrame);
		m_wFill = MRX_UIStyle.CreateWidget(WidgetType.ImageWidgetTypeID, MRX_UIStyle.GetFillColor(), box);
		MRX_UIStyle.Stretch(m_wFill);
		AlignableSlot.SetPadding(m_wFill, 1, 1, 1, 1);

		m_wContent = MRX_UIStyle.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, Color.FromInt(Color.WHITE), box);
		AlignableSlot.SetHorizontalAlign(m_wContent, LayoutHorizontalAlign.Center);
		AlignableSlot.SetVerticalAlign(m_wContent, LayoutVerticalAlign.Center);
		AlignableSlot.SetPadding(m_wContent, 14, 0, 14, 0);
		m_wLabel = MRX_UIStyle.CreateText(m_wContent, label, fontSize, Color.FromInt(Color.WHITE), true);
		LayoutSlot.SetVerticalAlign(m_wLabel, LayoutVerticalAlign.Center);
		UpdateLook();
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateLook()
	{
		if (!m_wFill)
			return;

		Color fill = MRX_UIStyle.GetFillColor();
		Color frame = MRX_UIStyle.GetFrameColor();
		Color text = Color.FromInt(Color.WHITE);
		if (m_bSelected)
		{
			fill = MRX_UIStyle.GetAccentColor();
			frame = MRX_UIStyle.GetAccentColor();
			text = MRX_UIStyle.GetOnAccentColor();
		}
		else if (m_bHovered && m_bEnabled)
		{
			fill = MRX_UIStyle.GetHoverColor();
			frame = MRX_UIStyle.GetAccentColor();
		}

		m_wFill.SetColor(fill);
		m_wFrame.SetColor(frame);
		m_wLabel.SetColor(text);
		float opacity = 1;
		if (!m_bEnabled)
			opacity = 0.35;

		m_wLabel.SetOpacity(opacity);
		if (m_wIcon)
		{
			m_wIcon.SetColor(text);
			m_wIcon.SetOpacity(opacity);
		}
	}
}

//------------------------------------------------------------------------------------------------
//! Passes the clicks and hovers of a flat button to it. Internal.
class MRX_FlatButtonHandler : ScriptedWidgetEventHandler
{
	//! Weak: the button owns this handler.
	MRX_FlatButton m_Button;

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		return m_Button && m_Button.OnClicked();
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
