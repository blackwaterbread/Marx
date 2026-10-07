#ifdef WORKBENCH
// Tests of the Native backend. They need a world whose persistence config has the Marx wallet collection,
// and are skipped otherwise. Test wallets persist between runs, so checks are relative to the loaded balance.

//------------------------------------------------------------------------------------------------
class MRX_NativeTests
{
	//------------------------------------------------------------------------------------------------
	static void Register(notnull MRX_TestRunner runner)
	{
		runner.Add(new MRX_Test_NativeRoundTrip());
		runner.Add(new MRX_Test_NativeCommitFailure());
		runner.Add(new MRX_Test_NativeCommitAfterSave());
	}

	//------------------------------------------------------------------------------------------------
	static string UniqueKey(string prefix)
	{
		string id = PersistenceIdUtils.Generate();
		return prefix + ":" + id;
	}
}

//------------------------------------------------------------------------------------------------
//! Native backend whose next commit fails without touching the storage.
class MRX_TestFailingNativeBackend : MRX_NativeBackend
{
	bool m_bFailNextCommit = true;

	//------------------------------------------------------------------------------------------------
	override protected void StartCommit()
	{
		if (!m_bFailNextCommit)
		{
			super.StartCommit();
			return;
		}

		m_bFailNextCommit = false;
		m_bCommitting = true;
		m_aCommitting = m_aCommitQueue;
		m_aCommitQueue = {};
		m_CommitId = new MRX_NativeCommitId();
		GetGame().GetCallqueue().CallLater(SimulateFailedCommit);
	}

	//------------------------------------------------------------------------------------------------
	protected void SimulateFailedCommit()
	{
		OnCommitted(EPersistenceStatusCode.WRITE_ERROR, m_CommitId);
	}
}

//------------------------------------------------------------------------------------------------
//! Native backend whose next commit never answers, as when a game save already wrote its changes, and a game save
//! completes meanwhile.
class MRX_TestSavedNativeBackend : MRX_NativeBackend
{
	bool m_bSaveNextCommit = true;

	//------------------------------------------------------------------------------------------------
	override protected void StartCommit()
	{
		if (!m_bSaveNextCommit)
		{
			super.StartCommit();
			return;
		}

		m_bSaveNextCommit = false;
		m_bCommitting = true;
		m_aCommitting = m_aCommitQueue;
		m_aCommitQueue = {};
		m_iCommitSaveMark = m_iQueueSaveMark;
		m_CommitId = new MRX_NativeCommitId();
		GetGame().GetCallqueue().CallLater(OnCommitTimeout, COMMIT_TIMEOUT_MS);
		GetGame().GetCallqueue().CallLater(SimulateGameSave);
	}

	//------------------------------------------------------------------------------------------------
	protected void SimulateGameSave()
	{
		OnGameSaved(ESaveGameType.AUTO, true);
	}
}

//------------------------------------------------------------------------------------------------
//! Credits a persistent test wallet and reads it back. The logged balance grows by 7 on every run.
class MRX_Test_NativeRoundTrip : MRX_TestCase
{
	protected static const string OWNER = "marx-test:native-roundtrip";

	protected ref MRX_EconomyService m_Service;
	protected int m_iBalanceBefore;

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		if (!MRX_NativeBackend.IsAvailable())
		{
			Skip("no Marx persistence config in this world");
			return;
		}

		m_Service = new MRX_EconomyService(new MRX_NativeBackend(), MRX_TestUtils.CreateRules());
		m_Service.Init();

