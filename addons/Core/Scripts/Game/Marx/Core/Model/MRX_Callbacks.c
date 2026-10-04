// Callbacks (API v0). Script methods cannot take func arguments, so results are delivered through callback objects:
// subclass and override OnResult(), or subscribe a method with the matching delegate signature:
//   MRX_TxCallback callback = new MRX_TxCallback();
//   callback.GetOnResult().Insert(OnPaid);
// One callback object may be reused for several calls.

void MRX_TxDelegate(MRX_TxResult result);
typedef func MRX_TxDelegate;

void MRX_BalanceDelegate(MRX_ETxStatus status, int balance);
typedef func MRX_BalanceDelegate;

void MRX_HistoryDelegate(MRX_ETxStatus status, array<ref MRX_LedgerEntry> entries);
typedef func MRX_HistoryDelegate;

void MRX_WalletDelegate(MRX_ETxStatus status, MRX_WalletRecord record);
typedef func MRX_WalletDelegate;

void MRX_StatusDelegate(MRX_ETxStatus status);
typedef func MRX_StatusDelegate;

void MRX_TransactionCommittedDelegate(MRX_LedgerEntry entry);
typedef func MRX_TransactionCommittedDelegate;

void MRX_BalanceChangedDelegate(string ownerId, string currency, int balance);
typedef func MRX_BalanceChangedDelegate;

//------------------------------------------------------------------------------------------------
class MRX_TxCallback : Managed
{
	protected ref ScriptInvokerBase<MRX_TxDelegate> m_OnResult;

	//------------------------------------------------------------------------------------------------
	ScriptInvokerBase<MRX_TxDelegate> GetOnResult()
	{
		if (!m_OnResult)
			m_OnResult = new ScriptInvokerBase<MRX_TxDelegate>();

		return m_OnResult;
	}

	//------------------------------------------------------------------------------------------------
	//! Override to handle the result without subscribing.
	void OnResult(MRX_TxResult result)
	{
		if (m_OnResult)
			m_OnResult.Invoke(result);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_BalanceCallback : Managed
{
	protected ref ScriptInvokerBase<MRX_BalanceDelegate> m_OnResult;

	//------------------------------------------------------------------------------------------------
	ScriptInvokerBase<MRX_BalanceDelegate> GetOnResult()
	{
		if (!m_OnResult)
			m_OnResult = new ScriptInvokerBase<MRX_BalanceDelegate>();

		return m_OnResult;
	}

	//------------------------------------------------------------------------------------------------
	//! Override to handle the result without subscribing.
	void OnResult(MRX_ETxStatus status, int balance)
	{
		if (m_OnResult)
			m_OnResult.Invoke(status, balance);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_HistoryCallback : Managed
{
	protected ref ScriptInvokerBase<MRX_HistoryDelegate> m_OnResult;

	//------------------------------------------------------------------------------------------------
	ScriptInvokerBase<MRX_HistoryDelegate> GetOnResult()
	{
		if (!m_OnResult)
			m_OnResult = new ScriptInvokerBase<MRX_HistoryDelegate>();

		return m_OnResult;
	}

	//------------------------------------------------------------------------------------------------
	//! Override to handle the result without subscribing.
	void OnResult(MRX_ETxStatus status, array<ref MRX_LedgerEntry> entries)
	{
		if (m_OnResult)
			m_OnResult.Invoke(status, entries);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_WalletCallback : Managed
{
	protected ref ScriptInvokerBase<MRX_WalletDelegate> m_OnResult;

	//------------------------------------------------------------------------------------------------
	ScriptInvokerBase<MRX_WalletDelegate> GetOnResult()
	{
		if (!m_OnResult)
			m_OnResult = new ScriptInvokerBase<MRX_WalletDelegate>();

		return m_OnResult;
	}

	//------------------------------------------------------------------------------------------------
	//! Override to handle the result without subscribing.
	void OnResult(MRX_ETxStatus status, MRX_WalletRecord record)
	{
		if (m_OnResult)
			m_OnResult.Invoke(status, record);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_StatusCallback : Managed
{
	protected ref ScriptInvokerBase<MRX_StatusDelegate> m_OnResult;

	//------------------------------------------------------------------------------------------------
	ScriptInvokerBase<MRX_StatusDelegate> GetOnResult()
	{
		if (!m_OnResult)
			m_OnResult = new ScriptInvokerBase<MRX_StatusDelegate>();

		return m_OnResult;
	}

	//------------------------------------------------------------------------------------------------
	//! Override to handle the result without subscribing.
	void OnResult(MRX_ETxStatus status)
	{
		if (m_OnResult)
			m_OnResult.Invoke(status);
	}
}
