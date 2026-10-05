#ifdef WORKBENCH
// Economy test harness (Workbench only). Runs on server game start.
// Results are logged as "[MRX_TEST] PASS|FAIL <test>" followed by "[MRX_TEST] DONE".

//------------------------------------------------------------------------------------------------
class MRX_TestCase : Managed
{
	protected MRX_TestRunner m_Runner;
	protected ref array<string> m_aFailures = {};
	protected bool m_bFinished;
	protected string m_sSkipReason;

	//------------------------------------------------------------------------------------------------
	void Start(notnull MRX_TestRunner runner)
	{
		m_Runner = runner;
		Run();
	}

	//------------------------------------------------------------------------------------------------
	bool IsFinished()
	{
		return m_bFinished;
	}

	//------------------------------------------------------------------------------------------------
	array<string> GetFailures()
	{
		return m_aFailures;
	}

	//------------------------------------------------------------------------------------------------
	//! Override for tests that wait for players or the network.
	int GetTimeoutMs()
	{
		return MRX_TestRunner.TIMEOUT_MS;
	}

	//------------------------------------------------------------------------------------------------
	//! Empty unless the test was skipped.
	string GetSkipReason()
	{
		return m_sSkipReason;
	}

	//------------------------------------------------------------------------------------------------
	//! Override. Call Finish() when done, synchronously or from a callback.
	protected void Run()
	{
		Finish();
	}

