#ifdef WORKBENCH
// Asynchronous tests of MRX_EconomyService with the in-memory backend.

//------------------------------------------------------------------------------------------------
class MRX_EconomyServiceTests
{
	//------------------------------------------------------------------------------------------------
	static void Register(notnull MRX_TestRunner runner)
	{
		runner.Add(new MRX_Test_CreditDebit());
		runner.Add(new MRX_Test_InsufficientFunds());
		runner.Add(new MRX_Test_InvalidInput());
		runner.Add(new MRX_Test_Limits());
		runner.Add(new MRX_Test_Idempotency());
		runner.Add(new MRX_Test_Transfer());
		runner.Add(new MRX_Test_Validator());
		runner.Add(new MRX_Test_History());
		runner.Add(new MRX_Test_SameOwnerOrdering());
		runner.Add(new MRX_Test_QueueBeforeInit());
		runner.Add(new MRX_Test_WatchedOwner());
	}
}

//------------------------------------------------------------------------------------------------
enum MRX_ETestStepKind
{
	CREDIT,
	DEBIT,
	TRANSFER,
	BALANCE,
	HISTORY
}

//------------------------------------------------------------------------------------------------
class MRX_TestStep : Managed
{
	MRX_ETestStepKind m_eKind;
	string m_sOwnerId;
	string m_sOtherOwnerId;
	string m_sCurrency;
	int m_iAmount;
	string m_sSource = "test";
	string m_sKey;
	MRX_ETxStatus m_eExpectedStatus = MRX_ETxStatus.OK;
	bool m_bCheckBalance;
	int m_iExpectedBalance;
	bool m_bCheckCount;
	int m_iExpectedCount;

	//------------------------------------------------------------------------------------------------
	MRX_TestStep Expect(MRX_ETxStatus status)
	{
		m_eExpectedStatus = status;
		return this;
	}

	//------------------------------------------------------------------------------------------------
	//! Transactions: balance after the first posting. BALANCE: the returned balance.
	MRX_TestStep ExpectBalance(int balance)
	{
		m_bCheckBalance = true;
		m_iExpectedBalance = balance;
		return this;
	}

	//------------------------------------------------------------------------------------------------
	//! HISTORY: number of entries.
	MRX_TestStep ExpectCount(int count)
	{
		m_bCheckCount = true;
		m_iExpectedCount = count;
		return this;
	}

	//------------------------------------------------------------------------------------------------
	MRX_TestStep Source(string source)
	{
		m_sSource = source;
		return this;
	}

	//------------------------------------------------------------------------------------------------
	string Describe(int index)
	{
		return string.Format("step %1 %2 %3>%4 %5 %6 key=%7:%8", index, typename.EnumToString(MRX_ETestStepKind, m_eKind),
			m_sOwnerId, m_sOtherOwnerId, m_sCurrency, m_iAmount, m_sSource, m_sKey);
	}
}

//------------------------------------------------------------------------------------------------
//! Runs a list of steps one after another, each waiting for the previous callback.
class MRX_ScenarioTest : MRX_TestCase
{
	protected ref MRX_EconomyService m_Service;
	protected ref MRX_TxCallback m_TxCallback;
	protected ref MRX_BalanceCallback m_BalanceCallback;
	protected ref MRX_HistoryCallback m_HistoryCallback;
	protected ref array<ref MRX_TestStep> m_aSteps = {};
	protected int m_iStep = -1;
	//! "source|key" -> tx ID of the committed transaction, to check DUPLICATE results.
	protected ref map<string, string> m_mCommittedTxIds = new map<string, string>();

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		m_Service = CreateTestService();

		m_TxCallback = new MRX_TxCallback();
		m_TxCallback.GetOnResult().Insert(OnTxResult);
		m_BalanceCallback = new MRX_BalanceCallback();
		m_BalanceCallback.GetOnResult().Insert(OnBalance);
		m_HistoryCallback = new MRX_HistoryCallback();
		m_HistoryCallback.GetOnResult().Insert(OnHistory);

		DefineSteps();
		NextStep();
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_EconomyService CreateTestService()
	{
		return MRX_TestUtils.CreateService();
	}

