//! Captures and restores item snapshots through component APIs (server). Walks storages like the vanilla arsenal
//! loadout (SCR_PlayerArsenalLoadout): every storage component of an item, its directly owned items by slot, recursively.
class MRX_EntitySnapshots
{
	//------------------------------------------------------------------------------------------------
	static MRX_ItemSnapshot Capture(notnull IEntity entity)
	{
		MRX_ItemSnapshot snapshot = MRX_ItemSnapshot.Create(SCR_ResourceNameUtils.GetPrefabName(entity));
		set<BaseInventoryStorageComponent> storages = new set<BaseInventoryStorageComponent>();
		FindStorages(entity, storages);
		CaptureInto(entity, snapshot, storages, false);
		return snapshot;
	}

	//------------------------------------------------------------------------------------------------
	//! Gear of a character as a loadout (API v0): the items in its clothing, weapon and equipment storages (those the
	//! vanilla arsenal loadout saves, not e.g. applied tourniquets or the identity item), captured as new items: full
	//! magazines, no damage, not issued. The root is the character; its own prefab is not part of the loadout.
	static MRX_ItemSnapshot CaptureLoadout(notnull IEntity character)
	{
		MRX_ItemSnapshot snapshot = MRX_ItemSnapshot.Create(SCR_ResourceNameUtils.GetPrefabName(character));
		CaptureInto(character, snapshot, GetLoadoutStorages(character), true);
		return snapshot;
	}

	//------------------------------------------------------------------------------------------------
	//! Gear of a character like CaptureLoadout, but with its exact state (ammo, damage, issued marks), e.g. to give a
	//! player back the gear they left with (API v0).
	static MRX_ItemSnapshot CaptureLoadoutState(notnull IEntity character)
	{
		MRX_ItemSnapshot snapshot = MRX_ItemSnapshot.Create(SCR_ResourceNameUtils.GetPrefabName(character));
		CaptureInto(character, snapshot, GetLoadoutStorages(character), false);
		return snapshot;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts a loadout (CaptureLoadout or CaptureLoadoutState) on a character: items of the loadout storages the loadout does not list are
	//! deleted, items of the same prefab in the same slot kept (with the loadout's state), missing ones spawned.
	//! \return False when part of the loadout could not be put on.
	static bool ApplyLoadout(notnull IEntity character, notnull MRX_ItemSnapshot loadout, notnull InventoryStorageManagerComponent manager)
	{
		return ApplyContents(character, loadout, manager, GetLoadoutStorages(character));
	}

	//------------------------------------------------------------------------------------------------
	//! Items in the character's loadout storages (see CaptureLoadout), at any depth.
	static void GetLoadoutItems(notnull IEntity character, notnull array<IEntity> outItems)
	{
		foreach (BaseInventoryStorageComponent storage : GetLoadoutStorages(character))
		{
			CollectItems(storage, outItems);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Storages of a character that hold its loadout: the component types the vanilla arsenal loadout saves (exactly
	//! these types, as vanilla checks them).
	static set<BaseInventoryStorageComponent> GetLoadoutStorages(notnull IEntity character)
	{
		array<typename> types;
		SCR_ArsenalManagerComponent.GetArsenalLoadoutComponentsToCheck(types);
		set<BaseInventoryStorageComponent> all = new set<BaseInventoryStorageComponent>();
		FindStorages(character, all);
		set<BaseInventoryStorageComponent> storages = new set<BaseInventoryStorageComponent>();
		foreach (BaseInventoryStorageComponent storage : all)
		{
			if (types && types.Contains(storage.Type()))
				storages.Insert(storage);
		}

		return storages;
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
		return ApplyContents(entity, snapshot, manager, storages);
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the snapshot's contents to the given storages of the entity (see Apply).
	protected static bool ApplyContents(notnull IEntity entity, notnull MRX_ItemSnapshot snapshot, notnull InventoryStorageManagerComponent manager, notnull set<BaseInventoryStorageComponent> storages)
	{
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
	//! \param storages Storages of the entity whose contents are captured.
	//! \param asNew Captures the items as new ones: full magazines, no damage, not issued.
	protected static void CaptureInto(notnull IEntity entity, notnull MRX_ItemSnapshot snapshot, notnull set<BaseInventoryStorageComponent> storages, bool asNew)
	{
		CaptureState(entity, snapshot, asNew);

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
				set<BaseInventoryStorageComponent> childStorages = new set<BaseInventoryStorageComponent>();
				FindStorages(childEntity, childStorages);
				CaptureInto(childEntity, child, childStorages, asNew);
				snapshot.m_aChildren.Insert(child);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Items of the storage and everything they hold, at any depth.
	protected static void CollectItems(notnull BaseInventoryStorageComponent storage, notnull array<IEntity> outItems)
	{
		array<InventoryItemComponent> items = {};
		storage.GetOwnedItems(items, false);
		foreach (InventoryItemComponent item : items)
		{
			IEntity owner = item.GetOwner();
			if (!owner || outItems.Contains(owner))
				continue;

			outItems.Insert(owner);
			set<BaseInventoryStorageComponent> storages = new set<BaseInventoryStorageComponent>();
			FindStorages(owner, storages);
			foreach (BaseInventoryStorageComponent childStorage : storages)
			{
				CollectItems(childStorage, outItems);
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
	protected static void CaptureState(notnull IEntity entity, notnull MRX_ItemSnapshot snapshot, bool asNew)
	{
		BaseMagazineComponent magazine = BaseMagazineComponent.Cast(entity.FindComponent(BaseMagazineComponent));
		if (magazine && asNew)
			snapshot.m_iAmmo = magazine.GetMaxAmmoCount();
		else if (magazine)
			snapshot.m_iAmmo = magazine.GetAmmoCount();

		if (asNew)
			return;

		snapshot.m_bIssued = MRX_IssuedItems.IsIssued(entity);

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
		if (snapshot.m_bIssued)
			MRX_IssuedItems.Mark(entity, false);
		else
			MRX_IssuedItems.Unmark(entity, false);

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
