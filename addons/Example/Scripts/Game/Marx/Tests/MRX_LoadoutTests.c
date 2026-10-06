#ifdef WORKBENCH
// Tests of stash properties, loadout prices (MRX_LoadoutMath), issued items and saved loadouts on a real character.

//------------------------------------------------------------------------------------------------
class MRX_LoadoutTests
{
	static const ResourceName RIFLE = "{96DFD2E7E63B3386}Prefabs/Weapons/Rifles/AK74/Rifle_AK74N.et";
	static const ResourceName MAGAZINE = "{BBB50A815A2F916B}Prefabs/Weapons/Magazines/Magazine_545x39_AK_30rnd_Ball.et";
	static const ResourceName BANDAGE = "{A81F501D3EF6F38E}Prefabs/Items/Medicine/FieldDressing_01/FieldDressing_US_01.et";
	static const ResourceName COMPASS = "{61D4F80E49BF9B12}Prefabs/Items/Equipment/Compass/Compass_SY183.et";
	static const ResourceName SCOPE = "{ACDF49FACD0701A8}Prefabs/Weapons/Attachments/Optics/Optic_1P29/Optic_1P29.et";

	//------------------------------------------------------------------------------------------------
	static void Register(notnull MRX_TestRunner runner)
	{
		runner.Add(new MRX_Test_StashProperties());
		runner.Add(new MRX_Test_LoadoutPlan());
		runner.Add(new MRX_Test_IssuedItems());
		runner.Add(new MRX_Test_LoadoutFlow());
	}

	//------------------------------------------------------------------------------------------------
	//! Rifle 600 (sold 300), magazine 20 (10), bandage 8 (4), compass 100 (50). Everything else is not for sale.
	static MRX_TestPriceList CreatePrices()
	{
		MRX_TestPriceList prices = new MRX_TestPriceList();
		prices.Set(RIFLE, 600, 300);
		prices.Set(MAGAZINE, 20, 10);
		prices.Set(BANDAGE, 8, 4);
		prices.Set(COMPASS, 100, 50);
		return prices;
	}
}

//------------------------------------------------------------------------------------------------
class MRX_TestPriceList : MRX_PriceList
{
	protected ref map<string, int> m_mBuy = new map<string, int>();
	protected ref map<string, int> m_mSell = new map<string, int>();

	//------------------------------------------------------------------------------------------------
	void Set(ResourceName prefab, int buyPrice, int sellPrice)
	{
		m_mBuy.Set(MRX_LoadoutMath.GetPrefabKey(prefab), buyPrice);
		m_mSell.Set(MRX_LoadoutMath.GetPrefabKey(prefab), sellPrice);
	}

	//------------------------------------------------------------------------------------------------
	override string GetCurrency()
	{
		return "cash";
	}

	//------------------------------------------------------------------------------------------------
	override int GetBuyPrice(ResourceName prefab)
	{
		return m_mBuy.Get(MRX_LoadoutMath.GetPrefabKey(prefab));
	}

	//------------------------------------------------------------------------------------------------
	override int GetSellPrice(ResourceName prefab)
	{
		return m_mSell.Get(MRX_LoadoutMath.GetPrefabKey(prefab));
	}
}

//------------------------------------------------------------------------------------------------
//! Properties are set, checked against an expected value, removed, copied and saved like assets.
class MRX_Test_StashProperties : MRX_TestCase
{
	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		MRX_StorageRules rules = MRX_TestUtils.CreateRules();
		MRX_StashRecord record = MRX_StashRecord.Create("prop-owner");

		Apply(record, rules, "set", MRX_PropertyChange.Create("pages", "2"), MRX_EStashStatus.OK);
		CheckString(record.GetProperty("pages"), "2", "property set");

		Apply(record, rules, "stale", MRX_PropertyChange.Create("pages", "4").Expecting("1"), MRX_EStashStatus.INVALID_STATE);
		CheckString(record.GetProperty("pages"), "2", "change with a stale expected value refused");

		Apply(record, rules, "fresh", MRX_PropertyChange.Create("pages", "3").Expecting("2"), MRX_EStashStatus.OK);
		CheckString(record.GetProperty("pages"), "3", "change with the current value applied");

		Apply(record, rules, "new", MRX_PropertyChange.Create("other", "x").Expecting(string.Empty), MRX_EStashStatus.OK);
		CheckString(record.GetProperty("other"), "x", "expected empty = not set yet");

