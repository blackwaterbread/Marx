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
	int m_iCommitsStarted;

	//------------------------------------------------------------------------------------------------
	//! No commit running, queued or waiting for a retry.
	bool IsIdle()
	{
		return !m_bCommitting && m_aCommitQueue.IsEmpty() && m_aRetryQueue.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	override protected void StartCommit()
	{
		m_iCommitsStarted++;
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
	bool IsCommitting()
	{
		return m_bCommitting;
	}

	//------------------------------------------------------------------------------------------------
	bool HasRetries()
	{
		return !m_aRetryQueue.IsEmpty();
	}

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
//! A wallet change answers before its commit. A failed commit keeps the change and is retried in the background; the
//! idempotency key stays used.
class MRX_Test_NativeCommitFailure : MRX_TestCase
{
	protected static const string OWNER = "marx-test:native-commit-failure";
	protected static const int POLL_MS = 250;

	protected ref MRX_TestFailingNativeBackend m_Backend;
	protected ref MRX_EconomyService m_Service;
	protected string m_sKey;
	protected int m_iBalanceBefore;

	//------------------------------------------------------------------------------------------------
	override int GetTimeoutMs()
	{
		return 20000;
	}

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		if (!MRX_NativeBackend.IsAvailable())
		{
			Skip("no Marx persistence config in this world");
			return;
		}

		m_Backend = new MRX_TestFailingNativeBackend();
		m_Service = new MRX_EconomyService(m_Backend, MRX_TestUtils.CreateRules());
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
		callback.GetOnResult().Insert(OnCredited);
		m_Service.Credit(OWNER, "cash", 5, MRX_TxContext.Create("test", "commit failure", m_sKey), callback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCredited(MRX_TxResult result)
	{
		CheckStatus(result.m_eStatus, MRX_ETxStatus.OK, "credit answers before its (failing) commit");
		if (!result.m_aEntries.IsEmpty())
			CheckInt(result.m_aEntries[0].m_iBalanceAfter, m_iBalanceBefore + 5, "balance after credit");

		GetGame().GetCallqueue().CallLater(WaitForRetry, POLL_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void WaitForRetry()
	{
		if (!m_Backend.IsIdle())
		{
			GetGame().GetCallqueue().CallLater(WaitForRetry, POLL_MS);
			return;
		}

		CheckInt(m_Backend.m_iCommitsStarted, 2, "failed commit, then its retry");
		MRX_BalanceCallback callback = new MRX_BalanceCallback();
		callback.GetOnResult().Insert(OnBalanceAfterRetry);
		m_Service.GetBalance(OWNER, "cash", callback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBalanceAfterRetry(MRX_ETxStatus status, int balance)
	{
		CheckInt(balance, m_iBalanceBefore + 5, "change kept after the failed commit");

		MRX_TxCallback callback = new MRX_TxCallback();
		callback.GetOnResult().Insert(OnSameKey);
		m_Service.Credit(OWNER, "cash", 5, MRX_TxContext.Create("test", "commit failure retry", m_sKey), callback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSameKey(MRX_TxResult result)
	{
		CheckStatus(result.m_eStatus, MRX_ETxStatus.DUPLICATE, "same key again");
		Finish();
	}
}

//------------------------------------------------------------------------------------------------
//! A commit that never answers while a game save completes counts as written: it ends soon after the save, long before
//! the commit timeout, and is not retried.
class MRX_Test_NativeCommitAfterSave : MRX_TestCase
{
	protected static const string OWNER = "marx-test:native-commit-after-save";
	//! Well below the commit timeout of the backend.
	protected static const int MAX_WAIT_MS = 4000;
	protected static const int POLL_MS = 100;

	protected ref MRX_TestSavedNativeBackend m_Backend;
	protected ref MRX_EconomyService m_Service;
	protected int m_iBalanceBefore;
	protected int m_iStartTick;

	//------------------------------------------------------------------------------------------------
	override int GetTimeoutMs()
	{
		return 10000;
	}

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		if (!MRX_NativeBackend.IsAvailable())
		{
			Skip("no Marx persistence config in this world");
			return;
		}

		m_Backend = new MRX_TestSavedNativeBackend();
		m_Service = new MRX_EconomyService(m_Backend, MRX_TestUtils.CreateRules());
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
		CheckStatus(result.m_eStatus, MRX_ETxStatus.OK, "credit whose changes a game save wrote");
		if (!result.m_aEntries.IsEmpty())
			CheckInt(result.m_aEntries[0].m_iBalanceAfter, m_iBalanceBefore + 3, "balance after credit");

		GetGame().GetCallqueue().CallLater(WaitForCommit, POLL_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void WaitForCommit()
	{
		if (m_Backend.IsCommitting())
		{
			GetGame().GetCallqueue().CallLater(WaitForCommit, POLL_MS);
			return;
		}

		int waitedMs = System.GetTickCount() - m_iStartTick;
		Check(waitedMs < MAX_WAIT_MS, string.Format("commit ended soon after the save (%1 ms)", waitedMs));
		Check(!m_Backend.HasRetries(), "commit counted as written (no retry)");
		Finish();
	}
}
#endif
