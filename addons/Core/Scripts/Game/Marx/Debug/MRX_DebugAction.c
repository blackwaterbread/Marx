//! Outcome of a debug action.
enum MRX_EDebugStatus
{
	OK,
	//! The server runs debug actions only in developer builds (MRX_DebugRunner.IsAllowed).
	REJECTED,
	//! The action ran and reported a failure.
	FAILED,
	//! No answer from the server in time.
	TIMEOUT,
	UNKNOWN_ACTION,
	BAD_ARGS
}

//------------------------------------------------------------------------------------------------
//! A positional argument of a debug action.
class MRX_DebugArg : Managed
{
	string m_sName;
	//! Used when the value is missing or empty. Empty: the argument is required.
	string m_sDefault;
	bool m_bInt;

	//------------------------------------------------------------------------------------------------
	static MRX_DebugArg Create(string name, string defaultValue = "", bool isInt = false)
	{
		MRX_DebugArg arg = new MRX_DebugArg();
		arg.m_sName = name;
		arg.m_sDefault = defaultValue;
		arg.m_bInt = isInt;
		return arg;
	}

	//------------------------------------------------------------------------------------------------
	bool IsRequired()
	{
		return m_sDefault.IsEmpty();
	}
}

//------------------------------------------------------------------------------------------------
//! Result of a debug action, shown in the panel and logged as "[MRX_DBG] <action ID> <STATUS> <text>".
class MRX_DebugResult : Managed
{
	MRX_EDebugStatus m_eStatus;
	string m_sText;
	//! An entity the server part spawned or found, for the client part (MRX_DebugContext.m_Entity).
	RplId m_EntityId = RplId.Invalid();

	//------------------------------------------------------------------------------------------------
	static MRX_DebugResult Create(MRX_EDebugStatus status, string text = "")
	{
		MRX_DebugResult result = new MRX_DebugResult();
		result.m_eStatus = status;
		result.m_sText = text;
		return result;
	}

	//------------------------------------------------------------------------------------------------
	static MRX_DebugResult Ok(string text = "")
	{
		return Create(MRX_EDebugStatus.OK, text);
	}

	//------------------------------------------------------------------------------------------------
	static MRX_DebugResult Failed(string text)
	{
		return Create(MRX_EDebugStatus.FAILED, text);
	}

	//------------------------------------------------------------------------------------------------
	//! Hands the entity to the client part. \return This result.
	MRX_DebugResult SetEntity(IEntity entity)
	{
		if (entity)
			m_EntityId = SCR_EntityHelper.EntityToRplId(entity);
		else
			m_EntityId = RplId.Invalid();

		return this;
	}
}

//------------------------------------------------------------------------------------------------
//! What a debug action runs for: the requesting player and the argument values.
class MRX_DebugContext : Managed
{
	//! The player argument value that means the requesting player.
	static const string ME = "me";

	string m_sActionId;
	//! 0 for RCON.
	int m_iPlayerId;
	//! Weak. The requesting player's controller (client part: the local one); null for RCON.
	SCR_PlayerController m_Controller;
	//! Weak. The entity the player controls, if any.
	IEntity m_Character;
	//! Weak. Client part only: the entity of the server result (MRX_DebugResult.m_EntityId), once it replicated.
	IEntity m_Entity;
	//! One value per argument, defaults filled in (MRX_DebugArgs.Parse).
	ref array<string> m_aValues = {};

	//------------------------------------------------------------------------------------------------
	//! Server: the context of a player's request (playerId 0: RCON, no controller).
	static MRX_DebugContext Create(string actionId, int playerId, notnull array<string> values)
	{
		MRX_DebugContext context = new MRX_DebugContext();
		context.m_sActionId = actionId;
		context.m_iPlayerId = playerId;
		context.m_aValues.Copy(values);
		if (playerId > 0)
			context.SetController(SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId)));

		return context;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the context of the local player.
	static MRX_DebugContext CreateLocal(string actionId, notnull array<string> values)
	{
		MRX_DebugContext context = new MRX_DebugContext();
		context.m_sActionId = actionId;
		context.m_aValues.Copy(values);
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		context.SetController(controller);
		if (controller)
			context.m_iPlayerId = controller.GetPlayerId();

		return context;
	}

	//------------------------------------------------------------------------------------------------
	string GetString(int index)
	{
		if (!m_aValues.IsIndexValid(index))
			return string.Empty;

		return m_aValues[index];
	}

	//------------------------------------------------------------------------------------------------
	int GetInt(int index)
	{
		return GetString(index).ToInt();
	}

	//------------------------------------------------------------------------------------------------
	//! A player argument: ME, a connected player's ID or exact name. \return 0 when no such player is connected.
	int GetPlayer(int index)
	{
		string value = GetString(index);
		if (value == ME)
			return m_iPlayerId;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);
		if (MRX_DebugArgs.IsInt(value) && playerIds.Contains(value.ToInt()))
			return value.ToInt();

		foreach (int playerId : playerIds)
		{
			if (playerManager.GetPlayerName(playerId) == value)
				return playerId;
		}

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The character of the player as ChimeraCharacter, or null.
	ChimeraCharacter GetChimera()
	{
		return ChimeraCharacter.Cast(m_Character);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetController(SCR_PlayerController controller)
	{
		m_Controller = controller;
		if (controller)
			m_Character = controller.GetControlledEntity();
	}
}

