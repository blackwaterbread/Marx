//! Runs debug actions (MRX_DebugRegistry).
//! - Request (any machine with a local player): runs an action for the local player. The server part runs on the
//!   server (directly on a host, by RPC from a remote client), then the client part runs here. Every result is kept for
//!   the panel (GetRecent) and logged as "[MRX_DBG] <action ID> <STATUS> <text>".
//! - Execute (server): runs the server part of an action for a player, only behind the gate (IsAllowed).
//! Debug requests are the only Marx RPCs that carry values (amounts, prefabs); the gate is what allows it.
class MRX_DebugRunner
{
	static const string LOG_TAG = "[MRX_DBG] ";
	//! A request whose server part does not answer in time ends as TIMEOUT.
	static const int TIMEOUT_MS = 10000;
	//! The client part waits this long for the entity of the server result to replicate.
	protected static const int ENTITY_WAIT_MS = 5000;
	protected static const int ENTITY_STEP_MS = 100;
	protected static const int RECENT_LIMIT = 10;

	protected static int s_iLastRequestId;
	protected static ref map<int, ref MRX_DebugRequest> s_mRequests = new map<int, ref MRX_DebugRequest>();
	protected static ref array<ref MRX_DebugRecord> s_aRecent = {};

	//------------------------------------------------------------------------------------------------
	//! Server gate: debug actions run only in developer builds (Workbench, diag executables).
	static bool IsAllowed()
	{
		return Game.IsDev();
	}

	//------------------------------------------------------------------------------------------------
	//! Runs the action for the local player. \param words Argument values in order; missing or empty ones take the default.
	static void Request(string actionId, notnull array<string> words)
	{
		MRX_DebugRegistry registry = MRX_DebugRegistry.Get();
		MRX_DebugAction action = registry.Find(actionId);
		if (!action)
		{
			Record(actionId, MRX_DebugResult.Create(MRX_EDebugStatus.UNKNOWN_ACTION, "Unknown action"));
			return;
		}

		array<string> values = {};
		string error;
		if (MRX_DebugArgs.Parse(action, words, 0, values, error) != MRX_EDebugStatus.OK)
		{
			Record(actionId, MRX_DebugResult.Create(MRX_EDebugStatus.BAD_ARGS, error));
			return;
		}

		s_iLastRequestId++;
		MRX_DebugRequest request = new MRX_DebugRequest();
		request.m_iId = s_iLastRequestId;
		request.m_sActionId = actionId;
		request.m_aValues.Copy(values);
		s_mRequests.Set(request.m_iId, request);

		if (!action.HasServerPart())
		{
			OnServerResult(request.m_iId, MRX_DebugResult.Ok());
			return;
		}

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			OnServerResult(request.m_iId, MRX_DebugResult.Failed("No local player controller"));
			return;
		}

		GetGame().GetCallqueue().CallLater(CheckTimeout, TIMEOUT_MS, false, request.m_iId);
		if (Replication.IsServer())
			Execute(registry, controller.GetPlayerId(), actionId, values, new MRX_DebugLocalReply(request.m_iId));
		else
			controller.MRX_RequestDebug(request.m_iId, actionId, values);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: runs the server part of the action for the player (0: RCON) and answers through the reply.
	//! \param values Argument values; checked again here.
	static void Execute(notnull MRX_DebugRegistry registry, int playerId, string actionId, notnull array<string> values, notnull MRX_DebugReply reply)
	{
		if (!IsAllowed())
		{
			Print(LOG_TAG + string.Format("refused '%1' for player %2: not a developer build", actionId, playerId), LogLevel.WARNING);
			reply.Done(MRX_DebugResult.Create(MRX_EDebugStatus.REJECTED, "Debug actions run only in developer builds"));
			return;
		}

		MRX_DebugAction action = registry.Find(actionId);
		if (!action)
		{
			reply.Done(MRX_DebugResult.Create(MRX_EDebugStatus.UNKNOWN_ACTION, "Unknown action"));
			return;
		}

		if (!action.HasServerPart())
		{
			reply.Done(MRX_DebugResult.Failed("The action has no server part"));
			return;
		}

		array<string> parsed = {};
		string error;
		if (MRX_DebugArgs.Parse(action, values, 0, parsed, error) != MRX_EDebugStatus.OK)
		{
			reply.Done(MRX_DebugResult.Create(MRX_EDebugStatus.BAD_ARGS, error));
			return;
		}

		action.RunServer(MRX_DebugContext.Create(actionId, playerId, parsed), reply);
	}

	//------------------------------------------------------------------------------------------------
	//! The answer of the server part to a request of this machine. Internal (RPC and host replies).
	static void OnServerResult(int requestId, notnull MRX_DebugResult result)
	{
		MRX_DebugRequest request = s_mRequests.Get(requestId);
		if (!request || request.m_ServerResult)
			return;

		request.m_ServerResult = result;
		request.m_iEntityWaitStart = System.GetTickCount();
		MRX_DebugAction action = MRX_DebugRegistry.Get().Find(request.m_sActionId);
		if (result.m_eStatus != MRX_EDebugStatus.OK || !action || !action.HasClientPart())
		{
			Finish(requestId, result);
			return;
		}

		RunClientPart(requestId);
	}

