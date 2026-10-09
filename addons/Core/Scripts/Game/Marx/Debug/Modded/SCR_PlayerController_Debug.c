//! Debug requests of a remote client (MRX_DebugRunner). Compiled in every build, so client and server always have the
//! same RPCs; the server runs them only behind MRX_DebugRunner.IsAllowed.
modded class SCR_PlayerController
{
	//! Server: a refused debug request of this player was logged; later ones are refused silently.
	protected bool m_bMRX_DebugRefusalLogged;

	//------------------------------------------------------------------------------------------------
	//! Client: asks the server to run the server part of a debug action. Use MRX_DebugRunner.Request.
	void MRX_RequestDebug(int requestId, string actionId, notnull array<string> values)
	{
		Rpc(MRX_RpcAsk_Debug, requestId, actionId, values);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sends the result of a debug request to the player who owns this controller.
	void MRX_SendDebugResult(int requestId, notnull MRX_DebugResult result)
	{
		Rpc(MRX_RpcDo_DebugResult, requestId, result.m_eStatus, result.m_sText, result.m_EntityId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MRX_RpcAsk_Debug(int requestId, string actionId, array<string> values)
	{
		if (!MRX_DebugRunner.IsAllowed())
		{
			if (!m_bMRX_DebugRefusalLogged)
				Print(MRX_DebugRunner.LOG_TAG + string.Format("refused debug requests of player %1: not a developer build", GetPlayerId()), LogLevel.WARNING);

			m_bMRX_DebugRefusalLogged = true;
			MRX_SendDebugResult(requestId, MRX_DebugResult.Create(MRX_EDebugStatus.REJECTED, "Debug actions run only in developer builds"));
			return;
		}

		array<string> received = {};
		if (values)
			received.Copy(values);

		MRX_DebugRunner.Execute(MRX_DebugRegistry.Get(), GetPlayerId(), actionId, received, new MRX_DebugRpcReply(this, requestId, actionId));
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void MRX_RpcDo_DebugResult(int requestId, MRX_EDebugStatus status, string text, RplId entityId)
	{
		MRX_DebugResult result = MRX_DebugResult.Create(status, text);
		result.m_EntityId = entityId;
		MRX_DebugRunner.OnServerResult(requestId, result);
	}
}

//------------------------------------------------------------------------------------------------
//! Server: sends the result of a remote client's debug request back to it. Internal.
class MRX_DebugRpcReply : MRX_DebugReply
{
	//! Weak: the controller may be gone when the result arrives.
	protected SCR_PlayerController m_Controller;
	protected int m_iRequestId;
	protected string m_sActionId;

	//------------------------------------------------------------------------------------------------
	void MRX_DebugRpcReply(SCR_PlayerController controller, int requestId, string actionId)
	{
		m_Controller = controller;
		m_iRequestId = requestId;
		m_sActionId = actionId;
	}

	//------------------------------------------------------------------------------------------------
	override protected void Deliver(notnull MRX_DebugResult result)
	{
		int playerId;
		if (m_Controller)
			playerId = m_Controller.GetPlayerId();

		Print(MRX_DebugRunner.LOG_TAG + string.Format("player %1: %2 %3 %4", playerId, m_sActionId, typename.EnumToString(MRX_EDebugStatus, result.m_eStatus), result.m_sText), LogLevel.NORMAL);
		if (m_Controller)
			m_Controller.MRX_SendDebugResult(m_iRequestId, result);
	}
}
