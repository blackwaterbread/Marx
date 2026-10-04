//! Signed balance change of one owner in one currency.
class MRX_Posting : Managed
{
	string m_sOwnerId;
	string m_sCurrency;
	int m_iDelta;

	//------------------------------------------------------------------------------------------------
	static MRX_Posting Create(string ownerId, string currency, int delta)
	{
		MRX_Posting posting = new MRX_Posting();
		posting.m_sOwnerId = ownerId;
		posting.m_sCurrency = currency;
		posting.m_iDelta = delta;
		return posting;
	}
}
