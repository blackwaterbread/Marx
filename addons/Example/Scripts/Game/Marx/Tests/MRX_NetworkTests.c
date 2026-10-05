#ifdef WORKBENCH
// Tests of the balance push from the server to client wallets.

//------------------------------------------------------------------------------------------------
class MRX_NetworkTests
{
	//------------------------------------------------------------------------------------------------
	static void Register(notnull MRX_TestRunner runner)
	{
		runner.Add(new MRX_Test_ClientWalletLocal());
		runner.Add(new MRX_Test_PeerSync());
	}

	//------------------------------------------------------------------------------------------------
	static string UniqueKey(string prefix)
	{
		string id = PersistenceIdUtils.Generate();
		return prefix + ":" + id;
	}
}

//------------------------------------------------------------------------------------------------
//! The local player's client wallet mirrors the server balance, first after the initial sync and then after
//! a credit and a debit. On a host the owner RPC runs locally, so this covers the host path.
class MRX_Test_ClientWalletLocal : MRX_TestCase
{
	protected static const int POLL_MS = 250;

	protected string m_sOwnerId;
	protected int m_iExpected;
	//! 0 = initial sync, 1 = after credit, 2 = after debit
	protected int m_iStage;

	//------------------------------------------------------------------------------------------------
	override int GetTimeoutMs()
	{
		return 20000;
	}

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		Poll();
	}

	//------------------------------------------------------------------------------------------------
	protected void Poll()
	{
		MRX_EconomyService economy = MRX_Marx.GetEconomy();
		MRX_IdentityService identity = MRX_Marx.GetIdentity();
		if (!economy || !identity)
		{
			Check(false, "Marx system not running");
			Finish();
			return;
		}

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			Skip("no local player");
			return;
		}

		int playerId = controller.GetPlayerId();
		if (identity.IsIdentityMissing(playerId))
		{
			Skip("the local player has no backend identity in this session");
			return;
		}

		int clientBalance;
		bool synced = controller.MRX_GetWallet().TryGetBalance(MRX_Settings.DEFAULT_CURRENCY, clientBalance);
		if (m_iStage == 0)
		{
			m_sOwnerId = identity.GetOwnerId(playerId);
			int serverBalance;
			if (!m_sOwnerId.IsEmpty() && synced && economy.TryGetCachedBalance(m_sOwnerId, MRX_Settings.DEFAULT_CURRENCY, serverBalance) && clientBalance == serverBalance)
			{
				Print(string.Format("[MRX_TEST]   local client wallet synced: %1", clientBalance));
				m_iStage = 1;
				m_iExpected = serverBalance + 5;
				economy.Credit(m_sOwnerId, MRX_Settings.DEFAULT_CURRENCY, 5, MRX_TxContext.Create("test", "client sync credit", MRX_NetworkTests.UniqueKey("client-sync")));
			}
		}
		else if (clientBalance == m_iExpected)
		{
			if (m_iStage == 2)
			{
				Finish();
				return;
			}

			m_iStage = 2;
			m_iExpected -= 5;
			economy.Debit(m_sOwnerId, MRX_Settings.DEFAULT_CURRENCY, 5, MRX_TxContext.Create("test", "client sync debit", MRX_NetworkTests.UniqueKey("client-sync")));
		}

		GetGame().GetCallqueue().CallLater(Poll, POLL_MS);
	}
}

//------------------------------------------------------------------------------------------------
//! PeerTool sessions only: a joining peer gets an owner and its balances, then receives a change.
//! The peer side is checked in the peer's console.log ("[MRX] Client wallet cash = ...").
class MRX_Test_PeerSync : MRX_TestCase
{
	protected static const int POLL_MS = 1000;
	protected static const int WAIT_FOR_PEER_MS = 120000;
	protected static const int AMOUNT = 3;

	protected int m_iWaitedMs;
	protected int m_iPeerId;

	//------------------------------------------------------------------------------------------------
	override int GetTimeoutMs()
	{
		return WAIT_FOR_PEER_MS + 15000;
	}

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		if (RplSession.Mode() == RplMode.None)
		{
			Skip("not a multiplayer session");
			return;
		}

		if (!System.IsCLIParam(MRX_TestRunner.PEER_TEST_PARAM))
		{
			Skip("start Workbench with -" + MRX_TestRunner.PEER_TEST_PARAM + " to wait for a PeerTool client");
			return;
		}

		Poll();
	}

	//------------------------------------------------------------------------------------------------
	protected void Poll()
	{
		int localPlayerId;
		PlayerController localController = GetGame().GetPlayerController();
		if (localController)
			localPlayerId = localController.GetPlayerId();

		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			if (playerId == localPlayerId)
				continue;

			string ownerId = MRX_Marx.GetOwnerId(playerId);
			int balance;
			if (ownerId.IsEmpty() || !MRX_Marx.GetEconomy().TryGetCachedBalance(ownerId, MRX_Settings.DEFAULT_CURRENCY, balance))
				continue;

			m_iPeerId = playerId;
			Print(string.Format("[MRX_TEST]   peer player %1 owner %2 cash %3, sending +%4 (expect the peer log to show cash = %5)", playerId, ownerId, balance, AMOUNT, balance + AMOUNT));

			MRX_TxCallback callback = new MRX_TxCallback();
			callback.GetOnResult().Insert(OnPeerCredited);
			MRX_Marx.GetEconomy().Credit(ownerId, MRX_Settings.DEFAULT_CURRENCY, AMOUNT, MRX_TxContext.Create("test", "peer sync", MRX_NetworkTests.UniqueKey("peer-sync")), callback);
			return;
		}

		m_iWaitedMs += POLL_MS;
		if (m_iWaitedMs >= WAIT_FOR_PEER_MS)
		{
			Skip(string.Format("no peer with an owner joined within %1 s", WAIT_FOR_PEER_MS / 1000));
			return;
		}

		GetGame().GetCallqueue().CallLater(Poll, POLL_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPeerCredited(MRX_TxResult result)
	{
		CheckStatus(result.m_eStatus, MRX_ETxStatus.OK, "credit to peer");

		// Give the RPC time to arrive before the run (and Play) ends.
		GetGame().GetCallqueue().CallLater(Finish, 3000);
	}
}
#endif