		Apply(record, rules, "fresh", MRX_PropertyChange.Create("pages", "9"), MRX_EStashStatus.DUPLICATE);
		CheckString(record.GetProperty("pages"), "3", "same key again changes nothing");

		Apply(record, rules, "remove", MRX_PropertyChange.Create("other", string.Empty), MRX_EStashStatus.OK);
		Check(!record.FindProperty("other"), "empty value removes the property");
		Apply(record, rules, "empty-key", MRX_PropertyChange.Create(string.Empty, "1"), MRX_EStashStatus.INVALID_STATE);

		CheckString(record.Copy().GetProperty("pages"), "3", "copy keeps the properties");

		// Saved like the Native backend saves them (MRX_StashState).
		JsonSaveContext save = new JsonSaveContext();
		save.EnableTypeDiscriminator(false);
		save.WriteValue("properties", record.m_aProperties);
		JsonLoadContext load = new JsonLoadContext();
		load.EnableTypeDiscriminator(false);
		array<ref MRX_StashProperty> loaded = {};
		Check(load.LoadFromString(save.SaveToString()) && load.ReadValue("properties", loaded), "properties saved and loaded");
		Check(loaded.Count() == 1 && loaded[0].m_sKey == "pages" && loaded[0].m_sValue == "3", "loaded property");
		Finish();
	}

	//------------------------------------------------------------------------------------------------
	protected void Apply(MRX_StashRecord record, MRX_StorageRules rules, string key, MRX_PropertyChange change, MRX_EStashStatus expected)
	{
		MRX_StashRequest request = MRX_StashTests.CreateRequest(record.m_sOwnerId, key);
		request.m_aPropertyChanges.Insert(change);
		MRX_EStashStatus status = MRX_StashMath.Apply(record, request, rules).m_eStatus;
		if (status != expected)
			Check(false, string.Format("%1: expected %2, got %3", key, typename.EnumToString(MRX_EStashStatus, expected), typename.EnumToString(MRX_EStashStatus, status)));
	}
}

//------------------------------------------------------------------------------------------------
//! The player's own items are used again, missing ones bought, the others sold; items neither owned nor for sale are
//! left out with their contents; the price of items that did not arrive is refunded.
class MRX_Test_LoadoutPlan : MRX_TestCase
{
	protected static const ResourceName UNKNOWN_VEST = "Prefabs/Test/UnknownVest.et";
	protected static const ResourceName UNIFORM = "Prefabs/Test/Uniform.et";

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		// Character: uniform (2 magazines), rifle (scope, magazine), vest not for sale (magazine, bandage).
		MRX_ItemSnapshot loadout = MRX_ItemSnapshot.Create("Prefabs/Test/Character.et");
		MRX_ItemSnapshot uniform = AddChild(loadout, UNIFORM);
		AddChild(uniform, MRX_LoadoutTests.MAGAZINE);
		AddChild(uniform, MRX_LoadoutTests.MAGAZINE);
		MRX_ItemSnapshot rifle = AddChild(loadout, MRX_LoadoutTests.RIFLE);
		AddChild(rifle, MRX_LoadoutTests.SCOPE);
		AddChild(rifle, MRX_LoadoutTests.MAGAZINE);
		MRX_ItemSnapshot vest = AddChild(loadout, UNKNOWN_VEST);
		AddChild(vest, MRX_LoadoutTests.MAGAZINE);
		AddChild(vest, MRX_LoadoutTests.BANDAGE);

		MRX_TestPriceList prices = MRX_LoadoutTests.CreatePrices();

		// Naked player: the uniform is not for sale either, the scope neither.
		MRX_LoadoutPlan plan = MRX_LoadoutMath.Plan(loadout, {}, prices);
		CheckInt(plan.m_iBoughtCount, 2, "nothing owned: bought rifle and its magazine");
		CheckInt(plan.m_iBuyTotal, 620, "nothing owned: price");
		CheckInt(plan.m_iUnavailableCount, 7, "nothing owned: uniform (3 items), scope, vest (3 items) left out");
		CheckInt(plan.m_Loadout.CountItems() - 1, 2, "nothing owned: loadout keeps rifle and magazine");
		CheckString(plan.m_sCurrency, "cash", "currency of the price list");

