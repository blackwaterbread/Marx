#ifdef WORKBENCH
//! Workbench only: opens Marx windows on request, for looking at them while nobody types in the game (automation
//! cannot send keys to the game view). Every half second the server reads one command line from
//! $profile:mrx_ui_cmd.txt and deletes the file:
//! - "shop": spawns the sample shop table next to the player, adds a sample product and opens the shop window;
//! - "stash": spawns a stash wardrobe next to the player and opens the stash;
//! - "loadout <slot>": saves the player's gear into the slot (0-based) of the open stash, with test prices;
//! - "loadouts": opens the loadout window of the open stash;
//! - "addslot": gives the player one more loadout slot (shown from the next opening of the stash);
//! - "load <slot>": puts the slot's loadout on as the Load button of the loadout window does;
//! - "tostash": moves the weapon in the player's hands into the open stash;
//! - "give": spawns a compass into the player's inventory;
//! - "lang <code>": switches the UI language, e.g. "lang ko_kr";
//! - "close": closes open menus.
class MRX_TestUiCommands
{
	static const string FILE = "$profile:mrx_ui_cmd.txt";
	protected static const int POLL_MS = 500;
	protected static const ResourceName SHOP_PREFAB = "{10C12BB88B37C571}Prefabs/Marx/Shop/MRX_ShopTable.et";
	protected static const ResourceName STASH_PREFAB = "{66BACE8BD545B8C2}Prefabs/Marx/Stash/MRX_StashWardrobe.et";
	protected static const ResourceName PRODUCT_PREVIEW = "{06B68C58B72EAAC6}Prefabs/Items/Equipment/Backpacks/Backpack_ALICE_Medium.et";

	protected static IEntity s_Shop;
	protected static IEntity s_Stash;

