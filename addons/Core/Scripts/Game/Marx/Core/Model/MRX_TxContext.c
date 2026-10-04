//! Caller-supplied metadata for a balance change (API v0).
class MRX_TxContext : Managed
{
	//! Identifier of the calling mod or system, e.g. "pve_ops".
	string m_sSource;
	//! Free-form reason recorded in the ledger, e.g. "op_complete:destroy_cache".
	string m_sReason;
	//! Required. Retrying with the same source and key returns the original result instead of applying twice.
	string m_sIdempotencyKey;

	//------------------------------------------------------------------------------------------------
	static MRX_TxContext Create(string source, string reason, string idempotencyKey)
	{
		MRX_TxContext context = new MRX_TxContext();
		context.m_sSource = source;
		context.m_sReason = reason;
		context.m_sIdempotencyKey = idempotencyKey;
		return context;
	}

	//------------------------------------------------------------------------------------------------
	bool IsValid()
	{
		return !m_sSource.IsEmpty() && !m_sIdempotencyKey.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	MRX_TxContext Copy()
	{
		return Create(m_sSource, m_sReason, m_sIdempotencyKey);
	}
}
