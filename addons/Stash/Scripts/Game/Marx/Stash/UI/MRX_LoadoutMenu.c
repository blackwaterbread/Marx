//! Loadout window: the player's loadout slots (MRX_LoadoutBar), opened with the button below the stash panel
//! (MRX_LoadoutButton) and closed with it. Client. Internal.
class MRX_LoadoutMenu : MRX_ScriptedDialog
{
	protected static const float WINDOW_WIDTH = 720;

	//! Weak: the open window.
	protected static MRX_LoadoutMenu s_Open;

	protected ref MRX_LoadoutBar m_Bar;

	//------------------------------------------------------------------------------------------------
	//! Opens the window, or returns the open one.
	static MRX_LoadoutMenu Open()
	{
		if (s_Open)
			return s_Open;

		MRX_LoadoutMenu menu = new MRX_LoadoutMenu();
		s_Open = menu;
		OpenDialog(menu, "#MRX-Loadout_Title", "MRX_Loadouts", DIALOG_LAYOUT_MEDIUM);
		return menu;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The open window, or null.
	static MRX_LoadoutMenu GetOpen()
	{
		return s_Open;
	}

	//------------------------------------------------------------------------------------------------
	static void CloseOpen()
	{
		if (s_Open)
			s_Open.Close();
	}

	//------------------------------------------------------------------------------------------------
	MRX_LoadoutBar GetBar()
	{
		return m_Bar;
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnDialogOpened()
	{
		SetDialogWidth(WINDOW_WIDTH);
		if (m_wRows)
			m_Bar = MRX_LoadoutBar.Create(m_wRows);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		if (m_Bar)
			m_Bar.Stop();

		if (s_Open == this)
			s_Open = null;

		super.OnMenuClose();
	}
}

//------------------------------------------------------------------------------------------------
//! Button below the items of the stash panel that opens the loadout window. Shown once the server reports loadout
//! slots. Client. Internal.
class MRX_LoadoutButton : Managed
{
	//! Vertical layout of a storage panel: header, title, item grid, pages.
	protected static const string PANEL_CONTAINER = "Container";
	protected static const float HEIGHT = 36;
	protected static const ResourceName ICONS = "{3262679C50EF4F01}UI/Textures/Icons/icons_wrapperUI.imageset";
	protected static const string ARROW_ICON = "sortArrowRight";
	protected static const float ARROW_SIZE = 14;

	//! Weak.
	protected SCR_PlayerController m_Controller;
	protected ref MRX_FlatButton m_Button;

	//------------------------------------------------------------------------------------------------
	//! Adds the button below the item grid of a storage panel and asks the server whether there are loadout slots.
	//! \return Null when the panel has an unexpected layout or there is no local player controller.
	static MRX_LoadoutButton Create(Widget panelRoot)
	{
		if (!panelRoot)
			return null;

		Widget container = panelRoot.FindAnyWidget(PANEL_CONTAINER);
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!container || !controller)
			return null;

		MRX_LoadoutButton button = new MRX_LoadoutButton();
		button.m_Controller = controller;
		button.m_Button = MRX_FlatButton.Create(container, "#MRX-Loadout_Open", 0, HEIGHT);
		button.m_Button.SetTrailingIcon(ICONS, ARROW_ICON, ARROW_SIZE);
		Widget root = button.m_Button.GetRootWidget();
		AlignableSlot.SetHorizontalAlign(root, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetPadding(root, 0, 8, 0, 0);
		root.SetVisible(false);
		button.m_Button.GetOnClicked().Insert(button.OnClicked);
		controller.MRX_GetOnLoadoutInfo().Insert(button.OnInfo);
		controller.MRX_RequestLoadoutInfo();
		return button;
	}

	//------------------------------------------------------------------------------------------------
	//! Stops listening and closes the window; call when the stash panel closes.
	void Stop()
	{
		if (m_Controller)
			m_Controller.MRX_GetOnLoadoutInfo().Remove(OnInfo);

		MRX_LoadoutMenu.CloseOpen();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnClicked(MRX_FlatButton button)
	{
		MRX_LoadoutMenu.Open();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnInfo(MRX_ELoadoutStatus status, string currency, array<string> mainItems, array<int> itemCounts, array<int> nets, array<int> unavailable, array<int> stashNets, array<int> stashUnavailable)
	{
		if (m_Button && m_Button.GetRootWidget())
			m_Button.GetRootWidget().SetVisible(status == MRX_ELoadoutStatus.OK && !mainItems.IsEmpty());
	}
}
