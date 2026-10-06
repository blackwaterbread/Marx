//! Look of the Marx windows and panels (API v0): dark boxes with a thin frame and an amber accent, RobotoCondensed
//! texts. Helpers for widgets built in script. Client.
class MRX_UIStyle
{
	static const ResourceName BOLD_FONT = "{EABA4FE9D014CCEF}UI/Fonts/RobotoCondensed/RobotoCondensed_Bold.fnt";
	static const ResourceName REGULAR_FONT = "{3E7733BAC8C831F6}UI/Fonts/RobotoCondensed/RobotoCondensed_Regular.fnt";
	static const float ACCENT_WIDTH = 4;
	//! Images and texts: the cursor and the focus pass through them.
	static const int DECOR = WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS | WidgetFlags.INHERIT_CLIPPING;
	//! Containers of buttons: a container that ignores the cursor hides its children from it too.
	static const int CONTAINER = WidgetFlags.VISIBLE | WidgetFlags.INHERIT_CLIPPING;

	//------------------------------------------------------------------------------------------------
	static Widget CreateWidget(WidgetType type, Color color, Widget parent, int flags = DECOR)
	{
		return GetGame().GetWorkspace().CreateWidget(type, flags, color, 0, parent);
	}

	//------------------------------------------------------------------------------------------------
	static TextWidget CreateText(Widget parent, string text, int size, Color color, bool bold = false)
	{
		TextWidget widget = TextWidget.Cast(CreateWidget(WidgetType.TextWidgetTypeID, color, parent));
		if (bold)
			widget.SetFont(BOLD_FONT);
		else
			widget.SetFont(REGULAR_FONT);

		widget.SetExactFontSize(size);
		widget.SetText(text);
		return widget;
	}

	//------------------------------------------------------------------------------------------------
	//! Dark box with a thin frame, and an accent stripe on the left when asked.
	//! \param fill Fill colour, e.g. GetFillColor() or GetRowColor().
	//! \return The box (an overlay) to put content into; pad the content by at least 1 for the frame.
	static Widget CreateBox(notnull Widget parent, Color fill, bool accent = false, int flags = CONTAINER)
	{
		Widget box = CreateWidget(WidgetType.OverlayWidgetTypeID, Color.FromInt(Color.WHITE), parent, flags);
		Stretch(CreateWidget(WidgetType.ImageWidgetTypeID, GetFrameColor(), box));
		Widget background = CreateWidget(WidgetType.ImageWidgetTypeID, fill, box);
		Stretch(background);
		AlignableSlot.SetPadding(background, 1, 1, 1, 1);
		if (!accent)
			return box;

		ImageWidget stripe = ImageWidget.Cast(CreateWidget(WidgetType.ImageWidgetTypeID, GetAccentColor(), box));
		AlignableSlot.SetHorizontalAlign(stripe, LayoutHorizontalAlign.Left);
		AlignableSlot.SetVerticalAlign(stripe, LayoutVerticalAlign.Stretch);
		stripe.SetSize(ACCENT_WIDTH, 1);
		return box;
	}

	//------------------------------------------------------------------------------------------------
	static void Stretch(notnull Widget widget)
	{
		AlignableSlot.SetHorizontalAlign(widget, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetVerticalAlign(widget, LayoutVerticalAlign.Stretch);
	}

	//------------------------------------------------------------------------------------------------
	static Color GetAccentColor()
	{
		return Color.FromSRGBA(226, 167, 79, 255);
	}

	//------------------------------------------------------------------------------------------------
	static Color GetFrameColor()
	{
		return Color.FromSRGBA(62, 66, 72, 255);
	}

	//------------------------------------------------------------------------------------------------
	static Color GetFillColor()
	{
		return Color.FromSRGBA(20, 22, 25, 240);
	}

	//------------------------------------------------------------------------------------------------
	//! Rows of lists alternate between this and GetFillColor().
	static Color GetRowColor()
	{
		return Color.FromSRGBA(28, 31, 35, 240);
	}

	//------------------------------------------------------------------------------------------------
	static Color GetHoverColor()
	{
		return Color.FromSRGBA(90, 72, 44, 255);
	}

	//------------------------------------------------------------------------------------------------
	//! Text on accent-coloured fills.
	static Color GetOnAccentColor()
	{
		return Color.FromSRGBA(24, 20, 14, 255);
	}

	//------------------------------------------------------------------------------------------------
	static Color GetMutedColor()
	{
		return Color.FromSRGBA(160, 164, 170, 255);
	}

	//------------------------------------------------------------------------------------------------
	static Color GetIncreaseColor()
	{
		return Color.FromSRGBA(120, 205, 100, 255);
	}

	//------------------------------------------------------------------------------------------------
	static Color GetDecreaseColor()
	{
		return Color.FromSRGBA(235, 90, 75, 255);
	}
}
