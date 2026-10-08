#ifdef ENABLE_DIAG
//! Diag builds only (Workbench and its PeerTool peers, which have ENABLE_DIAG but not WORKBENCH): a character given
//! without the respawn system (the marx.char debug action) does not close the deploy menu, so close it here.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	override void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		super.OnControlledEntityChanged(from, to);
		if (!to || GetGame().GetPlayerController() != this)
			return;

		SCR_DeployMenuMain.CloseDeployMenu();
		SCR_RoleSelectionMenu.CloseRoleSelectionMenu();
		GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.WelcomeScreenMenu);
	}
}
#endif