	//------------------------------------------------------------------------------------------------
	protected void Check(bool condition, string message)
	{
		if (!condition)
			m_aFailures.Insert(message);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckInt(int actual, int expected, string message)
	{
		if (actual != expected)
			m_aFailures.Insert(string.Format("%1: expected %2, got %3", message, expected, actual));
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckString(string actual, string expected, string message)
	{
		if (actual != expected)
			m_aFailures.Insert(string.Format("%1: expected '%2', got '%3'", message, expected, actual));
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckStatus(MRX_ETxStatus actual, MRX_ETxStatus expected, string message)
	{
		if (actual != expected)
			m_aFailures.Insert(string.Format("%1: expected %2, got %3", message, typename.EnumToString(MRX_ETxStatus, expected), typename.EnumToString(MRX_ETxStatus, actual)));
	}

	//------------------------------------------------------------------------------------------------
	protected void Skip(string reason)
	{
		m_sSkipReason = reason;
		Finish();
	}

	//------------------------------------------------------------------------------------------------
	protected void Finish()
	{
		if (m_bFinished)
			return;

		m_bFinished = true;
		m_Runner.OnTestFinished(this);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_TestRunner : Managed
{
	static const string TAG = "[MRX_TEST] ";
	static const int TIMEOUT_MS = 5000;
	//! The tests run on game start only when Workbench was started with this parameter.
	static const string RUN_PARAM = "mrxTests";
	static const string AUTO_CLOSE_PARAM = "mrxTestsAutoClose";
	//! Players without a backend identity get a name-based test owner ID (see MRX_TestIdentity.c).
	static const string TEST_IDENTITY_PARAM = "mrxTestIdentity";
	//! Waits for a PeerTool client in MRX_Test_PeerSync instead of skipping it.
	static const string PEER_TEST_PARAM = "mrxTestsPeer";
	static const int AUTO_CLOSE_DELAY_MS = 2000;

	protected static ref MRX_TestRunner s_Instance;

	protected ref array<ref MRX_TestCase> m_aTests = {};
	protected int m_iCurrent = -1;
	protected int m_iPassed;
	protected int m_iFailed;
	protected int m_iSkipped;

	//------------------------------------------------------------------------------------------------
	static void RunAll()
	{
		s_Instance = new MRX_TestRunner();
		MRX_WalletMathTests.Register(s_Instance);
		MRX_EconomyServiceTests.Register(s_Instance);
		MRX_BootstrapTests.Register(s_Instance);
		MRX_NativeTests.Register(s_Instance);
		MRX_ShopTests.Register(s_Instance);
		MRX_ShopEntityTests.Register(s_Instance);
		MRX_StashTests.Register(s_Instance);
		MRX_StashGridTests.Register(s_Instance);
		MRX_StashEntityTests.Register(s_Instance);
		MRX_AdminCommandTests.Register(s_Instance);
		MRX_StashContainerTests.Register(s_Instance);
		MRX_NetworkTests.Register(s_Instance);
		Print(TAG + string.Format("START tests=%1", s_Instance.m_aTests.Count()));
		s_Instance.RunNext();
	}

	//------------------------------------------------------------------------------------------------
	void Add(notnull MRX_TestCase test)
	{
		m_aTests.Insert(test);
	}

	//------------------------------------------------------------------------------------------------
	void OnTestFinished(notnull MRX_TestCase test)
	{
		// Ignore tests that finish after their timeout was reported.
		if (m_aTests.Find(test) != m_iCurrent)
			return;

		Report(test, false);
		GetGame().GetCallqueue().CallLater(RunNext);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunNext()
	{
		m_iCurrent++;
		if (m_iCurrent >= m_aTests.Count())
		{
			Print(TAG + string.Format("DONE passed=%1 failed=%2 skipped=%3", m_iPassed, m_iFailed, m_iSkipped));
			GetGame().GetCallqueue().CallLater(Release);

			// For automated runs: Workbench started with -mrxTestsAutoClose returns to edit mode.
			if (System.IsCLIParam(AUTO_CLOSE_PARAM))
				GetGame().GetCallqueue().CallLater(CloseGame, AUTO_CLOSE_DELAY_MS);

			return;
		}

		GetGame().GetCallqueue().CallLater(CheckTimeout, m_aTests[m_iCurrent].GetTimeoutMs(), false, m_iCurrent);
		m_aTests[m_iCurrent].Start(this);
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the finished run, so its services do not outlive the game (they show up as leaks on script reload).
	protected static void Release()
	{
		s_Instance = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static void CloseGame()
	{
		Print(TAG + "closing the game (" + AUTO_CLOSE_PARAM + ")");
		GetGame().RequestClose();
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckTimeout(int index)
	{
		if (index != m_iCurrent || m_aTests[index].IsFinished())
			return;

		Report(m_aTests[index], true);
		RunNext();
	}

	//------------------------------------------------------------------------------------------------
	protected void Report(notnull MRX_TestCase test, bool timedOut)
	{
		array<string> failures = test.GetFailures();
		if (!timedOut && failures.IsEmpty() && !test.GetSkipReason().IsEmpty())
		{
			m_iSkipped++;
			Print(TAG + "SKIP " + test.ClassName() + ": " + test.GetSkipReason());
			return;
		}

		if (!timedOut && failures.IsEmpty())
		{
			m_iPassed++;
			Print(TAG + "PASS " + test.ClassName());
			return;
		}

		m_iFailed++;
		Print(TAG + "FAIL " + test.ClassName(), LogLevel.ERROR);
		foreach (string failure : failures)
		{
			Print(TAG + "  " + failure, LogLevel.ERROR);
		}

		if (timedOut)
			Print(TAG + string.Format("  timed out after %1 ms", test.GetTimeoutMs()), LogLevel.ERROR);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_TestUtils
{
	//------------------------------------------------------------------------------------------------
	//! Currencies: "cash" (initial 100, max 1000000), "debt" (initial 0, -1000..1000), "big" (initial 100, max int.MAX).
	static MRX_StorageRules CreateRules(int maxRecentEntries = 50, int maxRecentKeys = 200)
	{
		MRX_CurrencyRegistry currencies = new MRX_CurrencyRegistry();
		currencies.Register(MRX_CurrencyDef.Create("cash", 100, 1000000));
		currencies.Register(MRX_CurrencyDef.Create("debt", 0, 1000, true));
		currencies.Register(MRX_CurrencyDef.Create("big", 100));
		return MRX_StorageRules.Create(currencies, maxRecentEntries, maxRecentKeys);
	}

	//------------------------------------------------------------------------------------------------
	static MRX_EconomyService CreateService(int maxRecentEntries = 50, int maxRecentKeys = 200, bool init = true)
	{
		MRX_EconomyService service = new MRX_EconomyService(new MRX_InMemoryBackend(), CreateRules(maxRecentEntries, maxRecentKeys));
		if (init)
			service.Init();

		return service;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_BaseGameMode
{
	//------------------------------------------------------------------------------------------------
	override protected void OnGameStart()
	{
		super.OnGameStart();
		if (Replication.IsServer() && System.IsCLIParam(MRX_TestRunner.RUN_PARAM))
			GetGame().GetCallqueue().CallLater(MRX_TestRunner.RunAll, 1000);
	}
}
#endif
