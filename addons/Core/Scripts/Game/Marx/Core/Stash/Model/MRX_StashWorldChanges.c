//! New asset for an item that came out of a stashed asset's item shown in the world (see MRX_StashWorldChanges).
class MRX_StashWorldAdd : Managed
{
	string m_sAssetId;
	//! Weak: an engine entity.
	Managed m_Item;
	ref MRX_ItemSnapshot m_Snapshot;
	//! DEPLOYED and bound to the item (it left the stash); otherwise STASHED.
	bool m_bDeployed;
	//! STASHED only.
	string m_sPlacement;
}

//! Changes of stashed assets shown in the world that belong together, because items moved between their items (e.g. into
//! or out of a bag in an open stash container). Applied as one request (MRX_StashService.ApplyWorldChanges), so an item
//! is never kept twice or lost in between.
class MRX_StashWorldChanges : Managed
{
	ref array<string> m_aUpdatedIds = {};
	ref array<ref MRX_ItemSnapshot> m_aUpdatedSnapshots = {};
	ref array<string> m_aRemovedIds = {};
	ref array<ref MRX_StashWorldAdd> m_aAdded = {};

	//------------------------------------------------------------------------------------------------
	//! A STASHED asset's item has other contents now.
	void UpdateSnapshot(string assetId, notnull MRX_ItemSnapshot snapshot)
	{
		int index = m_aUpdatedIds.Find(assetId);
		if (index >= 0)
		{
			m_aUpdatedSnapshots[index] = snapshot;
			return;
		}

		m_aUpdatedIds.Insert(assetId);
		m_aUpdatedSnapshots.Insert(snapshot);
	}

	//------------------------------------------------------------------------------------------------
	//! The asset's item went into another stashed asset's item, whose snapshot holds it now. The asset may be STASHED or
	//! DEPLOYED; it is removed in the state the stash has it in.
	void Remove(string assetId)
	{
		if (!m_aRemovedIds.Contains(assetId))
			m_aRemovedIds.Insert(assetId);
	}

	//------------------------------------------------------------------------------------------------
	//! An item came out of a stashed asset's item and becomes an asset of its own.
	//! \param deployed The item left the stash: DEPLOYED and bound to it. Otherwise STASHED at the placement.
	//! \return The new asset's ID.
	string Add(Managed item, notnull MRX_ItemSnapshot snapshot, bool deployed, string placement = string.Empty)
	{
		MRX_StashWorldAdd added = new MRX_StashWorldAdd();
		added.m_sAssetId = MRX_StashService.CreateId();
		added.m_Item = item;
		added.m_Snapshot = snapshot;
		added.m_bDeployed = deployed;
		if (!deployed)
			added.m_sPlacement = placement;

		m_aAdded.Insert(added);
		return added.m_sAssetId;
	}

	//------------------------------------------------------------------------------------------------
	bool IsEmpty()
	{
		return m_aUpdatedIds.IsEmpty() && m_aRemovedIds.IsEmpty() && m_aAdded.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The same changes without new assets (e.g. to keep the snapshots right when the stash is full).
	MRX_StashWorldChanges WithoutAdds()
	{
		MRX_StashWorldChanges changes = new MRX_StashWorldChanges();
		changes.m_aUpdatedIds.Copy(m_aUpdatedIds);
		foreach (MRX_ItemSnapshot snapshot : m_aUpdatedSnapshots)
		{
			changes.m_aUpdatedSnapshots.Insert(snapshot);
		}

		changes.m_aRemovedIds.Copy(m_aRemovedIds);
		return changes;
	}
}
