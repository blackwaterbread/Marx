//! Result of a balance change (API v0).
class MRX_TxResult : Managed
{
	MRX_ETxStatus m_eStatus;
	string m_sTxId;
	//! One entry per posting on OK. On DUPLICATE, the original entries that are still retained.
	ref array<ref MRX_LedgerEntry> m_aEntries = {};

	//------------------------------------------------------------------------------------------------
	static MRX_TxResult Create(MRX_ETxStatus status, string txId = string.Empty)
	{
		MRX_TxResult result = new MRX_TxResult();
		result.m_eStatus = status;
		result.m_sTxId = txId;
		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the transaction is committed, either now (OK) or by an earlier request (DUPLICATE).
	bool IsCommitted()
	{
		return m_eStatus == MRX_ETxStatus.OK || m_eStatus == MRX_ETxStatus.DUPLICATE;
	}
}
