//! "#mrxdbg <action ID> [values...]" in the chat (logged-in administrators) or over RCON (admin permission): runs the
//! server part of a debug action (MRX_DebugRegistry) for the executing player, only in developer builds
//! (MRX_DebugRunner.IsAllowed). Actions with a client part run from the debug panel instead. Over RCON there is no
//! executing player (player 0). "#mrxdbg" alone lists the action IDs.
class MRX_DebugCommand : ScrServerCommand
{
	static const string KEYWORD = "mrxdbg";

	protected ref MRX_DebugReply m_Pending;
	protected string m_sPendingActionId;
	protected int m_iPendingStartTick;

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
		return HandleCommand(argv, playerId);
	}

	//------------------------------------------------------------------------------------------------
	override ref ScrServerCmdResult OnRCONExecution(array<string> argv)
	{
		return HandleCommand(argv, 0);
	}

	//------------------------------------------------------------------------------------------------
	override ref ScrServerCmdResult OnUpdate()
	{
		if (!m_Pending)
			return ScrServerCmdResult(string.Empty, EServerCmdResultType.ERR);

		if (m_Pending.IsDone())
			return TakeResult();

		if (System.GetTickCount() - m_iPendingStartTick < MRX_DebugRunner.TIMEOUT_MS)
			return ScrServerCmdResult(string.Empty, EServerCmdResultType.PENDING);

		string actionId = m_sPendingActionId;
		m_Pending = null;
		Print(MRX_DebugRunner.LOG_TAG + string.Format("%1 TIMEOUT (#%2)", actionId, KEYWORD), LogLevel.WARNING);
		return ScrServerCmdResult(string.Format("%1: no answer within %2 ms", actionId, MRX_DebugRunner.TIMEOUT_MS), EServerCmdResultType.ERR);
	}

	//------------------------------------------------------------------------------------------------
	protected ScrServerCmdResult HandleCommand(array<string> argv, int executorId)
	{
		if (!MRX_DebugRunner.IsAllowed())
		{
			Print(MRX_DebugRunner.LOG_TAG + string.Format("refused #%1 for player %2: not a developer build", KEYWORD, executorId), LogLevel.WARNING);
			return ScrServerCmdResult("Debug actions run only in developer builds", EServerCmdResultType.ERR);
		}

		MRX_DebugRegistry registry = MRX_DebugRegistry.Get();
		if (argv.Count() < 2)
			return ScrServerCmdResult(GetActionList(registry), EServerCmdResultType.OK);

		string actionId = argv[1];
		MRX_DebugAction action = registry.Find(actionId);
		if (!action)
			return ScrServerCmdResult(string.Format("Unknown action '%1'. #%2 lists them.", actionId, KEYWORD), EServerCmdResultType.PARAMETERS);

		if (action.HasClientPart())
			return ScrServerCmdResult(string.Format("%1 has a client part: run it from the debug panel", actionId), EServerCmdResultType.ERR);

		array<string> values = {};
		string error;
		if (MRX_DebugArgs.Parse(action, argv, 2, values, error) != MRX_EDebugStatus.OK)
			return ScrServerCmdResult(error, EServerCmdResultType.PARAMETERS);

		if (m_Pending && !m_Pending.IsDone())
			return ScrServerCmdResult(string.Format("%1 is still running", m_sPendingActionId), EServerCmdResultType.ERR);

		m_Pending = new MRX_DebugReply();
		m_sPendingActionId = actionId;
		m_iPendingStartTick = System.GetTickCount();
		MRX_DebugRunner.Execute(registry, executorId, actionId, values, m_Pending);
		if (m_Pending.IsDone())
			return TakeResult();

		return ScrServerCmdResult(string.Empty, EServerCmdResultType.PENDING);
	}

	//------------------------------------------------------------------------------------------------
	protected ScrServerCmdResult TakeResult()
	{
		MRX_DebugResult result = m_Pending.GetResult();
		string actionId = m_sPendingActionId;
		m_Pending = null;

		string status = typename.EnumToString(MRX_EDebugStatus, result.m_eStatus);
		Print(MRX_DebugRunner.LOG_TAG + string.Format("%1 %2 %3 (#%4)", actionId, status, result.m_sText, KEYWORD), LogLevel.NORMAL);
		EServerCmdResultType type = EServerCmdResultType.OK;
		if (result.m_eStatus == MRX_EDebugStatus.BAD_ARGS)
			type = EServerCmdResultType.PARAMETERS;
		else if (result.m_eStatus != MRX_EDebugStatus.OK)
			type = EServerCmdResultType.ERR;

		return ScrServerCmdResult(string.Format("%1 %2 %3", actionId, status, result.m_sText), type);
	}

	//------------------------------------------------------------------------------------------------
	protected static string GetActionList(notnull MRX_DebugRegistry registry)
	{
		array<string> ids = {};
		registry.GetIds(ids);
		array<string> serverOnly = {};
		foreach (string id : ids)
		{
			if (!registry.Find(id).HasClientPart())
				serverOnly.Insert(id);
		}

		return string.Format("#%1 <action ID> [values...]. Actions: %2", KEYWORD, SCR_StringHelper.Join(", ", serverOnly));
	}
}
