//! Server-side stash of owned assets (API v0). Every asset change goes through this service.
//! - Operations of the same owner run one at a time, in call order. Callbacks run on a later frame.
//! - Calls made before the storage backend is ready wait for it.
//! - Deposit and Withdraw move real items; Grant, Remove and MarkConsumed are for consumer mods (context required).
class MRX_StashService : Managed
{
	static const string LEDGER_SOURCE = "marx_stash";
	protected static const int READY_POLL_MS = 250;

	protected ref MRX_EconomyService m_Economy;
	protected ref MRX_IdentityService m_Identity;
	protected ref MRX_AssetWorld m_World;
	protected ref MRX_CallQueue m_CallQueue = new MRX_CallQueue();
	protected ref MRX_AssetBindings m_Bindings = new MRX_AssetBindings();
	protected ref array<ref MRX_StashValidator> m_aValidators = {};
	//! Owner ID -> queued ops, the running one first.
	protected ref map<string, ref array<ref MRX_StashOp>> m_mQueues = new map<string, ref array<ref MRX_StashOp>>();
	//! Finished ops, released on a later frame (never inside their own call stack).
	protected ref array<ref MRX_StashOp> m_aDoneOps = {};
	protected string m_sSessionId;
	protected ref MRX_LossPolicy m_LossPolicy = MRX_LossPolicy.Create();

	protected ref ScriptInvokerBase<MRX_AssetStateChangedDelegate> m_OnAssetStateChanged;

