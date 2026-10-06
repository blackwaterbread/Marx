//! Items handed out for free (API v0), e.g. the kit a player respawns with. Marx gives them no value: shops buy them
//! back for nothing and stash loadouts do not count them as the player's own gear. The mark follows the item into
//! snapshots (MRX_ItemSnapshot.m_bIssued), so a stashed issued item comes back issued. Server.
class MRX_IssuedItems
{
	//! Marks between two sweeps of entities that no longer exist.
	protected static const int PRUNE_INTERVAL = 512;

	protected static ref map<EntityID, bool> s_mIssued = new map<EntityID, bool>();
	protected static int s_iMarksSincePrune;

	//------------------------------------------------------------------------------------------------
	//! \param withContents Also marks everything the item holds (attachments, magazines, stored items).
	static void Mark(IEntity item, bool withContents = true)
	{
		Set(item, true, withContents);
	}

	//------------------------------------------------------------------------------------------------
	//! The item counts as the player's own gear again (e.g. paid for).
	static void Unmark(IEntity item, bool withContents = true)
	{
		Set(item, false, withContents);
	}

	//------------------------------------------------------------------------------------------------
	static bool IsIssued(IEntity item)
	{
		return item && s_mIssued.Contains(item.GetID());
	}

	//------------------------------------------------------------------------------------------------
	//! Marks every item the character carries (worn, held and stored).
	static void MarkCarried(IEntity character)
	{
		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if (!chimera || !chimera.GetCharacterController())
			return;

		InventoryStorageManagerComponent manager = chimera.GetCharacterController().GetInventoryStorageManager();
		if (!manager)
			return;

		array<IEntity> items = {};
		manager.GetItems(items);
		foreach (IEntity item : items)
		{
			Mark(item);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Forgets all marks; the Marx system calls it when a game starts.
	static void Clear()
	{
		s_mIssued.Clear();
		s_iMarksSincePrune = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Set(IEntity item, bool issued, bool withContents)
	{
		if (!item)
			return;

		if (issued)
		{
			s_mIssued.Set(item.GetID(), true);
			s_iMarksSincePrune++;
			if (s_iMarksSincePrune >= PRUNE_INTERVAL)
				Prune();
		}
		else
		{
			s_mIssued.Remove(item.GetID());
		}

		if (!withContents)
			return;

		set<BaseInventoryStorageComponent> storages = new set<BaseInventoryStorageComponent>();
		MRX_EntitySnapshots.FindStorages(item, storages);
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			array<InventoryItemComponent> contents = {};
			storage.GetOwnedItems(contents, false);
			foreach (InventoryItemComponent content : contents)
			{
				if (content.GetOwner() && content.GetOwner() != item)
					Set(content.GetOwner(), issued, true);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the marks of deleted entities.
	protected static void Prune()
	{
		s_iMarksSincePrune = 0;
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		array<EntityID> gone = {};
		foreach (EntityID id, bool issued : s_mIssued)
		{
			if (!world.FindEntityByID(id))
				gone.Insert(id);
		}

		foreach (EntityID goneId : gone)
		{
			s_mIssued.Remove(goneId);
		}
	}
}
