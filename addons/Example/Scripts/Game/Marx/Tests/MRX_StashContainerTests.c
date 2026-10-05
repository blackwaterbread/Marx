#ifdef WORKBENCH
// Tests of the stash container prefab and components: opened as its own panel of the vanilla inventory and kept out of
// the vicinity list, item moves reported on the server, other characters kept from taking items out or putting their
// items in, room for large items like rifles, pages when the panel is full.

//------------------------------------------------------------------------------------------------
class MRX_StashContainerTests
{
	//------------------------------------------------------------------------------------------------
	static void Register(notnull MRX_TestRunner runner)
	{
		runner.Add(new MRX_Test_StashContainer());
		runner.Add(new MRX_Test_StashPanelPaging());
		runner.Add(new MRX_Test_StashPageLimit());
	}
}

//------------------------------------------------------------------------------------------------
//! Page limit: identical items count one by one, a full container refuses spawns and moves (nothing is dropped on the
//! ground), restoring ignores it.
class MRX_Test_StashPageLimit : MRX_TestCase
{
	//! More spawn requests than one page holds.
	protected static const int SPAWN_REQUESTS = 60;
	protected static const float GROUND_RADIUS = 5;
	protected static const int STEP_MS = 600;

	protected ref MRX_TestPossession m_Possession;
	protected IEntity m_Container;
	protected IEntity m_Carried;
	protected MRX_StashStorageComponent m_Storage;
	protected MRX_StashContainerManagerComponent m_ContainerManager;
	protected int m_iOnGround;