	//------------------------------------------------------------------------------------------------
	protected void DefineSteps()
	{
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_TestStep AddStep(MRX_ETestStepKind kind, string ownerId, string currency, int amount, string key)
	{
		MRX_TestStep step = new MRX_TestStep();
		step.m_eKind = kind;
		step.m_sOwnerId = ownerId;
		step.m_sCurrency = currency;
		step.m_iAmount = amount;
		step.m_sKey = key;
		m_aSteps.Insert(step);
		return step;
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_TestStep Credit(string ownerId, string currency, int amount, string key)
	{
		return AddStep(MRX_ETestStepKind.CREDIT, ownerId, currency, amount, key);
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_TestStep Debit(string ownerId, string currency, int amount, string key)
	{
		return AddStep(MRX_ETestStepKind.DEBIT, ownerId, currency, amount, key);
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_TestStep Transfer(string fromOwnerId, string toOwnerId, string currency, int amount, string key)
	{
		MRX_TestStep step = AddStep(MRX_ETestStepKind.TRANSFER, fromOwnerId, currency, amount, key);
		step.m_sOtherOwnerId = toOwnerId;
		return step;
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_TestStep Balance(string ownerId, string currency)
	{
		return AddStep(MRX_ETestStepKind.BALANCE, ownerId, currency, 0, string.Empty);
	}

	//------------------------------------------------------------------------------------------------
	//! \param limit Stored in the amount field.
	protected MRX_TestStep History(string ownerId, string currency, int limit)
	{
		return AddStep(MRX_ETestStepKind.HISTORY, ownerId, currency, limit, string.Empty);
	}

	//------------------------------------------------------------------------------------------------
	protected void NextStep()
	{
		m_iStep++;
		if (m_iStep >= m_aSteps.Count())
		{
			Finish();
			return;
		}

		MRX_TestStep step = m_aSteps[m_iStep];
		MRX_TxContext context = MRX_TxContext.Create(step.m_sSource, "test", step.m_sKey);
		switch (step.m_eKind)
		{
			case MRX_ETestStepKind.CREDIT:
				m_Service.Credit(step.m_sOwnerId, step.m_sCurrency, step.m_iAmount, context, m_TxCallback);
				break;

			case MRX_ETestStepKind.DEBIT:
				m_Service.Debit(step.m_sOwnerId, step.m_sCurrency, step.m_iAmount, context, m_TxCallback);
				break;

			case MRX_ETestStepKind.TRANSFER:
				m_Service.Transfer(step.m_sOwnerId, step.m_sOtherOwnerId, step.m_sCurrency, step.m_iAmount, context, m_TxCallback);
				break;

			case MRX_ETestStepKind.BALANCE:
				m_Service.GetBalance(step.m_sOwnerId, step.m_sCurrency, m_BalanceCallback);
				break;

			case MRX_ETestStepKind.HISTORY:
				m_Service.GetHistory(step.m_sOwnerId, step.m_sCurrency, step.m_iAmount, m_HistoryCallback);
				break;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnTxResult(MRX_TxResult result)
	{
		MRX_TestStep step = m_aSteps[m_iStep];
		string label = step.Describe(m_iStep);
		CheckStatus(result.m_eStatus, step.m_eExpectedStatus, label);

		string keyId = step.m_sSource + "|" + step.m_sKey;
		if (result.m_eStatus == MRX_ETxStatus.OK)
			m_mCommittedTxIds.Set(keyId, result.m_sTxId);
		else if (result.m_eStatus == MRX_ETxStatus.DUPLICATE)
			CheckString(result.m_sTxId, m_mCommittedTxIds.Get(keyId), label + " original tx id");

		if (step.m_bCheckBalance)
		{
			if (result.m_aEntries.IsEmpty())
				Check(false, label + ": no ledger entry");
			else
				CheckInt(result.m_aEntries[0].m_iBalanceAfter, step.m_iExpectedBalance, label + " balance");
		}

		NextStep();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBalance(MRX_ETxStatus status, int balance)
	{
		MRX_TestStep step = m_aSteps[m_iStep];
		string label = step.Describe(m_iStep);
		CheckStatus(status, step.m_eExpectedStatus, label);
		if (step.m_bCheckBalance)
			CheckInt(balance, step.m_iExpectedBalance, label + " balance");

		NextStep();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnHistory(MRX_ETxStatus status, array<ref MRX_LedgerEntry> entries)
	{
		MRX_TestStep step = m_aSteps[m_iStep];
		string label = step.Describe(m_iStep);
		CheckStatus(status, step.m_eExpectedStatus, label);
		if (step.m_bCheckCount)
			CheckInt(entries.Count(), step.m_iExpectedCount, label + " count");

		if (step.m_bCheckBalance && !entries.IsEmpty())
			CheckInt(entries[0].m_iBalanceAfter, step.m_iExpectedBalance, label + " newest balance");

		NextStep();
	}
}

//------------------------------------------------------------------------------------------------
class MRX_Test_CreditDebit : MRX_ScenarioTest
{
	//------------------------------------------------------------------------------------------------
	override protected void DefineSteps()
	{
		Balance("a", "cash").ExpectBalance(100);
		Credit("a", "cash", 50, "k1").ExpectBalance(150);
		Debit("a", "cash", 30, "k2").ExpectBalance(120);
		Balance("a", "cash").ExpectBalance(120);
		Balance("a", "debt").ExpectBalance(0);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_Test_InsufficientFunds : MRX_ScenarioTest
{
	//------------------------------------------------------------------------------------------------
	override protected void DefineSteps()
	{
		Debit("a", "cash", 101, "k1").Expect(MRX_ETxStatus.INSUFFICIENT_FUNDS);
		Balance("a", "cash").ExpectBalance(100);
		Debit("a", "cash", 100, "k2").ExpectBalance(0);
		Debit("a", "cash", 1, "k3").Expect(MRX_ETxStatus.INSUFFICIENT_FUNDS);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_Test_InvalidInput : MRX_ScenarioTest
{
	//------------------------------------------------------------------------------------------------
	override protected void DefineSteps()
	{
		Credit("a", "cash", 0, "k1").Expect(MRX_ETxStatus.INVALID_AMOUNT);
		Credit("a", "cash", -5, "k2").Expect(MRX_ETxStatus.INVALID_AMOUNT);
		Debit("a", "cash", -5, "k3").Expect(MRX_ETxStatus.INVALID_AMOUNT);
		Debit("a", "cash", int.MIN, "k4").Expect(MRX_ETxStatus.INVALID_AMOUNT);
		Credit("a", "gold", 10, "k5").Expect(MRX_ETxStatus.UNKNOWN_CURRENCY);
		Credit("a", "cash", 10, "").Expect(MRX_ETxStatus.INVALID_CONTEXT);
		Credit("a", "cash", 10, "k6").Source("").Expect(MRX_ETxStatus.INVALID_CONTEXT);
		Credit("", "cash", 10, "k7").Expect(MRX_ETxStatus.OWNER_NOT_READY);
		Transfer("a", "", "cash", 10, "k8").Expect(MRX_ETxStatus.OWNER_NOT_READY);
		Transfer("a", "a", "cash", 10, "k9").Expect(MRX_ETxStatus.INVALID_OWNER);
		Balance("", "cash").Expect(MRX_ETxStatus.OWNER_NOT_READY);
		Balance("a", "gold").Expect(MRX_ETxStatus.UNKNOWN_CURRENCY);
		Balance("a", "cash").ExpectBalance(100);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_Test_Limits : MRX_ScenarioTest
{
	//------------------------------------------------------------------------------------------------
	override protected void DefineSteps()
	{
		Credit("a", "cash", 999900, "k1").ExpectBalance(1000000);
		Credit("a", "cash", 1, "k2").Expect(MRX_ETxStatus.LIMIT_EXCEEDED);
		Debit("a", "debt", 1000, "k3").ExpectBalance(-1000);
		Debit("a", "debt", 1, "k4").Expect(MRX_ETxStatus.LIMIT_EXCEEDED);
		Credit("a", "debt", 2000, "k5").ExpectBalance(1000);
		Credit("a", "big", int.MAX, "k6").Expect(MRX_ETxStatus.LIMIT_EXCEEDED);
		Credit("a", "big", int.MAX - 100, "k7").ExpectBalance(int.MAX);
		Debit("a", "big", int.MAX, "k8").ExpectBalance(0);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_Test_Idempotency : MRX_ScenarioTest
{
	//------------------------------------------------------------------------------------------------
	override protected void DefineSteps()
	{
		Credit("a", "cash", 50, "k1").ExpectBalance(150);
		Credit("a", "cash", 50, "k1").Expect(MRX_ETxStatus.DUPLICATE).ExpectBalance(150);
		// A retry with different parameters still returns the original transaction.
		Credit("a", "cash", 999, "k1").Expect(MRX_ETxStatus.DUPLICATE);
		Balance("a", "cash").ExpectBalance(150);
		Credit("a", "cash", 50, "k1").Source("other").ExpectBalance(200);
		// A failed request does not consume its key.
		Debit("a", "cash", 1000, "k2").Expect(MRX_ETxStatus.INSUFFICIENT_FUNDS);
		Credit("a", "cash", 800, "k3").ExpectBalance(1000);
		Debit("a", "cash", 1000, "k2").ExpectBalance(0);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_Test_Transfer : MRX_ScenarioTest
{
	//------------------------------------------------------------------------------------------------
	override protected void DefineSteps()
	{
		Transfer("a", "b", "cash", 30, "k1").ExpectBalance(70);
		Balance("b", "cash").ExpectBalance(130);
		Transfer("a", "b", "cash", 71, "k2").Expect(MRX_ETxStatus.INSUFFICIENT_FUNDS);
		Balance("a", "cash").ExpectBalance(70);
		Balance("b", "cash").ExpectBalance(130);
		// Receiver over max: the sender keeps its balance.
		Credit("c", "cash", 999900, "k3").ExpectBalance(1000000);
		Transfer("a", "c", "cash", 1, "k4").Expect(MRX_ETxStatus.LIMIT_EXCEEDED);
		Balance("a", "cash").ExpectBalance(70);
		// Owner that never had a wallet receives on top of the initial balance.
		Transfer("b", "d", "cash", 130, "k5").ExpectBalance(0);
		Balance("d", "cash").ExpectBalance(230);
		// Keys are stored per wallet: "a" already committed k1, so this is the same transaction.
		Transfer("d", "a", "cash", 10, "k1").Expect(MRX_ETxStatus.DUPLICATE);
		Balance("d", "cash").ExpectBalance(230);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_TestMaxAmountValidator : MRX_TxValidator
{
	//------------------------------------------------------------------------------------------------
	override bool Validate(MRX_TxRequest request)
	{
		foreach (MRX_Posting posting : request.m_aPostings)
		{
			if (posting.m_iDelta > 500 || posting.m_iDelta < -500)
				return false;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
class MRX_Test_Validator : MRX_ScenarioTest
{
	//------------------------------------------------------------------------------------------------
	override protected void DefineSteps()
	{
		m_Service.AddValidator(new MRX_TestMaxAmountValidator());
		Credit("a", "cash", 501, "k1").Expect(MRX_ETxStatus.REJECTED);
		Credit("a", "cash", 500, "k2").ExpectBalance(600);
		Transfer("a", "b", "cash", 501, "k3").Expect(MRX_ETxStatus.REJECTED);
		Balance("a", "cash").ExpectBalance(600);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_Test_History : MRX_ScenarioTest
{
	//------------------------------------------------------------------------------------------------
	override protected MRX_EconomyService CreateTestService()
	{
		return MRX_TestUtils.CreateService(3);
	}

	//------------------------------------------------------------------------------------------------
	override protected void DefineSteps()
	{
		Credit("a", "cash", 1, "k1");
		Credit("a", "cash", 2, "k2");
		Credit("a", "cash", 3, "k3");
		Credit("a", "cash", 4, "k4");
		Credit("a", "debt", 5, "k5");
		// Retained: cash +3, cash +4, debt +5.
		History("a", "cash", 0).ExpectCount(2).ExpectBalance(110);
		History("a", "", 0).ExpectCount(3).ExpectBalance(5);
		History("a", "", 1).ExpectCount(1);
		History("nobody", "", 0).ExpectCount(0);
		History("a", "gold", 0).Expect(MRX_ETxStatus.UNKNOWN_CURRENCY);
	}
}

//------------------------------------------------------------------------------------------------
//! Calls on the same owner issued in one frame complete in call order against the running balance.
class MRX_Test_SameOwnerOrdering : MRX_TestCase
{
	protected static const int DEBITS = 11;

	protected ref MRX_EconomyService m_Service;
	protected ref array<ref MRX_TxResult> m_aResults = {};

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		m_Service = MRX_TestUtils.CreateService();

		// One callback object shared by all debits.
		MRX_TxCallback callback = new MRX_TxCallback();
		callback.GetOnResult().Insert(OnDebited);
		for (int i = 0; i < DEBITS; i++)
		{
			m_Service.Debit("a", "cash", 10, MRX_TxContext.Create("test", "order", "k" + i.ToString()), callback);
			// Another owner in between must not disturb the order of "a".
			m_Service.Credit("b", "cash", 1, MRX_TxContext.Create("test", "other", "b" + i.ToString()));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnDebited(MRX_TxResult result)
	{
		m_aResults.Insert(result);
		if (m_aResults.Count() < DEBITS)
			return;

		for (int i = 0; i < DEBITS - 1; i++)
		{
			CheckStatus(m_aResults[i].m_eStatus, MRX_ETxStatus.OK, "debit " + i.ToString());
			if (!m_aResults[i].m_aEntries.IsEmpty())
				CheckInt(m_aResults[i].m_aEntries[0].m_iBalanceAfter, 90 - 10 * i, "balance after debit " + i.ToString());
		}

		CheckStatus(m_aResults[DEBITS - 1].m_eStatus, MRX_ETxStatus.INSUFFICIENT_FUNDS, "last debit");
		MRX_BalanceCallback balanceCallback = new MRX_BalanceCallback();
		balanceCallback.GetOnResult().Insert(OnOtherBalance);
		m_Service.GetBalance("b", "cash", balanceCallback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnOtherBalance(MRX_ETxStatus status, int balance)
	{
		CheckInt(balance, 100 + DEBITS, "other owner balance");
		Finish();
	}
}

//------------------------------------------------------------------------------------------------
//! Calls made before Init() wait for the backend, and callbacks never run synchronously.
class MRX_Test_QueueBeforeInit : MRX_TestCase
{
	protected ref MRX_EconomyService m_Service;
	protected bool m_bQueuedDone;
	protected bool m_bSecondCallReturned;

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		m_Service = MRX_TestUtils.CreateService(50, 200, false);
		MRX_TxCallback callback = new MRX_TxCallback();
		callback.GetOnResult().Insert(OnQueuedCredit);
		m_Service.Credit("a", "cash", 50, MRX_TxContext.Create("test", "queued", "k1"), callback);
		GetGame().GetCallqueue().CallLater(StartService, 300);
	}

	//------------------------------------------------------------------------------------------------
	protected void StartService()
	{
		Check(!m_bQueuedDone, "queued call completed before Init");
		m_Service.Init();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnQueuedCredit(MRX_TxResult result)
	{
		m_bQueuedDone = true;
		CheckStatus(result.m_eStatus, MRX_ETxStatus.OK, "queued credit");
		Check(m_Service.GetState() == MRX_EEconomyServiceState.READY, "service state");

		MRX_TxCallback callback = new MRX_TxCallback();
		callback.GetOnResult().Insert(OnSecondCredit);
		m_Service.Credit("a", "cash", 50, MRX_TxContext.Create("test", "async", "k2"), callback);
		m_bSecondCallReturned = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSecondCredit(MRX_TxResult result)
	{
		Check(m_bSecondCallReturned, "callback ran synchronously");
		if (!result.m_aEntries.IsEmpty())
			CheckInt(result.m_aEntries[0].m_iBalanceAfter, 200, "balance");

		Finish();
	}
}

//------------------------------------------------------------------------------------------------
//! Watched owners keep a cache that follows committed transactions, and events fire per change.
class MRX_Test_WatchedOwner : MRX_TestCase
{
	protected ref MRX_EconomyService m_Service;
	protected int m_iCommittedEvents;
	protected int m_iBalanceEvents;
	protected int m_iLastCashBalance = -1;

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		m_Service = MRX_TestUtils.CreateService();
		m_Service.GetOnTransactionCommitted().Insert(OnCommitted);
		m_Service.GetOnBalanceChanged().Insert(OnBalanceChanged);

		int cached;
		Check(!m_Service.TryGetCachedBalance("a", "cash", cached), "cache before watch");

		m_Service.WatchOwner("a");
		MRX_TxCallback callback = new MRX_TxCallback();
		callback.GetOnResult().Insert(OnCredited);
		m_Service.Credit("a", "cash", 50, MRX_TxContext.Create("test", "watch", "k1"), callback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCommitted(MRX_LedgerEntry entry)
	{
		m_iCommittedEvents++;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBalanceChanged(string ownerId, string currency, int balance)
	{
		m_iBalanceEvents++;
		if (ownerId == "a" && currency == "cash")
			m_iLastCashBalance = balance;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCredited(MRX_TxResult result)
	{
		int cash;
		Check(m_Service.TryGetCachedBalance("a", "cash", cash), "cached cash");
		CheckInt(cash, 150, "cached cash");

		int debt;
		Check(m_Service.TryGetCachedBalance("a", "debt", debt), "cached debt");
		CheckInt(debt, 0, "cached debt");

		int unknown;
		Check(!m_Service.TryGetCachedBalance("a", "gold", unknown), "cached unknown currency");

		// Initial load: one event per currency (3), then the credit (1).
		CheckInt(m_iBalanceEvents, 4, "balance events");
		CheckInt(m_iCommittedEvents, 1, "committed events");
		CheckInt(m_iLastCashBalance, 150, "last cash event");

		m_Service.UnwatchOwner("a");
		Check(!m_Service.TryGetCachedBalance("a", "cash", cash), "cache after unwatch");
		Finish();
	}
}
#endif
