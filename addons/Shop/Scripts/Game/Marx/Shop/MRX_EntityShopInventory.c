//! Shop inventory of player characters (server). Items for sale are IEntity handles, delivery targets are
//! MRX_ShopStorageTarget.
class MRX_EntityShopInventory : MRX_ShopInventory
{
	protected ref array<ref MRX_ShopSpawnCallback> m_aSpawnCallbacks = {};
	protected ref MRX_EntityAssetWorld m_AssetWorld = new MRX_EntityAssetWorld();

	//------------------------------------------------------------------------------------------------
	override bool CanGive(int playerId, ResourceName prefab, Managed target = null)
	{
		InventoryStorageManagerComponent manager = GetStorageManager(playerId);
		if (!manager)
			return false;

		if (target)
		{
			BaseInventoryStorageComponent storage = MRX_ShopStorageTarget.Resolve(target);
			return storage && manager.CanInsertResourceInStorage(prefab, storage);
		}

		return manager.CanInsertResource(prefab) && manager.FindStorageForResource(prefab) != null;
	}

	//------------------------------------------------------------------------------------------------
	override void Give(int playerId, ResourceName prefab, notnull MRX_ShopDeliveryCallback callback, Managed target = null)
	{
		for (int i = m_aSpawnCallbacks.Count() - 1; i >= 0; i--)
		{
			if (m_aSpawnCallbacks[i].IsDone())
				m_aSpawnCallbacks.Remove(i);
		}

		InventoryStorageManagerComponent manager = GetStorageManager(playerId);
		BaseInventoryStorageComponent storage;
		int slot = -1;
		if (manager && target)
		{
			storage = MRX_ShopStorageTarget.Resolve(target);
			if (storage)
				slot = FindSlot(manager, prefab, storage);
		}
		else if (manager)
		{
			storage = manager.FindStorageForResource(prefab);
		}

		if (!storage)
		{
			callback.OnResult(false);
			return;
		}

		MRX_ShopSpawnCallback spawnCallback = new MRX_ShopSpawnCallback(callback);
		m_aSpawnCallbacks.Insert(spawnCallback);
		if (!manager.TrySpawnPrefabToStorage(prefab, storage, slot, EStoragePurpose.PURPOSE_ANY, spawnCallback))
			spawnCallback.Fail();
	}