	//------------------------------------------------------------------------------------------------
	//! Uses the economy service's storage backend and rules.
	void MRX_StashService(notnull MRX_EconomyService economy, notnull MRX_IdentityService identity, notnull MRX_AssetWorld world)
	{
		m_Economy = economy;
		m_Identity = identity;
		m_World = world;
		m_sSessionId = CreateId();
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves a detached copy of the owner's stash.
	void List(string ownerId, notnull MRX_StashCallback callback)
	{
		Enqueue(ownerId, new MRX_StashListOp(this, ownerId, callback));
	}

	//------------------------------------------------------------------------------------------------
	//! Adds a new STASHED asset (e.g. a reward). \param snapshot Null for a fresh prefab.
	void Grant(string ownerId, ResourceName prefab, MRX_TxContext context, MRX_StashResultCallback callback = null, MRX_ItemSnapshot snapshot = null)
	{
		Enqueue(ownerId, new MRX_StashGrantOp(this, ownerId, prefab, context, snapshot, callback));
	}

	//------------------------------------------------------------------------------------------------
	//! Stores an item of the player's inventory: a new asset, or the deployed asset bound to the item.
	//! \param item IEntity for the engine world.
	void Deposit(int playerId, Managed item, MRX_StashResultCallback callback = null)
	{
		string ownerId = GetOwnerId(playerId);
		Enqueue(ownerId, new MRX_StashDepositOp(this, ownerId, playerId, item, callback));
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns a STASHED asset of the player into the player's inventory.
	void Withdraw(int playerId, string assetId, MRX_StashResultCallback callback = null)
	{
		string ownerId = GetOwnerId(playerId);
		Enqueue(ownerId, new MRX_StashWithdrawOp(this, ownerId, playerId, assetId, callback));
	}

	//------------------------------------------------------------------------------------------------
	//! Deletes a STASHED asset (e.g. sold or used from the stash by a consumer mod).
	void Remove(string ownerId, string assetId, MRX_TxContext context, MRX_StashResultCallback callback = null)
	{
		MRX_StashChangeOp op = new MRX_StashChangeOp(this, ownerId, MRX_AssetChange.Remove(assetId, MRX_EAssetState.STASHED), context, callback);
		Enqueue(ownerId, op);
	}

	//------------------------------------------------------------------------------------------------
	//! Marks the deployed asset bound to the item as used up. The item itself is left to the caller.
	void MarkConsumed(Managed item, MRX_TxContext context, MRX_StashResultCallback callback = null)
	{
		string ownerId;
		string assetId;
		MRX_AssetBinding binding = m_Bindings.FindByItem(item);
		if (binding)
		{
			ownerId = binding.m_sOwnerId;
			assetId = binding.m_sAssetId;
		}

		MRX_StashChangeOp op = new MRX_StashChangeOp(this, ownerId, MRX_AssetChange.Update(assetId, MRX_EAssetState.DEPLOYED, MRX_EAssetState.CONSUMED), context, callback);
		if (!binding)
		{
			Reject(op, MRX_EStashStatus.UNKNOWN_ASSET);
			return;
		}

		Enqueue(ownerId, op);
	}

	// Stash front-ends that show stashed items in the world (e.g. an open stash container) use the following four calls.
	// The item stays where it is; Marx only changes the record and the binding.

	//------------------------------------------------------------------------------------------------
	//! Stores an item of the world: a deployed asset of the owner bound to it goes back to STASHED, an unbound item
	//! becomes a new STASHED asset. The result carries the asset.
	//! \param placement Front-end placement of the stored asset (MRX_AssetRecord.m_sPlacement).
	void StoreWorldItem(string ownerId, Managed item, MRX_StashResultCallback callback = null, string placement = string.Empty)
	{
		Enqueue(ownerId, new MRX_StashStoreWorldOp(this, ownerId, item, callback, placement));
	}

	//------------------------------------------------------------------------------------------------
	//! Sets the front-end placement of a STASHED asset (MRX_AssetRecord.m_sPlacement).
	void SetPlacement(string ownerId, string assetId, string placement, MRX_StashResultCallback callback = null)
	{
		MRX_AssetChange change = MRX_AssetChange.Update(assetId, MRX_EAssetState.STASHED, MRX_EAssetState.STASHED).WithPlacement(placement);
		Enqueue(ownerId, new MRX_StashChangeOp(this, ownerId, change, CreateServiceContext("place"), callback));
	}

	//------------------------------------------------------------------------------------------------
	//! A STASHED asset shown in the world was taken: it becomes DEPLOYED and bound to the item.
	void TakeWorldItem(string ownerId, string assetId, Managed item, MRX_StashResultCallback callback = null)
	{
		Enqueue(ownerId, new MRX_StashTakeWorldOp(this, ownerId, assetId, item, callback));
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the snapshot of a STASHED asset (e.g. its contents changed while shown in the world).
	void UpdateStashedSnapshot(string ownerId, string assetId, notnull MRX_ItemSnapshot snapshot, MRX_StashResultCallback callback = null)
	{
		MRX_AssetChange change = MRX_AssetChange.Update(assetId, MRX_EAssetState.STASHED, MRX_EAssetState.STASHED, snapshot);
		Enqueue(ownerId, new MRX_StashChangeOp(this, ownerId, change, CreateServiceContext("update"), callback));
	}

	//------------------------------------------------------------------------------------------------
	//! Items of other assets went into the item of a STASHED asset shown in the world (e.g. into a bag in an open stash
	//! container): in one request the asset takes the snapshot that holds them and their assets are removed, so nothing
	//! is kept twice. Merged assets may be STASHED (shown in the world) or DEPLOYED (bound to the item that went in).
	void MergeIntoStashed(string ownerId, string assetId, notnull MRX_ItemSnapshot snapshot, notnull array<string> mergedAssetIds, MRX_StashResultCallback callback = null)
	{
		Enqueue(ownerId, new MRX_StashMergeOp(this, ownerId, assetId, snapshot, mergedAssetIds, CreateServiceContext("merge"), callback));
	}

	//------------------------------------------------------------------------------------------------
	//! A STASHED asset shown in the world lost its item (e.g. it was used up there): the record is removed.
	void RemoveVanished(string ownerId, string assetId, MRX_StashResultCallback callback = null)
	{
		MRX_AssetChange change = MRX_AssetChange.Remove(assetId, MRX_EAssetState.STASHED);
		Enqueue(ownerId, new MRX_StashChangeOp(this, ownerId, change, CreateServiceContext("vanished"), callback));
	}

	//------------------------------------------------------------------------------------------------
	void SetLossPolicy(notnull MRX_LossPolicy policy)
	{
		m_LossPolicy = policy;
	}

	//------------------------------------------------------------------------------------------------
	MRX_LossPolicy GetLossPolicy()
	{
		return m_LossPolicy;
	}

	//------------------------------------------------------------------------------------------------
	//! Reports deployed assets whose item disappeared: LOST while the owner is online (or the item was left on a body),
	//! back to STASHED with the stored state while the owner is offline. The Marx system calls it periodically.
	void CheckBindings()
	{
		array<MRX_AssetBinding> bindings = {};
		m_Bindings.GetAll(bindings);
		foreach (MRX_AssetBinding binding : bindings)
		{
			if (binding.m_bReleasing || binding.m_bVanishReported || m_World.IsAlive(binding.m_Item))
				continue;

			binding.m_bVanishReported = true;
			bool ownerOnline = m_Identity.GetPlayerId(binding.m_sOwnerId) != 0;
			if (ownerOnline || binding.m_bLostOnVanish)
				EnqueueSystemChange(binding.m_sOwnerId, binding.m_sAssetId, MRX_EAssetState.LOST, "lost");
			else
				EnqueueSystemChange(binding.m_sOwnerId, binding.m_sAssetId, MRX_EAssetState.STASHED, "restore");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the loss policy to the deployed assets the dead character carried. The Marx system calls it on player deaths.
	void OnCarrierKilled(Managed character)
	{
		array<MRX_AssetBinding> bindings = {};
		m_Bindings.GetAll(bindings);
		foreach (MRX_AssetBinding binding : bindings)
		{
			if (binding.m_bReleasing || !m_World.IsAlive(binding.m_Item) || !m_World.IsCarriedBy(binding.m_Item, character))
				continue;

			switch (m_LossPolicy.GetDeathAction(binding))
			{
				case MRX_EDeathAction.KEEP:
					binding.m_bLostOnVanish = true;
					break;

				case MRX_EDeathAction.LOSE:
					binding.m_bVanishReported = true;
					EnqueueSystemChange(binding.m_sOwnerId, binding.m_sAssetId, MRX_EAssetState.LOST, "lost");
					break;

				case MRX_EDeathAction.RETURN:
					Enqueue(binding.m_sOwnerId, new MRX_StashReturnOp(this, binding));
					break;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the recovery policy to the owner's assets deployed in an earlier session. The Marx system calls it when
	//! a player's owner becomes ready; consumers that use offline owners call it themselves.
	void RecoverOwner(string ownerId, MRX_StashResultCallback callback = null)
	{
		Enqueue(ownerId, new MRX_StashRecoverOp(this, ownerId, callback));
	}

	//------------------------------------------------------------------------------------------------
	//! \return Asset bound to the item, or empty when the item is not an owned asset.
	string GetAssetId(Managed item)
	{
		MRX_AssetBinding binding = m_Bindings.FindByItem(item);
		if (!binding)
			return string.Empty;

		return binding.m_sAssetId;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Owner of the asset bound to the item, or empty.
	string GetOwnerOf(Managed item)
	{
		MRX_AssetBinding binding = m_Bindings.FindByItem(item);
		if (!binding)
			return string.Empty;

		return binding.m_sOwnerId;
	}

	//------------------------------------------------------------------------------------------------
	//! ID of this server session, written to DEPLOYED assets.
	string GetSessionId()
	{
		return m_sSessionId;
	}

	//------------------------------------------------------------------------------------------------
	void AddValidator(notnull MRX_StashValidator validator)
	{
		if (!m_aValidators.Contains(validator))
			m_aValidators.Insert(validator);
	}

	//------------------------------------------------------------------------------------------------
	void RemoveValidator(MRX_StashValidator validator)
	{
		m_aValidators.RemoveItem(validator);
	}

	//------------------------------------------------------------------------------------------------
	//! Invoked for every committed asset change. A new asset reports oldState equal to its state.
	ScriptInvokerBase<MRX_AssetStateChangedDelegate> GetOnAssetStateChanged()
	{
		if (!m_OnAssetStateChanged)
			m_OnAssetStateChanged = new ScriptInvokerBase<MRX_AssetStateChangedDelegate>();

		return m_OnAssetStateChanged;
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, used by MRX_StashOp.
	MRX_StorageBackend GetBackend()
	{
		return m_Economy.GetBackend();
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, used by MRX_StashOp.
	MRX_StorageRules GetRules()
	{
		return m_Economy.GetRules();
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, used by MRX_StashOp.
	MRX_AssetWorld GetWorld()
	{
		return m_World;
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, used by MRX_StashOp.
	MRX_AssetBindings GetBindings()
	{
		return m_Bindings;
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, used by MRX_StashOp.
	bool IsDepositAllowed(int playerId, string ownerId, notnull MRX_ItemSnapshot snapshot)
	{
		foreach (MRX_StashValidator validator : m_aValidators)
		{
			if (!validator.CanDeposit(playerId, ownerId, snapshot))
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, used by MRX_StashOp.
	bool IsWithdrawAllowed(int playerId, notnull MRX_AssetRecord asset)
	{
		foreach (MRX_StashValidator validator : m_aValidators)
		{
			if (!validator.CanWithdraw(playerId, asset))
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Internal: a new request of the owner with the given context, or a service context for player actions.
	MRX_StashRequest CreateRequest(string ownerId, MRX_TxContext context, string action)
	{
		MRX_StashRequest request = new MRX_StashRequest();
		request.m_sRequestId = CreateId();
		request.m_sOwnerId = ownerId;
		request.m_iTimestamp = System.GetUnixTime();
		if (context)
			request.m_Context = context.Copy();
		else
			request.m_Context = MRX_TxContext.Create(LEDGER_SOURCE, action, action + ":" + request.m_sRequestId);

		return request;
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, used by MRX_StashListOp.
	void DeliverStash(MRX_StashCallback callback, MRX_EStashStatus status, MRX_StashRecord record)
	{
		MRX_StashDelivery.Post(m_CallQueue, callback, status, record);
	}

	//------------------------------------------------------------------------------------------------
	//! Internal: a new asset or request ID.
	static string CreateId()
	{
		return MRX_Marx.NewId();
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by MRX_StashOp once with its result.
	void OnOpFinished(notnull MRX_StashOp op, notnull MRX_StashResult result)
	{
		if (result.m_eStatus == MRX_EStashStatus.OK && m_OnAssetStateChanged)
		{
			foreach (MRX_AssetRecord asset : result.m_aAssets)
			{
				m_OnAssetStateChanged.Invoke(asset, op.GetOldStateOf(asset));
			}
		}

		MRX_StashResultDelivery.Post(m_CallQueue, op.GetCallback(), result);

		if (m_aDoneOps.IsEmpty())
			GetGame().GetCallqueue().CallLater(ReleaseDoneOps);

		m_aDoneOps.Insert(op);

		string ownerId = op.GetOwnerId();
		array<ref MRX_StashOp> queue = m_mQueues.Get(ownerId);
		if (!queue || queue.IsEmpty() || queue[0] != op)
			return;

		queue.RemoveOrdered(0);
		if (queue.IsEmpty())
			m_mQueues.Remove(ownerId);
		else
			GetGame().GetCallqueue().CallLater(RunHead, 0, false, ownerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Moves a deployed asset to a new state on behalf of Marx (losses, restores). Unbinds it when committed.
	protected void EnqueueSystemChange(string ownerId, string assetId, MRX_EAssetState newState, string action)
	{
		MRX_AssetChange change = MRX_AssetChange.Update(assetId, MRX_EAssetState.DEPLOYED, newState);
		MRX_TxContext context = MRX_TxContext.Create(LEDGER_SOURCE, action, string.Format("%1:%2:%3", action, assetId, m_sSessionId));
		Enqueue(ownerId, new MRX_StashChangeOp(this, ownerId, change, context, null));
	}

	//------------------------------------------------------------------------------------------------
	//! A context of this service for a one-off change.
	protected static MRX_TxContext CreateServiceContext(string action)
	{
		return MRX_TxContext.Create(LEDGER_SOURCE, action, action + ":" + CreateId());
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, used by MRX_StashOp: player ID of a connected owner, 0 when offline.
	int GetPlayerIdOf(string ownerId)
	{
		return m_Identity.GetPlayerId(ownerId);
	}

	//------------------------------------------------------------------------------------------------
	protected string GetOwnerId(int playerId)
	{
		return m_Identity.GetOwnerId(playerId);
	}

	//------------------------------------------------------------------------------------------------
	protected void Enqueue(string ownerId, notnull MRX_StashOp op)
	{
		if (ownerId.IsEmpty())
		{
			Reject(op, MRX_EStashStatus.OWNER_NOT_READY);
			return;
		}

		array<ref MRX_StashOp> queue = m_mQueues.Get(ownerId);
		if (!queue)
		{
			queue = {};
			m_mQueues.Insert(ownerId, queue);
		}

		queue.Insert(op);
		if (queue.Count() == 1)
			RunHead(ownerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Fails an op that never ran. Its result is delivered on a later frame like any other.
	protected void Reject(notnull MRX_StashOp op, MRX_EStashStatus status)
	{
		op.Fail(status);
	}

	//------------------------------------------------------------------------------------------------
	protected void ReleaseDoneOps()
	{
		m_aDoneOps.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunHead(string ownerId)
	{
		array<ref MRX_StashOp> queue = m_mQueues.Get(ownerId);
		if (!queue || queue.IsEmpty())
			return;

		MRX_EEconomyServiceState state = m_Economy.GetState();
		if (state == MRX_EEconomyServiceState.FAILED)
		{
			queue[0].Fail(MRX_EStashStatus.STORAGE_ERROR);
			return;
		}

		if (state != MRX_EEconomyServiceState.READY)
		{
			GetGame().GetCallqueue().CallLater(RunHead, READY_POLL_MS, false, ownerId);
			return;
		}

		queue[0].Start();
	}
}
