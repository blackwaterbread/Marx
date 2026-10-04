#ifdef WORKBENCH
// Economy test harness (Workbench only). Runs on server game start.
// Results are logged as "[MRX_TEST] PASS|FAIL <test>" followed by "[MRX_TEST] DONE".

//------------------------------------------------------------------------------------------------
class MRX_TestCase : Managed
{
	protected MRX_TestRunner m_Runner;
	protected ref array<string> m_aFailures = {};
	protected bool m_bFinished;

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

	protected static ref MRX_TestRunner s_Instance;

	protected ref array<ref MRX_TestCase> m_aTests = {};
	protected int m_iCurrent = -1;
	protected int m_iPassed;
	protected int m_iFailed;

	//------------------------------------------------------------------------------------------------
	static void RunAll()
	{
		s_Instance = new MRX_TestRunner();
		MRX_WalletMathTests.Register(s_Instance);
		MRX_EconomyServiceTests.Register(s_Instance);
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
			Print(TAG + string.Format("DONE passed=%1 failed=%2", m_iPassed, m_iFailed));
			return;
		}

		GetGame().GetCallqueue().CallLater(CheckTimeout, TIMEOUT_MS, false, m_iCurrent);
		m_aTests[m_iCurrent].Start(this);
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
			Print(TAG + string.Format("  timed out after %1 ms", TIMEOUT_MS), LogLevel.ERROR);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_TestUtils
{
	//------------------------------------------------------------------------------------------------
	//! Currencies: "cash" (initial 100, max 1000000), "debt" (initial 0, -1000..1000), "big" (initial 100, max int.MAX).
	static MRX_WalletRules CreateRules(int maxRecentEntries = 50, int maxRecentKeys = 200)
	{
		MRX_CurrencyRegistry currencies = new MRX_CurrencyRegistry();
		currencies.Register(MRX_CurrencyDef.Create("cash", 100, 1000000));
		currencies.Register(MRX_CurrencyDef.Create("debt", 0, 1000, true));
		currencies.Register(MRX_CurrencyDef.Create("big", 100));
		return MRX_WalletRules.Create(currencies, maxRecentEntries, maxRecentKeys);
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
		if (Replication.IsServer())
			GetGame().GetCallqueue().CallLater(MRX_TestRunner.RunAll, 1000);
	}
}
#endif
