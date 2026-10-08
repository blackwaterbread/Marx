//! status: MRX_ELoadoutStatus. Parallel arrays by slot, see MRX_LoadoutInfo.
void MRX_ClientLoadoutInfoDelegate(MRX_ELoadoutStatus status, string currency, array<string> mainItems, array<int> itemCounts, array<int> nets, array<int> unavailable, array<int> stashNets, array<int> stashUnavailable);
typedef func MRX_ClientLoadoutInfoDelegate;

void MRX_ClientLoadoutResultDelegate(MRX_ELoadoutStatus status, MRX_ETxStatus txStatus, int slot, int net, string currency, int unavailable, int fromStash, int stored);
typedef func MRX_ClientLoadoutResultDelegate;

//! Saved loadouts at an open stash (API v0): the client asks to save or put on a slot, the server answers with the
//! result and keeps the client's slot list (with current prices) up to date while the stash is open.
modded class SCR_PlayerController
{
	protected ref ScriptInvokerBase<MRX_ClientLoadoutInfoDelegate> m_MRX_OnLoadoutInfo;
	protected ref ScriptInvokerBase<MRX_ClientLoadoutResultDelegate> m_MRX_OnLoadoutResult;

	//------------------------------------------------------------------------------------------------
	//! Client: invoked with the slots of the player's loadouts.
	ScriptInvokerBase<MRX_ClientLoadoutInfoDelegate> MRX_GetOnLoadoutInfo()
	{
		if (!m_MRX_OnLoadoutInfo)
			m_MRX_OnLoadoutInfo = new ScriptInvokerBase<MRX_ClientLoadoutInfoDelegate>();

		return m_MRX_OnLoadoutInfo;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: invoked with the server's answer to a save or load request.
	ScriptInvokerBase<MRX_ClientLoadoutResultDelegate> MRX_GetOnLoadoutResult()
	{
		if (!m_MRX_OnLoadoutResult)
			m_MRX_OnLoadoutResult = new ScriptInvokerBase<MRX_ClientLoadoutResultDelegate>();

		return m_MRX_OnLoadoutResult;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: saves what the character wears and carries into the slot (0-based). Needs the open stash.
	void MRX_RequestLoadoutSave(int slot)
	{
		Rpc(MRX_RpcAsk_LoadoutSave, slot);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: puts the loadout of the slot (0-based) on and pays or receives the difference. Needs the open stash.
	//! \param useStash See MRX_LoadoutService.Load.
	void MRX_RequestLoadoutLoad(int slot, bool useStash)
	{
		Rpc(MRX_RpcAsk_LoadoutLoad, slot, useStash);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: asks for the slot list.
	void MRX_RequestLoadoutInfo()
	{
		Rpc(MRX_RpcAsk_LoadoutInfo);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sends the slot list to the player who owns this controller (see also MRX_LoadoutInfoPush).
	void MRX_SendLoadoutInfo()
	{
		MRX_LoadoutService loadouts = MRX_Loadouts.Get();
		if (!loadouts)
		{
			MRX_DeliverLoadoutInfo(MRX_ELoadoutStatus.NOT_AVAILABLE, null);
			return;
		}

		loadouts.Describe(GetPlayerId(), new MRX_LoadoutInfoRpcReply(this));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: internal, called by MRX_LoadoutInfoRpcReply.
	void MRX_DeliverLoadoutInfo(MRX_ELoadoutStatus status, MRX_LoadoutInfo info)
	{
		MRX_LoadoutInfo shown = info;
		if (!shown)
			shown = new MRX_LoadoutInfo();

		Rpc(MRX_RpcDo_LoadoutInfo, status, shown.m_sCurrency, shown.m_aMainItems, shown.m_aItemCounts, shown.m_aNets, shown.m_aUnavailable, shown.m_aStashNets, shown.m_aStashUnavailable);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: internal, called by MRX_LoadoutRpcReply.
	void MRX_DeliverLoadoutResult(notnull MRX_LoadoutResult result)
	{
		Rpc(MRX_RpcDo_LoadoutResult, result.m_eStatus, result.m_eTxStatus, result.m_iSlot, result.m_iNet, result.m_sCurrency, result.m_iUnavailableCount, result.m_iFromStashCount, result.m_iStoredCount);
		MRX_LoadoutInfoPush.Schedule(GetPlayerId());
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MRX_RpcAsk_LoadoutSave(int slot)
	{
		MRX_LoadoutService loadouts = MRX_Loadouts.Get();
		if (!loadouts)
		{
			MRX_DeliverLoadoutResult(MRX_LoadoutResult.Create(MRX_ELoadoutStatus.NOT_AVAILABLE, slot));
			return;
		}

		loadouts.Save(GetPlayerId(), slot, new MRX_LoadoutRpcReply(this));
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MRX_RpcAsk_LoadoutLoad(int slot, bool useStash)
	{
		MRX_LoadoutService loadouts = MRX_Loadouts.Get();
		if (!loadouts)
		{
			MRX_DeliverLoadoutResult(MRX_LoadoutResult.Create(MRX_ELoadoutStatus.NOT_AVAILABLE, slot));
			return;
		}

		loadouts.Load(GetPlayerId(), slot, new MRX_LoadoutRpcReply(this), useStash);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MRX_RpcAsk_LoadoutInfo()
	{
		MRX_LoadoutInfoPush.Schedule(GetPlayerId());
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void MRX_RpcDo_LoadoutInfo(MRX_ELoadoutStatus status, string currency, array<string> mainItems, array<int> itemCounts, array<int> nets, array<int> unavailable, array<int> stashNets, array<int> stashUnavailable)
	{
		if (m_MRX_OnLoadoutInfo)
			m_MRX_OnLoadoutInfo.Invoke(status, currency, mainItems, itemCounts, nets, unavailable, stashNets, stashUnavailable);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void MRX_RpcDo_LoadoutResult(MRX_ELoadoutStatus status, MRX_ETxStatus txStatus, int slot, int net, string currency, int unavailable, int fromStash, int stored)
	{
		if (m_MRX_OnLoadoutResult)
			m_MRX_OnLoadoutResult.Invoke(status, txStatus, slot, net, currency, unavailable, fromStash, stored);
	}
}

//------------------------------------------------------------------------------------------------
//! Server: sends players their slot lists a little after changes, once for several changes in a row (API v0).
class MRX_LoadoutInfoPush
{
	protected static const int DELAY_MS = 300;
	protected static ref set<int> s_aPending = new set<int>();

	//------------------------------------------------------------------------------------------------
	static void Schedule(int playerId)
	{
		if (playerId <= 0)
			return;

		if (s_aPending.IsEmpty())
			GetGame().GetCallqueue().CallLater(Flush, DELAY_MS);

		s_aPending.Insert(playerId);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Flush()
	{
		array<int> playerIds = {};
		foreach (int playerId : s_aPending)
		{
			playerIds.Insert(playerId);
		}

		s_aPending.Clear();
		foreach (int pendingId : playerIds)
		{
			SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(pendingId));
			if (controller)
				controller.MRX_SendLoadoutInfo();
		}
	}
}

//------------------------------------------------------------------------------------------------
//! Sends a loadout result back to the requesting player. Internal.
class MRX_LoadoutRpcReply : MRX_LoadoutCallback
{
	//! Weak: the controller may be gone when the result arrives.
	protected SCR_PlayerController m_Controller;

	//------------------------------------------------------------------------------------------------
	void MRX_LoadoutRpcReply(SCR_PlayerController controller)
	{
		m_Controller = controller;
	}

	//------------------------------------------------------------------------------------------------
	override void OnResult(MRX_LoadoutResult result)
	{
		if (m_Controller)
			m_Controller.MRX_DeliverLoadoutResult(result);
	}
}

//------------------------------------------------------------------------------------------------
//! Sends the slot list back to the requesting player. Internal.
class MRX_LoadoutInfoRpcReply : MRX_LoadoutInfoCallback
{
	//! Weak: the controller may be gone when the list arrives.
	protected SCR_PlayerController m_Controller;

	//------------------------------------------------------------------------------------------------
	void MRX_LoadoutInfoRpcReply(SCR_PlayerController controller)
	{
		m_Controller = controller;
	}

	//------------------------------------------------------------------------------------------------
	override void OnResult(MRX_ELoadoutStatus status, MRX_LoadoutInfo info)
	{
		if (m_Controller)
			m_Controller.MRX_DeliverLoadoutInfo(status, info);
	}
}
