//! Queued unit of work of MRX_EconomyService. Internal.
class MRX_EconomyOp : Managed
{
	//! Owner of this op (weak, the service keeps the op alive).
	protected MRX_EconomyService m_Service;
	//! Owners locked while the op runs.
	ref array<string> m_aOwnerIds = {};

	//------------------------------------------------------------------------------------------------
	//! Starts the backend call. Its callback must end with m_Service.OnOpFinished(this).
	void Execute(notnull MRX_StorageBackend backend)
	{
	}

	//------------------------------------------------------------------------------------------------
	//! Marks the op as failed without running it.
	void Fail(MRX_ETxStatus status)
	{
	}

	//------------------------------------------------------------------------------------------------
	//! Reports the outcome. Called once, after the owners are released.
	void Complete()
	{
	}
}

//------------------------------------------------------------------------------------------------
//! Completes an op on a later frame. Internal.
class MRX_DeferredOpCompletion : MRX_DeferredCall
{
	protected ref MRX_EconomyOp m_Op;

	//------------------------------------------------------------------------------------------------
	void MRX_DeferredOpCompletion(MRX_EconomyOp op)
	{
		m_Op = op;
	}

	//------------------------------------------------------------------------------------------------
	override void Run()
	{
		m_Op.Complete();
	}
}

//------------------------------------------------------------------------------------------------
class MRX_ApplyOp : MRX_EconomyOp
{
	protected ref MRX_TxRequest m_Request;
	protected ref MRX_TxCallback m_Callback;
	protected ref MRX_TxResult m_Result;

	//------------------------------------------------------------------------------------------------
	void MRX_ApplyOp(MRX_EconomyService service, MRX_TxRequest request, MRX_TxCallback callback)
	{
		m_Service = service;
		m_Request = request;
		m_Callback = callback;
		request.GetOwnerIds(m_aOwnerIds);
	}

	//------------------------------------------------------------------------------------------------
	override void Execute(notnull MRX_StorageBackend backend)
	{
		MRX_TxCallback backendCallback = new MRX_TxCallback();
		backendCallback.GetOnResult().Insert(OnApplied);
		backend.ApplyTransaction(m_Request, backendCallback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnApplied(MRX_TxResult result)
	{
		m_Result = result;
		if (!m_Result)
			m_Result = MRX_TxResult.Create(MRX_ETxStatus.STORAGE_ERROR, m_Request.m_sTxId);

		m_Service.OnOpFinished(this);
	}

	//------------------------------------------------------------------------------------------------
	override void Fail(MRX_ETxStatus status)
	{
		m_Result = MRX_TxResult.Create(status, m_Request.m_sTxId);
	}

	//------------------------------------------------------------------------------------------------
	override void Complete()
	{
		m_Service.OnTransactionCompleted(m_Result);
		if (m_Callback)
			m_Callback.OnResult(m_Result);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_BalanceOp : MRX_EconomyOp
{
	protected string m_sCurrency;
	protected ref MRX_BalanceCallback m_Callback;
	protected MRX_ETxStatus m_eStatus;
	protected int m_iBalance;

	//------------------------------------------------------------------------------------------------
	void MRX_BalanceOp(MRX_EconomyService service, string ownerId, string currency, MRX_BalanceCallback callback)
	{
		m_Service = service;
		m_aOwnerIds.Insert(ownerId);
		m_sCurrency = currency;
		m_Callback = callback;
	}

	//------------------------------------------------------------------------------------------------
	override void Execute(notnull MRX_StorageBackend backend)
	{
		MRX_WalletCallback backendCallback = new MRX_WalletCallback();
		backendCallback.GetOnResult().Insert(OnLoaded);
		backend.LoadWallet(m_aOwnerIds[0], backendCallback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnLoaded(MRX_ETxStatus status, MRX_WalletRecord record)
	{
		m_eStatus = status;
		if (status == MRX_ETxStatus.OK)
		{
			MRX_CurrencyDef def = m_Service.GetRules().m_Currencies.Find(m_sCurrency);
			if (!record)
				m_eStatus = MRX_ETxStatus.STORAGE_ERROR;
			else if (!def)
				m_eStatus = MRX_ETxStatus.UNKNOWN_CURRENCY;
			else
				m_iBalance = MRX_WalletMath.GetBalance(record, def);
		}

		m_Service.OnOpFinished(this);
	}

	//------------------------------------------------------------------------------------------------
	override void Fail(MRX_ETxStatus status)
	{
		m_eStatus = status;
	}

	//------------------------------------------------------------------------------------------------
	override void Complete()
	{
		if (m_Callback)
			m_Callback.OnResult(m_eStatus, m_iBalance);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_HistoryOp : MRX_EconomyOp
{
	protected string m_sCurrency;
	protected int m_iLimit;
	protected ref MRX_HistoryCallback m_Callback;
	protected MRX_ETxStatus m_eStatus;
	protected ref array<ref MRX_LedgerEntry> m_aEntries;

	//------------------------------------------------------------------------------------------------
	void MRX_HistoryOp(MRX_EconomyService service, string ownerId, string currency, int limit, MRX_HistoryCallback callback)
	{
		m_Service = service;
		m_aOwnerIds.Insert(ownerId);
		m_sCurrency = currency;
		m_iLimit = limit;
		m_Callback = callback;
	}

	//------------------------------------------------------------------------------------------------
	override void Execute(notnull MRX_StorageBackend backend)
	{
		MRX_HistoryCallback backendCallback = new MRX_HistoryCallback();
		backendCallback.GetOnResult().Insert(OnHistory);
		backend.GetHistory(m_aOwnerIds[0], m_sCurrency, m_iLimit, backendCallback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnHistory(MRX_ETxStatus status, array<ref MRX_LedgerEntry> entries)
	{
		m_eStatus = status;
		m_aEntries = entries;
		m_Service.OnOpFinished(this);
	}

	//------------------------------------------------------------------------------------------------
	override void Fail(MRX_ETxStatus status)
	{
		m_eStatus = status;
	}

	//------------------------------------------------------------------------------------------------
	override void Complete()
	{
		if (!m_aEntries)
			m_aEntries = {};

		if (m_Callback)
			m_Callback.OnResult(m_eStatus, m_aEntries);
	}
}

//------------------------------------------------------------------------------------------------
//! Loads the balances of a watched owner into the service cache.
class MRX_WatchOp : MRX_EconomyOp
{
	protected MRX_ETxStatus m_eStatus;
	protected ref MRX_WalletRecord m_Record;

	//------------------------------------------------------------------------------------------------
	void MRX_WatchOp(MRX_EconomyService service, string ownerId)
	{
		m_Service = service;
		m_aOwnerIds.Insert(ownerId);
	}

	//------------------------------------------------------------------------------------------------
	override void Execute(notnull MRX_StorageBackend backend)
	{
		MRX_WalletCallback backendCallback = new MRX_WalletCallback();
		backendCallback.GetOnResult().Insert(OnLoaded);
		backend.LoadWallet(m_aOwnerIds[0], backendCallback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnLoaded(MRX_ETxStatus status, MRX_WalletRecord record)
	{
		m_eStatus = status;
		m_Record = record;
		m_Service.OnOpFinished(this);
	}

	//------------------------------------------------------------------------------------------------
	override void Fail(MRX_ETxStatus status)
	{
		m_eStatus = status;
	}

	//------------------------------------------------------------------------------------------------
	override void Complete()
	{
		m_Service.OnWatchLoaded(m_aOwnerIds[0], m_eStatus, m_Record);
	}
}
