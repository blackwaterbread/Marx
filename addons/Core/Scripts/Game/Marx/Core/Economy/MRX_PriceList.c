//! Item prices for features that buy or sell items outside a shop (API v0), e.g. loading a saved loadout. One currency.
//! Marx_Shop builds one from shop catalogs (MRX_ShopPriceList); consumers set it with MRX_Marx.SetPriceList().
class MRX_PriceList : Managed
{
	//------------------------------------------------------------------------------------------------
	string GetCurrency()
	{
		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Price of a new item, 0 when it cannot be bought.
	int GetBuyPrice(ResourceName prefab)
	{
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! \return What a player gets for the item, 0 when it is not bought back.
	int GetSellPrice(ResourceName prefab)
	{
		return 0;
	}
}
