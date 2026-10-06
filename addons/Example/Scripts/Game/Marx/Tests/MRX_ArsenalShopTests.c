#ifdef WORKBENCH
// End-to-end test of the Marx arsenal with the sample prefab on the local (host) player: item list, server request
// checks, purchase into a weapon's attachment slot, sale with contents and the arsenal panel of the vanilla inventory.

//------------------------------------------------------------------------------------------------
class MRX_ArsenalShopTests
{
	//------------------------------------------------------------------------------------------------
	static void Register(notnull MRX_TestRunner runner)
	{
		runner.Add(new MRX_Test_ArsenalShopFlow());
		runner.Add(new MRX_Test_ShopContentsCheck());
	}
}

//------------------------------------------------------------------------------------------------
//! A rifle that comes with a magazine is too cheap when its magazine sells for more than half its price.
class MRX_Test_ShopContentsCheck : MRX_TestCase
{
	protected static const string REPORT_FILE = "$profile:mrx_test_shop_contents.csv";

	protected ref MRX_ShopContentsCheck m_Check;

	//------------------------------------------------------------------------------------------------
	override int GetTimeoutMs()
	{
		return 10000;
	}

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		MRX_ShopCatalog catalog = new MRX_ShopCatalog();
		catalog.m_aItems = {};
		catalog.m_aItems.Insert(MRX_ShopItem.Create("rifle", MRX_Test_ArsenalShopFlow.RIFLE, 10, MRX_Settings.DEFAULT_CURRENCY));
		catalog.m_aItems.Insert(MRX_ShopItem.Create("magazine_tracer", MRX_Test_ArsenalShopFlow.MAGAZINE_TRACER, 20, MRX_Settings.DEFAULT_CURRENCY));
		catalog.m_aItems.Insert(MRX_ShopItem.Create("magazine_ball", MRX_Test_ArsenalShopFlow.MAGAZINE_BALL, 20, MRX_Settings.DEFAULT_CURRENCY));
		catalog.m_aItems.Insert(MRX_ShopItem.Create("bandage", MRX_Test_ArsenalShopFlow.BANDAGE, 10, MRX_Settings.DEFAULT_CURRENCY));
		catalog.m_aItems.Insert(MRX_ShopItem.Create("not_for_sale", MRX_Test_ArsenalShopFlow.OPTIC, 0, MRX_Settings.DEFAULT_CURRENCY));

		m_Check = new MRX_ShopContentsCheck();
		m_Check.GetOnDone().Insert(OnDone);
		Check(m_Check.Start(MRX_ShopDefinition.Create("contents_check", catalog, 50), MRX_TestWorldUtils.GetCameraPosition() + "0 300 0"), "check starts");
		Check(!m_Check.Start(MRX_ShopDefinition.Create("contents_check", catalog, 50), vector.Zero), "a second start is refused while running");
	}

	//------------------------------------------------------------------------------------------------
	protected void OnDone(array<ref MRX_ShopContentsEntry> entries)
	{
		CheckInt(entries.Count(), 4, "one entry per item for sale");
		foreach (MRX_ShopContentsEntry entry : entries)
		{
			Check(entry.m_bMeasured, entry.m_sItemId + " measured");
			if (entry.m_sItemId == "rifle")
			{
				Check(entry.m_iContentsCount >= 1, "rifle comes with contents");
				CheckInt(entry.m_aContents.Count(), entry.m_iContentsCount, "rifle lists its content prefabs");
				Check(entry.m_iContentsSell >= 10, "its magazine is worth at least 10");
				Check(entry.IsProfitable(), "rifle at 10 is profitable to sell back");
			}
			else if (entry.m_sItemId == "bandage")
			{
				CheckInt(entry.m_iContentsCount, 0, "bandage holds nothing");
				Check(!entry.IsProfitable(), "bandage is not profitable");
			}
		}

		Check(MRX_ShopContentsCheck.WriteReport(REPORT_FILE, entries), "report written");
		FileIO.DeleteFile(REPORT_FILE);
		Finish();
	}
}

