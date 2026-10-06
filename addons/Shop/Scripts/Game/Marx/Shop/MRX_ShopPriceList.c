//! Item prices taken from shops (API v0), e.g. for MRX_Marx.SetPriceList(): the first added shop that sells an item in
//! the list's currency sets its buy price, the first that buys it back its sell price. Products are left out.
class MRX_ShopPriceList : MRX_PriceList
{
	protected string m_sCurrency;
	//! Prefab key (MRX_ShopCatalog.GetPrefabKey) -> price.
	protected ref map<string, int> m_mBuyPrices = new map<string, int>();
	protected ref map<string, int> m_mSellPrices = new map<string, int>();

	//------------------------------------------------------------------------------------------------
	void MRX_ShopPriceList(string currency)
	{
		m_sCurrency = currency;
	}

	//------------------------------------------------------------------------------------------------
	void AddShop(notnull MRX_ShopDefinition shop)
	{
		if (!shop.m_Catalog || !shop.m_Catalog.m_aItems)
			return;

		foreach (MRX_ShopItem item : shop.m_Catalog.m_aItems)
		{
			if (item.m_Product || item.m_sPrefab.IsEmpty() || item.m_sCurrency != m_sCurrency)
				continue;

			string key = MRX_ShopCatalog.GetPrefabKey(item.m_sPrefab);
			if (item.m_iPrice > 0 && !m_mBuyPrices.Contains(key))
				m_mBuyPrices.Set(key, item.m_iPrice);

			int sellPrice = shop.GetSellPrice(item);
			if (sellPrice > 0 && !m_mSellPrices.Contains(key))
				m_mSellPrices.Set(key, sellPrice);
		}
	}

	//------------------------------------------------------------------------------------------------
	override string GetCurrency()
	{
		return m_sCurrency;
	}

	//------------------------------------------------------------------------------------------------
	override int GetBuyPrice(ResourceName prefab)
	{
		return m_mBuyPrices.Get(MRX_ShopCatalog.GetPrefabKey(prefab));
	}

	//------------------------------------------------------------------------------------------------
	override int GetSellPrice(ResourceName prefab)
	{
		return m_mSellPrices.Get(MRX_ShopCatalog.GetPrefabKey(prefab));
	}
}