	//------------------------------------------------------------------------------------------------
	//! First slot of the storage that takes the prefab, or -1. A weapon's attachment slots take one kind of item each.
	protected static int FindSlot(notnull InventoryStorageManagerComponent manager, ResourceName prefab, notnull BaseInventoryStorageComponent storage)
	{
		int count = storage.GetSlotsCount();
		for (int i = 0; i < count; i++)
		{
			if (manager.CanInsertResourceInStorage(prefab, storage, i))
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	override MRX_EShopStatus InspectForSale(int playerId, Managed item, out ResourceName prefab, array<ResourceName> outContents = null)
	{
		IEntity entity = IEntity.Cast(item);
		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!entity || !character || !IsCarriedBy(entity, character))
			return MRX_EShopStatus.NOT_IN_INVENTORY;

		InventoryItemComponent itemComponent = InventoryItemComponent.Cast(entity.FindComponent(InventoryItemComponent));
		if (!itemComponent || itemComponent.IsLocked())
			return MRX_EShopStatus.NOT_IN_INVENTORY;

		if (MRX_IssuedItems.IsIssued(entity))
			return MRX_EShopStatus.ISSUED;

		if (outContents)
		{
			// Mission items (not refundable) are never deleted, neither alone nor inside another item.
			if (!IsRefundable(itemComponent) || !CollectContents(entity, outContents))
				return MRX_EShopStatus.NOT_BUYABLE;
		}
		else if (!IsEmpty(entity))
		{
			return MRX_EShopStatus.NOT_EMPTY;
		}

		prefab = SCR_ResourceNameUtils.GetPrefabName(entity);
		return MRX_EShopStatus.OK;
	}

	//------------------------------------------------------------------------------------------------
	override Managed CaptureForReturn(int playerId, Managed item)
	{
		IEntity entity = IEntity.Cast(item);
		if (!entity)
			return null;

		MRX_ShopReturnCapture capture = new MRX_ShopReturnCapture();
		capture.m_Snapshot = MRX_EntitySnapshots.Capture(entity);
		InventoryItemComponent itemComponent = InventoryItemComponent.Cast(entity.FindComponent(InventoryItemComponent));
		if (itemComponent && itemComponent.GetParentSlot() && itemComponent.GetParentSlot().GetStorage())
			capture.m_StorageId = Replication.FindItemId(itemComponent.GetParentSlot().GetStorage());

		return capture;
	}

	//------------------------------------------------------------------------------------------------
	override bool Remove(Managed item)
	{
		IEntity entity = IEntity.Cast(item);
		if (!entity)
			return false;

		// Like the vanilla arsenal refund: worn items through the character's inventory, the rest directly. Delete right
		// away either way: the payment follows, so the item must not stay usable.
		IEntity parent = entity.GetParent();
		InventoryStorageManagerComponent manager;
		if (parent)
			manager = InventoryStorageManagerComponent.Cast(parent.FindComponent(SCR_InventoryStorageManagerComponent));

		if (manager)
			return manager.TryDeleteItem(entity);

		RplComponent.DeleteRplEntity(entity, false);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Back into the storage it came from, else anywhere in the inventory, else on the ground next to the player.
	override void GiveBack(int playerId, ResourceName prefab, Managed capture, notnull MRX_ShopDeliveryCallback callback)
	{
		MRX_ShopReturnCapture returnCapture = MRX_ShopReturnCapture.Cast(capture);
		if (!returnCapture)
		{
			Give(playerId, prefab, callback);
			return;
		}

		InventoryStorageManagerComponent manager = GetStorageManager(playerId);
		BaseInventoryStorageComponent storage;
		if (manager && returnCapture.m_StorageId.IsValid())
		{
			storage = BaseInventoryStorageComponent.Cast(Replication.FindItem(returnCapture.m_StorageId));
			if (storage && !manager.CanInsertResourceInStorage(prefab, storage))
				storage = null;
		}

		if (manager && !storage && manager.CanInsertResource(prefab))
			storage = manager.FindStorageForResource(prefab);

		if (storage)
		{
			m_AssetWorld.SpawnIntoStorage(manager, storage, prefab, returnCapture.m_Snapshot, new MRX_ShopReturnSpawn(callback));
			return;
		}

		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		Resource resource = Resource.Load(prefab);
		if (!character || !resource || !resource.IsValid())
		{
			callback.OnResult(false);
			return;
		}

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = character.GetOrigin();
		IEntity dropped = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
		InventoryStorageManagerComponent characterManager = InventoryStorageManagerComponent.Cast(character.FindComponent(SCR_InventoryStorageManagerComponent));
		if (dropped && characterManager && !MRX_EntitySnapshots.Apply(dropped, returnCapture.m_Snapshot, characterManager))
			Print(string.Format("[MRX] %1 was returned without part of its contents", prefab), LogLevel.ERROR);

		callback.OnResult(dropped != null);
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

	//------------------------------------------------------------------------------------------------
	//! Adds the prefab of everything the entity holds, recursively, walking storages like MRX_EntitySnapshots.
	//! \return False when one of them must not be sold (not refundable) or is being moved.
	protected bool CollectContents(notnull IEntity entity, notnull array<ResourceName> outContents)
	{
		set<BaseInventoryStorageComponent> storages = new set<BaseInventoryStorageComponent>();
		MRX_EntitySnapshots.FindStorages(entity, storages);
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			array<InventoryItemComponent> items = {};
			storage.GetOwnedItems(items, false);
			foreach (InventoryItemComponent item : items)
			{
				IEntity child = item.GetOwner();
				if (!child)
					continue;

				if (item.IsLocked() || !IsRefundable(item))
					return false;

				if (MRX_IssuedItems.IsIssued(child))
					outContents.Insert(ResourceName.Empty);
				else
					outContents.Insert(SCR_ResourceNameUtils.GetPrefabName(child));

				if (!CollectContents(child, outContents))
					return false;
			}
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsRefundable(notnull InventoryItemComponent item)
	{
		SCR_ItemAttributeCollection attributes = SCR_ItemAttributeCollection.Cast(item.GetAttributes());
		return !attributes || attributes.IsRefundable();
	}
}

//------------------------------------------------------------------------------------------------
//! Delivery target of MRX_ShopService.Buy() for the entity inventory (API v0): a storage, kept by RplId so a storage
//! deleted meanwhile is simply not found.
class MRX_ShopStorageTarget : Managed
{
	protected RplId m_StorageId;

	//------------------------------------------------------------------------------------------------
	//! \return Null for a storage without replication ID.
	static MRX_ShopStorageTarget Create(notnull BaseInventoryStorageComponent storage)
	{
		RplId storageId = Replication.FindItemId(storage);
		if (!storageId.IsValid())
			return null;

		MRX_ShopStorageTarget target = new MRX_ShopStorageTarget();
		target.m_StorageId = storageId;
		return target;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The storage, or null when the target is not a MRX_ShopStorageTarget or its storage is gone.
	static BaseInventoryStorageComponent Resolve(Managed target)
	{
		MRX_ShopStorageTarget storageTarget = MRX_ShopStorageTarget.Cast(target);
		if (!storageTarget)
			return null;

		return BaseInventoryStorageComponent.Cast(Replication.FindItem(storageTarget.m_StorageId));
	}
}

//------------------------------------------------------------------------------------------------
//! What MRX_EntityShopInventory needs to give a sold item back. Internal.
class MRX_ShopReturnCapture : Managed
{
	ref MRX_ItemSnapshot m_Snapshot;
	//! Storage the item was in.
	RplId m_StorageId;
}

//------------------------------------------------------------------------------------------------
//! Bridges a restoring spawn to a shop delivery callback. Internal.
class MRX_ShopReturnSpawn : MRX_AssetSpawnCallback
{
	protected ref MRX_ShopDeliveryCallback m_Callback;

	//------------------------------------------------------------------------------------------------
	void MRX_ShopReturnSpawn(MRX_ShopDeliveryCallback callback)
	{
		m_Callback = callback;
	}

	//------------------------------------------------------------------------------------------------
	override void OnSpawned(Managed item)
	{
		m_Callback.OnResult(item != null);
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