//------------------------------------------------------------------------------------------------
class MRX_Test_ArsenalShopFlow : MRX_TestCase
{
	static const ResourceName ARSENAL_PREFAB = "{64FF7A025A74FDFA}Prefabs/Marx/Shop/MRX_ArsenalBox.et";
	static const ResourceName RIFLE = "{96DFD2E7E63B3386}Prefabs/Weapons/Rifles/AK74/Rifle_AK74N.et";
	static const ResourceName OPTIC = "{ACDF49FACD0701A8}Prefabs/Weapons/Attachments/Optics/Optic_1P29/Optic_1P29.et";
	static const ResourceName MAGAZINE_TRACER = "{0A84AA5A3884176F}Prefabs/Weapons/Magazines/Magazine_545x39_AK_30rnd_Last_5Tracer.et";
	static const ResourceName MAGAZINE_BALL = "{BBB50A815A2F916B}Prefabs/Weapons/Magazines/Magazine_545x39_AK_30rnd_Ball.et";
	static const ResourceName BANDAGE = "{A81F501D3EF6F38E}Prefabs/Items/Medicine/FieldDressing_01/FieldDressing_US_01.et";
	protected static const int STEP_MS = 500;
	protected static const int FUNDS = 5000;

	protected ref MRX_TestPossession m_Possession;
	protected IEntity m_Character;
	protected IEntity m_Arsenal;
	protected IEntity m_Other;
	protected IEntity m_Rifle;
	protected ref MRX_ShopDefinition m_Shop;
	protected ref MRX_ShopCallback m_Callback;
	protected string m_sOwnerId;
	protected int m_iPlayerId;
	protected string m_sStep;
	protected int m_iExpectedSale;
	protected int m_iExpectedCount;

	//------------------------------------------------------------------------------------------------
	override int GetTimeoutMs()
	{
		return 20000;
	}

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		m_Possession = MRX_TestPossession.Acquire();
		if (!m_Possession || !m_Possession.GetCharacter())
		{
			Skip("no local character");
			return;
		}

		m_Character = m_Possession.GetCharacter();
		m_iPlayerId = m_Possession.GetController().GetPlayerId();
		MRX_IdentityService identity = MRX_Marx.GetIdentity();
		if (identity)
			m_sOwnerId = identity.GetOwnerId(m_iPlayerId);

		if (m_sOwnerId.IsEmpty() || !MRX_Shop.GetService())
		{
			Skip("no owner or shop service");
			End(false);
			return;
		}

		m_Arsenal = MRX_TestWorldUtils.SpawnPrefab(ARSENAL_PREFAB, m_Character.GetOrigin() + "1.5 0 0");
		Check(m_Arsenal != null, "arsenal prefab spawns");
		if (!m_Arsenal)
		{
			End(false);
			return;
		}

		CheckPrefab();

		m_Shop = CreateShop();
		MRX_ShopComponent shopComponent = MRX_ShopComponent.Cast(m_Arsenal.FindComponent(MRX_ShopComponent));
		if (shopComponent)
			shopComponent.SetDefinition(m_Shop);

		CheckList();
		CheckStaysEnabled();
		CheckResupplyHidden();