	//------------------------------------------------------------------------------------------------
	static void Start()
	{
		Print("[MRX_UI] waiting for commands in " + FILE);
		GetGame().GetCallqueue().Remove(Poll);
		GetGame().GetCallqueue().CallLater(Poll, POLL_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Poll()
	{
		if (!FileIO.FileExists(FILE))
			return;

		FileHandle file = FileIO.OpenFile(FILE, FileMode.READ);
		string line;
		if (file)
		{
			file.ReadLine(line);
			file.Close();
		}

		FileIO.DeleteFile(FILE);
		line.Trim();
		Print("[MRX_UI] command: " + line);
		array<string> words = {};
		line.Split(" ", words, true);
		if (words.IsEmpty())
			return;

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		IEntity character;
		if (controller)
			character = controller.GetControlledEntity();

		switch (words[0])
		{
			case "shop": OpenShop(character); break;
			case "stash": OpenStash(controller, character); break;
			case "loadout": SaveLoadout(controller, words); break;
			case "loadouts": MRX_LoadoutMenu.Open(); break;
			case "addslot": AddLoadoutSlot(controller); break;
			case "load": LoadLoadout(words); break;
			case "tostash": MoveWeaponToStash(controller, character); break;
			case "give": GiveCompass(character); break;
			case "lang": if (words.Count() > 1) WidgetManager.SetLanguage(words[1]); break;
			case "close": GetGame().GetMenuManager().CloseAllMenus(); break;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void OpenShop(IEntity character)
	{
		if (!character)
			return;

		if (s_Shop)
			SCR_EntityHelper.DeleteEntityAndChildren(s_Shop);

		s_Shop = MRX_TestWorldUtils.SpawnPrefab(SHOP_PREFAB, character.GetOrigin() + character.GetTransformAxis(2) * 1.5);
		MRX_ShopComponent shop = MRX_ShopComponent.Cast(s_Shop.FindComponent(MRX_ShopComponent));
		if (!shop || !shop.GetDefinition())
			return;

		MRX_ShopDefinition definition = shop.GetDefinition();
		if (!definition.m_Catalog.FindItem("upgrade"))
		{
			MRX_TestShopProduct product = new MRX_TestShopProduct();
			product.m_iLevel = 3;
			product.m_iMax = 8;
			MRX_ShopItem item = MRX_ShopItem.Create("upgrade", PRODUCT_PREVIEW, 1000000);
			item.m_sName = "Stash Expansion";
			item.m_sCategory = "Services";
			item.m_sDescription = "One more stash page (6 x 8 cells)";
			item.m_Product = product;
			definition.m_Catalog.m_aItems.InsertAt(item, 0);
		}

		MRX_ShopMenu.Open(shop);
	}

	//------------------------------------------------------------------------------------------------
	protected static void OpenStash(SCR_PlayerController controller, IEntity character)
	{
		if (!controller || !character)
			return;

		if (!s_Stash)
			s_Stash = MRX_TestWorldUtils.SpawnPrefab(STASH_PREFAB, character.GetOrigin() + character.GetTransformAxis(2) * 1.2);

		controller.MRX_RequestStashOpen(s_Stash);
	}

	//------------------------------------------------------------------------------------------------
	protected static void SaveLoadout(SCR_PlayerController controller, notnull array<string> words)
	{
		if (!controller || !MRX_Loadouts.Get())
			return;

		if (!MRX_Marx.GetPriceList())
			MRX_Marx.SetPriceList(MRX_LoadoutTests.CreatePrices());

		int slot;
		if (words.Count() > 1)
			slot = words[1].ToInt();

		MRX_Loadouts.Get().Save(controller.GetPlayerId(), slot, new MRX_LoadoutRpcReply(controller));
	}

	//------------------------------------------------------------------------------------------------
	//! As if the Load button of the slot was held in the loadout window (opened with "loadouts").
	protected static void LoadLoadout(notnull array<string> words)
	{
		MRX_LoadoutMenu menu = MRX_LoadoutMenu.GetOpen();
		if (!menu || !menu.GetBar())
			return;

		int slot;
		if (words.Count() > 1)
			slot = words[1].ToInt();

		menu.GetBar().RequestLoad(slot);
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddLoadoutSlot(SCR_PlayerController controller)
	{
		if (!controller)
			return;

		string ownerId = MRX_Marx.GetOwnerId(controller.GetPlayerId());
		if (ownerId.IsEmpty())
			return;

		MRX_TxContext context = MRX_TxContext.Create("test", "loadout slot", "test:" + MRX_Marx.NewId());
		MRX_LoadoutSlots.AddSlots(ownerId, 1, context, new MRX_LoadoutSlotsCallback());
	}

	//------------------------------------------------------------------------------------------------
	//! Moves the weapon in the player's hands into the open stash.
	protected static void MoveWeaponToStash(SCR_PlayerController controller, IEntity character)
	{
		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		MRX_StashSessionManager sessions = MRX_StashSessions.Get();
		if (!controller || !chimera || !sessions)
			return;

		MRX_StashSession session = sessions.Find(controller.GetPlayerId());
		BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(chimera.FindComponent(BaseWeaponManagerComponent));
		if (!session || !session.GetStorage() || !weapons || !weapons.GetCurrentWeapon())
			return;

		chimera.GetCharacterController().GetInventoryStorageManager().TryMoveItemToStorage(weapons.GetCurrentWeapon().GetOwner(), session.GetStorage());
	}

	//------------------------------------------------------------------------------------------------
	protected static void GiveCompass(IEntity character)
	{
		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if (chimera && chimera.GetCharacterController())
			chimera.GetCharacterController().GetInventoryStorageManager().TrySpawnPrefabToStorage(MRX_LoadoutTests.COMPASS);
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_BaseGameMode
{
	//------------------------------------------------------------------------------------------------
	override protected void OnGameStart()
	{
		super.OnGameStart();
		if (Replication.IsServer())
			MRX_TestUiCommands.Start();
	}
}
#endif
