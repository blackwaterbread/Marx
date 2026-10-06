//! Server: one open stash container of a player. The container holds the player's STASHED assets as real items, so the
//! vanilla inventory can show them. Their records stay STASHED; moving items in or out becomes StoreWorldItem or
//! TakeWorldItem right away (reconciled one frame after the inventory events, so intermediate moves do not count).
class MRX_StashSession : Managed
{
	//! Weak: the manager owns its sessions.
	protected MRX_StashSessionManager m_Manager;
	protected int m_iPlayerId;
	protected string m_sOwnerId;
	//! Weak entity handles.
	protected IEntity m_Character;
	protected IEntity m_StashPoint;
	protected IEntity m_Container;
	protected MRX_StashStorageComponent m_Storage;
	protected MRX_StashContainerManagerComponent m_ContainerManager;

	//! Asset ID -> item, for STASHED assets whose item is in the container (or just left it).
	protected ref map<string, IEntity> m_mShown = new map<string, IEntity>();
	//! Asset ID -> JSON of the stored snapshot, to notice changed contents.
	protected ref map<string, string> m_mStoredJson = new map<string, string>();
	//! Asset ID -> stored placement (MRX_AssetRecord.m_sPlacement), to notice moves on the grid.
	protected ref map<string, string> m_mStoredPlacement = new map<string, string>();
	//! Asset ID -> items inside its item (e.g. a bag's contents) at the last reconcile, to notice items coming out.
	protected ref map<string, ref array<IEntity>> m_mNested = new map<string, ref array<IEntity>>();
	//! New items whose store request runs, so they are not stored twice.
	protected ref array<IEntity> m_aStoring = {};
	protected int m_iRestoring;
	protected bool m_bReady;
	protected bool m_bClosing;
	protected bool m_bReconcileScheduled;

	//------------------------------------------------------------------------------------------------
	void MRX_StashSession(MRX_StashSessionManager manager, int playerId, string ownerId, IEntity character, IEntity stashPoint)
	{
		m_Manager = manager;
		m_iPlayerId = playerId;
		m_sOwnerId = ownerId;
		m_Character = character;
		m_StashPoint = stashPoint;
	}

	//------------------------------------------------------------------------------------------------
	void ~MRX_StashSession()
	{
		GetGame().GetCallqueue().Remove(Reconcile);
	}

	//------------------------------------------------------------------------------------------------
	int GetPlayerId()
	{
		return m_iPlayerId;
	}

	//------------------------------------------------------------------------------------------------
	IEntity GetCharacter()
	{
		return m_Character;
	}

	//------------------------------------------------------------------------------------------------
	IEntity GetStashPoint()
	{
		return m_StashPoint;
	}

	//------------------------------------------------------------------------------------------------
	IEntity GetContainer()
	{
		return m_Container;
	}

	//------------------------------------------------------------------------------------------------
	MRX_StashStorageComponent GetStorage()
	{
		return m_Storage;
	}

	//------------------------------------------------------------------------------------------------
	bool IsReady()
	{
		return m_bReady;
	}

