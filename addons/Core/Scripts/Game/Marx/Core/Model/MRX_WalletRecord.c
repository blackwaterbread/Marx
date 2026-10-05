//! Committed idempotency key. Keys are scoped by source.
class MRX_IdempotencyEntry : Managed
{
	string m_sSource;
	string m_sKey;
	string m_sTxId;

	//------------------------------------------------------------------------------------------------
	static MRX_IdempotencyEntry Create(string source, string key, string txId)
	{
		MRX_IdempotencyEntry entry = new MRX_IdempotencyEntry();
		entry.m_sSource = source;
		entry.m_sKey = key;
		entry.m_sTxId = txId;
		return entry;
	}
}

//! Stored state of one owner: balances, recent ledger entries and recent idempotency keys.
class MRX_WalletRecord : Managed
{
	string m_sOwnerId;
	//! Currency ID -> balance. A currency without a key has never been touched.
	ref map<string, int> m_mBalances = new map<string, int>();
	//! Oldest first, trimmed to MRX_StorageRules.m_iMaxRecentEntries.
	ref array<ref MRX_LedgerEntry> m_aRecentEntries = {};
	//! Oldest first, trimmed to MRX_StorageRules.m_iMaxRecentKeys.
	ref array<ref MRX_IdempotencyEntry> m_aRecentKeys = {};

	//------------------------------------------------------------------------------------------------
	static MRX_WalletRecord Create(string ownerId)
	{
		MRX_WalletRecord record = new MRX_WalletRecord();
		record.m_sOwnerId = ownerId;
		return record;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Transaction ID committed with this source and key, or empty when unknown.
	string FindTxId(string source, string key)
	{
		foreach (MRX_IdempotencyEntry entry : m_aRecentKeys)
		{
			if (entry.m_sKey == key && entry.m_sSource == source)
				return entry.m_sTxId;
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	MRX_WalletRecord Copy()
	{
		MRX_WalletRecord record = Create(m_sOwnerId);
		record.m_mBalances.Copy(m_mBalances);

		foreach (MRX_LedgerEntry entry : m_aRecentEntries)
		{
			record.m_aRecentEntries.Insert(entry.Copy());
		}

		foreach (MRX_IdempotencyEntry key : m_aRecentKeys)
		{
			record.m_aRecentKeys.Insert(MRX_IdempotencyEntry.Create(key.m_sSource, key.m_sKey, key.m_sTxId));
		}

		return record;
	}
}