	//------------------------------------------------------------------------------------------------
	override int GetTimeoutMs()
	{
		return 10000;
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

		IEntity character = m_Possession.GetCharacter();
		m_Container = MRX_TestWorldUtils.SpawnPrefab(MRX_Test_StashContainer.CONTAINER_PREFAB, character.GetOrigin() + "0.6 0 0");
		m_Storage = MRX_StashStorageComponent.Cast(m_Container.FindComponent(MRX_StashStorageComponent));
		m_ContainerManager = MRX_StashContainerManagerComponent.Cast(m_Container.FindComponent(MRX_StashContainerManagerComponent));
		m_ContainerManager.SetUser(character);
		m_Storage.SetMaxPages(1);

		for (int i = 0; i < SPAWN_REQUESTS; i++)
		{
			m_ContainerManager.TrySpawnPrefabToStorage(MRX_Test_StashContainer.ITEM_PREFAB, m_Storage);
		}

		GetCharacterManager().TrySpawnPrefabToStorage(MRX_Test_StashContainer.ITEM_PREFAB);
		GetGame().GetCallqueue().CallLater(CheckFull, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckFull()
	{
		array<InventoryItemComponent> ownedItems = {};
		m_Storage.GetOwnedItems(ownedItems, false);
		int cellsPerItem = 1;
		if (!ownedItems.IsEmpty())
			cellsPerItem = MRX_StashStorageComponent.GetCellCount(ownedItems[0]);

		CheckInt(ownedItems.Count(), m_Storage.GetMaxCells() / cellsPerItem, "identical items fill the page one by one");
		CheckInt(m_Storage.GetUsedCells(), m_Storage.GetMaxCells(), "used cells at the limit");
		CheckInt(CountOnGround(), 0, "refused spawns leave nothing on the ground");

		array<IEntity> items = {};
		GetCharacterManager().GetItems(items);
		foreach (IEntity item : items)
		{
			if (SCR_ResourceNameUtils.GetPrefabName(item) == MRX_Test_StashContainer.ITEM_PREFAB)
				m_Carried = item;
		}

		Check(m_Carried && !GetCharacterManager().CanMoveItemToStorage(m_Carried, m_Storage), "the user cannot move an item into a full container");

		m_Storage.SetRestoring(true);
		m_ContainerManager.TrySpawnPrefabToStorage(MRX_Test_StashContainer.ITEM_PREFAB, m_Storage);
		GetGame().GetCallqueue().CallLater(CheckRestored, STEP_MS, false, ownedItems.Count());
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckRestored(int countBefore)
	{
		m_Storage.SetRestoring(false);
		array<InventoryItemComponent> ownedItems = {};
		m_Storage.GetOwnedItems(ownedItems, false);
		CheckInt(ownedItems.Count(), countBefore + 1, "restoring ignores the page limit");

		if (m_Carried)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Carried);

		SCR_EntityHelper.DeleteEntityAndChildren(m_Container);
		m_Possession.Release();
		Finish();
	}

	//------------------------------------------------------------------------------------------------
	//! \return Test items lying loose near the container.
	protected int CountOnGround()
	{
		m_iOnGround = 0;
		GetGame().GetWorld().QueryEntitiesBySphere(m_Container.GetOrigin(), GROUND_RADIUS, AddIfOnGround, null, EQueryEntitiesFlags.ALL);
		return m_iOnGround;
	}

	//------------------------------------------------------------------------------------------------
	protected bool AddIfOnGround(IEntity entity)
	{
		if (!entity.GetParent() && SCR_ResourceNameUtils.GetPrefabName(entity) == MRX_Test_StashContainer.ITEM_PREFAB)
			m_iOnGround++;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected InventoryStorageManagerComponent GetCharacterManager()
	{
		return ChimeraCharacter.Cast(m_Possession.GetCharacter()).GetCharacterController().GetInventoryStorageManager();
	}
}

//------------------------------------------------------------------------------------------------
//! Test access to the paging buttons of a storage panel.
modded class SCR_InventoryStorageBaseUI
{
	//------------------------------------------------------------------------------------------------
	bool MRX_TestHasPagingButtons()
	{
		return m_wPagingSpinboxComponent != null;
	}
}

//------------------------------------------------------------------------------------------------
//! More items than one page of the stash panel holds: the panel gets more pages and paging buttons. Without a page
//! limit, so the pages come from the items. Identical items share one slot, so the items are different prefabs from
//! the US item catalog.
class MRX_Test_StashPanelPaging : MRX_TestCase
{
	//! One page holds 6 x 8 cells; every slot takes at least one.
	protected static const int ITEM_TYPES = 60;
	protected static const int STEP_MS = 600;

	protected ref MRX_TestPossession m_Possession;
	protected IEntity m_Container;
	protected MRX_StashStorageComponent m_Storage;

	//------------------------------------------------------------------------------------------------
	override int GetTimeoutMs()
	{
		return 15000;
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

		IEntity character = m_Possession.GetCharacter();
		m_Container = MRX_TestWorldUtils.SpawnPrefab(MRX_Test_StashContainer.CONTAINER_PREFAB, character.GetOrigin() + "0.6 0 0");
		m_Storage = MRX_StashStorageComponent.Cast(m_Container.FindComponent(MRX_StashStorageComponent));
		m_Storage.SetMaxPages(0);
		MRX_StashContainerManagerComponent containerManager = MRX_StashContainerManagerComponent.Cast(m_Container.FindComponent(MRX_StashContainerManagerComponent));
		containerManager.SetUser(character);

		SCR_Faction faction = SCR_Faction.Cast(GetGame().GetFactionManager().GetFactionByKey("US"));
		SCR_EntityCatalog catalog;
		if (faction)
			catalog = faction.GetFactionEntityCatalogOfType(EEntityCatalogType.ITEM);

		if (!catalog)
		{
			Skip("no US item catalog in this scenario");
			End();
			return;
		}

		array<SCR_EntityCatalogEntry> entries = {};
		catalog.GetEntityList(entries);
		array<ResourceName> stored = {};
		foreach (SCR_EntityCatalogEntry entry : entries)
		{
			if (stored.Count() >= ITEM_TYPES)
				break;

			ResourceName prefab = entry.GetPrefab();
			if (!prefab.IsEmpty() && !stored.Contains(prefab) && containerManager.TrySpawnPrefabToStorage(prefab, m_Storage))
				stored.Insert(prefab);
		}

		CheckInt(stored.Count(), ITEM_TYPES, "different items spawned into the container");
		SCR_InventoryStorageManagerComponent.Cast(ChimeraCharacter.Cast(character).GetCharacterController().GetInventoryStorageManager()).OpenInventory();
		GetGame().GetCallqueue().CallLater(OpenPanel, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void OpenPanel()
	{
		SCR_InventoryMenuUI menu = GetMenu();
		Check(menu != null, "vanilla inventory opens");
		if (menu)
			menu.MRX_OpenStashPanel(m_Storage);

		GetGame().GetCallqueue().CallLater(CheckPages, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckPages()
	{
		SCR_InventoryMenuUI menu = GetMenu();
		SCR_InventoryOpenedStorageUI panel;
		if (menu)
			panel = menu.GetOpenedStorage(m_Storage);

		Check(panel != null, "stash panel open");
		if (panel)
		{
			array<SCR_InventorySlotUI> slots = {};
			panel.GetSlots(slots);
			string sizes;
			foreach (SCR_InventorySlotUI slot : slots)
			{
				if (slot)
					sizes += string.Format(" %1x%2", slot.GetColumnSize(), slot.GetRowSize());
			}

			Print(string.Format("[MRX_TEST]   %1 slots on %2 pages:%3", slots.Count(), panel.GetPageCount(), sizes));
			Check(panel.GetPageCount() > 1, "panel has more than one page");
			Check(panel.MRX_TestHasPagingButtons(), "panel has paging buttons");
		}

		End();
	}

	//------------------------------------------------------------------------------------------------
	protected void End()
	{
		GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.Inventory20Menu);
		if (m_Container)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Container);

		if (m_Possession)
			m_Possession.Release();

		Finish();
	}

	//------------------------------------------------------------------------------------------------
	protected static SCR_InventoryMenuUI GetMenu()
	{
		return SCR_InventoryMenuUI.Cast(GetGame().GetMenuManager().FindMenuByPreset(ChimeraMenuPreset.Inventory20Menu));
	}
}

//------------------------------------------------------------------------------------------------
class MRX_Test_StashContainer : MRX_TestCase
{
	static const ResourceName CONTAINER_PREFAB = "{82A52DBC72E19BE8}Prefabs/Marx/Stash/MRX_StashContainer.et";
	static const ResourceName ITEM_PREFAB = "{A81F501D3EF6F38E}Prefabs/Items/Medicine/FieldDressing_01/FieldDressing_US_01.et";
	static const ResourceName RIFLE_PREFAB = "{EF73725A81669E2C}Prefabs/Weapons/Rifles/M16/Rifle_M16A2_carbine_M203.et";
	protected static const int STEP_MS = 600;

	protected ref MRX_TestPossession m_Possession;
	protected IEntity m_Container;
	protected IEntity m_Other;
	protected IEntity m_Item;
	protected IEntity m_OtherItem;
	protected IEntity m_Rifle;
	protected MRX_StashStorageComponent m_Storage;
	protected int m_iAdded;
	protected int m_iRemoved;

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

		IEntity character = m_Possession.GetCharacter();
		m_Container = MRX_TestWorldUtils.SpawnPrefab(CONTAINER_PREFAB, character.GetOrigin() + "0.6 0 0");
		if (!m_Container)
		{
			Check(false, "container prefab spawns");
			End();
			return;
		}

		m_Storage = MRX_StashStorageComponent.Cast(m_Container.FindComponent(MRX_StashStorageComponent));
		MRX_StashContainerManagerComponent containerManager = MRX_StashContainerManagerComponent.Cast(m_Container.FindComponent(MRX_StashContainerManagerComponent));
		Check(m_Storage != null, "container has MRX_StashStorageComponent");
		Check(containerManager != null, "container has MRX_StashContainerManagerComponent");
		if (!m_Storage || !containerManager)
		{
			End();
			return;
		}

		containerManager.SetUser(character);
		MRX_StashStorageComponent.GetOnItemMoved().Insert(OnItemMoved);

		GetManager(character).TrySpawnPrefabToStorage(ITEM_PREFAB);
		GetManager(character).TrySpawnPrefabToStorage(RIFLE_PREFAB);
		GetGame().GetCallqueue().CallLater(OpenInventory, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void OpenInventory()
	{
		m_Item = FindCarried(m_Possession.GetCharacter(), ITEM_PREFAB);
		m_Rifle = FindCarried(m_Possession.GetCharacter(), RIFLE_PREFAB);
		Check(m_Item != null, "test item in the character inventory");
		Check(m_Rifle != null, "rifle in the character inventory");

		SCR_InventoryStorageManagerComponent.Cast(GetManager(m_Possession.GetCharacter())).OpenInventory();
		GetGame().GetCallqueue().CallLater(OpenPanel, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void OpenPanel()
	{
		SCR_InventoryMenuUI menu = GetMenu();
		Check(menu != null, "vanilla inventory opens");
		if (menu)
			menu.MRX_OpenStashPanel(m_Storage);

		// Vicinity updates run meanwhile; vanilla would close a panel of a storage without bounds.
		GetGame().GetCallqueue().CallLater(MoveIn, STEP_MS * 3);
	}

	//------------------------------------------------------------------------------------------------
	protected void MoveIn()
	{
		SCR_InventoryMenuUI menu = GetMenu();
		Check(menu && menu.GetOpenedStorage(m_Storage) != null, "container shown as its own inventory panel");
		Check(menu && !menu.MRX_IsVicinityShown(), "vicinity panel hidden while the stash panel is open");
		if (menu && menu.GetOpenedStorage(m_Storage))
			CheckInt(menu.GetOpenedStorage(m_Storage).GetPageCount(), m_Storage.GetMaxPages(), "empty stash panel shows all pages");
		Check(!InVicinity(m_Possession.GetCharacter(), m_Container), "container not listed in the vicinity");
		if (m_Item)
		{
			// Same call as dropping an item on the panel.
			InventoryItemComponent itemComponent = InventoryItemComponent.Cast(m_Item.FindComponent(InventoryItemComponent));
			SCR_InventoryStorageManagerComponent.Cast(GetManager(m_Possession.GetCharacter())).InsertItem(m_Item, m_Storage, itemComponent.GetParentSlot().GetStorage(), null);
		}

		if (m_Rifle)
			Check(GetManager(m_Possession.GetCharacter()).TryMoveItemToStorage(m_Rifle, m_Storage), "rifle move into the container accepted");

		GetGame().GetCallqueue().CallLater(OtherTakes, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void OtherTakes()
	{
		Check(m_Item && m_Storage.Contains(m_Item), "item is in the container");
		Check(m_Rifle && m_Storage.Contains(m_Rifle), "rifle is in the container");
		Check(m_iAdded > 1, "container storage reported the added items");

		m_Other = MRX_TestWorldUtils.SpawnPrefab(MRX_TestWorldUtils.CHARACTER_PREFAB, m_Possession.GetCharacter().GetOrigin() + "0 0 1");
		if (m_Other)
			GetManager(m_Other).TrySpawnPrefabToStorage(ITEM_PREFAB);

		if (m_Other && m_Item)
		{
			InventoryStorageManagerComponent otherManager = GetManager(m_Other);
			BaseInventoryStorageComponent target = otherManager.FindStorageForItem(m_Item);
			bool canMove = target && otherManager.CanMoveItemToStorage(m_Item, target);
			Check(!canMove, "another character may not move the item out");
			if (target)
				otherManager.TryMoveItemToStorage(m_Item, target);
		}

		GetGame().GetCallqueue().CallLater(OtherPutsIn, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void OtherPutsIn()
	{
		m_OtherItem = FindCarried(m_Other, ITEM_PREFAB);
		Check(m_OtherItem != null, "item in the other character's inventory");
		if (m_OtherItem)
		{
			InventoryStorageManagerComponent otherManager = GetManager(m_Other);
			Check(!otherManager.CanMoveItemToStorage(m_OtherItem, m_Storage), "another character may not put its item in");
			otherManager.TryMoveItemToStorage(m_OtherItem, m_Storage);
		}

		GetGame().GetCallqueue().CallLater(MoveOut, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void MoveOut()
	{
		Check(m_Item && m_Storage.Contains(m_Item), "another character cannot take the item out");
		Check(!m_OtherItem || !m_Storage.Contains(m_OtherItem), "another character cannot put its item in");
		if (m_Item)
		{
			InventoryStorageManagerComponent manager = GetManager(m_Possession.GetCharacter());
			BaseInventoryStorageComponent target = manager.FindStorageForItem(m_Item);
			Check(target && manager.CanMoveItemToStorage(m_Item, target), "the user may move the item out");
			if (target)
				manager.TryMoveItemToStorage(m_Item, target);
		}

		GetGame().GetCallqueue().CallLater(CheckOut, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckOut()
	{
		Check(m_Item && !m_Storage.Contains(m_Item), "the user takes the item out");
		Check(m_iRemoved > 0, "container storage reported the removed item");

		SCR_InventoryMenuUI menu = GetMenu();
		if (menu && menu.GetOpenedStorage(m_Storage))
		{
			menu.GetOpenedStorage(m_Storage).CloseStorage();
			Check(menu.MRX_IsVicinityShown(), "vicinity panel shown again after the stash panel closed");
		}

		End();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnItemMoved(MRX_StashStorageComponent storage, IEntity item, bool added)
	{
		if (storage != m_Storage)
			return;

		if (added)
			m_iAdded++;
		else
			m_iRemoved++;
	}

	//------------------------------------------------------------------------------------------------
	protected void End()
	{
		MRX_StashStorageComponent.GetOnItemMoved().Remove(OnItemMoved);
		GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.Inventory20Menu);

		if (m_Item)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Item);

		if (m_Rifle)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Rifle);

		if (m_OtherItem)
			SCR_EntityHelper.DeleteEntityAndChildren(m_OtherItem);

		if (m_Other)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Other);

		if (m_Container)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Container);

		if (m_Possession)
			m_Possession.Release();

		Finish();
	}

	//------------------------------------------------------------------------------------------------
	protected static SCR_InventoryMenuUI GetMenu()
	{
		return SCR_InventoryMenuUI.Cast(GetGame().GetMenuManager().FindMenuByPreset(ChimeraMenuPreset.Inventory20Menu));
	}

	//------------------------------------------------------------------------------------------------
	protected static bool InVicinity(IEntity character, IEntity entity)
	{
		CharacterVicinityComponent vicinity = CharacterVicinityComponent.Cast(character.FindComponent(CharacterVicinityComponent));
		if (!vicinity)
			return false;

		array<IEntity> items = {};
		vicinity.GetAvailableItems(items);
		vicinity.ManipulationComplete();
		return items.Contains(entity);
	}

	//------------------------------------------------------------------------------------------------
	protected static InventoryStorageManagerComponent GetManager(IEntity character)
	{
		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if (!chimera || !chimera.GetCharacterController())
			return null;

		return chimera.GetCharacterController().GetInventoryStorageManager();
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity FindCarried(IEntity character, ResourceName prefab)
	{
		array<IEntity> items = {};
		GetManager(character).GetItems(items);
		foreach (IEntity item : items)
		{
			if (SCR_ResourceNameUtils.GetPrefabName(item) == prefab)
				return item;
		}

		return null;
	}
}
#endif
