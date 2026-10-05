enum MRX_EBackendType
{
	IN_MEMORY,
	NATIVE
}

//! Server settings (API v0). The default file ships with Marx_Core; server owners and consumers change it with Override in addon.
[BaseContainerProps(configRoot: true)]
class MRX_Settings
{
	static const string DEFAULT_CURRENCY = "cash";

	[Attribute(MRX_EBackendType.NATIVE.ToString(), UIWidgets.ComboBox, "Wallet storage. NATIVE falls back to IN_MEMORY when the world has no Marx persistence config. IN_MEMORY is lost when the server stops.", enums: ParamEnumArray.FromEnum(MRX_EBackendType))]
	MRX_EBackendType m_eBackend;

	[Attribute("50", desc: "Ledger entries kept per wallet by local backends")]
	int m_iMaxRecentEntries;

	[Attribute("200", desc: "Idempotency keys kept per wallet by local backends. Older keys are no longer detected as duplicates.")]
	int m_iMaxRecentKeys;

	[Attribute("100", desc: "Assets (stashed and deployed) per stash. 0 = no limit.")]
	int m_iMaxStashAssets;

	[Attribute(desc: "What happens to deployed assets on death and after a restart. Empty = keep on death, restore after a restart.")]
	ref MRX_LossPolicy m_LossPolicy;

	[Attribute(desc: "Currencies. Empty = a single 'cash' currency.")]
	ref array<ref MRX_CurrencyDef> m_aCurrencies;

	//------------------------------------------------------------------------------------------------
	//! Settings used when no config is set. Attribute defaults only apply to instances loaded from a config.
	static MRX_Settings CreateDefault()
	{
		MRX_Settings settings = new MRX_Settings();
		settings.m_eBackend = MRX_EBackendType.NATIVE;
		settings.m_iMaxRecentEntries = 50;
		settings.m_iMaxRecentKeys = 200;
		settings.m_iMaxStashAssets = 100;
		return settings;
	}

	//------------------------------------------------------------------------------------------------
	MRX_LossPolicy GetLossPolicy()
	{
		if (m_LossPolicy)
			return m_LossPolicy;

		return MRX_LossPolicy.Create();
	}

	//------------------------------------------------------------------------------------------------
	MRX_StorageRules CreateRules()
	{
		MRX_CurrencyRegistry currencies = new MRX_CurrencyRegistry();
		bool configured;
		if (m_aCurrencies)
			configured = !m_aCurrencies.IsEmpty();

		if (configured)
		{
			foreach (MRX_CurrencyDef def : m_aCurrencies)
			{
				currencies.Register(def);
			}
		}

		array<string> ids = {};
		if (currencies.GetIds(ids) == 0)
		{
			if (configured)
				Print(string.Format("[MRX] No valid currency configured, using '%1'", DEFAULT_CURRENCY), LogLevel.WARNING);

			currencies.Register(MRX_CurrencyDef.Create(DEFAULT_CURRENCY));
		}

		return MRX_StorageRules.Create(currencies, m_iMaxRecentEntries, m_iMaxRecentKeys, m_iMaxStashAssets);
	}
}
