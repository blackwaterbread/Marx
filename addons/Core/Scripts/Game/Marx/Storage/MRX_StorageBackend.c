//! Storage capability flags.
enum MRX_EStorageCapability
{
	//! Keeps every ledger entry, not only the most recent ones.
	FULL_HISTORY = 1,
	//! Safe to share between several game servers.
	MULTI_SERVER = 2,
	//! Survives a server restart.
	DURABLE = 4
}

//! Storage contract (API v0). Implementations resolve every callback on a later frame, also when the work is local.
class MRX_StorageBackend : Managed
{
	protected ref MRX_CallQueue m_CallQueue = new MRX_CallQueue();

	//------------------------------------------------------------------------------------------------
	//! Called once before any other method.
	void Init(notnull MRX_StorageRules rules, notnull MRX_StatusCallback callback)
	{
		ReportNotImplemented("Init");
		m_CallQueue.PostStatus(callback, MRX_ETxStatus.STORAGE_ERROR);
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves a detached copy of the wallet. An unknown owner resolves OK with an empty record.
	void LoadWallet(string ownerId, notnull MRX_WalletCallback callback)
	{
		ReportNotImplemented("LoadWallet");
		m_CallQueue.PostWallet(callback, MRX_ETxStatus.STORAGE_ERROR, null);
	}

	//------------------------------------------------------------------------------------------------
	//! Checks, applies and persists the request atomically, including the idempotency check.
	void ApplyTransaction(notnull MRX_TxRequest request, notnull MRX_TxCallback callback)
	{
		ReportNotImplemented("ApplyTransaction");
		m_CallQueue.PostTx(callback, MRX_TxResult.Create(MRX_ETxStatus.STORAGE_ERROR, request.m_sTxId));
	}

	//------------------------------------------------------------------------------------------------
	//! Newest first. Backends without FULL_HISTORY only know the retained entries.
	//! \param currency Empty for all currencies.
	//! \param limit 0 or less for no limit.
	void GetHistory(string ownerId, string currency, int limit, notnull MRX_HistoryCallback callback)
	{
		ReportNotImplemented("GetHistory");
		m_CallQueue.PostHistory(callback, MRX_ETxStatus.STORAGE_ERROR, new array<ref MRX_LedgerEntry>());
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves a detached copy of the owner's stash. An unknown owner resolves OK with an empty record.
	void LoadStash(string ownerId, notnull MRX_StashCallback callback)
	{
		ReportNotImplemented("LoadStash");
		MRX_StashDelivery.Post(m_CallQueue, callback, MRX_EStashStatus.STORAGE_ERROR, null);
	}

	//------------------------------------------------------------------------------------------------
	//! Checks, applies and persists the request atomically (MRX_StashMath rules), including the idempotency check.
	void ApplyStash(notnull MRX_StashRequest request, notnull MRX_StashResultCallback callback)
	{
		ReportNotImplemented("ApplyStash");
		MRX_StashResultDelivery.Post(m_CallQueue, callback, MRX_StashResult.Create(MRX_EStashStatus.STORAGE_ERROR, request.m_sRequestId));
	}

	//------------------------------------------------------------------------------------------------
	//! \return MRX_EStorageCapability flags.
	int GetCapabilities()
	{
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	string GetName()
	{
		return ClassName();
	}

	//------------------------------------------------------------------------------------------------
	protected void ReportNotImplemented(string method)
	{
		Print(string.Format("[MRX] %1 does not implement %2", ClassName(), method), LogLevel.ERROR);
	}
}
