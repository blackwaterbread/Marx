#ifdef WORKBENCH
//! Workbench only: debug actions of the "Sample" page, with the test classes of Marx Example (a sample shop product,
//! test prices, an arsenal with a weapon catalog), and the test owner note of the debug panel.

//------------------------------------------------------------------------------------------------
modded class MRX_DebugRegistry
{
	//------------------------------------------------------------------------------------------------
	override protected void RegisterActions()
	{
		super.RegisterActions();
		Register(new MRX_DebugSampleProduct());
		Register(new MRX_DebugSamplePrices());
		Register(new MRX_DebugSampleArsenal());
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

//------------------------------------------------------------------------------------------------
//! For trying the arsenal trade by hand: gives a player without a character (Game Master) one at the camera, puts the
//! sample arsenal box in front of the character (or takes the nearest Marx arsenal), gives it a weapon catalog, puts a
//! few items of the player's own to sell into the inventory and opens the inventory at the arsenal.
class MRX_DebugSampleArsenal : MRX_DebugAction
{
	protected static const float SPAWN_DISTANCE = 2;
	protected static const ResourceName BINOCULARS = "{0CF54B9A85D8E0D4}Prefabs/Items/Equipment/Binoculars/Binoculars_M22/Binoculars_M22.et";
	protected static const ResourceName BACKPACK = "{06B68C58B72EAAC6}Prefabs/Items/Equipment/Backpacks/Backpack_ALICE_Medium.et";
	protected static const ResourceName TOURNIQUET = "{D70216B1B2889129}Prefabs/Items/Medicine/Tourniquet_01/Tourniquet_US_01.et";

	//------------------------------------------------------------------------------------------------
	void MRX_DebugSampleArsenal()
	{
		Setup("sample.arsenal", "Sample", "Arsenal trade");
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
		IEntity character = context.m_Character;
		if (!character && context.m_Controller)
		{
			character = MRX_TestWorldUtils.SpawnPrefab(MRX_TestWorldUtils.CHARACTER_PREFAB, MRX_TestWorldUtils.GetCameraPosition());
			if (character)
				context.m_Controller.SetPossessedEntity(character);
		}

		if (!character)
		{
			reply.Done(MRX_DebugActionUtils.NoCharacter());
			return;
		}

		IEntity arsenal = MRX_DebugActionUtils.FindNear(character.GetOrigin(), MRX_DebugShopOpen.SEARCH_RADIUS, MRX_ArsenalShopComponent);
		if (!arsenal)
			arsenal = MRX_DebugActionUtils.SpawnInFront(character, MRX_Test_ArsenalShopFlow.ARSENAL_PREFAB, SPAWN_DISTANCE);

		if (!arsenal || !SetCatalog(arsenal))
		{
			reply.Done(MRX_DebugResult.Failed("No Marx arsenal nearby and the sample arsenal box did not spawn"));
			return;
		}

		// The player's own items, to sell.
		InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(character.FindComponent(SCR_InventoryStorageManagerComponent));
		if (manager)
		{
			manager.TrySpawnPrefabToStorage(MRX_Test_ArsenalShopFlow.RIFLE);
			manager.TrySpawnPrefabToStorage(MRX_Test_ArsenalShopFlow.BANDAGE);
			manager.TrySpawnPrefabToStorage(BINOCULARS);
		}

		reply.Done(MRX_DebugResult.Ok(MRX_DebugActionUtils.GetPrefabFileName(arsenal)).SetEntity(arsenal));
	}

	//------------------------------------------------------------------------------------------------
	override MRX_DebugResult RunClient(notnull MRX_DebugContext context, notnull MRX_DebugResult serverResult)
	{
		if (!context.m_Entity || !SetCatalog(context.m_Entity))
			return MRX_DebugResult.Failed("The arsenal is not here");

		IEntity character = SCR_PlayerController.GetLocalControlledEntity();
		SCR_InventoryStorageManagerComponent manager;
		if (character)
			manager = SCR_InventoryStorageManagerComponent.Cast(character.FindComponent(SCR_InventoryStorageManagerComponent));

		if (!manager)
			return MRX_DebugResult.Failed("No character to open the inventory with");

		manager.SetStorageToOpen(context.m_Entity);
		manager.OpenInventory();
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Rifle, scope, magazines, medical items and a backpack for sale; binoculars bought back only. Same on server and
	//! client. \return False without a shop.
	protected static bool SetCatalog(notnull IEntity arsenal)
	{
		MRX_ShopComponent shop = MRX_ShopComponent.Cast(arsenal.FindComponent(MRX_ShopComponent));
		if (!shop)
			return false;

		if (shop.GetDefinition() && shop.GetDefinition().m_sShopId == "sample_arsenal_trade")
			return true;

		MRX_ShopCatalog catalog = new MRX_ShopCatalog();
		catalog.m_aItems = {};
		AddItem(catalog, "rifle", MRX_Test_ArsenalShopFlow.RIFLE, 1000, "Rifles");
		AddItem(catalog, "scope", MRX_Test_ArsenalShopFlow.OPTIC, 400, "Optics");
		AddItem(catalog, "magazine_ball", MRX_Test_ArsenalShopFlow.MAGAZINE_BALL, 20, "Ammunition");
		AddItem(catalog, "magazine_tracer", MRX_Test_ArsenalShopFlow.MAGAZINE_TRACER, 25, "Ammunition");
		AddItem(catalog, "bandage", MRX_Test_ArsenalShopFlow.BANDAGE, 10, "Medical");
		AddItem(catalog, "tourniquet", TOURNIQUET, 40, "Medical");
		AddItem(catalog, "backpack", BACKPACK, 300, "Gear");
		MRX_ShopItem binoculars = MRX_ShopItem.Create("binoculars", BINOCULARS, 0, MRX_Settings.DEFAULT_CURRENCY, 75);
		binoculars.m_sCategory = "Gear";
		catalog.m_aItems.Insert(binoculars);
		shop.SetDefinition(MRX_ShopDefinition.Create("sample_arsenal_trade", catalog, 50));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddItem(notnull MRX_ShopCatalog catalog, string id, ResourceName prefab, int price, string category)
	{
		MRX_ShopItem item = MRX_ShopItem.Create(id, prefab, price, MRX_Settings.DEFAULT_CURRENCY);
		item.m_sCategory = category;
		catalog.m_aItems.Insert(item);
	}
}
#endif
