//! Unit of work handed to a storage backend. All postings are applied atomically or not at all.
class MRX_TxRequest : Managed
{
	//! Server-generated transaction ID.
	string m_sTxId;
	//! Credit/Debit = 1 posting, Transfer = 2 postings.
	ref array<ref MRX_Posting> m_aPostings = {};
	ref MRX_TxContext m_Context;
	//! Unix time (seconds).
	int m_iTimestamp;

	//------------------------------------------------------------------------------------------------
	//! Distinct owner IDs in posting order.
	int GetOwnerIds(notnull array<string> outOwnerIds)
	{
		outOwnerIds.Clear();
		foreach (MRX_Posting posting : m_aPostings)
		{
			if (!outOwnerIds.Contains(posting.m_sOwnerId))
				outOwnerIds.Insert(posting.m_sOwnerId);
		}

		return outOwnerIds.Count();
	}
}
