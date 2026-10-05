//! Runtime settings of one shop (API v0). Built by MRX_ShopComponent, or directly by consumers.
class MRX_ShopDefinition : Managed
{
	static const int DEFAULT_SELL_PERCENT = 50;

	//! Stable ID, written to the ledger reason.
	string m_sShopId;
	ref MRX_ShopCatalog m_Catalog;
	//! Percentage of the price paid when buying an item back (items without their own sell price).
	int m_iSellPercent = DEFAULT_SELL_PERCENT;
	bool m_bAllowSell = true;

	//------------------------------------------------------------------------------------------------
	static MRX_ShopDefinition Create(string shopId, notnull MRX_ShopCatalog catalog, int sellPercent = DEFAULT_SELL_PERCENT, bool allowSell = true)
	{
		MRX_ShopDefinition shop = new MRX_ShopDefinition();
		shop.m_sShopId = shopId;
		shop.m_Catalog = catalog;
		shop.m_iSellPercent = Math.ClampInt(sellPercent, 0, 100);
		shop.m_bAllowSell = allowSell;
		return shop;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Price paid for buying the item back, 0 when the shop does not buy it.
	int GetSellPrice(notnull MRX_ShopItem item)
	{
		if (!m_bAllowSell)
			return 0;

		if (item.m_iSellPrice >= 0)
			return item.m_iSellPrice;

		return Percent(item.m_iPrice, m_iSellPercent);
	}

	//------------------------------------------------------------------------------------------------
	//! value * percent / 100, rounded down, without int overflow. Negative values give 0.
	static int Percent(int value, int percent)
	{
		if (value <= 0 || percent <= 0)
			return 0;

		percent = Math.ClampInt(percent, 0, 100);
		return (value / 100) * percent + (value % 100) * percent / 100;
	}
}
