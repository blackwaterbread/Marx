#ifdef WORKBENCH
// Synchronous tests of MRX_WalletMath (no backend, no service).

//------------------------------------------------------------------------------------------------
class MRX_WalletMathTests
{
	//------------------------------------------------------------------------------------------------
	static void Register(notnull MRX_TestRunner runner)
	{
		runner.Add(new MRX_Test_WalletMathLimits());
		runner.Add(new MRX_Test_WalletMathRetention());
		runner.Add(new MRX_Test_WalletMathDuplicate());
		runner.Add(new MRX_Test_WalletMathAtomicTransfer());
		runner.Add(new MRX_Test_WalletMathSameWalletPostings());
	}

	//------------------------------------------------------------------------------------------------
	static MRX_TxRequest CreateRequest(string txId, string key, string source = "test")
	{
		MRX_TxRequest request = new MRX_TxRequest();
		request.m_sTxId = txId;
		request.m_Context = MRX_TxContext.Create(source, "test", key);
		request.m_iTimestamp = 1;
		return request;
	}

	//------------------------------------------------------------------------------------------------
	static void AddPosting(notnull MRX_TxRequest request, string ownerId, string currency, int delta)
	{
		request.m_aPostings.Insert(MRX_Posting.Create(ownerId, currency, delta));
	}

	//------------------------------------------------------------------------------------------------
	static map<string, MRX_WalletRecord> Index(notnull MRX_WalletRecord first, MRX_WalletRecord second = null)
	{
		map<string, MRX_WalletRecord> records = new map<string, MRX_WalletRecord>();
		records.Insert(first.m_sOwnerId, first);
		if (second)
			records.Insert(second.m_sOwnerId, second);

		return records;
	}
}

//------------------------------------------------------------------------------------------------
class MRX_Test_WalletMathLimits : MRX_TestCase
{
	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		MRX_CurrencyDef cash = MRX_CurrencyDef.Create("cash", 100, 1000000);
		MRX_CurrencyDef debt = MRX_CurrencyDef.Create("debt", 0, 1000, true);
		MRX_CurrencyDef big = MRX_CurrencyDef.Create("big");

		CheckStatus(MRX_WalletMath.CheckLimits(999999, 1, cash), MRX_ETxStatus.OK, "credit up to max");
		CheckStatus(MRX_WalletMath.CheckLimits(1000000, 1, cash), MRX_ETxStatus.LIMIT_EXCEEDED, "credit above max");
		CheckStatus(MRX_WalletMath.CheckLimits(int.MAX - 1, 1, big), MRX_ETxStatus.OK, "credit to int.MAX");
		CheckStatus(MRX_WalletMath.CheckLimits(int.MAX, 1, big), MRX_ETxStatus.LIMIT_EXCEEDED, "credit past int.MAX");
		CheckStatus(MRX_WalletMath.CheckLimits(100, int.MAX, big), MRX_ETxStatus.LIMIT_EXCEEDED, "overflowing credit");
		CheckStatus(MRX_WalletMath.CheckLimits(5, -5, cash), MRX_ETxStatus.OK, "debit to zero");
		CheckStatus(MRX_WalletMath.CheckLimits(5, -6, cash), MRX_ETxStatus.INSUFFICIENT_FUNDS, "debit below zero");
		CheckStatus(MRX_WalletMath.CheckLimits(0, -int.MAX, big), MRX_ETxStatus.INSUFFICIENT_FUNDS, "largest debit");
		CheckStatus(MRX_WalletMath.CheckLimits(-999, -1, debt), MRX_ETxStatus.OK, "negative balance down to -max");
		CheckStatus(MRX_WalletMath.CheckLimits(-1000, -1, debt), MRX_ETxStatus.LIMIT_EXCEEDED, "negative balance below -max");
		CheckStatus(MRX_WalletMath.CheckLimits(0, 0, cash), MRX_ETxStatus.INVALID_AMOUNT, "zero delta");
		CheckStatus(MRX_WalletMath.CheckLimits(0, int.MIN, debt), MRX_ETxStatus.INVALID_AMOUNT, "int.MIN delta");
		Finish();
	}
}

//------------------------------------------------------------------------------------------------
class MRX_Test_WalletMathRetention : MRX_TestCase
{
	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		MRX_WalletRules rules = MRX_TestUtils.CreateRules(3, 2);
		MRX_WalletRecord record = MRX_WalletRecord.Create("a");
		map<string, MRX_WalletRecord> records = MRX_WalletMathTests.Index(record);

