//! Base of the Marx dialogs (API v0): the vanilla configurable dialog with header lines and rows of text and buttons
//! built in script, so no Marx layout asset is needed. Client side.
class MRX_ScriptedDialog : SCR_ConfigurableDialogUi
{
	static const ResourceName DIALOG_LAYOUT = "{E6B607B27BCC1477}UI/layouts/Menus/Dialogs/ConfigurableDialog.layout";
	//! Wider dialog (876 px) for content like lists.
	static const ResourceName DIALOG_LAYOUT_MEDIUM = "{604721ED8E6ED458}UI/layouts/Menus/Dialogs/ConfigurableDialog_Medium.layout";
	//! Underlined text button; can be toggled, e.g. for tabs.
	static const ResourceName BUTTON_LAYOUT = "{D27D044A8145DC7C}UI/layouts/WidgetLibrary/Buttons/WLib_ButtonTextUnderlined.layout";
	static const ResourceName BUTTON_TEXT_LAYOUT = "{75C912A1C89BE6C2}UI/layouts/WidgetLibrary/Buttons/WLib_ButtonText.layout";
	//! Vanilla dialog content with a scroll area ("SizeLayout0" > ScrollLayout > "ContentVerticalLayout").
	static const ResourceName SCROLL_LAYOUT = "{CC2566ADAD892072}UI/layouts/Menus/Dialogs/ReportDialog/ScrollMessageDialogContent.layout";
	//! Vanilla inventory slot with an item preview ("item").
	static const ResourceName ITEM_SLOT_LAYOUT = "{F437ACE2BD5F11E2}UI/layouts/Menus/Inventory/InventoryItemSlot.layout";
	static const ResourceName FONT = "{3E7733BAC8C831F6}UI/Fonts/RobotoCondensed/RobotoCondensed_Regular.fnt";
	protected static const int FONT_SIZE = 20;
	//! Widgets created in script inherit the clipping of their parent, so rows in a scroll list stay inside it.
	protected static const int WIDGET_FLAGS = WidgetFlags.VISIBLE | WidgetFlags.INHERIT_CLIPPING;

	//! Open Marx dialogs (weak: a dialog destroyed without closing, e.g. when the game ends, drops out by itself).
	//! Interactions are off meanwhile (see the modded SCR_InteractionHandlerComponent).
	protected static ref array<MRX_ScriptedDialog> s_aOpenDialogs = {};

	protected VerticalLayoutWidget m_wHeader;
	protected VerticalLayoutWidget m_wRows;

	//! Row buttons and the action string each one stands for.
	protected ref array<SCR_ButtonBaseComponent> m_aButtons = {};
	protected ref array<string> m_aButtonActions = {};

	//------------------------------------------------------------------------------------------------
	//! Opens the dialog with a Close button.
	protected static void OpenDialog(notnull MRX_ScriptedDialog dialog, string title, string tag, ResourceName layout = DIALOG_LAYOUT)
	{
		SCR_ConfigurableDialogUiPreset preset = new SCR_ConfigurableDialogUiPreset();
		preset.m_sLayout = layout;
		preset.m_sTag = tag;
		preset.m_sTitle = title;
		preset.m_aButtons = {};

		// Created with new, so the attribute defaults of the presets are not applied.
		SCR_ConfigurableDialogUiButtonPreset closeButton = new SCR_ConfigurableDialogUiButtonPreset();
		closeButton.m_sTag = BUTTON_CANCEL;
		closeButton.m_sActionName = "MenuBack";
		closeButton.m_sLabel = "#MRX-UI_Close";
		closeButton.m_sSoundHovered = SCR_SoundEvent.SOUND_FE_BUTTON_HOVER;
		closeButton.m_sSoundClicked = SCR_SoundEvent.CLICK;
		closeButton.m_bShowButton = true;
		preset.m_aButtons.Insert(closeButton);

		CreateByPreset(preset, dialog);
	}

