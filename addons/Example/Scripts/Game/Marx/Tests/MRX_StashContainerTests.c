#ifdef WORKBENCH
// Tests of the stash container prefab and components: opened in the vanilla inventory, item moves reported on the
// server, other characters kept from taking items out, room for large items like rifles.

//------------------------------------------------------------------------------------------------
class MRX_StashContainerTests
{
	//------------------------------------------------------------------------------------------------
	static void Register(notnull MRX_TestRunner runner)
	{
		runner.Add(new MRX_Test_StashContainer());
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

		SCR_InventoryStorageManagerComponent manager = SCR_InventoryStorageManagerComponent.Cast(GetManager(m_Possession.GetCharacter()));
		manager.SetStorageToOpen(m_Container);
		manager.OpenInventory();
		GetGame().GetCallqueue().CallLater(MoveIn, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void MoveIn()
	{
		Check(GetGame().GetMenuManager().FindMenuByPreset(ChimeraMenuPreset.Inventory20Menu) != null, "vanilla inventory opens with the container");
		if (m_Item)
			Check(GetManager(m_Possession.GetCharacter()).TryMoveItemToStorage(m_Item, m_Storage), "move into the container accepted");

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
		if (m_Other && m_Item)
		{
			InventoryStorageManagerComponent otherManager = GetManager(m_Other);
			BaseInventoryStorageComponent target = otherManager.FindStorageForItem(m_Item);
			bool canMove = target && otherManager.CanMoveItemToStorage(m_Item, target);
			Check(!canMove, "another character may not move the item out");
			if (target)
				otherManager.TryMoveItemToStorage(m_Item, target);
		}

		GetGame().GetCallqueue().CallLater(MoveOut, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void MoveOut()
	{
		Check(m_Item && m_Storage.Contains(m_Item), "another character cannot take the item out");
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

		if (m_Other)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Other);

		if (m_Container)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Container);

		if (m_Possession)
			m_Possession.Release();

		Finish();
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
