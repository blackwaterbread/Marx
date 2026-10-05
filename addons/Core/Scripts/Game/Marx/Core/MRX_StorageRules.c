//! Currencies and limits shared by the economy and stash services and the storage backends.
class MRX_StorageRules : Managed
{
	ref MRX_CurrencyRegistry m_Currencies;
	//! Ledger entries kept per wallet by local backends.
	int m_iMaxRecentEntries = 50;
	//! Idempotency keys kept per wallet and per stash by local backends. Older keys are no longer detected as duplicates.
	int m_iMaxRecentKeys = 200;
	//! Assets (STASHED and DEPLOYED) per stash, 0 for no limit.
	int m_iMaxStashAssets = 100;

	//------------------------------------------------------------------------------------------------
	static MRX_StorageRules Create(notnull MRX_CurrencyRegistry currencies, int maxRecentEntries = 50, int maxRecentKeys = 200, int maxStashAssets = 100)
	{
		MRX_StorageRules rules = new MRX_StorageRules();
		rules.m_Currencies = currencies;
		rules.m_iMaxRecentEntries = Math.MaxInt(maxRecentEntries, 0);
		rules.m_iMaxRecentKeys = Math.MaxInt(maxRecentKeys, 1);
		rules.m_iMaxStashAssets = Math.MaxInt(maxStashAssets, 0);
		return rules;
	}
}
