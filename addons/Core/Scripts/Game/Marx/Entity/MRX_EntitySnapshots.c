//! Captures and restores item snapshots through component APIs (server). Walks storages like the vanilla arsenal
//! loadout (SCR_PlayerArsenalLoadout): every storage component of an item, its directly owned items by slot, recursively.
class MRX_EntitySnapshots
{
	//------------------------------------------------------------------------------------------------
	static MRX_ItemSnapshot Capture(notnull IEntity entity)
	{
		MRX_ItemSnapshot snapshot = MRX_ItemSnapshot.Create(SCR_ResourceNameUtils.GetPrefabName(entity));
		CaptureInto(entity, snapshot);
		return snapshot;
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the snapshot to an entity of the same prefab: its own state, then its contents. Items the snapshot
	//! does not list are removed (prefab defaults), missing ones are spawned into their saved slot.
	//! \param manager Inventory manager used for the spawns and deletions (e.g. the carrying character's).
	//! \return False when part of the contents could not be restored.
	static bool Apply(notnull IEntity entity, notnull MRX_ItemSnapshot snapshot, notnull InventoryStorageManagerComponent manager)
	{
		ApplyState(entity, snapshot);

		set<BaseInventoryStorageComponent> storages = new set<BaseInventoryStorageComponent>();
		FindStorages(entity, storages);

		bool complete = true;
		array<string> restoredStorageIds = {};
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			string storageId = GetStorageId(entity, storage);
			restoredStorageIds.Insert(storageId);

			map<int, IEntity> existing = new map<int, IEntity>();
			array<InventoryItemComponent> ownedItems = {};
			storage.GetOwnedItems(ownedItems, false);
			foreach (InventoryItemComponent ownedItem : ownedItems)
			{
				existing.Set(ownedItem.GetParentSlot().GetID(), ownedItem.GetOwner());
			}

			foreach (MRX_ItemSnapshot child : snapshot.m_aChildren)
			{
				if (child.m_sStorage != storageId)
					continue;

				IEntity current;
				existing.Take(child.m_iSlot, current);
				if (current && SCR_ResourceNameUtils.GetPrefabName(current) != child.m_sPrefab)
				{
					manager.TryDeleteItem(current);
					current = null;
				}

				if (!current)
				{
					// Synchronous on the server, as the vanilla arsenal relies on.
					if (manager.TrySpawnPrefabToStorage(child.m_sPrefab, storage, child.m_iSlot))
						current = storage.Get(child.m_iSlot);
				}

				if (!current)
				{
					Print(string.Format("[MRX] Could not restore %1 into slot %2 of %3", child.m_sPrefab, child.m_iSlot, storageId), LogLevel.ERROR);
					complete = false;
					continue;
				}

				if (!Apply(current, child, manager))
					complete = false;
			}

			foreach (int slotId, IEntity leftover : existing)
			{
				if (leftover)
					manager.TryDeleteItem(leftover);
			}
		}

		foreach (MRX_ItemSnapshot orphan : snapshot.m_aChildren)
		{
			if (!restoredStorageIds.Contains(orphan.m_sStorage))
			{
				Print(string.Format("[MRX] %1 has no storage %2 for %3 any more", snapshot.m_sPrefab, orphan.m_sStorage, orphan.m_sPrefab), LogLevel.ERROR);
				complete = false;
			}
		}

		return complete;
	}