//------------------------------------------------------------------------------------------------
//! The answer to one run of a server part. The server part calls Done exactly once, also when it fails; an asynchronous
//! one keeps a strong reference to the reply until then. Later calls are ignored.
class MRX_DebugReply : Managed
{
	protected bool m_bDone;
	protected ref MRX_DebugResult m_Result;

	//------------------------------------------------------------------------------------------------
	void Done(notnull MRX_DebugResult result)
	{
		if (m_bDone)
		{
			Print(MRX_DebugRunner.LOG_TAG + "a server part answered twice; the second answer is ignored: " + result.m_sText, LogLevel.WARNING);
			return;
		}

		m_bDone = true;
		m_Result = result;
		Deliver(result);
	}

	//------------------------------------------------------------------------------------------------
	bool IsDone()
	{
		return m_bDone;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Null until Done.
	MRX_DebugResult GetResult()
	{
		return m_Result;
	}

	//------------------------------------------------------------------------------------------------
	//! Override to pass the result on. Called once.
	protected void Deliver(notnull MRX_DebugResult result)
	{
	}
}

//------------------------------------------------------------------------------------------------
//! A debug action, registered in MRX_DebugRegistry. Subclasses call Setup (and AddArg) in their constructor and override
//! the parts they have:
//! - server part (HasServerPart, RunServer): runs on the server for the requesting player, only in developer builds;
//! - client part (HasClientPart, RunClient): runs on the requesting machine (also the host) after the server part
//!   answered OK, or alone.
//! Panel texts are English: the panel is a development tool and is not localized.
class MRX_DebugAction : Managed
{
	protected string m_sId;
	protected string m_sPage;
	protected string m_sLabel;
	protected ref array<ref MRX_DebugArg> m_aArgs = {};

	//------------------------------------------------------------------------------------------------
	//! "<page>.<name>" in lower case; file and chat commands use it too.
	string GetId()
	{
		return m_sId;
	}

	//------------------------------------------------------------------------------------------------
	//! Panel page.
	string GetPage()
	{
		return m_sPage;
	}

	//------------------------------------------------------------------------------------------------
	//! Panel button text.
	string GetLabel()
	{
		return m_sLabel;
	}

	//------------------------------------------------------------------------------------------------
	void GetArgs(notnull array<ref MRX_DebugArg> outArgs)
	{
		outArgs.Clear();
		foreach (MRX_DebugArg arg : m_aArgs)
		{
			outArgs.Insert(arg);
		}
	}

	//------------------------------------------------------------------------------------------------
	bool HasServerPart()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	bool HasClientPart()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Server part, may finish later: call reply.Done once in every case.
	void RunServer(notnull MRX_DebugContext context, notnull MRX_DebugReply reply)
	{
		reply.Done(MRX_DebugResult.Failed("the action has no server part"));
	}

	//------------------------------------------------------------------------------------------------
	//! Client part. \param serverResult OK (the server part's result, or an empty OK without a server part).
	//! \return The result to show; null keeps the server result.
	MRX_DebugResult RunClient(notnull MRX_DebugContext context, notnull MRX_DebugResult serverResult)
	{
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected void Setup(string id, string page, string label)
	{
		m_sId = id;
		m_sPage = page;
		m_sLabel = label;
	}

	//------------------------------------------------------------------------------------------------
	//! \param defaultValue Empty: required.
	protected void AddArg(string name, string defaultValue = "", bool isInt = false)
	{
		m_aArgs.Insert(MRX_DebugArg.Create(name, defaultValue, isInt));
	}
}
