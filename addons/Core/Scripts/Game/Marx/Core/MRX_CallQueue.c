//! Work item run by MRX_CallQueue on a later frame.
class MRX_DeferredCall : Managed
{
	//------------------------------------------------------------------------------------------------
	void Run()
	{
	}
}

//! Runs deferred calls on a later frame, so callers never see a synchronous completion.
//! The Post* helpers keep the result next to the callback, so one callback object can serve several calls.
class MRX_CallQueue : Managed
{
	protected ref array<ref MRX_DeferredCall> m_aPending = {};
	protected bool m_bScheduled;

	//------------------------------------------------------------------------------------------------
	void Post(notnull MRX_DeferredCall call)
	{
		m_aPending.Insert(call);
		if (m_bScheduled)
			return;

		m_bScheduled = true;
		GetGame().GetCallqueue().CallLater(Flush);
	}

	//------------------------------------------------------------------------------------------------
	void PostTx(MRX_TxCallback callback, MRX_TxResult result)
	{
		if (callback)
			Post(new MRX_TxDelivery(callback, result));
	}

	//------------------------------------------------------------------------------------------------
	void PostBalance(MRX_BalanceCallback callback, MRX_ETxStatus status, int balance)
	{
		if (callback)
			Post(new MRX_BalanceDelivery(callback, status, balance));
	}

	//------------------------------------------------------------------------------------------------
	void PostHistory(MRX_HistoryCallback callback, MRX_ETxStatus status, array<ref MRX_LedgerEntry> entries)
	{
		if (callback)
			Post(new MRX_HistoryDelivery(callback, status, entries));
	}

	//------------------------------------------------------------------------------------------------
	void PostWallet(MRX_WalletCallback callback, MRX_ETxStatus status, MRX_WalletRecord record)
	{
		if (callback)
			Post(new MRX_WalletDelivery(callback, status, record));
	}

	//------------------------------------------------------------------------------------------------
	void PostStatus(MRX_StatusCallback callback, MRX_ETxStatus status)
	{
		if (callback)
			Post(new MRX_StatusDelivery(callback, status));
	}

	//------------------------------------------------------------------------------------------------
	protected void Flush()
	{
		m_bScheduled = false;

		// Calls posted while flushing run on the next flush.
		array<ref MRX_DeferredCall> calls = m_aPending;
		m_aPending = {};
		foreach (MRX_DeferredCall call : calls)
		{
			call.Run();
		}
	}
}

//------------------------------------------------------------------------------------------------
class MRX_TxDelivery : MRX_DeferredCall
{
	protected ref MRX_TxCallback m_Callback;
	protected ref MRX_TxResult m_Result;

	//------------------------------------------------------------------------------------------------
	void MRX_TxDelivery(MRX_TxCallback callback, MRX_TxResult result)
	{
		m_Callback = callback;
		m_Result = result;
	}

	//------------------------------------------------------------------------------------------------
	override void Run()
	{
		m_Callback.OnResult(m_Result);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_BalanceDelivery : MRX_DeferredCall
{
	protected ref MRX_BalanceCallback m_Callback;
	protected MRX_ETxStatus m_eStatus;
	protected int m_iBalance;

	//------------------------------------------------------------------------------------------------
	void MRX_BalanceDelivery(MRX_BalanceCallback callback, MRX_ETxStatus status, int balance)
	{
		m_Callback = callback;
		m_eStatus = status;
		m_iBalance = balance;
	}

	//------------------------------------------------------------------------------------------------
	override void Run()
	{
		m_Callback.OnResult(m_eStatus, m_iBalance);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_HistoryDelivery : MRX_DeferredCall
{
	protected ref MRX_HistoryCallback m_Callback;
	protected MRX_ETxStatus m_eStatus;
	protected ref array<ref MRX_LedgerEntry> m_aEntries;

	//------------------------------------------------------------------------------------------------
	void MRX_HistoryDelivery(MRX_HistoryCallback callback, MRX_ETxStatus status, array<ref MRX_LedgerEntry> entries)
	{
		m_Callback = callback;
		m_eStatus = status;
		m_aEntries = entries;
	}

	//------------------------------------------------------------------------------------------------
	override void Run()
	{
		m_Callback.OnResult(m_eStatus, m_aEntries);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_WalletDelivery : MRX_DeferredCall
{
	protected ref MRX_WalletCallback m_Callback;
	protected MRX_ETxStatus m_eStatus;
	protected ref MRX_WalletRecord m_Record;

	//------------------------------------------------------------------------------------------------
	void MRX_WalletDelivery(MRX_WalletCallback callback, MRX_ETxStatus status, MRX_WalletRecord record)
	{
		m_Callback = callback;
		m_eStatus = status;
		m_Record = record;
	}

	//------------------------------------------------------------------------------------------------
	override void Run()
	{
		m_Callback.OnResult(m_eStatus, m_Record);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_StatusDelivery : MRX_DeferredCall
{
	protected ref MRX_StatusCallback m_Callback;
	protected MRX_ETxStatus m_eStatus;

	//------------------------------------------------------------------------------------------------
	void MRX_StatusDelivery(MRX_StatusCallback callback, MRX_ETxStatus status)
	{
		m_Callback = callback;
		m_eStatus = status;
	}

	//------------------------------------------------------------------------------------------------
	override void Run()
	{
		m_Callback.OnResult(m_eStatus);
	}
}
