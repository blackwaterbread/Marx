//! Base of the Marx dialogs (API v0): the vanilla configurable dialog with header lines and rows of text and buttons
//! built in script, so no Marx layout asset is needed. Client side.
class MRX_ScriptedDialog : SCR_ConfigurableDialogUi
{
	static const ResourceName DIALOG_LAYOUT = "{E6B607B27BCC1477}UI/layouts/Menus/Dialogs/ConfigurableDialog.layout";
	static const ResourceName BUTTON_LAYOUT = "{D27D044A8145DC7C}UI/layouts/WidgetLibrary/Buttons/WLib_ButtonTextUnderlined.layout";
	static const ResourceName FONT = "{3E7733BAC8C831F6}UI/Fonts/RobotoCondensed/RobotoCondensed_Regular.fnt";
	protected static const int FONT_SIZE = 20;

	protected VerticalLayoutWidget m_wHeader;
	protected VerticalLayoutWidget m_wRows;

	//! Row buttons and the action string each one stands for.
	protected ref array<SCR_ButtonBaseComponent> m_aButtons = {};
	protected ref array<string> m_aButtonActions = {};

	//------------------------------------------------------------------------------------------------
	//! Opens the dialog with a Close button.
	protected static void OpenDialog(notnull MRX_ScriptedDialog dialog, string title, string tag)
	{
		SCR_ConfigurableDialogUiPreset preset = new SCR_ConfigurableDialogUiPreset();
		preset.m_sLayout = DIALOG_LAYOUT;
		preset.m_sTag = tag;
		preset.m_sTitle = title;
		preset.m_aButtons = {};

		// Created with new, so the attribute defaults of the presets are not applied.
		SCR_ConfigurableDialogUiButtonPreset closeButton = new SCR_ConfigurableDialogUiButtonPreset();
		closeButton.m_sTag = BUTTON_CANCEL;
		closeButton.m_sActionName = "MenuBack";
		closeButton.m_sLabel = "Close";
		closeButton.m_sSoundHovered = SCR_SoundEvent.SOUND_FE_BUTTON_HOVER;
		closeButton.m_sSoundClicked = SCR_SoundEvent.CLICK;
		closeButton.m_bShowButton = true;
		preset.m_aButtons.Insert(closeButton);

		CreateByPreset(preset, dialog);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnMenuOpen(SCR_ConfigurableDialogUiPreset preset)
	{
		Widget container = GetContentWidget(GetRootWidget());
		if (!container)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		Widget column = workspace.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, container);
		m_wHeader = VerticalLayoutWidget.Cast(workspace.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, column));
		m_wRows = VerticalLayoutWidget.Cast(workspace.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, column));
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
		m_aButtons.Clear();
		m_aButtonActions.Clear();
		if (!m_wRows)
			return;

		Widget child = m_wRows.GetChildren();
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

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		Widget row = workspace.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_wRows);
		CreateText(row, label);
		if (buttonText.IsEmpty())
			return;

		Widget buttonRoot = workspace.CreateWidgets(BUTTON_LAYOUT, row);
		if (!buttonRoot)
			return;

		SCR_ButtonTextComponent button = SCR_ButtonTextComponent.FindButtonTextComponent(buttonRoot);
		if (!button)
			return;

		button.SetText(buttonText);
		button.m_OnClicked.Insert(OnRowButton);
		m_aButtons.Insert(button);
		m_aButtonActions.Insert(action);
	}

	//------------------------------------------------------------------------------------------------
	protected TextWidget CreateText(notnull Widget parent, string text)
	{
		TextWidget widget = TextWidget.Cast(GetGame().GetWorkspace().CreateWidget(WidgetType.TextWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, parent));
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
