//! Stash world of player characters (server). Items are IEntity handles carried by the player's alive character.
class MRX_EntityAssetWorld : MRX_AssetWorld
{
	protected ref array<ref MRX_AssetSpawnOperation> m_aSpawns = {};

	//------------------------------------------------------------------------------------------------
	override MRX_EStashStatus CheckDeposit(int playerId, Managed item)
	{
		IEntity entity = IEntity.Cast(item);
		IEntity character = GetAliveCharacter(playerId);
		if (!entity || !character || entity == character || entity.IsDeleted() || !IsCarriedBy(entity, character))
			return MRX_EStashStatus.NOT_IN_INVENTORY;

		InventoryItemComponent itemComponent = InventoryItemComponent.Cast(entity.FindComponent(InventoryItemComponent));
		if (!itemComponent || itemComponent.IsLocked())
			return MRX_EStashStatus.NOT_IN_INVENTORY;

		return MRX_EStashStatus.OK;
	}

	//------------------------------------------------------------------------------------------------
	override MRX_ItemSnapshot Capture(Managed item)
	{
		IEntity entity = IEntity.Cast(item);
		if (!entity)
			return null;

		return MRX_EntitySnapshots.Capture(entity);
	}

	//------------------------------------------------------------------------------------------------
	override bool Delete(Managed item)
	{
		IEntity entity = IEntity.Cast(item);
		if (!entity || entity.IsDeleted())
			return false;

		SCR_EntityHelper.DeleteEntityAndChildren(entity);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanDeliver(int playerId, ResourceName prefab)
	{
		InventoryStorageManagerComponent manager = GetStorageManager(playerId);
		if (!manager)
			return false;

		return manager.CanInsertResource(prefab) && manager.FindStorageForResource(prefab) != null;
	}

	//------------------------------------------------------------------------------------------------
	override void Deliver(int playerId, ResourceName prefab, MRX_ItemSnapshot snapshot, notnull MRX_AssetSpawnCallback callback)
	{
		for (int i = m_aSpawns.Count() - 1; i >= 0; i--)
		{
			if (m_aSpawns[i].IsDone())
				m_aSpawns.Remove(i);
		}

		InventoryStorageManagerComponent manager = GetStorageManager(playerId);
		BaseInventoryStorageComponent storage;
		if (manager)
			storage = manager.FindStorageForResource(prefab);

		if (!storage)
		{
			callback.OnSpawned(null);
			return;
		}

		MRX_AssetSpawnOperation operation = new MRX_AssetSpawnOperation(manager, prefab, snapshot, callback);
		m_aSpawns.Insert(operation);
		if (!manager.TrySpawnPrefabToStorage(prefab, storage, -1, EStoragePurpose.PURPOSE_ANY, operation))
			operation.Fail();
	}

	//------------------------------------------------------------------------------------------------
	override bool IsAlive(Managed item)
	{
		IEntity entity = IEntity.Cast(item);
		if (!entity || entity.IsDeleted())
			return false;

		SCR_DamageManagerComponent damageManager = SCR_DamageManagerComponent.GetDamageManager(entity);
		return !damageManager || !damageManager.IsDestroyed();
	}

	//------------------------------------------------------------------------------------------------
	override bool IsCarriedBy(Managed item, Managed holder)
	{
		IEntity entity = IEntity.Cast(item);
		IEntity character = IEntity.Cast(holder);
		if (!entity || !character)
			return false;

		return IsCarriedBy(entity, character);
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity GetAliveCharacter(int playerId)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId));
		if (!character)
			return null;

		CharacterControllerComponent controller = character.GetCharacterController();
		if (!controller || controller.GetLifeState() != ECharacterLifeState.ALIVE)
			return null;

		return character;
	}

	//------------------------------------------------------------------------------------------------
	protected InventoryStorageManagerComponent GetStorageManager(int playerId)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(GetAliveCharacter(playerId));
		if (!character)
			return null;

		return character.GetCharacterController().GetInventoryStorageManager();
	}

	//------------------------------------------------------------------------------------------------
	//! True when the entity hangs below the character, without another character in between.
	protected bool IsCarriedBy(notnull IEntity entity, notnull IEntity character)
	{
		IEntity parent = entity.GetParent();
		while (parent)
		{
			if (parent == character)
				return true;

			if (ChimeraCharacter.Cast(parent))
				return false;

			parent = parent.GetParent();
		}

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! Spawns the root item into the inventory, then restores the snapshot on it (if any). Internal.
class MRX_AssetSpawnOperation : ScriptedInventoryOperationCallback
{
	protected InventoryStorageManagerComponent m_Manager;
	protected ResourceName m_sPrefab;
	protected ref MRX_ItemSnapshot m_Snapshot;
	protected ref MRX_AssetSpawnCallback m_Callback;
	protected bool m_bDone;

	//------------------------------------------------------------------------------------------------
	void MRX_AssetSpawnOperation(InventoryStorageManagerComponent manager, ResourceName prefab, MRX_ItemSnapshot snapshot, MRX_AssetSpawnCallback callback)
	{
		m_Manager = manager;
		m_sPrefab = prefab;
		m_Snapshot = snapshot;
		m_Callback = callback;
	}

	//------------------------------------------------------------------------------------------------
	bool IsDone()
	{
		return m_bDone;
	}

	//------------------------------------------------------------------------------------------------
	//! Reports failure when the spawn request was not accepted.
	void Fail()
	{
		Report(null);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnComplete()
	{
		IEntity entity;
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(GetItem()));
		if (rpl)
			entity = rpl.GetEntity();

		if (entity && m_Manager && m_Snapshot && !MRX_EntitySnapshots.Apply(entity, m_Snapshot, m_Manager))
			Print(string.Format("[MRX] %1 was restored without part of its contents", m_sPrefab), LogLevel.ERROR);

		Report(entity);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnFailed()
	{
		Report(null);
	}

	//------------------------------------------------------------------------------------------------
	protected void Report(IEntity entity)
	{
		if (m_bDone)
			return;

		m_bDone = true;
		m_Callback.OnSpawned(entity);
	}
}
