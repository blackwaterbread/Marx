//! One queued stash operation. Internal: started by MRX_StashService when it is the owner's oldest op,
//! finishes exactly once through Finish().
class MRX_StashOp : Managed
{
	//! Weak: the service owns its ops.
	protected MRX_StashService m_Service;
	protected string m_sOwnerId;
	protected ref MRX_StashResultCallback m_Callback;
	protected bool m_bDone;
	//! State of the changed asset before the op, for MRX_StashService.GetOnAssetStateChanged().
	protected MRX_EAssetState m_eOldState;
	protected bool m_bCreatesAsset;

	//------------------------------------------------------------------------------------------------
	protected void Init(MRX_StashService service, string ownerId, MRX_StashResultCallback callback)
	{
		m_Service = service;
		m_sOwnerId = ownerId;
		m_Callback = callback;
	}

	//------------------------------------------------------------------------------------------------
	void Start()
	{
		Fail(MRX_EStashStatus.STORAGE_ERROR);
	}

	//------------------------------------------------------------------------------------------------
	void Fail(MRX_EStashStatus status)
	{
		Finish(MRX_StashResult.Create(status));
	}

	//------------------------------------------------------------------------------------------------
	bool IsDone()
	{
		return m_bDone;
	}

	//------------------------------------------------------------------------------------------------
	string GetOwnerId()
	{
		return m_sOwnerId;
	}

	//------------------------------------------------------------------------------------------------
	MRX_StashResultCallback GetCallback()
	{
		return m_Callback;
	}

	//------------------------------------------------------------------------------------------------
	MRX_EAssetState GetOldState()
	{
		return m_eOldState;
	}

	//------------------------------------------------------------------------------------------------
	//! \return State of a changed asset before the op (ops that change several assets override this).
	MRX_EAssetState GetOldStateOf(notnull MRX_AssetRecord asset)
	{
		if (m_bCreatesAsset)
			return asset.m_eState;

		return m_eOldState;
	}

	//------------------------------------------------------------------------------------------------
	bool CreatesAsset()
	{
		return m_bCreatesAsset;
	}

	//------------------------------------------------------------------------------------------------
	protected void Finish(notnull MRX_StashResult result)
	{
		if (m_bDone)
			return;

		m_bDone = true;
		if (m_Service)
			m_Service.OnOpFinished(this, result);
	}