		for (int i = 1; i <= 5; i++)
		{
			MRX_TxRequest request = MRX_WalletMathTests.CreateRequest("tx" + i.ToString(), "k" + i.ToString());
			MRX_WalletMathTests.AddPosting(request, "a", "cash", i);
			CheckStatus(MRX_WalletMath.Apply(records, request, rules).m_eStatus, MRX_ETxStatus.OK, "credit " + i.ToString());
		}

		CheckInt(MRX_WalletMath.GetBalance(record, rules.m_Currencies.Find("cash")), 115, "balance");
		CheckInt(record.m_aRecentEntries.Count(), 3, "retained entries");
		if (record.m_aRecentEntries.Count() == 3)
			CheckInt(record.m_aRecentEntries[0].m_iDelta, 3, "oldest retained entry");

		CheckInt(record.m_aRecentKeys.Count(), 2, "retained keys");
		CheckString(record.FindTxId("test", "k3"), "", "evicted key");
		CheckString(record.FindTxId("test", "k5"), "tx5", "latest key");

		// Evicted keys are not detected anymore. This is the documented limit of local backends.
		MRX_TxRequest retry = MRX_WalletMathTests.CreateRequest("tx6", "k1");
		MRX_WalletMathTests.AddPosting(retry, "a", "cash", 1);
		CheckStatus(MRX_WalletMath.Apply(records, retry, rules).m_eStatus, MRX_ETxStatus.OK, "retry of evicted key");

		array<ref MRX_LedgerEntry> history = {};
		CheckInt(MRX_WalletMath.CollectHistory(record, "cash", 2, history), 2, "history limit");
		if (history.Count() == 2)
		{
			CheckString(history[0].m_sTxId, "tx6", "history newest first");
			CheckString(history[1].m_sTxId, "tx5", "history order");
		}

		CheckInt(MRX_WalletMath.CollectHistory(record, "debt", 0, history), 0, "history currency filter");
		Finish();
	}
}

//------------------------------------------------------------------------------------------------
class MRX_Test_WalletMathDuplicate : MRX_TestCase
{
	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		MRX_WalletRules rules = MRX_TestUtils.CreateRules();
		MRX_CurrencyDef cash = rules.m_Currencies.Find("cash");
		MRX_WalletRecord sender = MRX_WalletRecord.Create("a");
		MRX_WalletRecord receiver = MRX_WalletRecord.Create("b");
		map<string, MRX_WalletRecord> records = MRX_WalletMathTests.Index(sender, receiver);

		MRX_TxRequest first = MRX_WalletMathTests.CreateRequest("tx1", "k1");
		MRX_WalletMathTests.AddPosting(first, "a", "cash", -30);
		MRX_WalletMathTests.AddPosting(first, "b", "cash", 30);
		MRX_TxResult firstResult = MRX_WalletMath.Apply(records, first, rules);
		CheckStatus(firstResult.m_eStatus, MRX_ETxStatus.OK, "transfer");
		CheckInt(firstResult.m_aEntries.Count(), 2, "transfer entries");
		if (firstResult.m_aEntries.Count() == 2)
		{
			CheckString(firstResult.m_aEntries[0].m_sCounterpartyOwnerId, "b", "sender counterparty");
			CheckString(firstResult.m_aEntries[1].m_sCounterpartyOwnerId, "a", "receiver counterparty");
		}

		MRX_TxRequest retry = MRX_WalletMathTests.CreateRequest("tx2", "k1");
		MRX_WalletMathTests.AddPosting(retry, "a", "cash", -30);
		MRX_WalletMathTests.AddPosting(retry, "b", "cash", 30);
		MRX_TxResult retryResult = MRX_WalletMath.Apply(records, retry, rules);
		CheckStatus(retryResult.m_eStatus, MRX_ETxStatus.DUPLICATE, "retry");
		CheckString(retryResult.m_sTxId, "tx1", "retry returns original tx id");
		CheckInt(retryResult.m_aEntries.Count(), 2, "retry returns original entries");
		CheckInt(MRX_WalletMath.GetBalance(sender, cash), 70, "sender balance after retry");
		CheckInt(MRX_WalletMath.GetBalance(receiver, cash), 130, "receiver balance after retry");

