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
		GetGame().GetCallqueue().CallLater(SimulateFailedCommit);
	}

	//------------------------------------------------------------------------------------------------
	protected void SimulateFailedCommit()
	{
		OnCommitted(EPersistenceStatusCode.WRITE_ERROR, null);
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
#endif