	//------------------------------------------------------------------------------------------------
	protected void LoadStash()
	{
		MRX_StashCallback callback = new MRX_StashCallback();
		callback.GetOnResult().Insert(OnStashLoaded);
		m_Service.GetBackend().LoadStash(m_sOwnerId, callback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnStashLoaded(MRX_EStashStatus status, MRX_StashRecord record)
	{
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyRequest(notnull MRX_StashRequest request)
	{
		MRX_StashResultCallback callback = new MRX_StashResultCallback();
		callback.GetOnResult().Insert(OnApplied);
		m_Service.GetBackend().ApplyStash(request, callback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnApplied(MRX_StashResult result)
	{
		Finish(result);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_StashListOp : MRX_StashOp
{
	protected ref MRX_StashCallback m_ListCallback;

	//------------------------------------------------------------------------------------------------
	void MRX_StashListOp(MRX_StashService service, string ownerId, MRX_StashCallback callback)
	{
		Init(service, ownerId, null);
		m_ListCallback = callback;
	}

	//------------------------------------------------------------------------------------------------
	override void Start()
	{
		LoadStash();
	}

	//------------------------------------------------------------------------------------------------
	override void Fail(MRX_EStashStatus status)
	{
		m_Service.DeliverStash(m_ListCallback, status, null);
		super.Fail(status);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnStashLoaded(MRX_EStashStatus status, MRX_StashRecord record)
	{
		m_Service.DeliverStash(m_ListCallback, status, record);
		Finish(MRX_StashResult.Create(status));
	}
}

//------------------------------------------------------------------------------------------------
//! Applies one prepared change (Remove, MarkConsumed, loss).
class MRX_StashChangeOp : MRX_StashOp
{
	protected ref MRX_AssetChange m_Change;
	protected ref MRX_TxContext m_Context;

	//------------------------------------------------------------------------------------------------
	void MRX_StashChangeOp(MRX_StashService service, string ownerId, notnull MRX_AssetChange change, MRX_TxContext context, MRX_StashResultCallback callback)
	{
		Init(service, ownerId, callback);
		m_Change = change;
		m_eOldState = change.m_eExpectedState;
		if (context)
			m_Context = context.Copy();
	}

	//------------------------------------------------------------------------------------------------
	override void Start()
	{
		if (!m_Context || !m_Context.IsValid())
		{
			Fail(MRX_EStashStatus.INVALID_CONTEXT);
			return;
		}

		MRX_StashRequest request = m_Service.CreateRequest(m_sOwnerId, m_Context, string.Empty);
		request.m_aChanges.Insert(m_Change);
		ApplyRequest(request);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnApplied(MRX_StashResult result)
	{
		// A deployed asset that left the DEPLOYED state no longer has a world item (a lost one stays as ordinary loot).
		bool leftWorld = m_Change.m_eType == MRX_EAssetChangeType.UPDATE && m_Change.m_eExpectedState == MRX_EAssetState.DEPLOYED && m_Change.m_eNewState != MRX_EAssetState.DEPLOYED;
		if (result.m_eStatus == MRX_EStashStatus.OK && leftWorld)
			m_Service.GetBindings().Unbind(m_Change.m_sAssetId);

		Finish(result);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_StashGrantOp : MRX_StashOp
{
	protected ResourceName m_sPrefab;
	protected ref MRX_TxContext m_Context;
	protected ref MRX_ItemSnapshot m_Snapshot;

	//------------------------------------------------------------------------------------------------
	void MRX_StashGrantOp(MRX_StashService service, string ownerId, ResourceName prefab, MRX_TxContext context, MRX_ItemSnapshot snapshot, MRX_StashResultCallback callback)
	{
		Init(service, ownerId, callback);
		m_sPrefab = prefab;
		m_bCreatesAsset = true;
		if (context)
			m_Context = context.Copy();

		if (snapshot)
			m_Snapshot = snapshot.Copy();
	}

	//------------------------------------------------------------------------------------------------
	override void Start()
	{
		if (!m_Context || !m_Context.IsValid())
		{
			Fail(MRX_EStashStatus.INVALID_CONTEXT);
			return;
		}

		if (m_sPrefab.IsEmpty())
		{
			Fail(MRX_EStashStatus.INVALID_PREFAB);
			return;
		}

		MRX_StashRequest request = m_Service.CreateRequest(m_sOwnerId, m_Context, string.Empty);
		request.m_aChanges.Insert(MRX_AssetChange.Add(MRX_AssetRecord.Create(MRX_StashService.CreateId(), m_sPrefab, m_Snapshot)));
		ApplyRequest(request);
	}
}

//------------------------------------------------------------------------------------------------
//! Item -> snapshot -> item removed -> record committed. A failed commit gives the item back.
class MRX_StashDepositOp : MRX_StashOp
{
	protected int m_iPlayerId;
	//! Weak: an engine entity.
	protected Managed m_Item;
	protected ref MRX_ItemSnapshot m_Snapshot;
	//! Set when the item is the world item of a deployed asset of this owner.
	protected MRX_AssetBinding m_Binding;

	//------------------------------------------------------------------------------------------------
	void MRX_StashDepositOp(MRX_StashService service, string ownerId, int playerId, Managed item, MRX_StashResultCallback callback)
	{
		Init(service, ownerId, callback);
		m_iPlayerId = playerId;
		m_Item = item;
	}

	//------------------------------------------------------------------------------------------------
	override void Start()
	{
		MRX_AssetWorld world = m_Service.GetWorld();
		MRX_EStashStatus status = world.CheckDeposit(m_iPlayerId, m_Item);
		if (status != MRX_EStashStatus.OK)
		{
			Fail(status);
			return;
		}

		m_Binding = m_Service.GetBindings().FindByItem(m_Item);
		if (m_Binding && m_Binding.m_sOwnerId != m_sOwnerId)
		{
			Fail(MRX_EStashStatus.NOT_OWNED);
			return;
		}

		LoadStash();
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnStashLoaded(MRX_EStashStatus status, MRX_StashRecord record)
	{
		if (status != MRX_EStashStatus.OK || !record)
		{
			Fail(MRX_EStashStatus.STORAGE_ERROR);
			return;
		}

		if (m_Binding)
		{
			MRX_AssetRecord asset = record.FindAsset(m_Binding.m_sAssetId);
			if (!asset || asset.m_eState != MRX_EAssetState.DEPLOYED)
			{
				Fail(MRX_EStashStatus.INVALID_STATE);
				return;
			}

			m_eOldState = MRX_EAssetState.DEPLOYED;
		}
		else
		{
			int maxAssets = m_Service.GetRules().m_iMaxStashAssets;
			if (maxAssets > 0 && record.m_aAssets.Count() >= maxAssets)
			{
				Fail(MRX_EStashStatus.STASH_FULL);
				return;
			}

			m_bCreatesAsset = true;
		}

		// The item may have changed or left the inventory while the stash was loading.
		MRX_AssetWorld world = m_Service.GetWorld();
		MRX_EStashStatus itemStatus = world.CheckDeposit(m_iPlayerId, m_Item);
		if (itemStatus != MRX_EStashStatus.OK)
		{
			Fail(itemStatus);
			return;
		}

		m_Snapshot = world.Capture(m_Item);
		if (!m_Snapshot)
		{
			Fail(MRX_EStashStatus.CAPTURE_FAILED);
			return;
		}

		if (!m_Service.IsDepositAllowed(m_iPlayerId, m_sOwnerId, m_Snapshot))
		{
			Fail(MRX_EStashStatus.REJECTED);
			return;
		}

		if (m_Binding)
			m_Binding.m_bReleasing = true;

		if (!world.Delete(m_Item))
		{
			if (m_Binding)
				m_Binding.m_bReleasing = false;

			Fail(MRX_EStashStatus.CAPTURE_FAILED);
			return;
		}

		MRX_StashRequest request = m_Service.CreateRequest(m_sOwnerId, null, "deposit");
		if (m_Binding)
			request.m_aChanges.Insert(MRX_AssetChange.Update(m_Binding.m_sAssetId, MRX_EAssetState.DEPLOYED, MRX_EAssetState.STASHED, m_Snapshot));
		else
			request.m_aChanges.Insert(MRX_AssetChange.Add(MRX_AssetRecord.Create(MRX_StashService.CreateId(), m_Snapshot.m_sPrefab, m_Snapshot)));

		ApplyRequest(request);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnApplied(MRX_StashResult result)
	{
		if (result.m_eStatus == MRX_EStashStatus.OK)
		{
			if (m_Binding)
				m_Service.GetBindings().Unbind(m_Binding.m_sAssetId);

			Finish(result);
			return;
		}

		// The item is already gone: give it back rather than lose it.
		Print(string.Format("[MRX] Deposit of %1 for %2 failed (%3), returning the item", m_Snapshot.m_sPrefab, m_sOwnerId, typename.EnumToString(MRX_EStashStatus, result.m_eStatus)), LogLevel.ERROR);
		m_Service.GetWorld().Deliver(m_iPlayerId, m_Snapshot.m_sPrefab, m_Snapshot, new MRX_StashDepositRollback(this, result));
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by MRX_StashDepositRollback.
	void OnReturned(Managed item, notnull MRX_StashResult failure)
	{
		if (!item)
			Print(string.Format("[MRX] Could not return %1 to player %2 after a failed deposit, the item is lost", m_Snapshot.m_sPrefab, m_iPlayerId), LogLevel.ERROR);

		if (m_Binding)
		{
			m_Binding.m_Item = item;
			m_Binding.m_bReleasing = false;
		}

		Finish(failure);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_StashDepositRollback : MRX_AssetSpawnCallback
{
	protected ref MRX_StashDepositOp m_Op;
	protected ref MRX_StashResult m_Failure;

	//------------------------------------------------------------------------------------------------
	void MRX_StashDepositRollback(MRX_StashDepositOp op, MRX_StashResult failure)
	{
		m_Op = op;
		m_Failure = failure;
	}

	//------------------------------------------------------------------------------------------------
	override void OnSpawned(Managed item)
	{
		m_Op.OnReturned(item, m_Failure);
	}
}

//------------------------------------------------------------------------------------------------
//! Record locked by the owner queue -> item spawned -> bound -> record committed. A failed commit removes the item.
class MRX_StashWithdrawOp : MRX_StashOp
{
	protected int m_iPlayerId;
	protected string m_sAssetId;
	//! Weak: an engine entity.
	protected Managed m_Item;

	//------------------------------------------------------------------------------------------------
	void MRX_StashWithdrawOp(MRX_StashService service, string ownerId, int playerId, string assetId, MRX_StashResultCallback callback)
	{
		Init(service, ownerId, callback);
		m_iPlayerId = playerId;
		m_sAssetId = assetId;
		m_eOldState = MRX_EAssetState.STASHED;
	}

	//------------------------------------------------------------------------------------------------
	override void Start()
	{
		LoadStash();
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnStashLoaded(MRX_EStashStatus status, MRX_StashRecord record)
	{
		if (status != MRX_EStashStatus.OK || !record)
		{
			Fail(MRX_EStashStatus.STORAGE_ERROR);
			return;
		}

		MRX_AssetRecord asset = record.FindAsset(m_sAssetId);
		if (!asset)
		{
			Fail(MRX_EStashStatus.UNKNOWN_ASSET);
			return;
		}

		if (asset.m_eState != MRX_EAssetState.STASHED)
		{
			Fail(MRX_EStashStatus.INVALID_STATE);
			return;
		}

		if (!m_Service.IsWithdrawAllowed(m_iPlayerId, asset))
		{
			Fail(MRX_EStashStatus.REJECTED);
			return;
		}

		MRX_AssetWorld world = m_Service.GetWorld();
		if (!world.CanDeliver(m_iPlayerId, asset.m_sPrefab))
		{
			Fail(MRX_EStashStatus.NO_SPACE);
			return;
		}

		world.Deliver(m_iPlayerId, asset.m_sPrefab, asset.m_Snapshot, new MRX_StashWithdrawSpawn(this));
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by MRX_StashWithdrawSpawn.
	void OnSpawned(Managed item)
	{
		if (!item)
		{
			Fail(MRX_EStashStatus.SPAWN_FAILED);
			return;
		}

		m_Item = item;
		m_Service.GetBindings().Bind(m_sAssetId, m_sOwnerId, item);

		MRX_StashRequest request = m_Service.CreateRequest(m_sOwnerId, null, "withdraw");
		request.m_aChanges.Insert(MRX_AssetChange.Update(m_sAssetId, MRX_EAssetState.STASHED, MRX_EAssetState.DEPLOYED, null, m_Service.GetSessionId()));
		ApplyRequest(request);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnApplied(MRX_StashResult result)
	{
		if (result.m_eStatus != MRX_EStashStatus.OK)
		{
			// Not committed: the spawned item must not exist next to a STASHED record.
			MRX_AssetBinding binding = m_Service.GetBindings().Get(m_sAssetId);
			if (binding)
				binding.m_bReleasing = true;

			if (m_Item)
				m_Service.GetWorld().Delete(m_Item);

			m_Service.GetBindings().Unbind(m_sAssetId);
		}

		Finish(result);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_StashWithdrawSpawn : MRX_AssetSpawnCallback
{
	protected ref MRX_StashWithdrawOp m_Op;

	//------------------------------------------------------------------------------------------------
	void MRX_StashWithdrawSpawn(MRX_StashWithdrawOp op)
	{
		m_Op = op;
	}

	//------------------------------------------------------------------------------------------------
	override void OnSpawned(Managed item)
	{
		m_Op.OnSpawned(item);
	}
}

//------------------------------------------------------------------------------------------------
//! Loss policy RETURN: takes the item off the body and stores its current state. Without the item, the stored state is restored.
class MRX_StashReturnOp : MRX_StashOp
{
	protected MRX_AssetBinding m_Binding;

	//------------------------------------------------------------------------------------------------
	void MRX_StashReturnOp(MRX_StashService service, notnull MRX_AssetBinding binding)
	{
		Init(service, binding.m_sOwnerId, null);
		m_Binding = binding;
		m_eOldState = MRX_EAssetState.DEPLOYED;
	}

	//------------------------------------------------------------------------------------------------
	override void Start()
	{
		if (!m_Binding)
		{
			Fail(MRX_EStashStatus.UNKNOWN_ASSET);
			return;
		}

		MRX_AssetWorld world = m_Service.GetWorld();
		MRX_ItemSnapshot snapshot;
		if (world.IsAlive(m_Binding.m_Item))
			snapshot = world.Capture(m_Binding.m_Item);

		m_Binding.m_bReleasing = true;
		if (snapshot && !world.Delete(m_Binding.m_Item))
		{
			m_Binding.m_bReleasing = false;
			Fail(MRX_EStashStatus.CAPTURE_FAILED);
			return;
		}

		MRX_StashRequest request = m_Service.CreateRequest(m_sOwnerId, null, "return");
		request.m_aChanges.Insert(MRX_AssetChange.Update(m_Binding.m_sAssetId, MRX_EAssetState.DEPLOYED, MRX_EAssetState.STASHED, snapshot));
		ApplyRequest(request);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnApplied(MRX_StashResult result)
	{
		if (result.m_eStatus == MRX_EStashStatus.OK)
			m_Service.GetBindings().Unbind(m_Binding.m_sAssetId);
		else
			Print(string.Format("[MRX] Could not return asset %1 of %2 to the stash: %3. It is recovered in the next session.", m_Binding.m_sAssetId, m_sOwnerId, typename.EnumToString(MRX_EStashStatus, result.m_eStatus)), LogLevel.ERROR);

		Finish(result);
	}
}

//------------------------------------------------------------------------------------------------
//! Applies the recovery policy to assets deployed in an earlier session (their bindings did not survive).
class MRX_StashRecoverOp : MRX_StashOp
{
	//------------------------------------------------------------------------------------------------
	void MRX_StashRecoverOp(MRX_StashService service, string ownerId, MRX_StashResultCallback callback)
	{
		Init(service, ownerId, callback);
		m_eOldState = MRX_EAssetState.DEPLOYED;
	}

	//------------------------------------------------------------------------------------------------
	override void Start()
	{
		LoadStash();
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnStashLoaded(MRX_EStashStatus status, MRX_StashRecord record)
	{
		if (status != MRX_EStashStatus.OK || !record)
		{
			Fail(MRX_EStashStatus.STORAGE_ERROR);
			return;
		}

		string sessionId = m_Service.GetSessionId();
		// Only assets of other sessions change, so running it again is harmless.
		MRX_StashRequest request = m_Service.CreateRequest(m_sOwnerId, null, "recover");
		foreach (MRX_AssetRecord asset : record.m_aAssets)
		{
			if (asset.m_eState != MRX_EAssetState.DEPLOYED || asset.m_sDeploySession == sessionId)
				continue;

			MRX_EAssetState newState = MRX_EAssetState.STASHED;
			if (m_Service.GetLossPolicy().GetRecoveryAction(asset) == MRX_ERecoveryAction.LOSE)
				newState = MRX_EAssetState.LOST;

			request.m_aChanges.Insert(MRX_AssetChange.Update(asset.m_sId, MRX_EAssetState.DEPLOYED, newState));
		}

		if (request.m_aChanges.IsEmpty())
		{
			Finish(MRX_StashResult.Create(MRX_EStashStatus.OK, request.m_sRequestId));
			return;
		}

		ApplyRequest(request);
	}

}

//------------------------------------------------------------------------------------------------
//! StoreWorldItem: the item stays in the world; only the record and the binding change.
class MRX_StashStoreWorldOp : MRX_StashOp
{
	//! Weak: an engine entity.
	protected Managed m_Item;
	protected ref MRX_ItemSnapshot m_Snapshot;
	protected MRX_AssetBinding m_Binding;
	protected string m_sPlacement;

	//------------------------------------------------------------------------------------------------
	void MRX_StashStoreWorldOp(MRX_StashService service, string ownerId, Managed item, MRX_StashResultCallback callback, string placement)
	{
		Init(service, ownerId, callback);
		m_Item = item;
		m_sPlacement = placement;
	}

	//------------------------------------------------------------------------------------------------
	override void Start()
	{
		m_Binding = m_Service.GetBindings().FindByItem(m_Item);
		if (m_Binding && m_Binding.m_sOwnerId != m_sOwnerId)
		{
			Fail(MRX_EStashStatus.NOT_OWNED);
			return;
		}

		if (!m_Service.GetWorld().IsAlive(m_Item))
		{
			Fail(MRX_EStashStatus.NOT_IN_INVENTORY);
			return;
		}

		LoadStash();
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnStashLoaded(MRX_EStashStatus status, MRX_StashRecord record)
	{
		if (status != MRX_EStashStatus.OK || !record)
		{
			Fail(MRX_EStashStatus.STORAGE_ERROR);
			return;
		}

		if (m_Binding)
		{
			MRX_AssetRecord asset = record.FindAsset(m_Binding.m_sAssetId);
			if (!asset || asset.m_eState != MRX_EAssetState.DEPLOYED)
			{
				Fail(MRX_EStashStatus.INVALID_STATE);
				return;
			}

			m_eOldState = MRX_EAssetState.DEPLOYED;
		}
		else
		{
			int maxAssets = m_Service.GetRules().m_iMaxStashAssets;
			if (maxAssets > 0 && record.m_aAssets.Count() >= maxAssets)
			{
				Fail(MRX_EStashStatus.STASH_FULL);
				return;
			}

			m_bCreatesAsset = true;
		}

		m_Snapshot = m_Service.GetWorld().Capture(m_Item);
		if (!m_Snapshot)
		{
			Fail(MRX_EStashStatus.CAPTURE_FAILED);
			return;
		}

		if (!m_Service.IsDepositAllowed(m_Service.GetPlayerIdOf(m_sOwnerId), m_sOwnerId, m_Snapshot))
		{
			Fail(MRX_EStashStatus.REJECTED);
			return;
		}

		MRX_StashRequest request = m_Service.CreateRequest(m_sOwnerId, null, "store");
		if (m_Binding)
		{
			request.m_aChanges.Insert(MRX_AssetChange.Update(m_Binding.m_sAssetId, MRX_EAssetState.DEPLOYED, MRX_EAssetState.STASHED, m_Snapshot).WithPlacement(m_sPlacement));
		}
		else
		{
			MRX_AssetRecord asset = MRX_AssetRecord.Create(MRX_StashService.CreateId(), m_Snapshot.m_sPrefab, m_Snapshot);
			asset.m_sPlacement = m_sPlacement;
			request.m_aChanges.Insert(MRX_AssetChange.Add(asset));
		}

		ApplyRequest(request);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnApplied(MRX_StashResult result)
	{
		if (result.m_eStatus == MRX_EStashStatus.OK && m_Binding)
			m_Service.GetBindings().Unbind(m_Binding.m_sAssetId);

		Finish(result);
	}
}

//------------------------------------------------------------------------------------------------
//! TakeWorldItem: a STASHED asset whose item is already in the world becomes DEPLOYED and bound to it.
class MRX_StashTakeWorldOp : MRX_StashOp
{
	protected string m_sAssetId;
	//! Weak: an engine entity.
	protected Managed m_Item;

	//------------------------------------------------------------------------------------------------
	void MRX_StashTakeWorldOp(MRX_StashService service, string ownerId, string assetId, Managed item, MRX_StashResultCallback callback)
	{
		Init(service, ownerId, callback);
		m_sAssetId = assetId;
		m_Item = item;
		m_eOldState = MRX_EAssetState.STASHED;
	}

	//------------------------------------------------------------------------------------------------
	override void Start()
	{
		if (!m_Service.GetWorld().IsAlive(m_Item))
		{
			Fail(MRX_EStashStatus.NOT_IN_INVENTORY);
			return;
		}

		LoadStash();
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnStashLoaded(MRX_EStashStatus status, MRX_StashRecord record)
	{
		if (status != MRX_EStashStatus.OK || !record)
		{
			Fail(MRX_EStashStatus.STORAGE_ERROR);
			return;
		}

		MRX_AssetRecord asset = record.FindAsset(m_sAssetId);
		if (!asset)
		{
			Fail(MRX_EStashStatus.UNKNOWN_ASSET);
			return;
		}

		if (asset.m_eState != MRX_EAssetState.STASHED)
		{
			Fail(MRX_EStashStatus.INVALID_STATE);
			return;
		}

		if (!m_Service.IsWithdrawAllowed(m_Service.GetPlayerIdOf(m_sOwnerId), asset))
		{
			Fail(MRX_EStashStatus.REJECTED);
			return;
		}

		MRX_StashRequest request = m_Service.CreateRequest(m_sOwnerId, null, "take");
		request.m_aChanges.Insert(MRX_AssetChange.Update(m_sAssetId, MRX_EAssetState.STASHED, MRX_EAssetState.DEPLOYED, null, m_Service.GetSessionId()));
		ApplyRequest(request);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnApplied(MRX_StashResult result)
	{
		if (result.m_eStatus == MRX_EStashStatus.OK)
			m_Service.GetBindings().Bind(m_sAssetId, m_sOwnerId, m_Item);

		Finish(result);
	}
}

//------------------------------------------------------------------------------------------------
//! ApplyWorldChanges: one request for changes of stashed assets shown in the world that belong together. Changes are
//! built from the stash as it is when the op runs: updates of assets no longer STASHED and removals of gone assets are
//! left out. Removed DEPLOYED assets are unbound, new DEPLOYED ones bound to their items.
class MRX_StashWorldChangesOp : MRX_StashOp
{
	protected ref MRX_StashWorldChanges m_Changes;
	protected ref MRX_TxContext m_Context;
	protected ref map<string, MRX_EAssetState> m_mOldStates = new map<string, MRX_EAssetState>();

	//------------------------------------------------------------------------------------------------
	void MRX_StashWorldChangesOp(MRX_StashService service, string ownerId, notnull MRX_StashWorldChanges changes, MRX_TxContext context, MRX_StashResultCallback callback)
	{
		Init(service, ownerId, callback);
		m_Changes = changes;
		m_eOldState = MRX_EAssetState.STASHED;
		if (context)
			m_Context = context.Copy();
	}

	//------------------------------------------------------------------------------------------------
	override void Start()
	{
		if (!m_Context || !m_Context.IsValid())
		{
			Fail(MRX_EStashStatus.INVALID_CONTEXT);
			return;
		}

		LoadStash();
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnStashLoaded(MRX_EStashStatus status, MRX_StashRecord record)
	{
		if (status != MRX_EStashStatus.OK || !record)
		{
			Fail(MRX_EStashStatus.STORAGE_ERROR);
			return;
		}

		MRX_StashRequest request = m_Service.CreateRequest(m_sOwnerId, m_Context, string.Empty);
		foreach (int i, string updatedId : m_Changes.m_aUpdatedIds)
		{
			MRX_AssetRecord updated = record.FindAsset(updatedId);
			if (!updated || updated.m_eState != MRX_EAssetState.STASHED || m_Changes.m_aRemovedIds.Contains(updatedId))
				continue;

			request.m_aChanges.Insert(MRX_AssetChange.Update(updatedId, MRX_EAssetState.STASHED, MRX_EAssetState.STASHED, m_Changes.m_aUpdatedSnapshots[i]));
			m_mOldStates.Set(updatedId, MRX_EAssetState.STASHED);
		}

		foreach (string removedId : m_Changes.m_aRemovedIds)
		{
			MRX_AssetRecord removed = record.FindAsset(removedId);
			if (!removed)
				continue;

			request.m_aChanges.Insert(MRX_AssetChange.Remove(removedId, removed.m_eState));
			m_mOldStates.Set(removedId, removed.m_eState);
		}

		foreach (MRX_StashWorldAdd added : m_Changes.m_aAdded)
		{
			MRX_AssetRecord asset = MRX_AssetRecord.Create(added.m_sAssetId, added.m_Snapshot.m_sPrefab, added.m_Snapshot);
			if (added.m_bDeployed)
			{
				asset.m_eState = MRX_EAssetState.DEPLOYED;
				asset.m_sDeploySession = m_Service.GetSessionId();
			}
			else
			{
				asset.m_sPlacement = added.m_sPlacement;
			}

			request.m_aChanges.Insert(MRX_AssetChange.Add(asset));
		}

		if (request.m_aChanges.IsEmpty())
		{
			Finish(MRX_StashResult.Create(MRX_EStashStatus.OK));
			return;
		}

		ApplyRequest(request);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnApplied(MRX_StashResult result)
	{
		if (result.m_eStatus == MRX_EStashStatus.OK)
		{
			foreach (string removedId, MRX_EAssetState oldState : m_mOldStates)
			{
				if (oldState == MRX_EAssetState.DEPLOYED && m_Changes.m_aRemovedIds.Contains(removedId))
					m_Service.GetBindings().Unbind(removedId);
			}

			foreach (MRX_StashWorldAdd added : m_Changes.m_aAdded)
			{
				if (added.m_bDeployed && m_Service.GetWorld().IsAlive(added.m_Item))
					m_Service.GetBindings().Bind(added.m_sAssetId, m_sOwnerId, added.m_Item);
			}
		}

		Finish(result);
	}

	//------------------------------------------------------------------------------------------------
	//! New assets report their own state; changed ones the state they had.
	override MRX_EAssetState GetOldStateOf(notnull MRX_AssetRecord asset)
	{
		if (m_mOldStates.Contains(asset.m_sId))
			return m_mOldStates.Get(asset.m_sId);

		return asset.m_eState;
	}
}
