#ifdef WORKBENCH
//! Workbench only: debug actions of the "Sample" page, with the test classes of Marx Example (a sample shop product,
//! test prices), and the test owner note of the debug panel.

//------------------------------------------------------------------------------------------------
modded class MRX_DebugRegistry
{
	//------------------------------------------------------------------------------------------------
	override protected void RegisterActions()
	{
		super.RegisterActions();
		Register(new MRX_DebugSampleProduct());
		Register(new MRX_DebugSamplePrices());
	}

	//------------------------------------------------------------------------------------------------
	override string DescribeOwner(int playerId, string ownerId)
	{
		string note = super.DescribeOwner(playerId, ownerId);
		if (!note.IsEmpty() || ownerId.IsEmpty() || !System.IsCLIParam(MRX_TestRunner.TEST_IDENTITY_PARAM))
			return note;

		// As MRX_TestIdentity derives it.
		string testOwnerId = PersistenceIdUtils.FromString("marx-test-identity:" + GetGame().GetPlayerManager().GetPlayerName(playerId));
		if (ownerId == testOwnerId)
			return "test owner";

		return note;
	}
}

//------------------------------------------------------------------------------------------------
//! Adds the test product "upgrade" (MRX_TestShopProduct, level 3 of 8) to the catalog of the nearest shop, on the server
//! and on this machine. Then open the shop with shop.open.
class MRX_DebugSampleProduct : MRX_DebugAction
{
	protected static const string ITEM_ID = "upgrade";
	protected static const ResourceName PREVIEW = "{06B68C58B72EAAC6}Prefabs/Items/Equipment/Backpacks/Backpack_ALICE_Medium.et";
	protected static const int PRICE = 1000000;

	//------------------------------------------------------------------------------------------------
	void MRX_DebugSampleProduct()
	{
		Setup("sample.product", "Sample", "Add product");
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

		IEntity shop = MRX_DebugActionUtils.FindNear(context.m_Character.GetOrigin(), MRX_DebugShopOpen.SEARCH_RADIUS, MRX_ShopComponent);
		if (!shop || !AddProduct(shop))
		{
			reply.Done(MRX_DebugResult.Failed("No shop with a catalog nearby (shop.open spawns one)"));
			return;
		}

		reply.Done(MRX_DebugResult.Ok(MRX_DebugActionUtils.GetPrefabFileName(shop)).SetEntity(shop));
	}

	//------------------------------------------------------------------------------------------------
	override MRX_DebugResult RunClient(notnull MRX_DebugContext context, notnull MRX_DebugResult serverResult)
	{
		// A host shares the catalog with the server: AddProduct finds the item and adds nothing.
		if (!context.m_Entity || !AddProduct(context.m_Entity))
			return MRX_DebugResult.Failed("The shop is not here");

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return False when the entity has no shop catalog.
	protected static bool AddProduct(notnull IEntity entity)
	{
		MRX_ShopComponent shop = MRX_ShopComponent.Cast(entity.FindComponent(MRX_ShopComponent));
		if (!shop || !shop.GetDefinition() || !shop.GetDefinition().m_Catalog)
			return false;

		MRX_ShopCatalog catalog = shop.GetDefinition().m_Catalog;
		if (catalog.FindItem(ITEM_ID))
			return true;

		MRX_TestShopProduct product = new MRX_TestShopProduct();
		product.m_iLevel = 3;
		product.m_iMax = 8;
		MRX_ShopItem item = MRX_ShopItem.Create(ITEM_ID, PREVIEW, PRICE);
		item.m_sName = "Stash Expansion";
		item.m_sCategory = "Services";
		item.m_sDescription = "One more stash page (6 x 8 cells)";
		item.m_Product = product;
		if (!catalog.m_aItems)
			catalog.m_aItems = {};

		catalog.m_aItems.InsertAt(item, 0);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Sets the test prices of the loadout tests as Marx's price list when there is none, so loadouts work.
class MRX_DebugSamplePrices : MRX_DebugAction
{
	//------------------------------------------------------------------------------------------------
	void MRX_DebugSamplePrices()
	{
		Setup("sample.prices", "Sample", "Set test prices");
	}

	//------------------------------------------------------------------------------------------------
	override bool HasServerPart()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void RunServer(notnull MRX_DebugContext context, notnull MRX_DebugReply reply)
	{
		if (!MRX_MarxSystem.GetInstance())
		{
			reply.Done(MRX_DebugResult.Failed("The Marx system is not running"));
			return;
		}

		if (MRX_Marx.GetPriceList())
		{
			reply.Done(MRX_DebugResult.Ok("A price list is already set; kept"));
			return;
		}

		MRX_Marx.SetPriceList(MRX_LoadoutTests.CreatePrices());
		reply.Done(MRX_DebugResult.Ok("Test prices set"));
	}
}
#endif