	//------------------------------------------------------------------------------------------------
	//! \return True while a Marx dialog is open.
	static bool IsAnyOpen()
	{
		for (int i = s_aOpenDialogs.Count() - 1; i >= 0; i--)
		{
			if (!s_aOpenDialogs[i])
				s_aOpenDialogs.Remove(i);
		}

		return !s_aOpenDialogs.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		s_aOpenDialogs.RemoveItem(this);
		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnMenuOpen(SCR_ConfigurableDialogUiPreset preset)
	{
		s_aOpenDialogs.Insert(this);
		Widget container = GetContentWidget(GetRootWidget());
		if (!container)
			return;

		// The content takes the full width of the dialog.
		Widget column = CreateLayout(WidgetType.VerticalLayoutWidgetTypeID, container);
		AlignableSlot.SetHorizontalAlign(column, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetVerticalAlign(column, LayoutVerticalAlign.Stretch);
		m_wHeader = VerticalLayoutWidget.Cast(CreateLayout(WidgetType.VerticalLayoutWidgetTypeID, column));
		AlignableSlot.SetHorizontalAlign(m_wHeader, LayoutHorizontalAlign.Stretch);
		m_wRows = VerticalLayoutWidget.Cast(CreateLayout(WidgetType.VerticalLayoutWidgetTypeID, column));
		AlignableSlot.SetHorizontalAlign(m_wRows, LayoutHorizontalAlign.Stretch);
		OnDialogOpened();
	}

	//------------------------------------------------------------------------------------------------
	//! Override: build the content (header lines, rows).
	protected void OnDialogOpened()
	{
	}

	//------------------------------------------------------------------------------------------------
	//! Override: a row button was pressed.
	protected void OnRowAction(string action)
	{
	}

	//------------------------------------------------------------------------------------------------
	protected TextWidget AddHeaderLine(string text)
	{
		if (!m_wHeader)
			return null;

		return CreateText(m_wHeader, text);
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearRows()
	{
		ClearRowButtons();
		if (m_wRows)
			ClearChildren(m_wRows);
	}

	//------------------------------------------------------------------------------------------------
	//! Forgets the buttons added with AddButton (call when removing their widgets).
	protected void ClearRowButtons()
	{
		m_aButtons.Clear();
		m_aButtonActions.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected static void ClearChildren(notnull Widget parent)
	{
		Widget child = parent.GetChildren();
		while (child)
		{
			Widget next = child.GetSibling();
			child.RemoveFromHierarchy();
			child = next;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \param buttonText Empty for a row without a button.
	protected void AddRow(string label, string buttonText = string.Empty, string action = string.Empty)
	{
		if (!m_wRows)
			return;

		Widget row = CreateLayout(WidgetType.HorizontalLayoutWidgetTypeID, m_wRows);
		CreateText(row, label);
		if (!buttonText.IsEmpty())
			AddButton(row, buttonText, action, BUTTON_LAYOUT);
	}

	//------------------------------------------------------------------------------------------------
	//! Button from a widget library layout; pressing it calls OnRowAction(action).
	protected SCR_ButtonTextComponent AddButton(notnull Widget parent, string text, string action, ResourceName layout = BUTTON_TEXT_LAYOUT)
	{
		Widget buttonRoot = GetGame().GetWorkspace().CreateWidgets(layout, parent);
		if (!buttonRoot)
			return null;

		SCR_ButtonTextComponent button = SCR_ButtonTextComponent.FindButtonTextComponent(buttonRoot);
		if (!button)
			return null;

		button.SetText(text);
		button.m_OnClicked.Insert(OnRowButton);
		m_aButtons.Insert(button);
		m_aButtonActions.Insert(action);
		return button;
	}

	//------------------------------------------------------------------------------------------------
	//! Width of the whole dialog, in reference pixels (the layouts set 560 or more).
	protected void SetDialogWidth(float width)
	{
		SizeLayoutWidget size = SizeLayoutWidget.Cast(GetRootWidget().FindAnyWidget("SizeBase"));
		if (!size)
			return;

		size.EnableWidthOverride(true);
		size.SetWidthOverride(width);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Screen height in reference pixels (what layouts are sized in).
	static float GetScreenHeight()
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		return workspace.DPIUnscale(workspace.GetHeight());
	}

	//------------------------------------------------------------------------------------------------
	protected static Widget CreateLayout(WidgetType type, notnull Widget parent)
	{
		return GetGame().GetWorkspace().CreateWidget(type, WIDGET_FLAGS, Color.FromInt(Color.WHITE), 0, parent);
	}

	//------------------------------------------------------------------------------------------------
	//! Scrolling column of a fixed height (vanilla scroll layout). \return The column to add rows to.
	protected static VerticalLayoutWidget CreateScrollList(notnull Widget parent, float height)
	{
		Widget root = GetGame().GetWorkspace().CreateWidgets(SCROLL_LAYOUT, parent);
		if (!root)
			return null;

		SizeLayoutWidget size = SizeLayoutWidget.Cast(FindNamed(root, "SizeLayout0"));
		if (size)
		{
			size.EnableMinDesiredHeight(true);
			size.SetMinDesiredHeight(height);
			size.EnableMaxDesiredHeight(true);
			size.SetMaxDesiredHeight(height);
		}

		Widget message = FindNamed(root, "ScrollMessage");
		if (message)
			message.SetVisible(false);

		return VerticalLayoutWidget.Cast(FindNamed(root, "ContentVerticalLayout"));
	}

	//------------------------------------------------------------------------------------------------
	//! Inventory slot showing an item preview (vanilla inventory slot layout). \return The preview widget.
	protected static ItemPreviewWidget CreateItemPreview(notnull Widget parent, float size)
	{
		Widget root = GetGame().GetWorkspace().CreateWidgets(ITEM_SLOT_LAYOUT, parent);
		if (!root)
			return null;

		SizeLayoutWidget slot = SizeLayoutWidget.Cast(root);
		if (slot)
		{
			slot.EnableWidthOverride(true);
			slot.SetWidthOverride(size);
			slot.EnableHeightOverride(true);
			slot.SetHeightOverride(size);
		}

		ItemPreviewWidget preview = ItemPreviewWidget.Cast(FindNamed(root, "item"));
		if (preview)
			preview.SetVisible(true);

		// The slot's stack count is not filled here and would show 0.
		Widget stackNumber = FindNamed(root, "stackNumber");
		if (stackNumber)
			stackNumber.SetVisible(false);

		return preview;
	}

	//------------------------------------------------------------------------------------------------
	//! Shows a prefab without spawning it.
	static void ShowPrefabPreview(ItemPreviewWidget widget, ResourceName prefab)
	{
		ItemPreviewManagerEntity manager = GetPreviewManager();
		// Forced like the vanilla inventory slots: widgets are rebuilt with every list, and a cached preview is not redrawn otherwise.
		if (widget && manager && !prefab.IsEmpty())
			manager.SetPreviewItemFromPrefab(widget, prefab, null, true);
	}

	//------------------------------------------------------------------------------------------------
	static void ShowItemPreview(ItemPreviewWidget widget, IEntity item)
	{
		ItemPreviewManagerEntity manager = GetPreviewManager();
		if (widget && manager && item)
			manager.SetPreviewItem(widget, item, null, true);
	}

	//------------------------------------------------------------------------------------------------
	static ItemPreviewManagerEntity GetPreviewManager()
	{
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!world)
			return null;

		return world.GetItemPreviewManager();
	}

	//------------------------------------------------------------------------------------------------
	//! Name of an inventory item prefab as the vanilla inventory shows it, or its file name.
	static string GetItemDisplayName(ResourceName prefab)
	{
		ItemPreviewManagerEntity manager = GetPreviewManager();
		if (manager)
		{
			string name = GetEntityDisplayName(manager.ResolvePreviewEntityForPrefab(prefab));
			if (!name.IsEmpty())
				return name;
		}

		return GetPrefabDisplayName(prefab);
	}

	//------------------------------------------------------------------------------------------------
	//! Name of an inventory item as the vanilla inventory shows it, translated; empty when it has none.
	static string GetEntityDisplayName(IEntity entity)
	{
		if (!entity)
			return string.Empty;

		InventoryItemComponent item = InventoryItemComponent.Cast(entity.FindComponent(InventoryItemComponent));
		if (!item || !item.GetUIInfo())
			return string.Empty;

		return WidgetManager.Translate(item.GetUIInfo().GetName());
	}

	//------------------------------------------------------------------------------------------------
	//! The widget itself or a descendant with that name.
	protected static Widget FindNamed(notnull Widget root, string name)
	{
		if (root.GetName() == name)
			return root;

		return root.FindAnyWidget(name);
	}

	//------------------------------------------------------------------------------------------------
	protected TextWidget CreateText(notnull Widget parent, string text)
	{
		TextWidget widget = TextWidget.Cast(GetGame().GetWorkspace().CreateWidget(WidgetType.TextWidgetTypeID, WIDGET_FLAGS, Color.FromInt(Color.WHITE), 0, parent));
		widget.SetFont(FONT);
		widget.SetExactFontSize(FONT_SIZE);
		widget.SetText(text);
		return widget;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnRowButton(SCR_ButtonBaseComponent button)
	{
		int index = m_aButtons.Find(button);
		if (index >= 0)
			OnRowAction(m_aButtonActions[index]);
	}

	//------------------------------------------------------------------------------------------------
	//! Display name of a prefab: the file name without extension.
	static string GetPrefabDisplayName(ResourceName prefab)
	{
		return FilePath.StripExtension(FilePath.StripPath(prefab));
	}

	//------------------------------------------------------------------------------------------------
	protected static InventoryStorageManagerComponent GetLocalStorageManager()
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!character)
			return null;

		CharacterControllerComponent characterController = character.GetCharacterController();
		if (!characterController)
			return null;

		return characterController.GetInventoryStorageManager();
	}
}
