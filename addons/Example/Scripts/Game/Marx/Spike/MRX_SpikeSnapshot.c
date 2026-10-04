#ifdef WORKBENCH
// Runtime spike: Marx-owned entity snapshot format (custom serializer, arsenal-loadout style).
// Capture state through component APIs -> plain JSON -> delete -> spawn prefab -> restore state.
// Temporary - remove before release. Logs are prefixed with [MRX_SPIKE] snapshot.

//------------------------------------------------------------------------------------------------
class MRX_SpikeFuelSnapshot
{
	int m_iTankId;
	float m_fFuel;
}

//------------------------------------------------------------------------------------------------
class MRX_SpikeHitZoneSnapshot
{
	string m_sName;
	float m_fHealth;
}

//------------------------------------------------------------------------------------------------
class MRX_SpikeItemSnapshot
{
	string m_sPrefab;
	int m_iAmmo = -1;
	ref array<ref MRX_SpikeFuelSnapshot> m_aFuel = {};
	ref array<ref MRX_SpikeHitZoneSnapshot> m_aHitZones = {};
	ref array<ref MRX_SpikeItemSnapshot> m_aCargo = {};
}

//------------------------------------------------------------------------------------------------
class MRX_SpikeSnapshotUtils
{
	static const ResourceName PREFAB_MAGAZINE = "{D8F2CA92583B23D3}Prefabs/Weapons/Magazines/Magazine_556x45_STANAG_30rnd_M855_M856_Last_5Tracer.et";

	//------------------------------------------------------------------------------------------------
	static BaseMagazineComponent FindMagazine(IEntity entity)
	{
		BaseWeaponComponent weapon = BaseWeaponComponent.Cast(entity.FindComponent(BaseWeaponComponent));
		if (weapon)
			return weapon.GetCurrentMagazine();

		return BaseMagazineComponent.Cast(entity.FindComponent(BaseMagazineComponent));
	}

	//------------------------------------------------------------------------------------------------
	static MRX_SpikeItemSnapshot Capture(IEntity entity, bool includeCargo)
	{
		MRX_SpikeItemSnapshot snapshot = new MRX_SpikeItemSnapshot();
		snapshot.m_sPrefab = entity.GetPrefabData().GetPrefabName();

		BaseMagazineComponent magazine = FindMagazine(entity);
		if (magazine)
			snapshot.m_iAmmo = magazine.GetAmmoCount();

		SCR_FuelManagerComponent fuelManager = SCR_FuelManagerComponent.Cast(entity.FindComponent(SCR_FuelManagerComponent));
		if (fuelManager)
		{
			array<SCR_FuelNode> nodes = {};
			fuelManager.GetScriptedFuelNodesList(nodes);
			foreach (SCR_FuelNode node : nodes)
			{
				MRX_SpikeFuelSnapshot fuel = new MRX_SpikeFuelSnapshot();
				fuel.m_iTankId = node.GetFuelTankID();
				fuel.m_fFuel = node.GetFuel();
				snapshot.m_aFuel.Insert(fuel);
			}
		}

		HitZoneContainerComponent hitZoneContainer = HitZoneContainerComponent.Cast(entity.FindComponent(HitZoneContainerComponent));
		if (hitZoneContainer)
		{
			array<HitZone> zones = {};
			hitZoneContainer.GetAllHitZones(zones);
			foreach (HitZone zone : zones)
			{
				if (zone.GetHealth() >= zone.GetMaxHealth())
					continue;

				MRX_SpikeHitZoneSnapshot hitZone = new MRX_SpikeHitZoneSnapshot();
				hitZone.m_sName = zone.GetName();
				hitZone.m_fHealth = zone.GetHealth();
				snapshot.m_aHitZones.Insert(hitZone);
			}
		}

		if (includeCargo)
		{
			InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(entity.FindComponent(InventoryStorageManagerComponent));
			if (manager)
			{
				array<IEntity> items = {};
				manager.GetItems(items);
				foreach (IEntity item : items)
				{
					snapshot.m_aCargo.Insert(Capture(item, false));
				}
			}
		}

		return snapshot;
	}

