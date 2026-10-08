void MRX_CheckBoxDelegate(MRX_CheckBox box);
typedef func MRX_CheckBoxDelegate;

//! Check box in the Marx look (API v0): a small framed square, filled with the accent colour when checked, and a label.
//! A click on either toggles it. Mouse only. Client.
class MRX_CheckBox : Managed
{
	protected static const float BOX_SIZE = 16;

	protected ButtonWidget m_wButton;
	protected Widget m_wFrame;
	protected Widget m_wMark;
	protected TextWidget m_wLabel;
	protected ref MRX_CheckBoxHandler m_Handler;
	protected ref ScriptInvokerBase<MRX_CheckBoxDelegate> m_OnChanged;
	protected bool m_bChecked;
	protected bool m_bHovered;

	//------------------------------------------------------------------------------------------------
	static MRX_CheckBox Create(notnull Widget parent, string label, bool checked, int fontSize = 14)
	{
		MRX_CheckBox box = new MRX_CheckBox();
		box.m_bChecked = checked;
		box.Build(parent, label, fontSize);
		return box;
	}

	//------------------------------------------------------------------------------------------------
	//! Invoked when a click changed the state.
	ScriptInvokerBase<MRX_CheckBoxDelegate> GetOnChanged()
	{
		if (!m_OnChanged)
			m_OnChanged = new ScriptInvokerBase<MRX_CheckBoxDelegate>();

		return m_OnChanged;
	}

	//------------------------------------------------------------------------------------------------
	Widget GetRootWidget()
	{
		return m_wButton;
	}

	//------------------------------------------------------------------------------------------------
	bool IsChecked()
	{
		return m_bChecked;
	}

	//------------------------------------------------------------------------------------------------
	//! Sets the state without invoking GetOnChanged.
	void SetChecked(bool checked)
	{
		m_bChecked = checked;
		UpdateLook();
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by MRX_CheckBoxHandler.
	bool OnClicked()
	{
		m_bChecked = !m_bChecked;
		UpdateLook();
		SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.CLICK);
		if (m_OnChanged)
			m_OnChanged.Invoke(this);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by MRX_CheckBoxHandler.
	void OnHover(bool hovered)
	{
		m_bHovered = hovered;
		UpdateLook();
	}

	//------------------------------------------------------------------------------------------------
	protected void Build(notnull Widget parent, string label, int fontSize)
	{
		m_Handler = new MRX_CheckBoxHandler();
		m_Handler.m_Box = this;

		m_wButton = ButtonWidget.Cast(MRX_UIStyle.CreateWidget(WidgetType.ButtonWidgetTypeID, Color.FromSRGBA(0, 0, 0, 0), parent, WidgetFlags.VISIBLE | WidgetFlags.NOFOCUS | WidgetFlags.INHERIT_CLIPPING));
		m_wButton.AddHandler(m_Handler);

		Widget row = MRX_UIStyle.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, Color.FromInt(Color.WHITE), m_wButton);
		MRX_UIStyle.Stretch(row);

		SizeLayoutWidget size = SizeLayoutWidget.Cast(MRX_UIStyle.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, Color.FromInt(Color.WHITE), row));
		LayoutSlot.SetVerticalAlign(size, LayoutVerticalAlign.Center);
		size.EnableWidthOverride(true);
		size.SetWidthOverride(BOX_SIZE);
		size.EnableHeightOverride(true);
		size.SetHeightOverride(BOX_SIZE);

		Widget square = MRX_UIStyle.CreateWidget(WidgetType.OverlayWidgetTypeID, Color.FromInt(Color.WHITE), size);
		MRX_UIStyle.Stretch(square);
		m_wFrame = MRX_UIStyle.CreateWidget(WidgetType.ImageWidgetTypeID, MRX_UIStyle.GetFrameColor(), square);
		MRX_UIStyle.Stretch(m_wFrame);
		Widget fill = MRX_UIStyle.CreateWidget(WidgetType.ImageWidgetTypeID, MRX_UIStyle.GetFillColor(), square);
		MRX_UIStyle.Stretch(fill);
		AlignableSlot.SetPadding(fill, 1, 1, 1, 1);
		m_wMark = MRX_UIStyle.CreateWidget(WidgetType.ImageWidgetTypeID, MRX_UIStyle.GetAccentColor(), square);
		MRX_UIStyle.Stretch(m_wMark);
		AlignableSlot.SetPadding(m_wMark, 4, 4, 4, 4);

		m_wLabel = MRX_UIStyle.CreateText(row, label, fontSize, Color.FromInt(Color.WHITE), true);
		LayoutSlot.SetVerticalAlign(m_wLabel, LayoutVerticalAlign.Center);
		AlignableSlot.SetPadding(m_wLabel, 8, 0, 0, 0);
		UpdateLook();
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateLook()
	{
		if (!m_wFrame)
			return;

		if (m_bHovered || m_bChecked)
			m_wFrame.SetColor(MRX_UIStyle.GetAccentColor());
		else
			m_wFrame.SetColor(MRX_UIStyle.GetFrameColor());

		m_wMark.SetVisible(m_bChecked);
	}
}

//------------------------------------------------------------------------------------------------
//! Passes the clicks and hovers of a check box to it. Internal.
class MRX_CheckBoxHandler : ScriptedWidgetEventHandler
{
	//! Weak: the check box owns this handler.
	MRX_CheckBox m_Box;

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		return m_Box && m_Box.OnClicked();
	}

	//------------------------------------------------------------------------------------------------
	override bool OnMouseEnter(Widget w, int x, int y)
	{
		if (m_Box)
			m_Box.OnHover(true);

		return false;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
	{
		if (m_Box)
			m_Box.OnHover(false);

		return false;
	}
}