		MRX_TxRequest otherSource = MRX_WalletMathTests.CreateRequest("tx3", "k1", "other");
		MRX_WalletMathTests.AddPosting(otherSource, "a", "cash", 5);
		CheckStatus(MRX_WalletMath.Apply(records, otherSource, rules).m_eStatus, MRX_ETxStatus.OK, "same key from another source");

		MRX_TxRequest failed = MRX_WalletMathTests.CreateRequest("tx4", "k2");
		MRX_WalletMathTests.AddPosting(failed, "a", "cash", -1000);
		CheckStatus(MRX_WalletMath.Apply(records, failed, rules).m_eStatus, MRX_ETxStatus.INSUFFICIENT_FUNDS, "failing debit");
		CheckString(sender.FindTxId("test", "k2"), "", "failed request keeps its key unused");
		Finish();
	}
}

//------------------------------------------------------------------------------------------------
class MRX_Test_WalletMathAtomicTransfer : MRX_TestCase
{
	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		MRX_WalletRules rules = MRX_TestUtils.CreateRules();
		MRX_WalletRecord sender = MRX_WalletRecord.Create("a");
		MRX_WalletRecord receiver = MRX_WalletRecord.Create("b");
		receiver.m_mBalances.Set("cash", 999990);

		// The second posting exceeds the receiver's max, so the first one must not be applied either.
		MRX_TxRequest transfer = MRX_WalletMathTests.CreateRequest("tx1", "k1");
		MRX_WalletMathTests.AddPosting(transfer, "a", "cash", -20);
		MRX_WalletMathTests.AddPosting(transfer, "b", "cash", 20);
		CheckStatus(MRX_WalletMath.Apply(MRX_WalletMathTests.Index(sender, receiver), transfer, rules).m_eStatus, MRX_ETxStatus.LIMIT_EXCEEDED, "transfer over receiver max");
		Check(!sender.m_mBalances.Contains("cash"), "sender balance untouched");
		CheckInt(receiver.m_mBalances.Get("cash"), 999990, "receiver balance untouched");
		CheckInt(sender.m_aRecentEntries.Count() + receiver.m_aRecentEntries.Count(), 0, "no ledger entries");
		CheckInt(sender.m_aRecentKeys.Count() + receiver.m_aRecentKeys.Count(), 0, "no keys");

		CheckStatus(MRX_WalletMath.Apply(MRX_WalletMathTests.Index(sender), transfer, rules).m_eStatus, MRX_ETxStatus.STORAGE_ERROR, "missing receiver record");
		Finish();
	}
}

//------------------------------------------------------------------------------------------------
//! Two postings on the same wallet and currency are checked in order against the running balance.
class MRX_Test_WalletMathSameWalletPostings : MRX_TestCase
{
	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		MRX_WalletRules rules = MRX_TestUtils.CreateRules();
		MRX_WalletRecord record = MRX_WalletRecord.Create("a");
		map<string, MRX_WalletRecord> records = MRX_WalletMathTests.Index(record);

		MRX_TxRequest debitFirst = MRX_WalletMathTests.CreateRequest("tx1", "k1");
		MRX_WalletMathTests.AddPosting(debitFirst, "a", "cash", -150);
		MRX_WalletMathTests.AddPosting(debitFirst, "a", "cash", 100);
		CheckStatus(MRX_WalletMath.Apply(records, debitFirst, rules).m_eStatus, MRX_ETxStatus.INSUFFICIENT_FUNDS, "debit before credit");
		Check(!record.m_mBalances.Contains("cash"), "balance untouched");

		MRX_TxRequest creditFirst = MRX_WalletMathTests.CreateRequest("tx2", "k2");
		MRX_WalletMathTests.AddPosting(creditFirst, "a", "cash", 100);
		MRX_WalletMathTests.AddPosting(creditFirst, "a", "cash", -150);
		MRX_TxResult result = MRX_WalletMath.Apply(records, creditFirst, rules);
		CheckStatus(result.m_eStatus, MRX_ETxStatus.OK, "credit before debit");
		if (result.m_aEntries.Count() == 2)
		{
			CheckInt(result.m_aEntries[0].m_iBalanceAfter, 200, "balance after first posting");
			CheckInt(result.m_aEntries[1].m_iBalanceAfter, 50, "balance after second posting");
		}

		CheckInt(record.m_mBalances.Get("cash"), 50, "final balance");
		Finish();
	}
}
#endif
