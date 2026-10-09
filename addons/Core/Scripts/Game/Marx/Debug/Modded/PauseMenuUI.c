#ifdef ENABLE_DIAG
//! Diag builds only: a "Marx debug" button at the bottom of the pause menu shows or hides the debug panel
//! (MRX_DebugPanel), without holding the diag menu open. The pause menu also gives the panel a cursor.
modded class PauseMenuUI
{
	protected static const ResourceName MRX_DEBUG_BUTTON_LAYOUT = "{9ECCD201BCF07E95}UI/layouts/Menus/PauseMenu/PauseMenuButton.layout";

	protected SCR_ButtonTextComponent m_MRX_DebugButton;

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();
		MRX_AddDebugButton();
	}

	//------------------------------------------------------------------------------------------------
	protected void MRX_AddDebugButton()
	{
		Widget row = GetRootWidget().FindAnyWidget("ButtonRow");
		if (!row)
			return;

		Widget buttonRoot = GetGame().GetWorkspace().CreateWidgets(MRX_DEBUG_BUTTON_LAYOUT, row);
		if (!buttonRoot)
			return;

		m_MRX_DebugButton = SCR_ButtonTextComponent.FindButtonTextComponent(buttonRoot);
		if (!m_MRX_DebugButton)
			return;

		m_MRX_DebugButton.m_OnClicked.Insert(MRX_OnDebugButton);
		MRX_UpdateDebugButton();
	}

	//------------------------------------------------------------------------------------------------
	protected void MRX_OnDebugButton(SCR_ButtonBaseComponent button)
	{
		MRX_DebugPanel.SetShown(!MRX_DebugPanel.IsShown());
		MRX_UpdateDebugButton();
	}

	//------------------------------------------------------------------------------------------------
	protected void MRX_UpdateDebugButton()
	{
		if (!m_MRX_DebugButton)
			return;

		// English only, like the panel: a development tool.
		if (MRX_DebugPanel.IsShown())
			m_MRX_DebugButton.SetText("Hide Marx debug");
		else
			m_MRX_DebugButton.SetText("Marx debug");
	}
}
#endif
