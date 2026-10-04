//! Currencies and retention limits shared by the economy service and the storage backends.
class MRX_WalletRules : Managed
{
	ref MRX_CurrencyRegistry m_Currencies;
	//! Ledger entries kept per wallet by local backends.
	int m_iMaxRecentEntries = 50;
	//! Idempotency keys kept per wallet by local backends. Older keys are no longer detected as duplicates.
	int m_iMaxRecentKeys = 200;

	//------------------------------------------------------------------------------------------------
	static MRX_WalletRules Create(notnull MRX_CurrencyRegistry currencies, int maxRecentEntries = 50, int maxRecentKeys = 200)
	{
		MRX_WalletRules rules = new MRX_WalletRules();
		rules.m_Currencies = currencies;
		rules.m_iMaxRecentEntries = Math.MaxInt(maxRecentEntries, 0);
		rules.m_iMaxRecentKeys = Math.MaxInt(maxRecentKeys, 1);
		return rules;
	}
}
