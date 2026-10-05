//! Engine side of the stash (API v0): checks, captures, removes and spawns items for MRX_StashService.
//! MRX_EntityAssetWorld is the engine implementation; tests use a fake. Items are passed as Managed (IEntity in the engine).
class MRX_AssetWorld : Managed
{
	//------------------------------------------------------------------------------------------------
	//! \return OK when the player may store the item (e.g. it is in the player's inventory), otherwise the reason.
	MRX_EStashStatus CheckDeposit(int playerId, Managed item)
	{
		return MRX_EStashStatus.NOT_IN_INVENTORY;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Snapshot of the item and its contents, or null when it cannot be captured.
	MRX_ItemSnapshot Capture(Managed item)
	{
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Removes the item and its contents from the world, synchronously.
	bool Delete(Managed item)
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the player's inventory has room for the prefab.
	bool CanDeliver(int playerId, ResourceName prefab)
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns the prefab into the player's inventory and restores the snapshot on it.
	//! \param snapshot Null keeps the prefab's defaults (e.g. its default magazine and attachments).
	//! Calls callback.OnSpawned once, with the root item or null on failure (possibly synchronously).
	void Deliver(int playerId, ResourceName prefab, MRX_ItemSnapshot snapshot, notnull MRX_AssetSpawnCallback callback)
	{
		callback.OnSpawned(null);
	}

	//------------------------------------------------------------------------------------------------
	//! \return True while the item exists and is not destroyed.
	bool IsAlive(Managed item)
	{
		return item != null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the holder (a character in the engine world) carries the item, also inside other items.
	bool IsCarriedBy(Managed item, Managed holder)
	{
		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! Result of MRX_AssetWorld.Deliver.
class MRX_AssetSpawnCallback : Managed
{
	//------------------------------------------------------------------------------------------------
	void OnSpawned(Managed item)
	{
	}
}

//------------------------------------------------------------------------------------------------
//! Extension point (API v0): consumer rules for player stash operations. Return false to refuse (REJECTED).
class MRX_StashValidator : Managed
{
	//------------------------------------------------------------------------------------------------
	bool CanDeposit(int playerId, string ownerId, notnull MRX_ItemSnapshot snapshot)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	bool CanWithdraw(int playerId, notnull MRX_AssetRecord asset)
	{
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Link between a DEPLOYED asset and its item in the world. Session memory only.
class MRX_AssetBinding : Managed
{
	string m_sAssetId;
	string m_sOwnerId;
	//! Weak: the engine owns the entity.
	Managed m_Item;
	//! True while Marx itself removes the item (deposit, rollback), so the removal is not a loss.
	bool m_bReleasing;
	//! Set when the carrier died and the item stayed on the body: its disappearance is a loss even if the owner is offline.
	bool m_bLostOnVanish;
	//! Set once the disappearance is reported, so it is reported once.
	bool m_bVanishReported;
}

//------------------------------------------------------------------------------------------------
//! Server registry of deployed assets and their items (replaces a runtime ownership component, which the engine cannot add).
class MRX_AssetBindings : Managed
{
	protected ref map<string, ref MRX_AssetBinding> m_mBindings = new map<string, ref MRX_AssetBinding>();

	//------------------------------------------------------------------------------------------------
	MRX_AssetBinding Bind(string assetId, string ownerId, Managed item)
	{
		MRX_AssetBinding binding = new MRX_AssetBinding();
		binding.m_sAssetId = assetId;
		binding.m_sOwnerId = ownerId;
		binding.m_Item = item;
		m_mBindings.Set(assetId, binding);
		return binding;
	}

	//------------------------------------------------------------------------------------------------
	void Unbind(string assetId)
	{
		m_mBindings.Remove(assetId);
	}

	//------------------------------------------------------------------------------------------------
	MRX_AssetBinding Get(string assetId)
	{
		return m_mBindings.Get(assetId);
	}

	//------------------------------------------------------------------------------------------------
	MRX_AssetBinding FindByItem(Managed item)
	{
		if (!item)
			return null;

		foreach (string assetId, MRX_AssetBinding binding : m_mBindings)
		{
			if (binding.m_Item == item)
				return binding;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	int GetAll(notnull array<MRX_AssetBinding> outBindings)
	{
		outBindings.Clear();
		foreach (string assetId, MRX_AssetBinding binding : m_mBindings)
		{
			outBindings.Insert(binding);
		}

		return outBindings.Count();
	}
}
