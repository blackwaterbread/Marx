//! Immutable record of one posting after it was applied (API v0).
class MRX_LedgerEntry : Managed
{
	string m_sTxId;
	string m_sOwnerId;
	string m_sCurrency;
	int m_iDelta;
	int m_iBalanceAfter;
	int m_iTimestamp;
	string m_sSource;
	string m_sReason;
	string m_sIdempotencyKey;
	//! Other side of a transfer, otherwise empty.
	string m_sCounterpartyOwnerId;

	//------------------------------------------------------------------------------------------------
	MRX_LedgerEntry Copy()
	{
		MRX_LedgerEntry entry = new MRX_LedgerEntry();
		entry.m_sTxId = m_sTxId;
		entry.m_sOwnerId = m_sOwnerId;
		entry.m_sCurrency = m_sCurrency;
		entry.m_iDelta = m_iDelta;
		entry.m_iBalanceAfter = m_iBalanceAfter;
		entry.m_iTimestamp = m_iTimestamp;
		entry.m_sSource = m_sSource;
		entry.m_sReason = m_sReason;
		entry.m_sIdempotencyKey = m_sIdempotencyKey;
		entry.m_sCounterpartyOwnerId = m_sCounterpartyOwnerId;
		return entry;
	}
}