		// Owns the uniform, the vest, 3 magazines, a compass; a non-full magazine would not be passed in.
		array<ResourceName> ownItems = {UNIFORM, UNKNOWN_VEST, MRX_LoadoutTests.MAGAZINE, MRX_LoadoutTests.MAGAZINE, MRX_LoadoutTests.MAGAZINE, MRX_LoadoutTests.COMPASS};
		plan = MRX_LoadoutMath.Plan(loadout, ownItems, prices);
		CheckInt(plan.m_iReusedCount, 5, "reused: uniform, vest, 3 magazines");
		CheckInt(plan.m_iBoughtCount, 3, "bought: rifle, 1 magazine, bandage");
		CheckInt(plan.m_iBuyTotal, 628, "buy total");
		CheckInt(plan.m_iSoldCount, 1, "sold: compass");
		CheckInt(plan.m_iSellTotal, 50, "sell total");
		CheckInt(plan.GetNet(), 578, "net");
		CheckInt(plan.m_iUnavailableCount, 1, "scope left out");

		// Only the magazine arrived of the bought items: rifle and bandage are refunded.
		map<string, int> needed = new map<string, int>();
		MRX_LoadoutMath.CountItems(plan.m_Loadout, needed);
		map<string, int> received = new map<string, int>();
		received.Copy(needed);
		received.Remove(MRX_LoadoutMath.GetPrefabKey(MRX_LoadoutTests.RIFLE));
		received.Remove(MRX_LoadoutMath.GetPrefabKey(MRX_LoadoutTests.BANDAGE));
		CheckInt(plan.GetShortfallPrice(received, needed), 608, "shortfall: rifle and bandage");
		CheckInt(plan.GetShortfallPrice(needed, needed), 0, "no shortfall");

		// Selling more than buying: the net is negative.
		array<ResourceName> rich = {UNIFORM, UNKNOWN_VEST, MRX_LoadoutTests.RIFLE, MRX_LoadoutTests.RIFLE, MRX_LoadoutTests.MAGAZINE, MRX_LoadoutTests.MAGAZINE, MRX_LoadoutTests.MAGAZINE, MRX_LoadoutTests.MAGAZINE, MRX_LoadoutTests.BANDAGE};
		plan = MRX_LoadoutMath.Plan(loadout, rich, prices);
		CheckInt(plan.GetNet(), -300, "spare rifle sold");
		Check(plan.IsSameAs(MRX_LoadoutMath.Plan(loadout, rich, prices)), "same input, same plan");
		Check(!plan.IsSameAs(MRX_LoadoutMath.Plan(loadout, ownItems, prices)), "other gear, other plan");

		CheckInt(MRX_LoadoutMath.MultiplyCapped(int.MAX, 2), int.MAX, "price overflow capped");
		CheckInt(MRX_LoadoutMath.AddCapped(int.MAX, 1), int.MAX, "sum overflow capped");
		Finish();
	}

	//------------------------------------------------------------------------------------------------
	protected static MRX_ItemSnapshot AddChild(notnull MRX_ItemSnapshot parent, ResourceName prefab)
	{
		MRX_ItemSnapshot child = MRX_ItemSnapshot.Create(prefab);
		parent.m_aChildren.Insert(child);
		return child;
	}
}

//------------------------------------------------------------------------------------------------
//! Issued items: not sold by shops (alone: ISSUED, inside another item: worth nothing), kept issued through snapshots.
class MRX_Test_IssuedItems : MRX_TestCase
{
	protected ref MRX_TestPossession m_Possession;

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		m_Possession = MRX_TestPossession.Acquire();
		if (!m_Possession || !m_Possession.GetCharacter())
		{
			Skip("no local character");
			return;
		}

		ChimeraCharacter character = ChimeraCharacter.Cast(m_Possession.GetCharacter());
		InventoryStorageManagerComponent manager = character.GetCharacterController().GetInventoryStorageManager();
		int playerId = m_Possession.GetController().GetPlayerId();
		IEntity rifle = SpawnInto(manager, MRX_LoadoutTests.RIFLE);
		IEntity compass = SpawnInto(manager, MRX_LoadoutTests.COMPASS);
		if (!rifle || !compass)
		{
			Check(false, "test items spawn into the character");
			Cleanup(rifle, compass);
			return;
		}

