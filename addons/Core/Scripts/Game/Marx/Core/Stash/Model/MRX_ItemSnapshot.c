//! Health of one damaged hit zone.
class MRX_HitZoneSnapshot : Managed
{
	string m_sName;
	float m_fHealth;

	//------------------------------------------------------------------------------------------------
	static MRX_HitZoneSnapshot Create(string name, float health)
	{
		MRX_HitZoneSnapshot snapshot = new MRX_HitZoneSnapshot();
		snapshot.m_sName = name;
		snapshot.m_fHealth = health;
		return snapshot;
	}
}

//! Fuel of one tank.
class MRX_FuelSnapshot : Managed
{
	int m_iTankId;
	float m_fFuel;

	//------------------------------------------------------------------------------------------------
	static MRX_FuelSnapshot Create(int tankId, float fuel)
	{
		MRX_FuelSnapshot snapshot = new MRX_FuelSnapshot();
		snapshot.m_iTankId = tankId;
		snapshot.m_fFuel = fuel;
		return snapshot;
	}
}

//! Saved state of an item and everything stored in it (API v0). Plain data, independent of the storage backend;
//! capture and restore live in the entity bridge. Only state that differs from a fresh prefab needs to be set.
class MRX_ItemSnapshot : Managed
{
	static const int FORMAT = 1;

	//! Snapshot format version, set on the root item.
	int m_iFormat;
	ResourceName m_sPrefab;
	//! Storage of the parent that holds this item (component class and source GUID). Empty for the root item.
	string m_sStorage;
	//! Slot of that storage, -1 for any free slot.
	int m_iSlot = -1;
	//! Rounds in the item's own magazine, -1 when it has none.
	int m_iAmmo = -1;
	//! Damaged hit zones only.
	ref array<ref MRX_HitZoneSnapshot> m_aHitZones = {};
	ref array<ref MRX_FuelSnapshot> m_aFuel = {};
	//! Items stored in this item (attachments, magazines, cargo), recursively.
	ref array<ref MRX_ItemSnapshot> m_aChildren = {};

	//------------------------------------------------------------------------------------------------
	static MRX_ItemSnapshot Create(ResourceName prefab)
	{
		MRX_ItemSnapshot snapshot = new MRX_ItemSnapshot();
		snapshot.m_iFormat = FORMAT;
		snapshot.m_sPrefab = prefab;
		return snapshot;
	}

	//------------------------------------------------------------------------------------------------
	//! \return This item plus all stored items.
	int CountItems()
	{
		int count = 1;
		foreach (MRX_ItemSnapshot child : m_aChildren)
		{
			count += child.CountItems();
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	MRX_ItemSnapshot Copy()
	{
		MRX_ItemSnapshot snapshot = new MRX_ItemSnapshot();
		snapshot.m_iFormat = m_iFormat;
		snapshot.m_sPrefab = m_sPrefab;
		snapshot.m_sStorage = m_sStorage;
		snapshot.m_iSlot = m_iSlot;
		snapshot.m_iAmmo = m_iAmmo;

		foreach (MRX_HitZoneSnapshot hitZone : m_aHitZones)
		{
			snapshot.m_aHitZones.Insert(MRX_HitZoneSnapshot.Create(hitZone.m_sName, hitZone.m_fHealth));
		}

		foreach (MRX_FuelSnapshot fuel : m_aFuel)
		{
			snapshot.m_aFuel.Insert(MRX_FuelSnapshot.Create(fuel.m_iTankId, fuel.m_fFuel));
		}

		foreach (MRX_ItemSnapshot child : m_aChildren)
		{
			snapshot.m_aChildren.Insert(child.Copy());
		}

		return snapshot;
	}
}