	//------------------------------------------------------------------------------------------------
	//! Requests of this machine still waiting for the server.
	static int GetPendingCount()
	{
		return s_mRequests.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! The latest results of this machine, oldest first.
	static array<ref MRX_DebugRecord> GetRecent()
	{
		return s_aRecent;
	}

	//------------------------------------------------------------------------------------------------
	//! Keeps and logs a result.
	static void Record(string actionId, notnull MRX_DebugResult result)
	{
		MRX_DebugRecord record = MRX_DebugRecord.Create(actionId, result);
		s_aRecent.Insert(record);
		while (s_aRecent.Count() > RECENT_LIMIT)
		{
			s_aRecent.RemoveOrdered(0);
		}

		LogLevel level = LogLevel.NORMAL;
		if (result.m_eStatus != MRX_EDebugStatus.OK)
			level = LogLevel.WARNING;

		Print(LOG_TAG + record.GetLogText(), level);
	}

	//------------------------------------------------------------------------------------------------
	//! Waits for the entity of the server result to replicate (a remote client gets it later), then runs the client part.
	protected static void RunClientPart(int requestId)
	{
		MRX_DebugRequest request = s_mRequests.Get(requestId);
		if (!request)
			return;

		MRX_DebugResult serverResult = request.m_ServerResult;
		IEntity entity;
		if (serverResult.m_EntityId.IsValid())
		{
			entity = SCR_EntityHelper.RplIdToEntity(serverResult.m_EntityId);
			if (!entity)
			{
				if (System.GetTickCount() - request.m_iEntityWaitStart < ENTITY_WAIT_MS)
				{
					GetGame().GetCallqueue().CallLater(RunClientPart, ENTITY_STEP_MS, false, requestId);
					return;
				}

				Finish(requestId, MRX_DebugResult.Failed(string.Format("The entity from the server did not arrive within %1 ms", ENTITY_WAIT_MS)));
				return;
			}
		}

		MRX_DebugAction action = MRX_DebugRegistry.Get().Find(request.m_sActionId);
		MRX_DebugContext context = MRX_DebugContext.CreateLocal(request.m_sActionId, request.m_aValues);
		context.m_Entity = entity;
		MRX_DebugResult result = action.RunClient(context, serverResult);
		if (!result)
			result = serverResult;

		Finish(requestId, result);
	}

	//------------------------------------------------------------------------------------------------
	protected static void CheckTimeout(int requestId)
	{
		MRX_DebugRequest request = s_mRequests.Get(requestId);
		if (request && !request.m_ServerResult)
			Finish(requestId, MRX_DebugResult.Create(MRX_EDebugStatus.TIMEOUT, string.Format("No answer from the server within %1 ms", TIMEOUT_MS)));
	}

	//------------------------------------------------------------------------------------------------
	protected static void Finish(int requestId, notnull MRX_DebugResult result)
	{
		MRX_DebugRequest request = s_mRequests.Get(requestId);
		if (!request)
			return;

		string actionId = request.m_sActionId;
		s_mRequests.Remove(requestId);
		Record(actionId, result);
	}
}

//------------------------------------------------------------------------------------------------
//! A request of this machine. Internal.
class MRX_DebugRequest : Managed
{
	int m_iId;
	string m_sActionId;
	ref array<string> m_aValues = {};
	//! Set once the server part answered.
	ref MRX_DebugResult m_ServerResult;
	int m_iEntityWaitStart;
}

//------------------------------------------------------------------------------------------------
//! A finished request, as the panel lists it.
class MRX_DebugRecord : Managed
{
	int m_iHour;
	int m_iMinute;
	int m_iSecond;
	string m_sActionId;
	MRX_EDebugStatus m_eStatus;
	string m_sText;

	//------------------------------------------------------------------------------------------------
	static MRX_DebugRecord Create(string actionId, notnull MRX_DebugResult result)
	{
		int hour, minute, second;
		System.GetHourMinuteSecond(hour, minute, second);
		MRX_DebugRecord record = new MRX_DebugRecord();
		record.m_iHour = hour;
		record.m_iMinute = minute;
		record.m_iSecond = second;
		record.m_sActionId = actionId;
		record.m_eStatus = result.m_eStatus;
		record.m_sText = result.m_sText;
		return record;
	}

	//------------------------------------------------------------------------------------------------
	string GetStatusText()
	{
		return typename.EnumToString(MRX_EDebugStatus, m_eStatus);
	}

	//------------------------------------------------------------------------------------------------
	//! "<action ID> <STATUS> <text>".
	string GetLogText()
	{
		return string.Format("%1 %2 %3", m_sActionId, GetStatusText(), m_sText);
	}

	//------------------------------------------------------------------------------------------------
	string GetTimeText()
	{
		return string.Format("%1:%2:%3", m_iHour.ToString(2), m_iMinute.ToString(2), m_iSecond.ToString(2));
	}
}

//------------------------------------------------------------------------------------------------
//! Host: passes the server part's result of a local request back to the runner. Internal.
class MRX_DebugLocalReply : MRX_DebugReply
{
	protected int m_iRequestId;

	//------------------------------------------------------------------------------------------------
	void MRX_DebugLocalReply(int requestId)
	{
		m_iRequestId = requestId;
	}

	//------------------------------------------------------------------------------------------------
	override protected void Deliver(notnull MRX_DebugResult result)
	{
		MRX_DebugRunner.OnServerResult(m_iRequestId, result);
	}
}
