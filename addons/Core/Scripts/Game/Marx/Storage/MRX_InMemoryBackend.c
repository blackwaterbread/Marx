//! Non-persistent backend. Used by tests and when no persistent storage is available.
class MRX_InMemoryBackend : MRX_StorageBackend
{
	protected ref MRX_WalletRules m_Rules;
	protected ref map<string, ref MRX_WalletRecord> m_mWallets = new map<string, ref MRX_WalletRecord>();

	//------------------------------------------------------------------------------------------------
	override void Init(notnull MRX_WalletRules rules, notnull MRX_StatusCallback callback)
	{
		m_Rules = rules;
		m_CallQueue.PostStatus(callback, MRX_ETxStatus.OK);
	}

	//------------------------------------------------------------------------------------------------
	override void LoadWallet(string ownerId, notnull MRX_WalletCallback callback)
	{
		MRX_WalletRecord record = m_mWallets.Get(ownerId);
		if (record)
			record = record.Copy();
		else
			record = MRX_WalletRecord.Create(ownerId);

		m_CallQueue.PostWallet(callback, MRX_ETxStatus.OK, record);
	}

	//------------------------------------------------------------------------------------------------
	override void ApplyTransaction(notnull MRX_TxRequest request, notnull MRX_TxCallback callback)
	{
		map<string, MRX_WalletRecord> records = new map<string, MRX_WalletRecord>();
		// Wallets of new owners are stored only when the transaction succeeds.
		array<ref MRX_WalletRecord> newRecords = {};

		array<string> ownerIds = {};
		request.GetOwnerIds(ownerIds);
		foreach (string ownerId : ownerIds)
		{
			MRX_WalletRecord record = m_mWallets.Get(ownerId);
			if (!record)
			{
				record = MRX_WalletRecord.Create(ownerId);
				newRecords.Insert(record);
			}

			records.Insert(ownerId, record);
		}

		MRX_TxResult result = MRX_WalletMath.Apply(records, request, m_Rules);
		if (result.m_eStatus == MRX_ETxStatus.OK)
		{
			foreach (MRX_WalletRecord newRecord : newRecords)
			{
				m_mWallets.Insert(newRecord.m_sOwnerId, newRecord);
			}
		}

		m_CallQueue.PostTx(callback, result);
	}

	//------------------------------------------------------------------------------------------------
	override void GetHistory(string ownerId, string currency, int limit, notnull MRX_HistoryCallback callback)
	{
		array<ref MRX_LedgerEntry> entries = {};
		MRX_WalletRecord record = m_mWallets.Get(ownerId);
		if (record)
			MRX_WalletMath.CollectHistory(record, currency, limit, entries);

		m_CallQueue.PostHistory(callback, MRX_ETxStatus.OK, entries);
	}
}