		MRX_BalanceCallback callback = new MRX_BalanceCallback();
		callback.GetOnResult().Insert(OnBalanceBefore);
		m_Service.GetBalance(OWNER, "cash", callback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBalanceBefore(MRX_ETxStatus status, int balance)
	{
		CheckStatus(status, MRX_ETxStatus.OK, "balance before");
		Check((m_Service.GetBackend().GetCapabilities() & MRX_EStorageCapability.DURABLE) != 0, "durable backend (no fallback)");
		m_iBalanceBefore = balance;

		MRX_TxCallback callback = new MRX_TxCallback();
		callback.GetOnResult().Insert(OnCredited);
		m_Service.Credit(OWNER, "cash", 7, MRX_TxContext.Create("test", "native roundtrip", MRX_NativeTests.UniqueKey("roundtrip")), callback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCredited(MRX_TxResult result)
	{
		CheckStatus(result.m_eStatus, MRX_ETxStatus.OK, "credit");
		if (!result.m_aEntries.IsEmpty())
		{
			CheckInt(result.m_aEntries[0].m_iBalanceAfter, m_iBalanceBefore + 7, "balance after credit");
			Print(string.Format("[MRX_TEST]   native wallet %1: %2 -> %3 (grows by 7 on every run)", OWNER, m_iBalanceBefore, result.m_aEntries[0].m_iBalanceAfter));
		}

		MRX_HistoryCallback callback = new MRX_HistoryCallback();
		callback.GetOnResult().Insert(OnHistory);
		m_Service.GetHistory(OWNER, "cash", 1, callback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnHistory(MRX_ETxStatus status, array<ref MRX_LedgerEntry> entries)
	{
		CheckStatus(status, MRX_ETxStatus.OK, "history");
		CheckInt(entries.Count(), 1, "history entries");
		if (!entries.IsEmpty())
			CheckInt(entries[0].m_iDelta, 7, "newest entry");

		Finish();
	}
}

//------------------------------------------------------------------------------------------------
//! A failed commit restores the wallet, and the idempotency key stays usable for the retry.
class MRX_Test_NativeCommitFailure : MRX_TestCase
{
	protected static const string OWNER = "marx-test:native-commit-failure";

	protected ref MRX_EconomyService m_Service;
	protected string m_sKey;
	protected int m_iBalanceBefore;

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		if (!MRX_NativeBackend.IsAvailable())
		{
			Skip("no Marx persistence config in this world");
			return;
		}

		m_Service = new MRX_EconomyService(new MRX_TestFailingNativeBackend(), MRX_TestUtils.CreateRules());
		m_Service.Init();
		m_sKey = MRX_NativeTests.UniqueKey("commit-failure");

		MRX_BalanceCallback callback = new MRX_BalanceCallback();
		callback.GetOnResult().Insert(OnBalanceBefore);
		m_Service.GetBalance(OWNER, "cash", callback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBalanceBefore(MRX_ETxStatus status, int balance)
	{
		CheckStatus(status, MRX_ETxStatus.OK, "balance before");
		m_iBalanceBefore = balance;

		MRX_TxCallback callback = new MRX_TxCallback();
		callback.GetOnResult().Insert(OnFailedCredit);
		m_Service.Credit(OWNER, "cash", 5, MRX_TxContext.Create("test", "commit failure", m_sKey), callback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnFailedCredit(MRX_TxResult result)
	{
		CheckStatus(result.m_eStatus, MRX_ETxStatus.STORAGE_ERROR, "credit with failing commit");

		MRX_BalanceCallback callback = new MRX_BalanceCallback();
		callback.GetOnResult().Insert(OnBalanceAfterFailure);
		m_Service.GetBalance(OWNER, "cash", callback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBalanceAfterFailure(MRX_ETxStatus status, int balance)
	{
		CheckInt(balance, m_iBalanceBefore, "balance restored");

		MRX_TxCallback callback = new MRX_TxCallback();
		callback.GetOnResult().Insert(OnRetriedCredit);
		m_Service.Credit(OWNER, "cash", 5, MRX_TxContext.Create("test", "commit failure retry", m_sKey), callback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnRetriedCredit(MRX_TxResult result)
	{
		CheckStatus(result.m_eStatus, MRX_ETxStatus.OK, "retry with the same key");
		if (!result.m_aEntries.IsEmpty())
			CheckInt(result.m_aEntries[0].m_iBalanceAfter, m_iBalanceBefore + 5, "balance after retry");

		Finish();
	}
}

//------------------------------------------------------------------------------------------------
//! A commit that never answers while a game save completes counts as written: the credit succeeds soon after the save,
//! long before the commit timeout.
class MRX_Test_NativeCommitAfterSave : MRX_TestCase
{
	protected static const string OWNER = "marx-test:native-commit-after-save";
	//! Well below the commit timeout of the backend.
	protected static const int MAX_WAIT_MS = 4000;

	protected ref MRX_EconomyService m_Service;
	protected int m_iBalanceBefore;
	protected int m_iStartTick;

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		if (!MRX_NativeBackend.IsAvailable())
		{
			Skip("no Marx persistence config in this world");
			return;
		}

		m_Service = new MRX_EconomyService(new MRX_TestSavedNativeBackend(), MRX_TestUtils.CreateRules());
		m_Service.Init();

		MRX_BalanceCallback callback = new MRX_BalanceCallback();
		callback.GetOnResult().Insert(OnBalanceBefore);
		m_Service.GetBalance(OWNER, "cash", callback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBalanceBefore(MRX_ETxStatus status, int balance)
	{
		CheckStatus(status, MRX_ETxStatus.OK, "balance before");
		m_iBalanceBefore = balance;
		m_iStartTick = System.GetTickCount();

		MRX_TxCallback callback = new MRX_TxCallback();
		callback.GetOnResult().Insert(OnCredited);
		m_Service.Credit(OWNER, "cash", 3, MRX_TxContext.Create("test", "commit after save", MRX_NativeTests.UniqueKey("after-save")), callback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCredited(MRX_TxResult result)
	{
		int waitedMs = System.GetTickCount() - m_iStartTick;
		CheckStatus(result.m_eStatus, MRX_ETxStatus.OK, "credit whose changes a game save wrote");
		if (!result.m_aEntries.IsEmpty())
			CheckInt(result.m_aEntries[0].m_iBalanceAfter, m_iBalanceBefore + 3, "balance after credit");

		Check(waitedMs < MAX_WAIT_MS, string.Format("finished soon after the save (%1 ms)", waitedMs));
		Finish();
	}
}
#endif
