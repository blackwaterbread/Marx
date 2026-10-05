//! Shop inventory of player characters (server). Items for sale are IEntity handles.
class MRX_EntityShopInventory : MRX_ShopInventory
{
	protected ref array<ref MRX_ShopSpawnCallback> m_aSpawnCallbacks = {};

	//------------------------------------------------------------------------------------------------
	override bool CanGive(int playerId, ResourceName prefab)
	{
		InventoryStorageManagerComponent manager = GetStorageManager(playerId);
		if (!manager)
			return false;

		return manager.CanInsertResource(prefab) && manager.FindStorageForResource(prefab) != null;
	}

	//------------------------------------------------------------------------------------------------
	override void Give(int playerId, ResourceName prefab, notnull MRX_ShopDeliveryCallback callback)
	{
		for (int i = m_aSpawnCallbacks.Count() - 1; i >= 0; i--)
		{
			if (m_aSpawnCallbacks[i].IsDone())
				m_aSpawnCallbacks.Remove(i);
		}

		InventoryStorageManagerComponent manager = GetStorageManager(playerId);
		BaseInventoryStorageComponent storage;
		if (manager)
			storage = manager.FindStorageForResource(prefab);

		if (!storage)
		{
			callback.OnResult(false);
			return;
		}

		MRX_ShopSpawnCallback spawnCallback = new MRX_ShopSpawnCallback(callback);
		m_aSpawnCallbacks.Insert(spawnCallback);
		if (!manager.TrySpawnPrefabToStorage(prefab, storage, -1, EStoragePurpose.PURPOSE_ANY, spawnCallback))
			spawnCallback.Fail();
	}

	//------------------------------------------------------------------------------------------------
	override MRX_EShopStatus InspectForSale(int playerId, Managed item, out ResourceName prefab)
	{
		IEntity entity = IEntity.Cast(item);
		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!entity || !character || !IsCarriedBy(entity, character))
			return MRX_EShopStatus.NOT_IN_INVENTORY;

		InventoryItemComponent itemComponent = InventoryItemComponent.Cast(entity.FindComponent(InventoryItemComponent));
		if (!itemComponent || itemComponent.IsLocked())
			return MRX_EShopStatus.NOT_IN_INVENTORY;

		if (!IsEmpty(entity))
			return MRX_EShopStatus.NOT_EMPTY;

		prefab = SCR_ResourceNameUtils.GetPrefabName(entity);
		return MRX_EShopStatus.OK;
	}

	//------------------------------------------------------------------------------------------------
	override bool Remove(Managed item)
	{
		IEntity entity = IEntity.Cast(item);
		if (!entity)
			return false;

		// Delete right away: the payment follows, so the item must not stay usable.
		RplComponent.DeleteRplEntity(entity, false);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Alive character of the player, or null.
	protected InventoryStorageManagerComponent GetStorageManager(int playerId)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId));
		if (!character)
			return null;

		CharacterControllerComponent controller = character.GetCharacterController();
		if (!controller || controller.GetLifeState() != ECharacterLifeState.ALIVE)
			return null;

		return controller.GetInventoryStorageManager();
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

	//------------------------------------------------------------------------------------------------
	//! No attachment, loaded magazine or stored item in any of the entity's storages.
	protected bool IsEmpty(notnull IEntity entity)
	{
		array<Managed> storages = {};
		entity.FindComponents(BaseInventoryStorageComponent, storages);
		array<IEntity> contents = {};
		foreach (Managed component : storages)
		{
			BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(component);
			if (storage && storage.GetAll(contents) > 0)
				return false;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Bridges an inventory spawn to a shop delivery callback. Internal.
class MRX_ShopSpawnCallback : ScriptedInventoryOperationCallback
{
	protected ref MRX_ShopDeliveryCallback m_Callback;
	protected bool m_bDone;

	//------------------------------------------------------------------------------------------------
	void MRX_ShopSpawnCallback(MRX_ShopDeliveryCallback callback)
	{
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
		Report(false);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnComplete()
	{
		Report(true);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnFailed()
	{
		Report(false);
	}

	//------------------------------------------------------------------------------------------------
	protected void Report(bool success)
	{
		if (m_bDone)
			return;

		m_bDone = true;
		m_Callback.OnResult(success);
	}
}