		m_Callback = new MRX_ShopCallback();
		m_Callback.GetOnResult().Insert(OnResult);
		MRX_TxCallback funded = new MRX_TxCallback();
		funded.GetOnResult().Insert(OnFunded);
		MRX_Marx.GetEconomy().Credit(m_sOwnerId, MRX_Settings.DEFAULT_CURRENCY, FUNDS, MRX_TxContext.Create("test", "arsenal flow", MRX_NetworkTests.UniqueKey("arsenal")), funded);
	}

	//------------------------------------------------------------------------------------------------
	//! Rifle 1000, scope 400, magazines 20, bandage 10, bought back at half.
	protected MRX_ShopDefinition CreateShop()
	{
		MRX_ShopCatalog catalog = new MRX_ShopCatalog();
		catalog.m_aItems = {};
		catalog.m_aItems.Insert(MRX_ShopItem.Create("rifle", RIFLE, 1000, MRX_Settings.DEFAULT_CURRENCY));
		catalog.m_aItems.Insert(MRX_ShopItem.Create("scope", OPTIC, 400, MRX_Settings.DEFAULT_CURRENCY));
		catalog.m_aItems.Insert(MRX_ShopItem.Create("magazine_tracer", MAGAZINE_TRACER, 20, MRX_Settings.DEFAULT_CURRENCY));
		catalog.m_aItems.Insert(MRX_ShopItem.Create("magazine_ball", MAGAZINE_BALL, 20, MRX_Settings.DEFAULT_CURRENCY));
		catalog.m_aItems.Insert(MRX_ShopItem.Create("bandage", BANDAGE, 10, MRX_Settings.DEFAULT_CURRENCY));
		catalog.m_aItems.Insert(MRX_ShopItem.Create("not_for_sale", "{13772C903CB5E4F7}Prefabs/Items/Equipment/Maps/PaperMap_01_folded.et", 0, MRX_Settings.DEFAULT_CURRENCY));
		return MRX_ShopDefinition.Create("test_arsenal", catalog, 50);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckPrefab()
	{
		Check(MRX_ArsenalShopComponent.Find(m_Arsenal) != null, "prefab has MRX_ArsenalShopComponent");
		Check(m_Arsenal.FindComponent(MRX_ShopComponent) != null, "prefab has MRX_ShopComponent");
		Check(SCR_ResourceComponent.FindResourceComponent(m_Arsenal) != null, "prefab has SCR_ResourceComponent");
		SCR_ArsenalComponent arsenal = SCR_ArsenalComponent.FindArsenalComponent(m_Arsenal, false);
		Check(arsenal != null, "prefab has SCR_ArsenalComponent");
		if (arsenal)
			Check(arsenal.GetArsenalSaveType() == SCR_EArsenalSaveType.SAVING_DISABLED, "saved loadouts disabled");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckList()
	{
		SCR_ArsenalComponent arsenal = SCR_ArsenalComponent.FindArsenalComponent(m_Arsenal, false);
		if (!arsenal)
			return;

		array<ResourceName> prefabs = {};
		arsenal.GetAvailablePrefabs(prefabs);
		CheckInt(prefabs.Count(), 5, "arsenal lists the catalog items with a price");
		Check(prefabs.Contains(RIFLE) && prefabs.Contains(OPTIC) && prefabs.Contains(BANDAGE), "listed prefabs are the catalog's");

		SCR_ArsenalInventoryStorageManagerComponent storage = SCR_ArsenalInventoryStorageManagerComponent.Cast(m_Arsenal.FindComponent(SCR_ArsenalInventoryStorageManagerComponent));
		if (storage)
			Check(storage.IsPrefabInArsenalStorage(OPTIC), "arsenal storage knows the new list");

		MRX_ArsenalShopComponent shop = MRX_ArsenalShopComponent.Find(m_Arsenal);
		if (shop)
		{
			MRX_ShopItem rifle = shop.FindItem(RIFLE);
			Check(rifle && rifle.m_sId == "rifle", "catalog entry by prefab");
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckStaysEnabled()
	{
		SCR_ArsenalComponent arsenal = SCR_ArsenalComponent.FindArsenalComponent(m_Arsenal, false);
		if (!arsenal)
			return;

		arsenal.SetArsenalEnabled(false);
		Check(arsenal.IsArsenalEnabled(), "Marx arsenal stays enabled");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckResupplyHidden()
	{
		ActionsManagerComponent actions = ActionsManagerComponent.Cast(m_Arsenal.FindComponent(ActionsManagerComponent));
		if (!actions)
			return;

		array<BaseUserAction> list = {};
		actions.GetActionsList(list);
		int supportActions;
		foreach (BaseUserAction action : list)
		{
			SCR_BaseUseSupportStationAction supportAction = SCR_BaseUseSupportStationAction.Cast(action);
			if (!supportAction)
				continue;

			supportActions++;
			Check(!supportAction.CanBeShownScript(m_Character), "support station action hidden: " + supportAction.ClassName());
		}

		Check(supportActions > 0, "vanilla arsenal box has resupply actions");
	}

	//------------------------------------------------------------------------------------------------
	protected void OnFunded(MRX_TxResult result)
	{
		CheckStatus(result.m_eStatus, MRX_ETxStatus.OK, "fund the buyer");
		GetGame().GetCallqueue().CallLater(BuyRifle, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void BuyRifle()
	{
		m_sStep = "rifle";
		BaseInventoryStorageComponent storage = GetManager(m_Character).FindStorageForResource(RIFLE);
		Check(storage != null, "character has room for the rifle");
		MRX_EShopStatus status = MRX_ArsenalRequests.Buy(m_iPlayerId, m_Arsenal, storage, "rifle", m_Callback);
		if (status != MRX_EShopStatus.OK)
		{
			Check(false, "rifle request accepted, got " + typename.EnumToString(MRX_EShopStatus, status));
			End(true);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void BuyScope()
	{
		m_Rifle = FindCarried(RIFLE);
		Check(m_Rifle != null, "bought rifle carried");
		WeaponAttachmentsStorageComponent attachments;
		if (m_Rifle)
			attachments = WeaponAttachmentsStorageComponent.Cast(m_Rifle.FindComponent(WeaponAttachmentsStorageComponent));

		if (!attachments)
		{
			Check(false, "rifle has attachment slots");
			End(true);
			return;
		}

		// Another character's inventory is never a valid target.
		m_Other = MRX_TestWorldUtils.SpawnPrefab(MRX_TestWorldUtils.CHARACTER_PREFAB, m_Character.GetOrigin() + "0 0 1.5");
		if (m_Other)
		{
			BaseInventoryStorageComponent foreign = GetManager(m_Other).FindStorageForResource(BANDAGE);
			MRX_EShopStatus foreignStatus = MRX_ArsenalRequests.Buy(m_iPlayerId, m_Arsenal, foreign, "bandage", m_Callback);
			Check(foreignStatus == MRX_EShopStatus.REJECTED, "purchase into another character's inventory refused, got " + typename.EnumToString(MRX_EShopStatus, foreignStatus));
		}

		m_sStep = "scope";
		MRX_EShopStatus status = MRX_ArsenalRequests.Buy(m_iPlayerId, m_Arsenal, attachments, "scope", m_Callback);
		if (status != MRX_EShopStatus.OK)
		{
			Check(false, "scope request accepted, got " + typename.EnumToString(MRX_EShopStatus, status));
			End(true);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void SellRifle()
	{
		MRX_ItemSnapshot snapshot = MRX_EntitySnapshots.Capture(m_Rifle);
		array<ResourceName> contents = {};
		CollectPrefabs(snapshot, contents);
		Check(contents.Contains(OPTIC), "scope mounted on the rifle");
		if (!contents.Contains(OPTIC))
		{
			Print("[MRX_TEST]   rifle contents: " + contents.Count());
			foreach (ResourceName content : contents)
			{
				Print("[MRX_TEST]     " + content);
			}

			IEntity scope = FindCarried(OPTIC);
			if (scope)
			{
				IEntity parent = scope.GetParent();
				while (parent)
				{
					Print("[MRX_TEST]   scope parent: " + SCR_ResourceNameUtils.GetPrefabName(parent));
					parent = parent.GetParent();
				}
			}
			else
			{
				Print("[MRX_TEST]   scope not carried");
			}
		}

		m_iExpectedSale = MRX_ShopService.GetSellPriceWithContents(m_Shop, RIFLE, contents);
		m_iExpectedCount = 1 + contents.Count();
		Check(m_iExpectedSale >= 700, "rifle and scope worth at least 700");

		m_sStep = "sell";
		MRX_EShopStatus status = MRX_ArsenalRequests.Sell(m_iPlayerId, m_Arsenal, m_Rifle, m_Callback);
		if (status != MRX_EShopStatus.OK)
		{
			Check(false, "sale request accepted, got " + typename.EnumToString(MRX_EShopStatus, status));
			End(true);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnResult(MRX_ShopResult result)
	{
		string status = typename.EnumToString(MRX_EShopStatus, result.m_eStatus);
		Print(string.Format("[MRX_TEST]   arsenal %1: %2, price %3, items %4", m_sStep, status, result.m_iPrice, result.m_iItemCount));
		if (result.m_eStatus != MRX_EShopStatus.OK)
		{
			Check(false, string.Format("%1: expected OK, got %2 (tx %3)", m_sStep, status, typename.EnumToString(MRX_ETxStatus, result.m_eTxStatus)));
			End(true);
			return;
		}

		switch (m_sStep)
		{
			case "rifle":
				CheckInt(result.m_iPrice, 1000, "rifle price");
				GetGame().GetCallqueue().CallLater(BuyScope, STEP_MS);
				return;

			case "scope":
				CheckInt(result.m_iPrice, 400, "scope price");
				GetGame().GetCallqueue().CallLater(SellRifle, STEP_MS);
				return;

			case "sell":
				CheckInt(result.m_iPrice, m_iExpectedSale, "rifle sold with its contents");
				CheckInt(result.m_iItemCount, m_iExpectedCount, "items sold");
				GetGame().GetCallqueue().CallLater(OpenPanel, STEP_MS);
				return;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OpenPanel()
	{
		Check(FindCarried(RIFLE) == null, "sold rifle gone");
		SCR_InventoryStorageManagerComponent manager = SCR_InventoryStorageManagerComponent.Cast(GetManager(m_Character));
		manager.SetStorageToOpen(m_Arsenal);
		manager.OpenInventory();
		GetGame().GetCallqueue().CallLater(CheckPanel, STEP_MS * 2);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckPanel()
	{
		SCR_InventoryMenuUI menu = SCR_InventoryMenuUI.GetInventoryMenu();
		Check(menu != null, "vanilla inventory opens");
		BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(m_Arsenal.FindComponent(BaseInventoryStorageComponent));
		if (menu && storage)
		{
			SCR_InventoryOpenedStorageUI panel = menu.GetOpenedStorage(storage);
			Check(SCR_InventoryOpenedStorageArsenalUI.Cast(panel) != null, "arsenal opens as its own panel");
			Check(!menu.MRX_IsVicinityShown(), "vicinity panel hidden while the arsenal panel is open");
			if (panel)
			{
				Check(IsBalanceShown(panel), "panel shows the balance");
				// Vanilla rebuilds the storage title on every refresh, e.g. after each trade.
				panel.Refresh();
				Check(IsBalanceShown(panel), "panel still shows the balance after a refresh");
			}
		}

		GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.Inventory20Menu);
		End(true);
	}

	//------------------------------------------------------------------------------------------------
	protected void End(bool finish)
	{
		if (m_Arsenal)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Arsenal);

		if (m_Other)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Other);

		if (m_Rifle)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Rifle);

		if (m_Possession)
			m_Possession.Release();

		if (finish)
			Finish();
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsBalanceShown(notnull SCR_InventoryOpenedStorageUI panel)
	{
		SCR_InventoryOpenedStorageArsenalUI arsenalPanel = SCR_InventoryOpenedStorageArsenalUI.Cast(panel);
		if (!arsenalPanel)
			return false;

		TextWidget text = arsenalPanel.MRX_GetBalanceText();
		return text && text.IsVisible() && !text.GetText().IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity FindCarried(ResourceName prefab)
	{
		array<IEntity> items = {};
		m_Possession.GetItems(items);
		foreach (IEntity item : items)
		{
			if (MRX_ShopCatalog.GetPrefabKey(SCR_ResourceNameUtils.GetPrefabName(item)) == MRX_ShopCatalog.GetPrefabKey(prefab))
				return item;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static void CollectPrefabs(notnull MRX_ItemSnapshot snapshot, notnull array<ResourceName> outPrefabs)
	{
		foreach (MRX_ItemSnapshot child : snapshot.m_aChildren)
		{
			outPrefabs.Insert(child.m_sPrefab);
			CollectPrefabs(child, outPrefabs);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static InventoryStorageManagerComponent GetManager(notnull IEntity character)
	{
		return InventoryStorageManagerComponent.Cast(character.FindComponent(SCR_InventoryStorageManagerComponent));
	}
}
#endif
