#ifdef ENABLE_DIAG
// Diag builds only (Workbench and its PeerTool peers, which have ENABLE_DIAG but not WORKBENCH): "#marxchar <player>"
// in the chat gives a player without a character one next to the executing administrator. Game Master scenarios have
// no faction slots for a PeerTool peer, so it would otherwise stay in the deploy menu.

//------------------------------------------------------------------------------------------------
class MRX_TestCharCommand : ScrServerCommand
{
	static const string KEYWORD = "marxchar";
	protected static const ResourceName CHARACTER_PREFAB = "{2F912ED6E399FF47}Prefabs/Characters/Factions/BLUFOR/US_Army/Character_US_Unarmed.et";

	//------------------------------------------------------------------------------------------------
	override string GetKeyword()
	{
		return KEYWORD;
	}

	//------------------------------------------------------------------------------------------------
	override bool IsServerSide()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override int RequiredRCONPermission()
	{
		return ERCONPermissions.PERMISSIONS_ADMIN;
	}

	//------------------------------------------------------------------------------------------------
	override int RequiredChatPermission()
	{
		return EPlayerRole.ADMINISTRATOR;
	}

	//------------------------------------------------------------------------------------------------
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId)
	{
		return ScrServerCmdResult(string.Empty, EServerCmdResultType.OK);
	}

	//------------------------------------------------------------------------------------------------
	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		if (argv.Count() < 2 || !SCR_StringHelper.IsFormat(SCR_EStringFormat.DIGITS_ONLY, argv[1]))
			return ScrServerCmdResult("Usage: #marxchar <player ID>", EServerCmdResultType.PARAMETERS);

		int targetId = argv[1].ToInt();
		SCR_PlayerController target = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(targetId));
		if (!target)
			return ScrServerCmdResult("No such player", EServerCmdResultType.ERR);

		if (target.GetControlledEntity())
			return ScrServerCmdResult("That player already has a character", EServerCmdResultType.ERR);

		IEntity executor = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!executor)
			return ScrServerCmdResult("You need a character of your own; the new one appears next to it", EServerCmdResultType.ERR);

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = executor.GetOrigin() + "1.5 0 0";
		IEntity character = GetGame().SpawnEntityPrefab(Resource.Load(CHARACTER_PREFAB), GetGame().GetWorld(), params);
		if (!character)
			return ScrServerCmdResult("Could not spawn the character", EServerCmdResultType.ERR);

		// As the respawn system hands a spawned character to its player.
		target.SetInitialMainEntity(character);
		return ScrServerCmdResult(string.Format("Player %1 controls a new character", targetId), EServerCmdResultType.OK);
	}

	//------------------------------------------------------------------------------------------------
	override ref ScrServerCmdResult OnRCONExecution(array<string> argv)
	{
		return ScrServerCmdResult("Use it in the chat", EServerCmdResultType.ERR);
	}

	//------------------------------------------------------------------------------------------------
	override ref ScrServerCmdResult OnUpdate()
	{
		return ScrServerCmdResult(string.Empty, EServerCmdResultType.OK);
	}
}

//------------------------------------------------------------------------------------------------
//! A character given without the respawn system (e.g. by #marxchar) does not close the deploy menu: close it here.
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
