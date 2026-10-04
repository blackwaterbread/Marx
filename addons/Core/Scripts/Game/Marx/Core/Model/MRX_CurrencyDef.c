//! Currency definition (API v0).
[BaseContainerProps(), BaseContainerCustomTitleField("m_sId")]
class MRX_CurrencyDef
{
	[Attribute("cash", desc: "Currency ID used in API calls and storage keys. Do not change once data exists.")]
	string m_sId;

	[Attribute(desc: "Display name")]
	LocalizedString m_sName;

	[Attribute(params: "edds imageset", uiwidget: UIWidgets.ResourcePickerThumbnail)]
	ResourceName m_sIcon;

	[Attribute("0", desc: "Balance of an owner that never used this currency")]
	int m_iInitialBalance;

	[Attribute("2147483647", desc: "Upper balance limit. With negative balances allowed, the lower limit is -MaxBalance.")]
	int m_iMaxBalance;

	[Attribute("0")]
	bool m_bAllowNegative;

	//------------------------------------------------------------------------------------------------
	static MRX_CurrencyDef Create(string id, int initialBalance = 0, int maxBalance = 2147483647, bool allowNegative = false)
	{
		MRX_CurrencyDef def = new MRX_CurrencyDef();
		def.m_sId = id;
		def.m_iInitialBalance = initialBalance;
		def.m_iMaxBalance = maxBalance;
		def.m_bAllowNegative = allowNegative;
		return def;
	}

	//------------------------------------------------------------------------------------------------
	//! Lowest allowed balance.
	int GetMinBalance()
	{
		if (m_bAllowNegative)
			return -m_iMaxBalance;

		return 0;
	}
}