		MRX_EntityShopInventory inventory = new MRX_EntityShopInventory();
		ResourceName prefab;
		array<ResourceName> contents = {};
		MRX_IssuedItems.Mark(compass);
		Check(MRX_IssuedItems.IsIssued(compass), "marked");
		CheckInt(inventory.InspectForSale(playerId, compass, prefab, contents), MRX_EShopStatus.ISSUED, "issued item not for sale");

		// The rifle's magazine (prefab default) is issued, the rifle itself not.
		IEntity magazine = FindContent(rifle);
		Check(magazine != null, "rifle comes with a magazine");
		if (magazine)
		{
			MRX_IssuedItems.Mark(magazine);
			contents.Clear();
			CheckInt(inventory.InspectForSale(playerId, rifle, prefab, contents), MRX_EShopStatus.OK, "rifle with an issued magazine for sale");
			Check(contents.Contains(ResourceName.Empty), "issued content listed without prefab");

			MRX_ItemSnapshot snapshot = MRX_EntitySnapshots.Capture(rifle);
			Check(!snapshot.m_bIssued, "rifle snapshot not issued");
			Check(HasIssued(snapshot), "magazine snapshot issued");
			MRX_ItemSnapshot loadout = MRX_EntitySnapshots.CaptureLoadout(character);
			Check(!HasIssued(loadout), "loadout captures items as new (not issued)");
		}

		MRX_IssuedItems.Mark(rifle);
		Check(MRX_IssuedItems.IsIssued(rifle) && (!magazine || MRX_IssuedItems.IsIssued(magazine)), "mark with contents");
		MRX_IssuedItems.Unmark(rifle);
		Check(!MRX_IssuedItems.IsIssued(rifle) && (!magazine || !MRX_IssuedItems.IsIssued(magazine)), "unmark with contents");

