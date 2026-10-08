//! Debug actions of the "Shop" page.

//------------------------------------------------------------------------------------------------
modded class MRX_DebugRegistry
{
	//------------------------------------------------------------------------------------------------
	override protected void RegisterActions()
	{
		super.RegisterActions();
		Register(new MRX_DebugShopOpen());
	}
}

//------------------------------------------------------------------------------------------------
//! Opens the shop window of the nearest shop within SEARCH_RADIUS, spawning the sample shop table in front of the
//! player when there is none.
class MRX_DebugShopOpen : MRX_DebugAction
{
	static const float SEARCH_RADIUS = 30;
	protected static const ResourceName SHOP_PREFAB = "{10C12BB88B37C571}Prefabs/Marx/Shop/MRX_ShopTable.et";
	protected static const float SPAWN_DISTANCE = 1.5;

	//------------------------------------------------------------------------------------------------
	void MRX_DebugShopOpen()
	{
		Setup("shop.open", "Shop", "Open shop");
	}

	//------------------------------------------------------------------------------------------------
	override bool HasServerPart()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasClientPart()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void RunServer(notnull MRX_DebugContext context, notnull MRX_DebugReply reply)
	{
		if (!context.m_Character)
		{
			reply.Done(MRX_DebugActionUtils.NoCharacter());
			return;
		}

		string found = "nearby shop";
		IEntity shop = MRX_DebugActionUtils.FindNear(context.m_Character.GetOrigin(), SEARCH_RADIUS, MRX_ShopComponent);
		if (!shop)
		{
			found = "spawned the sample shop table";
			shop = MRX_DebugActionUtils.SpawnInFront(context.m_Character, SHOP_PREFAB, SPAWN_DISTANCE);
		}

		if (!shop || !shop.FindComponent(MRX_ShopComponent))
		{
			reply.Done(MRX_DebugResult.Failed("No shop nearby and the sample shop table did not spawn"));
			return;
		}

		reply.Done(MRX_DebugResult.Ok(string.Format("%1 %2", found, MRX_DebugActionUtils.GetPrefabFileName(shop))).SetEntity(shop));
	}

	//------------------------------------------------------------------------------------------------
	override MRX_DebugResult RunClient(notnull MRX_DebugContext context, notnull MRX_DebugResult serverResult)
	{
		MRX_ShopComponent shop;
		if (context.m_Entity)
			shop = MRX_ShopComponent.Cast(context.m_Entity.FindComponent(MRX_ShopComponent));

		if (!shop)
			return MRX_DebugResult.Failed("The shop is not here");

		MRX_ShopMenu.Open(shop);
		return null;
	}
}
