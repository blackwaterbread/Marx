[ComponentEditorProps(category: "Marx", description: "Makes the entity a shop. The entity also needs an RplComponent and MRX_OpenShopAction in its ActionsManagerComponent.")]
class MRX_ShopComponentClass : ScriptComponentClass
{
}

//! Turns any entity (prop, NPC) into a shop (API v0). Clients read the same catalog for the UI; the server re-validates everything.
class MRX_ShopComponent : ScriptComponent
{
	[Attribute(desc: "Stable shop ID, written to the ledger")]
	protected string m_sShopId;

	[Attribute(params: "conf class=MRX_ShopCatalog", desc: "Items this shop sells and buys back")]
	protected ResourceName m_sCatalog;

	[Attribute("50", UIWidgets.Slider, "Percentage of the price paid when buying an item back", "0 100 1")]
	protected int m_iSellPercent;

	[Attribute("1", desc: "Buy items back from players")]
	protected bool m_bAllowSell;

	[Attribute("5", UIWidgets.Slider, "Maximum distance in meters between the player and the shop for trading", "1 50 0.5")]
	protected float m_fMaxDistance;

	protected ref MRX_ShopDefinition m_Definition;

	//------------------------------------------------------------------------------------------------
	//! \return Null when the catalog cannot be loaded.
	MRX_ShopDefinition GetDefinition()
	{
		if (m_Definition)
			return m_Definition;

		MRX_ShopCatalog catalog = SCR_ConfigHelperT<MRX_ShopCatalog>.GetConfigObject(m_sCatalog);
		if (!catalog)
		{
			Print(string.Format("[MRX] Shop %1: could not load catalog %2", GetShopId(), m_sCatalog), LogLevel.ERROR);
			return null;
		}

		MRX_CurrencyRegistry currencies;
		MRX_EconomyService economy = MRX_Marx.GetEconomy();
		if (economy)
			currencies = economy.GetRules().m_Currencies;

		catalog.Validate(currencies, m_sCatalog);
		m_Definition = MRX_ShopDefinition.Create(GetShopId(), catalog, m_iSellPercent, m_bAllowSell);
		return m_Definition;
	}

	//------------------------------------------------------------------------------------------------
	string GetShopId()
	{
		if (!m_sShopId.IsEmpty())
			return m_sShopId;

		return "shop";
	}

	//------------------------------------------------------------------------------------------------
	float GetMaxDistance()
	{
		return m_fMaxDistance;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the entity is within trading distance of the shop.
	bool IsInRange(IEntity entity)
	{
		if (!entity)
			return false;

		return vector.DistanceSq(entity.GetOrigin(), GetOwner().GetOrigin()) <= m_fMaxDistance * m_fMaxDistance;
	}
}