		// The character may carry items of the same prefabs: compare the counts.
		array<ResourceName> ownBefore = {};
		MRX_LoadoutService.GetOwnedPrefabs(character, ownBefore);
		MRX_IssuedItems.Mark(rifle);
		array<ResourceName> ownAfter = {};
		MRX_LoadoutService.GetOwnedPrefabs(character, ownAfter);
		CheckInt(Count(ownBefore, MRX_LoadoutTests.RIFLE) - Count(ownAfter, MRX_LoadoutTests.RIFLE), 1, "issued rifle does not count as the player's own");
		Cleanup(rifle, compass);
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity rifle, IEntity compass)
	{
		if (rifle)
			SCR_EntityHelper.DeleteEntityAndChildren(rifle);

		if (compass)
			SCR_EntityHelper.DeleteEntityAndChildren(compass);

		m_Possession.Release();
		Finish();
	}

	//------------------------------------------------------------------------------------------------
	static int Count(notnull array<ResourceName> prefabs, ResourceName prefab)
	{
		int count;
		foreach (ResourceName entry : prefabs)
		{
			if (entry == prefab)
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool HasIssued(notnull MRX_ItemSnapshot snapshot)
	{
		if (snapshot.m_bIssued)
			return true;

		foreach (MRX_ItemSnapshot child : snapshot.m_aChildren)
		{
			if (HasIssued(child))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns the prefab into the inventory (synchronous on the server) and returns the new item.
	static IEntity SpawnInto(notnull InventoryStorageManagerComponent manager, ResourceName prefab)
	{
		array<IEntity> before = {};
		manager.GetItems(before);
		if (!manager.TrySpawnPrefabToStorage(prefab))
			return null;

		array<IEntity> after = {};
		manager.GetItems(after);
		foreach (IEntity item : after)
		{
			if (!before.Contains(item) && SCR_ResourceNameUtils.GetPrefabName(item) == prefab)
				return item;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity FindContent(notnull IEntity item)
	{
		set<BaseInventoryStorageComponent> storages = new set<BaseInventoryStorageComponent>();
		MRX_EntitySnapshots.FindStorages(item, storages);
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			array<InventoryItemComponent> items = {};
			storage.GetOwnedItems(items, false);
			if (!items.IsEmpty())
				return items[0].GetOwner();
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! At an open stash: save the gear into the last slot, lose the rifle and a magazine and take a compass, put the
//! loadout on: the rifle and the magazine are bought, the compass sold, the gear is the saved one again.
class MRX_Test_LoadoutFlow : MRX_TestCase
{
	protected static const int STEP_MS = 700;
	protected static const int WAIT_LIMIT_MS = 6000;

	protected ref MRX_TestPossession m_Possession;
	protected SCR_PlayerController m_Controller;
	protected IEntity m_StashPoint;
	protected string m_sOwnerId;
	protected int m_iPlayerId;
	protected int m_iSlot;
	protected string m_sOriginalSlot;
	protected ref MRX_PriceList m_OriginalPrices;
	protected int m_iWaitedMs;
	protected int m_iBalanceBefore;
	protected int m_iExpectedNet;
	//! Carried items before the test, and the counts of the test prefabs when the loadout was saved.
	protected ref array<IEntity> m_aBaseline = {};
	protected ref map<ResourceName, int> m_mSavedCounts = new map<ResourceName, int>();
	protected ref array<IEntity> m_aAdded = {};
	protected ref MRX_TestLoadoutReply m_Reply;
	protected ref MRX_StashCallback m_ListCallback;
	protected ref MRX_BalanceCallback m_BalanceCallback;
	protected ref MRX_TxCallback m_TxCallback;
	protected ref MRX_StashResultCallback m_RestoreCallback;

	//------------------------------------------------------------------------------------------------
	override int GetTimeoutMs()
	{
		return 30000;
	}

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		m_Possession = MRX_TestPossession.Acquire();
		if (!m_Possession || !m_Possession.GetCharacter() || !MRX_Marx.GetStash() || !MRX_Loadouts.Get() || !MRX_StashSessions.Get())
		{
			Skip("no stash, loadout service or local character");
			return;
		}

		m_Controller = m_Possession.GetController();
		m_iPlayerId = m_Controller.GetPlayerId();
		m_sOwnerId = MRX_Marx.GetOwnerId(m_iPlayerId);
		m_iSlot = MRX_Loadouts.Get().GetSlotCount() - 1;
		if (m_sOwnerId.IsEmpty() || m_iSlot < 0)
		{
			End(true, "the local player has no owner (start with -" + MRX_TestRunner.TEST_IDENTITY_PARAM + ") or loadouts are off");
			return;
		}

		m_OriginalPrices = MRX_Marx.GetPriceList();
		MRX_Marx.SetPriceList(MRX_LoadoutTests.CreatePrices());

		// Without an open stash nothing happens.
		m_Reply = new MRX_TestLoadoutReply();
		MRX_Loadouts.Get().Load(m_iPlayerId, m_iSlot, m_Reply);
		CheckLoadoutStatus(m_Reply.m_Result, MRX_ELoadoutStatus.NO_STASH, "load without an open stash");

		m_ListCallback = new MRX_StashCallback();
		m_ListCallback.GetOnResult().Insert(OnOriginalListed);
		MRX_Marx.GetStash().List(m_sOwnerId, m_ListCallback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnOriginalListed(MRX_EStashStatus status, MRX_StashRecord record)
	{
		if (record)
			m_sOriginalSlot = record.GetProperty(MRX_LoadoutService.GetPropertyKey(m_iSlot));

		m_StashPoint = MRX_TestWorldUtils.SpawnPrefab(MRX_Test_StashPointFlow.STASH_PREFAB, m_Possession.GetCharacter().GetOrigin() + "1 0 0");
		m_Controller.MRX_RequestStashOpen(m_StashPoint);
		GetGame().GetCallqueue().CallLater(WaitOpen, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void WaitOpen()
	{
		MRX_StashSession session = MRX_StashSessions.Get().Find(m_iPlayerId);
		if (!session || !session.IsReady())
		{
			m_iWaitedMs += STEP_MS;
			if (m_iWaitedMs < WAIT_LIMIT_MS)
			{
				GetGame().GetCallqueue().CallLater(WaitOpen, STEP_MS);
				return;
			}

			Check(false, "stash opens");
			End(false, string.Empty);
			return;
		}

		InventoryStorageManagerComponent manager = GetCharacterManager();
		MRX_EntitySnapshots.GetLoadoutItems(m_Possession.GetCharacter(), m_aBaseline);
		IEntity rifle = MRX_Test_IssuedItems.SpawnInto(manager, MRX_LoadoutTests.RIFLE);
		IEntity firstMagazine = MRX_Test_IssuedItems.SpawnInto(manager, MRX_LoadoutTests.MAGAZINE);
		IEntity secondMagazine = MRX_Test_IssuedItems.SpawnInto(manager, MRX_LoadoutTests.MAGAZINE);
		Check(rifle && firstMagazine && secondMagazine, "rifle and magazines spawn into the character");
		m_aAdded.Insert(rifle);
		m_aAdded.Insert(firstMagazine);
		m_aAdded.Insert(secondMagazine);

		m_Reply = new MRX_TestLoadoutReply();
		m_Reply.m_OnResult.Insert(OnSaved);
		MRX_Loadouts.Get().Save(m_iPlayerId, m_iSlot, m_Reply);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSaved(MRX_LoadoutResult result)
	{
		CheckLoadoutStatus(result, MRX_ELoadoutStatus.OK, "save");
		m_ListCallback = new MRX_StashCallback();
		m_ListCallback.GetOnResult().Insert(OnSavedListed);
		MRX_Marx.GetStash().List(m_sOwnerId, m_ListCallback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSavedListed(MRX_EStashStatus status, MRX_StashRecord record)
	{
		MRX_SavedLoadout saved;
		if (record)
			saved = MRX_LoadoutService.Parse(record.GetProperty(MRX_LoadoutService.GetPropertyKey(m_iSlot)));

		Check(saved != null, "saved loadout in the stash");
		if (saved)
		{
			map<string, int> counts = new map<string, int>();
			MRX_LoadoutMath.CountItems(saved.m_Loadout, counts);
			Check(counts.Get(MRX_LoadoutMath.GetPrefabKey(MRX_LoadoutTests.RIFLE)) >= 1, "rifle saved");
			Check(!saved.m_sMainItem.IsEmpty(), "main item saved");
		}

		array<ResourceName> counted = {MRX_LoadoutTests.RIFLE, MRX_LoadoutTests.MAGAZINE, MRX_LoadoutTests.COMPASS};
		foreach (ResourceName prefab : counted)
		{
			m_mSavedCounts.Set(prefab, CountCarried(prefab));
		}

		// Lose the rifle (with any magazine in it) and one magazine, pick up a compass. To pay: the rifle, the magazines;
		// to receive: the compass.
		array<IEntity> lost = {};
		lost.Insert(m_aAdded[0]);
		set<BaseInventoryStorageComponent> storages = new set<BaseInventoryStorageComponent>();
		MRX_EntitySnapshots.FindStorages(m_aAdded[0], storages);
		int lostMagazines = 1;
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			array<InventoryItemComponent> contents = {};
			storage.GetOwnedItems(contents, false);
			foreach (InventoryItemComponent content : contents)
			{
				if (content.GetOwner() && SCR_ResourceNameUtils.GetPrefabName(content.GetOwner()) == MRX_LoadoutTests.MAGAZINE)
					lostMagazines++;
			}
		}

		m_iExpectedNet = 600 + 20 * lostMagazines - 50;
		SCR_EntityHelper.DeleteEntityAndChildren(m_aAdded[0]);
		SCR_EntityHelper.DeleteEntityAndChildren(m_aAdded[1]);
		m_aAdded.Insert(MRX_Test_IssuedItems.SpawnInto(GetCharacterManager(), MRX_LoadoutTests.COMPASS));

		m_TxCallback = new MRX_TxCallback();
		m_TxCallback.GetOnResult().Insert(OnFunded);
		MRX_Marx.GetEconomy().Credit(m_sOwnerId, "cash", 2000, MRX_TxContext.Create("test", "loadout flow", MRX_NetworkTests.UniqueKey("loadout")), m_TxCallback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnFunded(MRX_TxResult result)
	{
		m_BalanceCallback = new MRX_BalanceCallback();
		m_BalanceCallback.GetOnResult().Insert(OnBalanceBefore);
		MRX_Marx.GetEconomy().GetBalance(m_sOwnerId, "cash", m_BalanceCallback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBalanceBefore(MRX_ETxStatus status, int balance)
	{
		m_iBalanceBefore = balance;
		m_Reply = new MRX_TestLoadoutReply();
		m_Reply.m_OnResult.Insert(OnLoaded);
		MRX_Loadouts.Get().Load(m_iPlayerId, m_iSlot, m_Reply);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnLoaded(MRX_LoadoutResult result)
	{
		CheckLoadoutStatus(result, MRX_ELoadoutStatus.OK, "load");
		CheckInt(result.m_iNet, m_iExpectedNet, "net price");
		foreach (ResourceName prefab, int count : m_mSavedCounts)
		{
			CheckInt(CountCarried(prefab), count, "carried as saved: " + FilePath.StripPath(prefab));
		}

		// Everything carried now that was not before the test is a test item.
		array<IEntity> items = {};
		MRX_EntitySnapshots.GetLoadoutItems(m_Possession.GetCharacter(), items);
		foreach (IEntity item : items)
		{
			if (!m_aBaseline.Contains(item))
				m_aAdded.Insert(item);
		}

		m_BalanceCallback = new MRX_BalanceCallback();
		m_BalanceCallback.GetOnResult().Insert(OnBalanceAfter);
		MRX_Marx.GetEconomy().GetBalance(m_sOwnerId, "cash", m_BalanceCallback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBalanceAfter(MRX_ETxStatus status, int balance)
	{
		CheckInt(m_iBalanceBefore - balance, m_iExpectedNet, "balance change");
		m_RestoreCallback = new MRX_StashResultCallback();
		m_RestoreCallback.GetOnResult().Insert(OnRestored);
		MRX_PropertyChange change = MRX_PropertyChange.Create(MRX_LoadoutService.GetPropertyKey(m_iSlot), m_sOriginalSlot);
		MRX_Marx.GetStash().SetProperty(m_sOwnerId, change, MRX_TxContext.Create("test", "loadout flow", MRX_NetworkTests.UniqueKey("loadout")), m_RestoreCallback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnRestored(MRX_StashResult result)
	{
		End(false, string.Empty);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckLoadoutStatus(MRX_LoadoutResult result, MRX_ELoadoutStatus expected, string message)
	{
		if (!result)
		{
			Check(false, message + ": no result");
			return;
		}

		if (result.m_eStatus != expected)
			Check(false, string.Format("%1: expected %2, got %3", message, typename.EnumToString(MRX_ELoadoutStatus, expected), typename.EnumToString(MRX_ELoadoutStatus, result.m_eStatus)));
	}

	//------------------------------------------------------------------------------------------------
	protected InventoryStorageManagerComponent GetCharacterManager()
	{
		return ChimeraCharacter.Cast(m_Possession.GetCharacter()).GetCharacterController().GetInventoryStorageManager();
	}

	//------------------------------------------------------------------------------------------------
	protected int CountCarried(ResourceName prefab)
	{
		array<IEntity> items = {};
		MRX_EntitySnapshots.GetLoadoutItems(m_Possession.GetCharacter(), items);
		int count;
		foreach (IEntity item : items)
		{
			if (SCR_ResourceNameUtils.GetPrefabName(item) == prefab)
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected void End(bool skip, string reason)
	{
		GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.Inventory20Menu);
		MRX_StashSessionManager sessions = MRX_StashSessions.Get();
		if (sessions)
			sessions.Close(m_iPlayerId, "test end");

		if (m_OriginalPrices || !skip)
			MRX_Marx.SetPriceList(m_OriginalPrices);

		foreach (IEntity item : m_aAdded)
		{
			if (item && !item.IsDeleted())
				SCR_EntityHelper.DeleteEntityAndChildren(item);
		}

		if (m_StashPoint)
			SCR_EntityHelper.DeleteEntityAndChildren(m_StashPoint);

		if (m_Possession)
			m_Possession.Release();

		if (skip)
			Skip(reason);
		else
			Finish();
	}
}

//------------------------------------------------------------------------------------------------
void MRX_TestLoadoutResultDelegate(MRX_LoadoutResult result);
typedef func MRX_TestLoadoutResultDelegate;

//------------------------------------------------------------------------------------------------
class MRX_TestLoadoutReply : MRX_LoadoutCallback
{
	ref MRX_LoadoutResult m_Result;
	ref ScriptInvokerBase<MRX_TestLoadoutResultDelegate> m_OnResult = new ScriptInvokerBase<MRX_TestLoadoutResultDelegate>();

	//------------------------------------------------------------------------------------------------
	override void OnResult(MRX_LoadoutResult result)
	{
		m_Result = result;
		m_OnResult.Invoke(result);
	}
}
#endif
