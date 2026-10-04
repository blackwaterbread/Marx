#ifdef ENABLE_DIAG
// Runtime spike: client -> server RPC request carrying only an id, server-owned value replicated
// back to the owning client. Diag builds only (Workbench + PeerTool peers). Temporary - remove before release.
// Logs are prefixed with [MRX_SPIKE_RPC].

//------------------------------------------------------------------------------------------------
class MRX_SpikeRpc
{
	static const string TAG = "[MRX_SPIKE_RPC] ";

	//------------------------------------------------------------------------------------------------
	static void Log(string msg)
	{
		Print(TAG + msg);
	}

	//------------------------------------------------------------------------------------------------
	//! Server-side catalog. Clients only send the id, never the amount.
	static int ResolveGrant(string grantId)
	{
		if (grantId == "small")
			return 10;

		if (grantId == "big")
			return 250;

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	static void Tick()
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (controller)
			controller.MRX_SpikeUpdate();
	}
}

//------------------------------------------------------------------------------------------------
modded class ArmaReforgerScripted
{
	//------------------------------------------------------------------------------------------------
	override bool OnGameStart()
	{
		bool result = super.OnGameStart();
		GetCallqueue().CallLater(MRX_SpikeRpc.Tick, 0, true);
		return result;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	[RplProp(onRplName: "MRX_OnSpikeBalanceReplicated", condition: RplCondition.OwnerOnly)]
	protected int m_iMrxSpikeBalance;

	protected int m_iMrxSpikeReplicationCount;
	protected int m_iMrxSpikeFirstTick;
	protected int m_iMrxSpikeAutoStep;

	//------------------------------------------------------------------------------------------------
	//! Called every frame on the local machine for its own controller.
	void MRX_SpikeUpdate()
	{
		int now = System.GetTickCount();
		if (m_iMrxSpikeFirstTick == 0)
		{
			m_iMrxSpikeFirstTick = now;
			MRX_SpikeRpc.Log(string.Format("local controller ready playerId=%1 isServer=%2 rplMode=%3",
				GetPlayerId(), Replication.IsServer(), typename.EnumToString(RplMode, RplSession.Mode())));
		}

		// Scripted requests so the test runs without input: small, big, then an unknown id.
		int elapsed = now - m_iMrxSpikeFirstTick;
		if (m_iMrxSpikeAutoStep == 0 && elapsed > 5000)
			MRX_SpikeAutoRequest("small");
		else if (m_iMrxSpikeAutoStep == 1 && elapsed > 7000)
			MRX_SpikeAutoRequest("big");
		else if (m_iMrxSpikeAutoStep == 2 && elapsed > 9000)
			MRX_SpikeAutoRequest("not_in_catalog");

		DbgUI.Begin("Marx RPC spike", 20, 200);
		DbgUI.Text(string.Format("playerId=%1 server=%2", GetPlayerId(), Replication.IsServer()));
		DbgUI.Text(string.Format("balance=%1 replications=%2", m_iMrxSpikeBalance, m_iMrxSpikeReplicationCount));
		if (DbgUI.Button("Grant small"))
			MRX_RequestSpikeGrant("small");

		if (DbgUI.Button("Grant big"))
			MRX_RequestSpikeGrant("big");

		DbgUI.End();
	}

	//------------------------------------------------------------------------------------------------
	protected void MRX_SpikeAutoRequest(string grantId)
	{
		m_iMrxSpikeAutoStep++;
		MRX_RequestSpikeGrant(grantId);
	}

	//------------------------------------------------------------------------------------------------
	void MRX_RequestSpikeGrant(string grantId)
	{
		MRX_SpikeRpc.Log(string.Format("client send grantId=%1 playerId=%2 balanceBefore=%3", grantId, GetPlayerId(), m_iMrxSpikeBalance));
		Rpc(MRX_RpcAsk_SpikeGrant, grantId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MRX_RpcAsk_SpikeGrant(string grantId)
	{
		int amount = MRX_SpikeRpc.ResolveGrant(grantId);
		UUID identity = SCR_PlayerIdentityUtils.GetPlayerIdentityId(GetPlayerId());
		if (amount <= 0)
		{
			MRX_SpikeRpc.Log(string.Format("server reject grantId=%1 playerId=%2 identity=%3", grantId, GetPlayerId(), identity));
			return;
		}

		m_iMrxSpikeBalance += amount;
		Replication.BumpMe();
		MRX_SpikeRpc.Log(string.Format("server apply grantId=%1 amount=%2 playerId=%3 identity=%4 balance=%5",
			grantId, amount, GetPlayerId(), identity, m_iMrxSpikeBalance));
	}

	//------------------------------------------------------------------------------------------------
	protected void MRX_OnSpikeBalanceReplicated()
	{
		m_iMrxSpikeReplicationCount++;
		MRX_SpikeRpc.Log(string.Format("client replicated balance=%1 playerId=%2 count=%3",
			m_iMrxSpikeBalance, GetPlayerId(), m_iMrxSpikeReplicationCount));
	}
}
#endif