	//------------------------------------------------------------------------------------------------
	//! Every storage component of the entity, including nested compartments.
	static void FindStorages(notnull IEntity entity, notnull set<BaseInventoryStorageComponent> storages)
	{
		array<Managed> components = {};
		entity.FindComponents(BaseInventoryStorageComponent, components);
		foreach (Managed component : components)
		{
			BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(component);
			if (storage)
			{
				storages.Insert(storage);
				FindChildStorages(storage, storages);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Stable within a prefab: component class plus the GUID of the component source.
	static string GetStorageId(notnull IEntity owner, notnull GenericComponent storage)
	{
		BaseContainer source = storage.GetComponentSource(owner);
		string guid;
		if (source)
			guid = SCR_ResourceNameUtils.GetPrefabGUID(source.GetResourceName());

		return storage.ClassName() + ":" + guid;
	}

	//------------------------------------------------------------------------------------------------
	protected static void FindChildStorages(notnull GenericComponent parent, notnull set<BaseInventoryStorageComponent> storages)
	{
		array<GenericComponent> components = {};
		parent.FindComponents(BaseInventoryStorageComponent, components);
		foreach (GenericComponent component : components)
		{
			BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(component);
			if (storage && !storages.Contains(storage))
			{
				storages.Insert(storage);
				FindChildStorages(storage, storages);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void CaptureInto(notnull IEntity entity, notnull MRX_ItemSnapshot snapshot)
	{
		CaptureState(entity, snapshot);

		set<BaseInventoryStorageComponent> storages = new set<BaseInventoryStorageComponent>();
		FindStorages(entity, storages);
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			string storageId = GetStorageId(entity, storage);
			array<InventoryItemComponent> items = {};
			GetOrderedItems(storage, items);
			foreach (InventoryItemComponent item : items)
			{
				IEntity childEntity = item.GetOwner();
				InventoryStorageSlot slot = item.GetParentSlot();
				if (!childEntity || !slot)
					continue;

				MRX_ItemSnapshot child = MRX_ItemSnapshot.Create(SCR_ResourceNameUtils.GetPrefabName(childEntity));
				child.m_iFormat = 0;
				child.m_sStorage = storageId;
				child.m_iSlot = slot.GetID();
				CaptureInto(childEntity, child);
				snapshot.m_aChildren.Insert(child);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Directly owned items. Weapon attachments that need other attachments (e.g. a bayonet) come last.
	protected static void GetOrderedItems(notnull BaseInventoryStorageComponent storage, notnull array<InventoryItemComponent> outItems)
	{
		storage.GetOwnedItems(outItems, false);
		if (!SCR_WeaponAttachmentsStorageComponent.Cast(storage))
			return;

		array<ref SCR_SortableItem<InventoryItemComponent>> sortable = {};
		foreach (InventoryItemComponent item : outItems)
		{
			int requirements;
			SCR_WeaponAttachmentObstructionAttributes attributes = SCR_WeaponAttachmentObstructionAttributes.Cast(item.FindAttribute(SCR_WeaponAttachmentObstructionAttributes));
			if (attributes)
				requirements = attributes.GetRequiredAttachmentTypes().Count();

			sortable.Insert(new SCR_SortableItem<InventoryItemComponent>(item, requirements));
		}

		sortable.Sort();
		outItems.Clear();
		foreach (SCR_SortableItem<InventoryItemComponent> sorted : sortable)
		{
			outItems.Insert(sorted.m_Item);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void CaptureState(notnull IEntity entity, notnull MRX_ItemSnapshot snapshot)
	{
		BaseMagazineComponent magazine = BaseMagazineComponent.Cast(entity.FindComponent(BaseMagazineComponent));
		if (magazine)
			snapshot.m_iAmmo = magazine.GetAmmoCount();

		HitZoneContainerComponent hitZoneContainer = HitZoneContainerComponent.Cast(entity.FindComponent(HitZoneContainerComponent));
		if (hitZoneContainer)
		{
			array<HitZone> zones = {};
			hitZoneContainer.GetAllHitZones(zones);
			foreach (HitZone zone : zones)
			{
				if (zone.GetHealth() < zone.GetMaxHealth())
					snapshot.m_aHitZones.Insert(MRX_HitZoneSnapshot.Create(zone.GetName(), zone.GetHealth()));
			}
		}

		SCR_FuelManagerComponent fuelManager = SCR_FuelManagerComponent.Cast(entity.FindComponent(SCR_FuelManagerComponent));
		if (fuelManager)
		{
			array<SCR_FuelNode> nodes = {};
			fuelManager.GetScriptedFuelNodesList(nodes);
			foreach (SCR_FuelNode node : nodes)
			{
				snapshot.m_aFuel.Insert(MRX_FuelSnapshot.Create(node.GetFuelTankID(), node.GetFuel()));
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void ApplyState(notnull IEntity entity, notnull MRX_ItemSnapshot snapshot)
	{
		BaseMagazineComponent magazine = BaseMagazineComponent.Cast(entity.FindComponent(BaseMagazineComponent));
		if (magazine && snapshot.m_iAmmo >= 0)
			magazine.SetAmmoCount(snapshot.m_iAmmo);

		HitZoneContainerComponent hitZoneContainer = HitZoneContainerComponent.Cast(entity.FindComponent(HitZoneContainerComponent));
		if (hitZoneContainer && !snapshot.m_aHitZones.IsEmpty())
		{
			array<HitZone> zones = {};
			hitZoneContainer.GetAllHitZones(zones);
			foreach (MRX_HitZoneSnapshot saved : snapshot.m_aHitZones)
			{
				foreach (HitZone zone : zones)
				{
					if (zone.GetName() == saved.m_sName)
						zone.SetHealth(saved.m_fHealth);
				}
			}
		}

		SCR_FuelManagerComponent fuelManager = SCR_FuelManagerComponent.Cast(entity.FindComponent(SCR_FuelManagerComponent));
		if (fuelManager && !snapshot.m_aFuel.IsEmpty())
		{
			array<SCR_FuelNode> nodes = {};
			fuelManager.GetScriptedFuelNodesList(nodes);
			foreach (MRX_FuelSnapshot fuel : snapshot.m_aFuel)
			{
				foreach (SCR_FuelNode node : nodes)
				{
					if (node.GetFuelTankID() == fuel.m_iTankId)
						node.SetFuel(fuel.m_fFuel);
				}
			}
		}
	}
}