	//------------------------------------------------------------------------------------------------
	//! Applies everything except cargo, which needs async inventory operations.
	static void ApplyState(IEntity entity, MRX_SpikeItemSnapshot snapshot)
	{
		BaseMagazineComponent magazine = FindMagazine(entity);
		if (magazine && snapshot.m_iAmmo >= 0)
			magazine.SetAmmoCount(snapshot.m_iAmmo);

		SCR_FuelManagerComponent fuelManager = SCR_FuelManagerComponent.Cast(entity.FindComponent(SCR_FuelManagerComponent));
		if (fuelManager)
		{
			array<SCR_FuelNode> nodes = {};
			fuelManager.GetScriptedFuelNodesList(nodes);
			foreach (MRX_SpikeFuelSnapshot fuel : snapshot.m_aFuel)
			{
				foreach (SCR_FuelNode node : nodes)
				{
					if (node.GetFuelTankID() == fuel.m_iTankId)
						node.SetFuel(fuel.m_fFuel);
				}
			}
		}

		HitZoneContainerComponent hitZoneContainer = HitZoneContainerComponent.Cast(entity.FindComponent(HitZoneContainerComponent));
		if (hitZoneContainer)
		{
			array<HitZone> zones = {};
			hitZoneContainer.GetAllHitZones(zones);
			foreach (MRX_SpikeHitZoneSnapshot hitZone : snapshot.m_aHitZones)
			{
				foreach (HitZone zone : zones)
				{
					if (zone.GetName() == hitZone.m_sName)
						zone.SetHealth(hitZone.m_fHealth);
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	static string ToJson(MRX_SpikeItemSnapshot snapshot)
	{
		JsonSaveContext save = new JsonSaveContext();
		save.WriteValue("", snapshot);
		return save.SaveToString();
	}

	//------------------------------------------------------------------------------------------------
	static MRX_SpikeItemSnapshot FromJson(string json)
	{
		JsonLoadContext load = new JsonLoadContext();
		if (!load.LoadFromString(json))
			return null;

		MRX_SpikeItemSnapshot snapshot = new MRX_SpikeItemSnapshot();
		load.ReadValue("", snapshot);
		return snapshot;
	}

	//------------------------------------------------------------------------------------------------
	static string DescribeCargo(IEntity entity)
	{
		InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(entity.FindComponent(InventoryStorageManagerComponent));
		if (!manager)
			return "cargo=<no manager>";

		array<IEntity> items = {};
		manager.GetItems(items);
		string text = string.Format("cargo(%1)=[", items.Count());
		foreach (IEntity item : items)
		{
			string name = FilePath.StripPath(item.GetPrefabData().GetPrefabName());
			BaseMagazineComponent magazine = FindMagazine(item);
			if (magazine)
				name += string.Format(":%1", magazine.GetAmmoCount());

			text += name + " ";
		}

		return text + "]";
	}
}

//------------------------------------------------------------------------------------------------
class MRX_SpikeSnapshotProbe
{
	protected string m_sLabel;
	protected ResourceName m_sPrefab;
	protected vector m_vPosition;
	protected IEntity m_Entity;
	protected IEntity m_Restored;
	protected string m_sJson;
	protected ref MRX_SpikeItemSnapshot m_Snapshot;

	//------------------------------------------------------------------------------------------------
	void MRX_SpikeSnapshotProbe(string label, ResourceName prefab, vector position)
	{
		m_sLabel = label;
		m_sPrefab = prefab;
		m_vPosition = position;
	}

	//------------------------------------------------------------------------------------------------
	protected void Log(string msg)
	{
		MRX_Spike.Log(string.Format("snapshot[%1] %2", m_sLabel, msg));
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnPrefab()
	{
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = m_vPosition;
		return GetGame().SpawnEntityPrefab(Resource.Load(m_sPrefab), GetGame().GetWorld(), params);
	}

	//------------------------------------------------------------------------------------------------
	protected string Describe(IEntity entity)
	{
		MRX_SpikeItemSnapshot state = MRX_SpikeSnapshotUtils.Capture(entity, false);
		string text = string.Format("ammo=%1 fuel=[", state.m_iAmmo);
		foreach (MRX_SpikeFuelSnapshot fuel : state.m_aFuel)
		{
			text += string.Format("%1:%2 ", fuel.m_iTankId, fuel.m_fFuel);
		}

		text += "] damaged=[";
		foreach (MRX_SpikeHitZoneSnapshot hitZone : state.m_aHitZones)
		{
			text += string.Format("%1:%2 ", hitZone.m_sName, hitZone.m_fHealth);
		}

		return text + "] " + MRX_SpikeSnapshotUtils.DescribeCargo(entity);
	}

	//------------------------------------------------------------------------------------------------
	void Start()
	{
		m_Entity = SpawnPrefab();
		Log("spawned default " + Describe(m_Entity));
		GetGame().GetCallqueue().CallLater(Mutate, 1500);
	}

	//------------------------------------------------------------------------------------------------
	protected void Mutate()
	{
		BaseMagazineComponent magazine = MRX_SpikeSnapshotUtils.FindMagazine(m_Entity);
		if (magazine)
			magazine.SetAmmoCount(7);

		SCR_FuelManagerComponent fuelManager = SCR_FuelManagerComponent.Cast(m_Entity.FindComponent(SCR_FuelManagerComponent));
		if (fuelManager)
			fuelManager.SetTotalFuelPercentage(0.3);

		HitZoneContainerComponent hitZoneContainer = HitZoneContainerComponent.Cast(m_Entity.FindComponent(HitZoneContainerComponent));
		if (hitZoneContainer)
		{
			array<HitZone> zones = {};
			hitZoneContainer.GetAllHitZones(zones);
			if (!zones.IsEmpty())
				zones[0].SetHealth(zones[0].GetMaxHealth() * 0.4);
		}

		InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(m_Entity.FindComponent(InventoryStorageManagerComponent));
		if (manager)
		{
			bool first = manager.TrySpawnPrefabToStorage(MRX_SpikeSnapshotUtils.PREFAB_MAGAZINE);
			bool second = manager.TrySpawnPrefabToStorage(MRX_SpikeSnapshotUtils.PREFAB_MAGAZINE);
			Log(string.Format("cargo spawn requested first=%1 second=%2", first, second));
		}

		GetGame().GetCallqueue().CallLater(MutateCargoAmmo, 1500);
	}

	//------------------------------------------------------------------------------------------------
	protected void MutateCargoAmmo()
	{
		InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(m_Entity.FindComponent(InventoryStorageManagerComponent));
		if (manager)
		{
			array<IEntity> items = {};
			manager.GetItems(items);
			int ammo = 5;
			foreach (IEntity item : items)
			{
				BaseMagazineComponent magazine = MRX_SpikeSnapshotUtils.FindMagazine(item);
				if (!magazine)
					continue;

				magazine.SetAmmoCount(ammo);
				ammo += 12;
			}
		}

		GetGame().GetCallqueue().CallLater(CaptureAndDelete, 500);
	}

	//------------------------------------------------------------------------------------------------
	protected void CaptureAndDelete()
	{
		Log("before " + Describe(m_Entity));
		int t0 = System.GetTickCount();
		m_sJson = MRX_SpikeSnapshotUtils.ToJson(MRX_SpikeSnapshotUtils.Capture(m_Entity, true));
		Log(string.Format("capture ms=%1", System.GetTickCount() - t0));
		MRX_Spike.LogLong(string.Format("snapshot[%1] json", m_sLabel), m_sJson);

		SCR_EntityHelper.DeleteEntityAndChildren(m_Entity);
		GetGame().GetCallqueue().CallLater(Restore, 1000);
	}

	//------------------------------------------------------------------------------------------------
	protected void Restore()
	{
		m_Snapshot = MRX_SpikeSnapshotUtils.FromJson(m_sJson);
		if (!m_Snapshot)
		{
			Log("restore: json parse failed");
			return;
		}

		m_Restored = SpawnPrefab();
		Log("restore: fresh spawn " + Describe(m_Restored));
		MRX_SpikeSnapshotUtils.ApplyState(m_Restored, m_Snapshot);

		InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(m_Restored.FindComponent(InventoryStorageManagerComponent));
		if (manager && !m_Snapshot.m_aCargo.IsEmpty())
		{
			// Prefab default cargo would duplicate saved cargo, so remove it before re-adding.
			array<IEntity> defaults = {};
			manager.GetItems(defaults);
			foreach (IEntity item : defaults)
			{
				manager.TryDeleteItem(item);
			}

			Log(string.Format("restore: removed %1 default cargo items", defaults.Count()));
			foreach (MRX_SpikeItemSnapshot cargo : m_Snapshot.m_aCargo)
			{
				manager.TrySpawnPrefabToStorage(cargo.m_sPrefab);
			}
		}

		GetGame().GetCallqueue().CallLater(RestoreCargoState, 1500);
	}

	//------------------------------------------------------------------------------------------------
	protected void RestoreCargoState()
	{
		InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(m_Restored.FindComponent(InventoryStorageManagerComponent));
		if (manager)
		{
			// Match restored cargo items to snapshot entries by prefab, in order.
			array<IEntity> items = {};
			manager.GetItems(items);
			array<bool> used = {};
			foreach (MRX_SpikeItemSnapshot cargo : m_Snapshot.m_aCargo)
			{
				used.Insert(false);
			}

			foreach (IEntity item : items)
			{
				string prefab = item.GetPrefabData().GetPrefabName();
				foreach (int i, MRX_SpikeItemSnapshot cargo : m_Snapshot.m_aCargo)
				{
					if (used[i] || cargo.m_sPrefab != prefab)
						continue;

					used[i] = true;
					MRX_SpikeSnapshotUtils.ApplyState(item, cargo);
					break;
				}
			}
		}

		GetGame().GetCallqueue().CallLater(Verify, 1000);
	}

	//------------------------------------------------------------------------------------------------
	protected void Verify()
	{
		Log("after " + Describe(m_Restored));
	}
}
#endif