	//------------------------------------------------------------------------------------------------
	bool IsClosing()
	{
		return m_bClosing;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The item of a STASHED asset in the container, or null.
	IEntity FindShownItem(string assetId)
	{
		return m_mShown.Get(assetId);
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns the container at the stash point and restores the stashed items into it.
	//! \return False when the container cannot be created.
	bool Start(ResourceName containerPrefab)
	{
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = m_StashPoint.GetOrigin();
		m_Container = GetGame().SpawnEntityPrefab(Resource.Load(containerPrefab), GetGame().GetWorld(), params);
		if (!m_Container)
			return false;

		m_Storage = MRX_StashStorageComponent.Cast(m_Container.FindComponent(MRX_StashStorageComponent));
		m_ContainerManager = MRX_StashContainerManagerComponent.Cast(m_Container.FindComponent(MRX_StashContainerManagerComponent));
		if (!m_Storage || !m_ContainerManager)
		{
			Print(string.Format("[MRX] Stash container %1 lacks MRX_StashStorageComponent or MRX_StashContainerManagerComponent", containerPrefab), LogLevel.ERROR);
			SCR_EntityHelper.DeleteEntityAndChildren(m_Container);
			return false;
		}

		m_ContainerManager.SetUser(m_Character);
		MRX_StashCallback callback = new MRX_StashCallback();
		callback.GetOnResult().Insert(OnStashListed);
		GetStash().List(m_sOwnerId, callback);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Runs the reconcile on the next frame.
	void MarkDirty()
	{
		if (!m_bReady || m_bReconcileScheduled)
			return;

		m_bReconcileScheduled = true;
		GetGame().GetCallqueue().CallLater(Reconcile);
	}

	//------------------------------------------------------------------------------------------------
	//! Stops taking items out, commits the last changes, then deletes the container. Items still inside are STASHED.
	void Close(string reason)
	{
		if (m_bClosing)
			return;

		Print(string.Format("[MRX] Stash of player %1 closing: %2", m_iPlayerId, reason), LogLevel.NORMAL);
		m_bClosing = true;
		if (m_ContainerManager)
			m_ContainerManager.SetUser(null);

		m_Manager.OnSessionClosing(this);
		if (m_bReady)
			Reconcile();

		// The list request runs after every request above (same owner queue).
		MRX_StashCallback barrier = new MRX_StashCallback();
		barrier.GetOnResult().Insert(OnCloseBarrier);
		GetStash().List(m_sOwnerId, barrier);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnStashListed(MRX_EStashStatus status, MRX_StashRecord record)
	{
		if (m_bClosing)
			return;

		if (status != MRX_EStashStatus.OK || !record)
		{
			Print(string.Format("[MRX] Could not load the stash of %1: %2", m_sOwnerId, typename.EnumToString(MRX_EStashStatus, status)), LogLevel.ERROR);
			m_Manager.ReportResult(this, status);
			Close("stash not loaded");
			return;
		}

		// The owner's extra pages (e.g. bought) on top of the container's own.
		int pages = MRX_StashPages.GetPages(record);
		if (pages > 0)
			m_Storage.SetMaxPages(pages);

		// Stashed items always show, also when they take more pages than the container allows now.
		m_Storage.SetRestoring(true);
		MRX_EntityAssetWorld world = MRX_EntityAssetWorld.Cast(GetStash().GetWorld());
		foreach (MRX_AssetRecord asset : record.m_aAssets)
		{
			if (asset.m_eState != MRX_EAssetState.STASHED || !world)
				continue;

			m_iRestoring++;
			m_mStoredPlacement.Set(asset.m_sId, asset.m_sPlacement);
			world.SpawnIntoStorage(m_ContainerManager, m_Storage, asset.m_sPrefab, asset.m_Snapshot, new MRX_StashSessionSpawn(this, asset.m_sId));
		}

		if (m_iRestoring == 0)
			OnRestored();
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by MRX_StashSessionSpawn.
	void OnAssetSpawned(string assetId, IEntity item)
	{
		m_iRestoring--;
		if (item)
		{
			m_mShown.Set(assetId, item);
			m_mStoredJson.Set(assetId, ToJson(item));
		}
		else
		{
			Print(string.Format("[MRX] Could not show stashed asset %1 in the container", assetId), LogLevel.ERROR);
		}

		if (m_iRestoring == 0)
			OnRestored();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnRestored()
	{
		if (m_Storage)
		{
			PlaceRestoredItems();
			m_Storage.SetRestoring(false);
		}

		if (m_bClosing)
		{
			DeleteContainer();
			return;
		}

		m_bReady = true;
		m_Manager.OnSessionReady(this);
		// Stores placements that changed (taken cells, old records without one) and sends the grid to the client.
		MarkDirty();
	}

	//------------------------------------------------------------------------------------------------
	//! Restored items go to their stored placements; items without one, or whose cells are taken, to the first free
	//! cells (behind the page limit if needed, so nothing stashed is hidden).
	protected void PlaceRestoredItems()
	{
		MRX_StashGrid grid = m_Storage.GetGrid();
		grid.Clear();
		array<string> unplaced = {};
		foreach (string assetId, IEntity item : m_mShown)
		{
			int width, height;
			MRX_StashStorageComponent.GetItemCellSize(item, width, height);
			MRX_StashPlacement stored = MRX_StashPlacement.Parse(m_mStoredPlacement.Get(assetId));
			if (!stored || !grid.Place(MRX_StashStorageComponent.GetItemKey(item), stored, width, height, true))
				unplaced.Insert(assetId);
		}

		foreach (string unplacedId : unplaced)
		{
			IEntity unplacedItem = m_mShown.Get(unplacedId);
			string key = MRX_StashStorageComponent.GetItemKey(unplacedItem);
			int itemWidth, itemHeight;
			MRX_StashStorageComponent.GetItemCellSize(unplacedItem, itemWidth, itemHeight);
			MRX_StashPlacement free = grid.FindFree(itemWidth, itemHeight, 0, key, true);
			if (free)
				grid.Place(key, free, itemWidth, itemHeight, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The user's client asks to put an item at a placement: an item in the container moves there if the cells are
	//! free, an item about to be put in goes there when it arrives (the request and the move may come in any order).
	void RequestPlacement(RplId itemId, notnull MRX_StashPlacement placement)
	{
		if (!m_bReady || m_bClosing || !m_Storage)
			return;

		string key = itemId.AsString();
		IEntity item = SCR_EntityHelper.RplIdToEntity(itemId);
		if (!item || !m_Storage.Contains(item))
		{
			m_Storage.SetPendingPlacement(key, placement);
			return;
		}

		int width, height;
		MRX_StashStorageComponent.GetItemCellSize(item, width, height);
		m_Storage.GetGrid().Place(key, placement, width, height);
		// Also when refused: sends the grid again, which puts the client's item back.
		MarkDirty();
	}

	//------------------------------------------------------------------------------------------------
	protected void Reconcile()
	{
		m_bReconcileScheduled = false;
		if (!m_Storage)
			return;

		array<IEntity> inside = {};
		GetRootItems(inside);
		map<string, ref array<IEntity>> nested = new map<string, ref array<IEntity>>();
		CollectNested(inside, nested);

		// Items going into or coming out of stashed items (bags) and the snapshots that change with them: one request.
		MRX_StashWorldChanges changes = new MRX_StashWorldChanges();
		MRX_StashSessionChangesReply reply = new MRX_StashSessionChangesReply(this, changes);
		CollectMerges(nested, changes, reply);
		CollectSplits(inside, nested, changes, reply);

		foreach (IEntity item : inside)
		{
			string assetId = FindShownAsset(item);
			if (assetId.IsEmpty())
			{
				if (m_aStoring.Contains(item))
					continue;

				m_aStoring.Insert(item);
				GetStash().StoreWorldItem(m_sOwnerId, item, new MRX_StashSessionStoreReply(this, item), GetPlacementText(item));
				continue;
			}

			string placement = GetPlacementText(item);
			if (placement != m_mStoredPlacement.Get(assetId))
			{
				m_mStoredPlacement.Set(assetId, placement);
				GetStash().SetPlacement(m_sOwnerId, assetId, placement);
			}

			string json = ToJson(item);
			if (json == m_mStoredJson.Get(assetId))
				continue;

			m_mStoredJson.Set(assetId, json);
			changes.UpdateSnapshot(assetId, GetStash().GetWorld().Capture(item));
		}

		if (!changes.IsEmpty())
			GetStash().ApplyWorldChanges(m_sOwnerId, changes, reply);

		array<string> leftIds = {};
		foreach (string shownId, IEntity shownItem : m_mShown)
		{
			if (!shownItem || !inside.Contains(shownItem))
				leftIds.Insert(shownId);
		}

		foreach (string leftId : leftIds)
		{
			IEntity left = m_mShown.Get(leftId);
			m_mShown.Remove(leftId);
			m_mStoredJson.Remove(leftId);
			m_mStoredPlacement.Remove(leftId);
			if (left && GetStash().GetWorld().IsAlive(left))
				GetStash().TakeWorldItem(m_sOwnerId, leftId, left, new MRX_StashSessionTakeReply(this, leftId, left));
			else
				GetStash().RemoveVanished(m_sOwnerId, leftId);
		}

		m_mNested = nested;
		if (m_bReady && !m_bClosing)
			m_Manager.SendPlacements(this);
	}

	//------------------------------------------------------------------------------------------------
	//! Items inside the container's stashed items (at any depth), by the stashed item's asset ID.
	protected void CollectNested(notnull array<IEntity> rootItems, notnull map<string, ref array<IEntity>> outNested)
	{
		array<IEntity> allItems = {};
		m_ContainerManager.GetItems(allItems);
		foreach (IEntity item : allItems)
		{
			if (!item || rootItems.Contains(item))
				continue;

			IEntity top = item;
			while (top && top.GetParent() != m_Container)
			{
				top = top.GetParent();
			}

			// Inside an item not stored yet: counted once it is.
			string topId = FindShownAsset(top);
			if (!top || topId.IsEmpty())
				continue;

			if (!outNested.Contains(topId))
				outNested.Set(topId, new array<IEntity>());

			outNested.Get(topId).Insert(item);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Items of the owner's assets that went into a stashed item belong to it now (its snapshot holds them): their own
	//! assets are removed with the same request, instead of counting as taken out. Covers stashed items moved into a bag
	//! and deployed items put into one.
	protected void CollectMerges(notnull map<string, ref array<IEntity>> nested, notnull MRX_StashWorldChanges changes, notnull MRX_StashSessionChangesReply reply)
	{
		foreach (string targetId, array<IEntity> items : nested)
		{
			foreach (IEntity item : items)
			{
				string mergedId = FindShownAsset(item);
				if (mergedId.IsEmpty() && GetStash().GetOwnerOf(item) == m_sOwnerId)
					mergedId = GetStash().GetAssetId(item);

				if (mergedId.IsEmpty() || mergedId == targetId)
					continue;

				changes.Remove(mergedId);
				if (!m_mShown.Contains(mergedId))
					continue;

				reply.m_mMergedShown.Set(mergedId, item);
				m_mShown.Remove(mergedId);
				m_mStoredJson.Remove(mergedId);
				m_mStoredPlacement.Remove(mergedId);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Items that came out of a stashed item become assets of their own with the same request: STASHED when they lie in
	//! the container now, DEPLOYED (bound to them) when they left the stash. Items moved into another stashed item only
	//! change both snapshots, which are in the same request too.
	protected void CollectSplits(notnull array<IEntity> rootItems, notnull map<string, ref array<IEntity>> nested, notnull MRX_StashWorldChanges changes, notnull MRX_StashSessionChangesReply reply)
	{
		foreach (string sourceId, array<IEntity> previous : m_mNested)
		{
			// Sources that left, were merged or vanished are handled with their own asset.
			IEntity source = m_mShown.Get(sourceId);
			if (!source)
				continue;

			array<IEntity> current = nested.Get(sourceId);
			foreach (IEntity item : previous)
			{
				if (!item || (current && current.Contains(item)) || IsInside(item, source))
					continue;

				// Used up, or an asset already.
				if (!GetStash().GetWorld().IsAlive(item) || !GetStash().GetAssetId(item).IsEmpty())
					continue;

				if (rootItems.Contains(item))
				{
					if (!FindShownAsset(item).IsEmpty() || m_aStoring.Contains(item))
						continue;

					string placement = GetPlacementText(item);
					string assetId = changes.Add(item, GetStash().GetWorld().Capture(item), false, placement);
					m_aStoring.Insert(item);
					reply.m_mStashedAdds.Set(assetId, item);
					reply.m_mPlacements.Set(assetId, placement);
				}
				else if (item.GetRootParent() != m_Container)
				{
					changes.Add(item, GetStash().GetWorld().Capture(item), true);
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsInside(notnull IEntity item, notnull IEntity ancestor)
	{
		IEntity parent = item.GetParent();
		while (parent)
		{
			if (parent == ancestor)
				return true;

			parent = parent.GetParent();
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by MRX_StashSessionChangesReply.
	void OnWorldChanged(notnull MRX_StashSessionChangesReply reply, notnull MRX_StashResult result)
	{
		if (result.m_eStatus == MRX_EStashStatus.OK)
		{
			foreach (string assetId, IEntity item : reply.m_mStashedAdds)
			{
				m_aStoring.RemoveItem(item);
				if (!item)
					continue;

				m_mShown.Set(assetId, item);
				m_mStoredJson.Set(assetId, ToJson(item));
				m_mStoredPlacement.Set(assetId, reply.m_mPlacements.Get(assetId));
			}

			MarkDirty();
			return;
		}

		// Nothing was applied. The next reconcile finds merges and snapshots again; items that came out of a bag are then
		// stored like any new item (or given back when that is refused too, e.g. a full stash).
		m_Manager.ReportResult(this, result.m_eStatus);
		foreach (string updatedId : reply.GetChanges().m_aUpdatedIds)
		{
			m_mStoredJson.Remove(updatedId);
		}

		foreach (string mergedId, IEntity mergedItem : reply.m_mMergedShown)
		{
			if (mergedItem)
				m_mShown.Set(mergedId, mergedItem);
		}

		foreach (string addedId, IEntity addedItem : reply.m_mStashedAdds)
		{
			m_aStoring.RemoveItem(addedItem);
		}

		if (!reply.GetChanges().m_aAdded.IsEmpty())
			MarkDirty();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The item's placement on the grid as stored in its record, or empty.
	protected string GetPlacementText(IEntity item)
	{
		MRX_StashPlacement placement = m_Storage.GetGrid().Get(MRX_StashStorageComponent.GetItemKey(item));
		if (!placement)
			return string.Empty;

		return placement.Format();
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by MRX_StashSessionStoreReply.
	void OnStored(IEntity item, notnull MRX_StashResult result)
	{
		m_aStoring.RemoveItem(item);
		MRX_AssetRecord asset = result.GetAsset();
		if (result.m_eStatus == MRX_EStashStatus.OK && asset && item)
		{
			// Counted as shown even if it left again meanwhile; the next reconcile then takes it out.
			m_mShown.Set(asset.m_sId, item);
			m_mStoredJson.Set(asset.m_sId, ToJson(item));
			// The next reconcile stores the placement again if the item moved on the grid meanwhile.
			m_mStoredPlacement.Set(asset.m_sId, asset.m_sPlacement);
			MarkDirty();
			return;
		}

		if (result.m_eStatus == MRX_EStashStatus.OK)
			return;

		// Not stored: the item must not stay in the container, which is deleted with its contents.
		m_Manager.ReportResult(this, result.m_eStatus);
		ReturnToCharacter(item);
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by MRX_StashSessionTakeReply.
	void OnTaken(string assetId, IEntity item, notnull MRX_StashResult result)
	{
		if (result.m_eStatus == MRX_EStashStatus.OK || !item)
			return;

		// Still STASHED: the item goes back into the container, or away if that is not possible (no duplicate).
		m_Manager.ReportResult(this, result.m_eStatus);
		InventoryStorageManagerComponent manager = GetCharacterManager();
		if (!m_bClosing && m_Storage && manager && manager.TryMoveItemToStorage(item, m_Storage))
		{
			m_mShown.Set(assetId, item);
			m_mStoredJson.Set(assetId, ToJson(item));
			return;
		}

		Print(string.Format("[MRX] Asset %1 stays stashed; its item taken out of the container is removed", assetId), LogLevel.WARNING);
		SCR_EntityHelper.DeleteEntityAndChildren(item);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCloseBarrier(MRX_EStashStatus status, MRX_StashRecord record)
	{
		// Items put in after the last reconcile have no record: give them back before the container goes.
		array<IEntity> inside = {};
		GetRootItems(inside);
		foreach (IEntity item : inside)
		{
			if (FindShownAsset(item).IsEmpty())
				ReturnToCharacter(item);
		}

		if (m_iRestoring == 0)
			DeleteContainer();
	}

	//------------------------------------------------------------------------------------------------
	protected void DeleteContainer()
	{
		Print(string.Format("[MRX] Stash of player %1 closed", m_iPlayerId), LogLevel.NORMAL);
		if (m_Container)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Container);

		m_Manager.OnSessionClosed(this);
	}

	//------------------------------------------------------------------------------------------------
	//! Moves the item into the character's inventory, or drops it next to the stash point.
	protected void ReturnToCharacter(IEntity item)
	{
		if (!item)
			return;

		InventoryStorageManagerComponent manager = GetCharacterManager();
		BaseInventoryStorageComponent target;
		if (manager)
			target = manager.FindStorageForItem(item);

		if (target && manager.TryMoveItemToStorage(item, target))
			return;

		if (m_ContainerManager && m_Storage)
			m_ContainerManager.TryRemoveItemFromStorage(item, m_Storage);
	}

	//------------------------------------------------------------------------------------------------
	protected void GetRootItems(notnull array<IEntity> outItems)
	{
		outItems.Clear();
		if (!m_Storage)
			return;

		array<InventoryItemComponent> ownedItems = {};
		m_Storage.GetOwnedItems(ownedItems, false);
		foreach (InventoryItemComponent itemComponent : ownedItems)
		{
			if (itemComponent.GetOwner())
				outItems.Insert(itemComponent.GetOwner());
		}
	}

	//------------------------------------------------------------------------------------------------
	protected string FindShownAsset(IEntity item)
	{
		foreach (string assetId, IEntity shown : m_mShown)
		{
			if (shown == item)
				return assetId;
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	protected InventoryStorageManagerComponent GetCharacterManager()
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(m_Character);
		if (!character || !character.GetCharacterController())
			return null;

		return character.GetCharacterController().GetInventoryStorageManager();
	}

	//------------------------------------------------------------------------------------------------
	protected static MRX_StashService GetStash()
	{
		return MRX_Marx.GetStash();
	}

	//------------------------------------------------------------------------------------------------
	protected static string ToJson(notnull IEntity item)
	{
		JsonSaveContext context = new JsonSaveContext();
		context.WriteValue("", MRX_EntitySnapshots.Capture(item));
		return context.SaveToString();
	}
}

//------------------------------------------------------------------------------------------------
class MRX_StashSessionSpawn : MRX_AssetSpawnCallback
{
	protected ref MRX_StashSession m_Session;
	protected string m_sAssetId;

	//------------------------------------------------------------------------------------------------
	void MRX_StashSessionSpawn(MRX_StashSession session, string assetId)
	{
		m_Session = session;
		m_sAssetId = assetId;
	}

	//------------------------------------------------------------------------------------------------
	override void OnSpawned(Managed item)
	{
		m_Session.OnAssetSpawned(m_sAssetId, IEntity.Cast(item));
	}
}

//------------------------------------------------------------------------------------------------
class MRX_StashSessionStoreReply : MRX_StashResultCallback
{
	protected ref MRX_StashSession m_Session;
	//! Weak: an engine entity.
	protected IEntity m_Item;

	//------------------------------------------------------------------------------------------------
	void MRX_StashSessionStoreReply(MRX_StashSession session, IEntity item)
	{
		m_Session = session;
		m_Item = item;
	}

	//------------------------------------------------------------------------------------------------
	override void OnResult(MRX_StashResult result)
	{
		super.OnResult(result);
		m_Session.OnStored(m_Item, result);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_StashSessionTakeReply : MRX_StashResultCallback
{
	protected ref MRX_StashSession m_Session;
	protected string m_sAssetId;
	//! Weak: an engine entity.
	protected IEntity m_Item;

	//------------------------------------------------------------------------------------------------
	void MRX_StashSessionTakeReply(MRX_StashSession session, string assetId, IEntity item)
	{
		m_Session = session;
		m_sAssetId = assetId;
		m_Item = item;
	}

	//------------------------------------------------------------------------------------------------
	override void OnResult(MRX_StashResult result)
	{
		super.OnResult(result);
		m_Session.OnTaken(m_sAssetId, m_Item, result);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_StashSessionChangesReply : MRX_StashResultCallback
{
	protected ref MRX_StashSession m_Session;
	protected ref MRX_StashWorldChanges m_Changes;
	//! New STASHED assets' items, by asset ID. Weak entities.
	ref map<string, IEntity> m_mStashedAdds = new map<string, IEntity>();
	//! New STASHED assets' placements, by asset ID.
	ref map<string, string> m_mPlacements = new map<string, string>();
	//! Merged assets that were shown, with their items (weak), to show them again if the request fails.
	ref map<string, IEntity> m_mMergedShown = new map<string, IEntity>();

	//------------------------------------------------------------------------------------------------
	void MRX_StashSessionChangesReply(MRX_StashSession session, MRX_StashWorldChanges changes)
	{
		m_Session = session;
		m_Changes = changes;
	}

	//------------------------------------------------------------------------------------------------
	MRX_StashWorldChanges GetChanges()
	{
		return m_Changes;
	}

	//------------------------------------------------------------------------------------------------
	override void OnResult(MRX_StashResult result)
	{
		super.OnResult(result);
		m_Session.OnWorldChanged(this, result);
	}
}
