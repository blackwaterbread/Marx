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

		MRX_EntityAssetWorld world = MRX_EntityAssetWorld.Cast(GetStash().GetWorld());
		foreach (MRX_AssetRecord asset : record.m_aAssets)
		{
			if (asset.m_eState != MRX_EAssetState.STASHED || !world)
				continue;

			m_iRestoring++;
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
		if (m_bClosing)
		{
			DeleteContainer();
			return;
		}

		m_bReady = true;
		m_Manager.OnSessionReady(this);
	}

	//------------------------------------------------------------------------------------------------
	protected void Reconcile()
	{
		m_bReconcileScheduled = false;
		if (!m_Storage)
			return;

		array<IEntity> inside = {};
		GetRootItems(inside);

		foreach (IEntity item : inside)
		{
			string assetId = FindShownAsset(item);
			if (assetId.IsEmpty())
			{
				if (m_aStoring.Contains(item))
					continue;

				m_aStoring.Insert(item);
				GetStash().StoreWorldItem(m_sOwnerId, item, new MRX_StashSessionStoreReply(this, item));
				continue;
			}

			string json = ToJson(item);
			if (json == m_mStoredJson.Get(assetId))
				continue;

			m_mStoredJson.Set(assetId, json);
			GetStash().UpdateStashedSnapshot(m_sOwnerId, assetId, GetStash().GetWorld().Capture(item));
		}

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
			if (left && GetStash().GetWorld().IsAlive(left))
				GetStash().TakeWorldItem(m_sOwnerId, leftId, left, new MRX_StashSessionTakeReply(this, leftId, left));
			else
				GetStash().RemoveVanished(m_sOwnerId, leftId);
		}
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
