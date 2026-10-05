//! Transaction rules for backends that keep wallet records themselves (InMemory, Native).
//! No engine dependencies.
class MRX_WalletMath
{
	//------------------------------------------------------------------------------------------------
	//! Balance as reported to readers. A currency the wallet never used reports its initial balance.
	static int GetBalance(notnull MRX_WalletRecord record, notnull MRX_CurrencyDef def)
	{
		int balance;
		if (record.m_mBalances.Find(def.m_sId, balance))
			return balance;

		return def.m_iInitialBalance;
	}

	//------------------------------------------------------------------------------------------------
	//! Checks one posting against the currency limits without overflowing int.
	static MRX_ETxStatus CheckLimits(int balance, int delta, notnull MRX_CurrencyDef def)
	{
		if (delta == 0 || delta == int.MIN)
			return MRX_ETxStatus.INVALID_AMOUNT;

		if (delta > 0)
		{
			if (balance > def.m_iMaxBalance - delta)
				return MRX_ETxStatus.LIMIT_EXCEEDED;

			return MRX_ETxStatus.OK;
		}

		if (balance < def.GetMinBalance() - delta)
		{
			if (def.m_bAllowNegative)
				return MRX_ETxStatus.LIMIT_EXCEEDED;

			return MRX_ETxStatus.INSUFFICIENT_FUNDS;
		}

		return MRX_ETxStatus.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Applies all postings or none. Committed keys are checked first, so a retry returns DUPLICATE.
	//! Failed requests do not consume their idempotency key.
	//! \param records Wallet record of every posting owner, keyed by owner ID. Modified only on OK.
	static MRX_TxResult Apply(notnull map<string, MRX_WalletRecord> records, notnull MRX_TxRequest request, notnull MRX_StorageRules rules)
	{
		MRX_TxContext context = request.m_Context;
		if (!context || !context.IsValid())
			return MRX_TxResult.Create(MRX_ETxStatus.INVALID_CONTEXT, request.m_sTxId);

		int count = request.m_aPostings.Count();
		if (count == 0)
			return MRX_TxResult.Create(MRX_ETxStatus.INVALID_AMOUNT, request.m_sTxId);

		foreach (MRX_Posting checkedPosting : request.m_aPostings)
		{
			MRX_WalletRecord checkedRecord = records.Get(checkedPosting.m_sOwnerId);
			if (!checkedRecord)
				return MRX_TxResult.Create(MRX_ETxStatus.STORAGE_ERROR, request.m_sTxId);

			string committedTxId = checkedRecord.FindTxId(context.m_sSource, context.m_sIdempotencyKey);
			if (!committedTxId.IsEmpty())
				return CreateDuplicateResult(records, committedTxId);
		}

		// Validate every posting before touching any record.
		array<int> balancesAfter = {};
		for (int i = 0; i < count; i++)
		{
			MRX_Posting posting = request.m_aPostings[i];
			MRX_CurrencyDef def = rules.m_Currencies.Find(posting.m_sCurrency);
			if (!def)
				return MRX_TxResult.Create(MRX_ETxStatus.UNKNOWN_CURRENCY, request.m_sTxId);

			int balanceBefore = GetBalance(records.Get(posting.m_sOwnerId), def);
			for (int j = 0; j < i; j++)
			{
				MRX_Posting earlier = request.m_aPostings[j];
				if (earlier.m_sOwnerId == posting.m_sOwnerId && earlier.m_sCurrency == posting.m_sCurrency)
					balanceBefore = balancesAfter[j];
			}

			MRX_ETxStatus status = CheckLimits(balanceBefore, posting.m_iDelta, def);
			if (status != MRX_ETxStatus.OK)
				return MRX_TxResult.Create(status, request.m_sTxId);

			balancesAfter.Insert(balanceBefore + posting.m_iDelta);
		}

		MRX_TxResult result = MRX_TxResult.Create(MRX_ETxStatus.OK, request.m_sTxId);
		for (int k = 0; k < count; k++)
		{
			MRX_Posting applied = request.m_aPostings[k];
			MRX_WalletRecord record = records.Get(applied.m_sOwnerId);
			record.m_mBalances.Set(applied.m_sCurrency, balancesAfter[k]);

			MRX_LedgerEntry entry = new MRX_LedgerEntry();
			entry.m_sTxId = request.m_sTxId;
			entry.m_sOwnerId = applied.m_sOwnerId;
			entry.m_sCurrency = applied.m_sCurrency;
			entry.m_iDelta = applied.m_iDelta;
			entry.m_iBalanceAfter = balancesAfter[k];
			entry.m_iTimestamp = request.m_iTimestamp;
			entry.m_sSource = context.m_sSource;
			entry.m_sReason = context.m_sReason;
			entry.m_sIdempotencyKey = context.m_sIdempotencyKey;
			entry.m_sCounterpartyOwnerId = FindCounterparty(request, applied.m_sOwnerId);

			record.m_aRecentEntries.Insert(entry);
			result.m_aEntries.Insert(entry.Copy());
			while (record.m_aRecentEntries.Count() > rules.m_iMaxRecentEntries)
			{
				record.m_aRecentEntries.RemoveOrdered(0);
			}
		}

		array<string> ownerIds = {};
		request.GetOwnerIds(ownerIds);
		foreach (string ownerId : ownerIds)
		{
			MRX_WalletRecord keyRecord = records.Get(ownerId);
			keyRecord.m_aRecentKeys.Insert(MRX_IdempotencyEntry.Create(context.m_sSource, context.m_sIdempotencyKey, request.m_sTxId));
			while (keyRecord.m_aRecentKeys.Count() > rules.m_iMaxRecentKeys)
			{
				keyRecord.m_aRecentKeys.RemoveOrdered(0);
			}
		}

		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! Copies retained entries, newest first.
	//! \param currency Empty for all currencies.
	//! \param limit 0 or less for all retained entries.
	static int CollectHistory(notnull MRX_WalletRecord record, string currency, int limit, notnull array<ref MRX_LedgerEntry> outEntries)
	{
		outEntries.Clear();
		for (int i = record.m_aRecentEntries.Count() - 1; i >= 0; i--)
		{
			if (limit > 0 && outEntries.Count() >= limit)
				break;

			MRX_LedgerEntry entry = record.m_aRecentEntries[i];
			if (currency.IsEmpty() || entry.m_sCurrency == currency)
				outEntries.Insert(entry.Copy());
		}

		return outEntries.Count();
	}

	//------------------------------------------------------------------------------------------------
	protected static MRX_TxResult CreateDuplicateResult(notnull map<string, MRX_WalletRecord> records, string txId)
	{
		MRX_TxResult result = MRX_TxResult.Create(MRX_ETxStatus.DUPLICATE, txId);
		foreach (string ownerId, MRX_WalletRecord record : records)
		{
			foreach (MRX_LedgerEntry entry : record.m_aRecentEntries)
			{
				if (entry.m_sTxId == txId)
					result.m_aEntries.Insert(entry.Copy());
			}
		}

		return result;
	}

	//------------------------------------------------------------------------------------------------
	protected static string FindCounterparty(notnull MRX_TxRequest request, string ownerId)
	{
		array<string> ownerIds = {};
		if (request.GetOwnerIds(ownerIds) != 2)
			return string.Empty;

		if (ownerIds[0] == ownerId)
			return ownerIds[1];

		return ownerIds[0];
	}
}
