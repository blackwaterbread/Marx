//! Adds the shop service to the Marx system (server).
modded class MRX_MarxSystem
{
	protected ref MRX_ShopService m_MRX_ShopService;

	//------------------------------------------------------------------------------------------------
	override event protected void OnInit()
	{
		super.OnInit();
		if (GetEconomy() && GetIdentity())
			m_MRX_ShopService = new MRX_ShopService(GetEconomy(), GetIdentity(), new MRX_EntityShopInventory());
	}

	//------------------------------------------------------------------------------------------------
	override event protected void OnCleanup()
	{
		m_MRX_ShopService = null;
		super.OnCleanup();
	}

	//------------------------------------------------------------------------------------------------
	MRX_ShopService MRX_GetShopService()
	{
		return m_MRX_ShopService;
	}
}

//! Static access to the shop service (API v0). Server only; null on clients.
class MRX_Shop
{
	//------------------------------------------------------------------------------------------------
	static MRX_ShopService GetService()
	{
		MRX_MarxSystem system = MRX_MarxSystem.GetInstance();
		if (!system)
			return null;

		return system.MRX_GetShopService();
	}
}
