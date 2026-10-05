//! Wallets and stashes as per-owner scripted states in the "MarxWallets" and "MarxStashes" collections, committed with
//! CommitStorage(GamemodeStorage) after every change. Needs a persistence config with those collections and the Marx
//! state serializers (Marx_Core ships reference configs). Without them, the backend falls back to in-memory storage.
class MRX_NativeBackend : MRX_StorageBackend
{
	//! Save data names allow only letters, digits, hyphen and dot (no underscore).
	static const string COLLECTION_NAME = "MarxWallets";
	static const string ID_PREFIX = "marx:wallet:";
	static const string STASH_COLLECTION_NAME = "MarxStashes";
	static const string STASH_ID_PREFIX = "marx:stash:";
	protected static const int READY_POLL_MS = 250;
	protected static const int READY_TIMEOUT_MS = 60000;
	protected static const int COMMIT_TIMEOUT_MS = 10000;

	protected ref MRX_StorageRules m_Rules;
	protected ref MRX_StatusCallback m_InitCallback;
	protected int m_iWaitedMs;
	//! Set when the persistence config has no Marx wallet collection.
	protected ref MRX_InMemoryBackend m_Fallback;
	//! Set when the persistence config has the wallet collection but no stash collection.
	protected ref MRX_InMemoryBackend m_StashFallback;

	protected ref map<string, ref MRX_NativeWallet> m_mWallets = new map<string, ref MRX_NativeWallet>();
	protected ref map<string, ref MRX_NativeStash> m_mStashes = new map<string, ref MRX_NativeStash>();
	protected ref array<ref MRX_NativeOp> m_aOps = {};

	protected ref array<ref MRX_NativeOp> m_aCommitQueue = {};
	protected ref array<ref MRX_NativeOp> m_aCommitting = {};
	protected ref PersistenceStatusCallback m_CommitCallback;
	protected bool m_bCommitting;

	//------------------------------------------------------------------------------------------------
	//! True when the running world has a persistence system with the Marx wallet collection.
	static bool IsAvailable()
	{
		PersistenceSystem persistence = PersistenceSystem.GetInstance();
		if (!persistence)
			return false;

		return persistence.FindCollection(COLLECTION_NAME) != null;
	}

	//------------------------------------------------------------------------------------------------
	override void Init(notnull MRX_StorageRules rules, notnull MRX_StatusCallback callback)
	{
		m_Rules = rules;
		m_InitCallback = callback;
		WaitForPersistence();
	}

	//------------------------------------------------------------------------------------------------
	override void LoadWallet(string ownerId, notnull MRX_WalletCallback callback)
	{
		if (m_Fallback)
		{
			m_Fallback.LoadWallet(ownerId, callback);
			return;
		}

		StartOp(new MRX_NativeLoadWalletOp(this, ownerId, callback));
	}

	//------------------------------------------------------------------------------------------------
	override void ApplyTransaction(notnull MRX_TxRequest request, notnull MRX_TxCallback callback)
	{
		if (m_Fallback)
		{
			m_Fallback.ApplyTransaction(request, callback);
			return;
		}

		StartOp(new MRX_NativeApplyOp(this, request, callback));
	}

	//------------------------------------------------------------------------------------------------
	override void GetHistory(string ownerId, string currency, int limit, notnull MRX_HistoryCallback callback)
	{
		if (m_Fallback)
		{
			m_Fallback.GetHistory(ownerId, currency, limit, callback);
			return;
		}

		StartOp(new MRX_NativeHistoryOp(this, ownerId, currency, limit, callback));
	}

	//------------------------------------------------------------------------------------------------
	override void LoadStash(string ownerId, notnull MRX_StashCallback callback)
	{
		MRX_InMemoryBackend fallback = GetStashFallback();
		if (fallback)
		{
			fallback.LoadStash(ownerId, callback);
			return;
		}

		StartOp(new MRX_NativeLoadStashOp(this, ownerId, callback));
	}

	//------------------------------------------------------------------------------------------------
	override void ApplyStash(notnull MRX_StashRequest request, notnull MRX_StashResultCallback callback)
	{
		MRX_InMemoryBackend fallback = GetStashFallback();
		if (fallback)
		{
			fallback.ApplyStash(request, callback);
			return;
		}

		StartOp(new MRX_NativeStashApplyOp(this, request, callback));
	}

	//------------------------------------------------------------------------------------------------
	override int GetCapabilities()
	{
		if (m_Fallback)
			return 0;

		return MRX_EStorageCapability.DURABLE;
	}

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		if (m_Fallback)
			return "MRX_NativeBackend (in-memory fallback)";

		return ClassName();
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, used by MRX_NativeOp.
	MRX_StorageRules GetRules()
	{
		return m_Rules;
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, used by MRX_NativeOp.
	MRX_CallQueue GetCallQueue()
	{
		return m_CallQueue;
	}

	//------------------------------------------------------------------------------------------------
	//! Internal: the loaded wallet of an owner, or null.
	MRX_NativeWallet GetLoadedWallet(string ownerId)
	{
		MRX_NativeWallet wallet = m_mWallets.Get(ownerId);
		if (!wallet || !wallet.m_bLoaded)
			return null;

		return wallet;
	}

	//------------------------------------------------------------------------------------------------
	//! Internal: loads the owner's wallet once, then calls op.OnWalletReady().
	void LoadWalletState(string ownerId, notnull MRX_NativeOp op)
	{
		MRX_NativeWallet wallet = m_mWallets.Get(ownerId);
		if (wallet && wallet.m_bLoaded)
		{
			op.OnWalletReady(MRX_ETxStatus.OK);
			return;
		}

		if (!wallet)
		{
			wallet = new MRX_NativeWallet();
			wallet.m_sOwnerId = ownerId;
			wallet.m_State = new MRX_WalletState();
			wallet.m_State.m_Record = MRX_WalletRecord.Create(ownerId);
			m_mWallets.Insert(ownerId, wallet);
		}

		wallet.m_aWaiting.Insert(op);
		if (wallet.m_bLoading)
			return;

		PersistenceSystem persistence = PersistenceSystem.GetInstance();
		if (!wallet.m_bIdSet)
		{
			wallet.m_bIdSet = persistence.SetId(wallet.m_State, PersistenceIdUtils.FromString(ID_PREFIX + ownerId), true);
			if (!wallet.m_bIdSet)
			{
				Print(string.Format("[MRX] Could not assign the persistence ID of wallet %1", ownerId), LogLevel.ERROR);
				NotifyWaiting(wallet, MRX_ETxStatus.STORAGE_ERROR);
				return;
			}
		}

		wallet.m_bLoading = true;
		PersistenceLoadRequest request = new PersistenceLoadRequest();
		request.Instances = {wallet.m_State};
		// The callback keeps its context alive, so it only gets a weak link back to the wallet (no ref cycle).
		wallet.m_LoadCallback = new PersistenceResultCallback(OnWalletStateLoaded, MRX_NativeWalletLink.Create(wallet));
		persistence.RequestLoad(request, wallet.m_LoadCallback);
	}

	//------------------------------------------------------------------------------------------------
	//! Internal: the loaded stash of an owner, or null.
	MRX_NativeStash GetLoadedStash(string ownerId)
	{
		MRX_NativeStash stash = m_mStashes.Get(ownerId);
		if (!stash || !stash.m_bLoaded)
			return null;

		return stash;
	}

	//------------------------------------------------------------------------------------------------
	//! Internal: loads the owner's stash once, then calls op.OnStashReady().
	void LoadStashState(string ownerId, notnull MRX_NativeOp op)
	{
		MRX_NativeStash stash = m_mStashes.Get(ownerId);
		if (stash && stash.m_bLoaded)
		{
			op.OnStashReady(MRX_EStashStatus.OK);
			return;
		}

		if (!stash)
		{
			stash = new MRX_NativeStash();
			stash.m_sOwnerId = ownerId;
			stash.m_State = new MRX_StashState();
			stash.m_State.m_Record = MRX_StashRecord.Create(ownerId);
			m_mStashes.Insert(ownerId, stash);
		}

		stash.m_aWaiting.Insert(op);
		if (stash.m_bLoading)
			return;

		PersistenceSystem persistence = PersistenceSystem.GetInstance();
		if (!stash.m_bIdSet)
		{
			stash.m_bIdSet = persistence.SetId(stash.m_State, PersistenceIdUtils.FromString(STASH_ID_PREFIX + ownerId), true);
			if (!stash.m_bIdSet)
			{
				Print(string.Format("[MRX] Could not assign the persistence ID of stash %1", ownerId), LogLevel.ERROR);
				NotifyStashWaiting(stash, MRX_EStashStatus.STORAGE_ERROR);
				return;
			}
		}

		stash.m_bLoading = true;
		PersistenceLoadRequest request = new PersistenceLoadRequest();
		request.Instances = {stash.m_State};
		stash.m_LoadCallback = new PersistenceResultCallback(OnStashStateLoaded, MRX_NativeStashLink.Create(stash));
		persistence.RequestLoad(request, stash.m_LoadCallback);
	}

	//------------------------------------------------------------------------------------------------
	//! Internal: commits the staged states of the op, batched with other ops waiting for a commit.
	void RequestCommit(notnull MRX_NativeOp op)
	{
		m_aCommitQueue.Insert(op);
		if (!m_bCommitting)
			StartCommit();
	}

	//------------------------------------------------------------------------------------------------
	protected void StartOp(notnull MRX_NativeOp op)
	{
		// Finished ops are released here, never from inside their own callbacks.
		for (int i = m_aOps.Count() - 1; i >= 0; i--)
		{
			if (m_aOps[i].IsDone())
				m_aOps.Remove(i);
		}

		m_aOps.Insert(op);
		op.Start();
	}

	//------------------------------------------------------------------------------------------------
	protected void StartCommit()
	{
		m_bCommitting = true;
		m_aCommitting = m_aCommitQueue;
		m_aCommitQueue = {};

		m_CommitCallback = new PersistenceStatusCallback(OnCommitted);
		GetGame().GetCallqueue().CallLater(OnCommitTimeout, COMMIT_TIMEOUT_MS);
		PersistenceSystem.GetInstance().CommitStorage(GamemodeStorage, m_CommitCallback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCommitted(EPersistenceStatusCode statusCode, Managed context)
	{
		if (!m_bCommitting)
		{
			Print(string.Format("[MRX] Late wallet commit result %1 (after timeout)", typename.EnumToString(EPersistenceStatusCode, statusCode)), LogLevel.WARNING);
			return;
		}

		GetGame().GetCallqueue().Remove(OnCommitTimeout);
		if (statusCode != EPersistenceStatusCode.OK)
			Print(string.Format("[MRX] Wallet commit failed: %1", typename.EnumToString(EPersistenceStatusCode, statusCode)), LogLevel.ERROR);

		FinishCommit(statusCode == EPersistenceStatusCode.OK, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCommitTimeout()
	{
		if (!m_bCommitting)
			return;

		Print(string.Format("[MRX] Wallet commit did not answer within %1 ms, outcome unknown", COMMIT_TIMEOUT_MS), LogLevel.ERROR);
		FinishCommit(false, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void FinishCommit(bool success, bool timedOut)
	{
		m_bCommitting = false;
		array<ref MRX_NativeOp> committed = m_aCommitting;
		m_aCommitting = {};
		foreach (MRX_NativeOp op : committed)
		{
			op.OnCommitFinished(success, timedOut);
		}

		if (!m_aCommitQueue.IsEmpty())
			StartCommit();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnWalletStateLoaded(EPersistenceStatusCode statusCode, Managed result, bool isLast, Managed context)
	{
		MRX_NativeWalletLink link = MRX_NativeWalletLink.Cast(context);
		if (!link || !link.m_Wallet)
			return;

		MRX_NativeWallet wallet = link.m_Wallet;
		wallet.m_bLoading = false;

		// NOT_FOUND = the owner has no saved wallet yet.
		if (statusCode == EPersistenceStatusCode.OK || statusCode == EPersistenceStatusCode.NOT_FOUND)
		{
			wallet.m_bLoaded = true;
			NotifyWaiting(wallet, MRX_ETxStatus.OK);
			return;
		}

		Print(string.Format("[MRX] Could not load wallet %1: %2", wallet.m_sOwnerId, typename.EnumToString(EPersistenceStatusCode, statusCode)), LogLevel.ERROR);
		NotifyWaiting(wallet, MRX_ETxStatus.STORAGE_ERROR);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnStashStateLoaded(EPersistenceStatusCode statusCode, Managed result, bool isLast, Managed context)
	{
		MRX_NativeStashLink link = MRX_NativeStashLink.Cast(context);
		if (!link || !link.m_Stash)
			return;

		MRX_NativeStash stash = link.m_Stash;
		stash.m_bLoading = false;

		// NOT_FOUND = the owner has no saved stash yet.
		if (statusCode == EPersistenceStatusCode.OK || statusCode == EPersistenceStatusCode.NOT_FOUND)
		{
			stash.m_bLoaded = true;
			NotifyStashWaiting(stash, MRX_EStashStatus.OK);
			return;
		}

		Print(string.Format("[MRX] Could not load stash %1: %2", stash.m_sOwnerId, typename.EnumToString(EPersistenceStatusCode, statusCode)), LogLevel.ERROR);
		NotifyStashWaiting(stash, MRX_EStashStatus.STORAGE_ERROR);
	}

	//------------------------------------------------------------------------------------------------
	protected void NotifyStashWaiting(notnull MRX_NativeStash stash, MRX_EStashStatus status)
	{
		array<ref MRX_NativeOp> waiting = stash.m_aWaiting;
		stash.m_aWaiting = {};
		foreach (MRX_NativeOp op : waiting)
		{
			op.OnStashReady(status);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_InMemoryBackend GetStashFallback()
	{
		if (m_Fallback)
			return m_Fallback;

		return m_StashFallback;
	}

	//------------------------------------------------------------------------------------------------
	protected void NotifyWaiting(notnull MRX_NativeWallet wallet, MRX_ETxStatus status)
	{
		array<ref MRX_NativeOp> waiting = wallet.m_aWaiting;
		wallet.m_aWaiting = {};
		foreach (MRX_NativeOp op : waiting)
		{
			op.OnWalletReady(status);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void WaitForPersistence()
	{
		PersistenceSystem persistence = PersistenceSystem.GetInstance();
		if (!persistence)
		{
			UseFallback("this world has no persistence system");
			return;
		}

		EPersistenceSystemState state = persistence.GetState();
		if (state == EPersistenceSystemState.ACTIVE)
		{
			if (!persistence.FindCollection(COLLECTION_NAME))
			{
				UseFallback(string.Format("the persistence config has no '%1' collection", COLLECTION_NAME));
				return;
			}

			Print(string.Format("[MRX] Wallets are stored in the '%1' collection", COLLECTION_NAME), LogLevel.NORMAL);
			if (persistence.FindCollection(STASH_COLLECTION_NAME))
			{
				Print(string.Format("[MRX] Stashes are stored in the '%1' collection", STASH_COLLECTION_NAME), LogLevel.NORMAL);
			}
			else
			{
				Print(string.Format("[MRX] The persistence config has no '%1' collection. Stashes are kept in memory only and are lost when the server stops.", STASH_COLLECTION_NAME), LogLevel.ERROR);
				m_StashFallback = new MRX_InMemoryBackend();
				m_StashFallback.Init(m_Rules, new MRX_StatusCallback());
			}

			m_CallQueue.PostStatus(m_InitCallback, MRX_ETxStatus.OK);
			return;
		}

		if (state == EPersistenceSystemState.FAILURE || state == EPersistenceSystemState.SHUTDOWN || m_iWaitedMs >= READY_TIMEOUT_MS)
		{
			Print(string.Format("[MRX] Persistence system not usable (state %1)", typename.EnumToString(EPersistenceSystemState, state)), LogLevel.ERROR);
			m_CallQueue.PostStatus(m_InitCallback, MRX_ETxStatus.STORAGE_ERROR);
			return;
		}

		m_iWaitedMs += READY_POLL_MS;
		GetGame().GetCallqueue().CallLater(WaitForPersistence, READY_POLL_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void UseFallback(string reason)
	{
		Print(string.Format("[MRX] NATIVE storage unavailable (%1). Wallets and stashes are kept in memory only and are lost when the server stops. Select a systems config that uses the Marx persistence config.", reason), LogLevel.ERROR);
		m_Fallback = new MRX_InMemoryBackend();
		m_Fallback.Init(m_Rules, m_InitCallback);
	}
}

//------------------------------------------------------------------------------------------------
//! Loaded (or loading) wallet of one owner. Kept for the session.
class MRX_NativeWallet : Managed
{
	string m_sOwnerId;
	ref MRX_WalletState m_State;
	ref PersistenceResultCallback m_LoadCallback;
	bool m_bIdSet;
	bool m_bLoading;
	bool m_bLoaded;
	ref array<ref MRX_NativeOp> m_aWaiting = {};
}

//------------------------------------------------------------------------------------------------
//! Weak reference to a wallet, passed as persistence callback context. Internal.
class MRX_NativeWalletLink : Managed
{
	MRX_NativeWallet m_Wallet;

	//------------------------------------------------------------------------------------------------
	static MRX_NativeWalletLink Create(MRX_NativeWallet wallet)
	{
		MRX_NativeWalletLink link = new MRX_NativeWalletLink();
		link.m_Wallet = wallet;
		return link;
	}
}

//------------------------------------------------------------------------------------------------
//! Native backend operation: loads the wallets of its owners one after another, then runs. Internal.
class MRX_NativeOp : Managed
{
	//! Weak, the backend owns its ops.
	protected MRX_NativeBackend m_Backend;
	protected ref array<string> m_aOwnerIds = {};
	protected int m_iNextOwner;
	protected bool m_bDone;

	//------------------------------------------------------------------------------------------------
	bool IsDone()
	{
		return m_bDone;
	}

	//------------------------------------------------------------------------------------------------
	void Start()
	{
		LoadNextOwner();
	}

	//------------------------------------------------------------------------------------------------
	void OnWalletReady(MRX_ETxStatus status)
	{
		if (status != MRX_ETxStatus.OK)
		{
			Fail(status);
			return;
		}

		LoadNextOwner();
	}

	//------------------------------------------------------------------------------------------------
	//! Stash ops: the owner's stash is loaded (or failed to load).
	void OnStashReady(MRX_EStashStatus status)
	{
	}

	//------------------------------------------------------------------------------------------------
	//! Ops that requested a commit: the commit that contains them finished.
	void OnCommitFinished(bool success, bool timedOut)
	{
	}

	//------------------------------------------------------------------------------------------------
	protected void LoadNextOwner()
	{
		if (m_iNextOwner >= m_aOwnerIds.Count())
		{
			Run();
			return;
		}

		string ownerId = m_aOwnerIds[m_iNextOwner];
		m_iNextOwner++;
		m_Backend.LoadWalletState(ownerId, this);
	}

	//------------------------------------------------------------------------------------------------
	//! All wallets are loaded.
	protected void Run()
	{
	}

	//------------------------------------------------------------------------------------------------
	protected void Fail(MRX_ETxStatus status)
	{
	}
}

//------------------------------------------------------------------------------------------------
class MRX_NativeLoadWalletOp : MRX_NativeOp
{
	protected ref MRX_WalletCallback m_Callback;

	//------------------------------------------------------------------------------------------------
	void MRX_NativeLoadWalletOp(MRX_NativeBackend backend, string ownerId, MRX_WalletCallback callback)
	{
		m_Backend = backend;
		m_aOwnerIds.Insert(ownerId);
		m_Callback = callback;
	}

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		MRX_NativeWallet wallet = m_Backend.GetLoadedWallet(m_aOwnerIds[0]);
		m_Backend.GetCallQueue().PostWallet(m_Callback, MRX_ETxStatus.OK, wallet.m_State.m_Record.Copy());
		m_bDone = true;
	}

	//------------------------------------------------------------------------------------------------
	override protected void Fail(MRX_ETxStatus status)
	{
		m_Backend.GetCallQueue().PostWallet(m_Callback, status, null);
		m_bDone = true;
	}
}

//------------------------------------------------------------------------------------------------
class MRX_NativeHistoryOp : MRX_NativeOp
{
	protected string m_sCurrency;
	protected int m_iLimit;
	protected ref MRX_HistoryCallback m_Callback;

	//------------------------------------------------------------------------------------------------
	void MRX_NativeHistoryOp(MRX_NativeBackend backend, string ownerId, string currency, int limit, MRX_HistoryCallback callback)
	{
		m_Backend = backend;
		m_aOwnerIds.Insert(ownerId);
		m_sCurrency = currency;
		m_iLimit = limit;
		m_Callback = callback;
	}

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		array<ref MRX_LedgerEntry> entries = {};
		MRX_NativeWallet wallet = m_Backend.GetLoadedWallet(m_aOwnerIds[0]);
		MRX_WalletMath.CollectHistory(wallet.m_State.m_Record, m_sCurrency, m_iLimit, entries);
		m_Backend.GetCallQueue().PostHistory(m_Callback, MRX_ETxStatus.OK, entries);
		m_bDone = true;
	}

	//------------------------------------------------------------------------------------------------
	override protected void Fail(MRX_ETxStatus status)
	{
		m_Backend.GetCallQueue().PostHistory(m_Callback, status, new array<ref MRX_LedgerEntry>());
		m_bDone = true;
	}
}

//------------------------------------------------------------------------------------------------
//! Applies a transaction to the loaded wallets, saves them and waits for the storage commit.
//! Commit failure: the wallets are restored and saved again. Commit timeout: nothing is restored,
//! because the commit may still succeed; a retry with the same idempotency key then returns DUPLICATE.
class MRX_NativeApplyOp : MRX_NativeOp
{
	protected ref MRX_TxRequest m_Request;
	protected ref MRX_TxCallback m_Callback;
	protected ref MRX_TxResult m_Result;
	protected ref map<string, ref MRX_WalletRecord> m_mSnapshots = new map<string, ref MRX_WalletRecord>();

	//------------------------------------------------------------------------------------------------
	void MRX_NativeApplyOp(MRX_NativeBackend backend, MRX_TxRequest request, MRX_TxCallback callback)
	{
		m_Backend = backend;
		m_Request = request;
		m_Callback = callback;
		request.GetOwnerIds(m_aOwnerIds);
	}

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		map<string, MRX_WalletRecord> records = new map<string, MRX_WalletRecord>();
		foreach (string ownerId : m_aOwnerIds)
		{
			MRX_WalletRecord record = m_Backend.GetLoadedWallet(ownerId).m_State.m_Record;
			records.Insert(ownerId, record);
			m_mSnapshots.Insert(ownerId, record.Copy());
		}

		m_Result = MRX_WalletMath.Apply(records, m_Request, m_Backend.GetRules());
		if (m_Result.m_eStatus != MRX_ETxStatus.OK)
		{
			Finish();
			return;
		}

		PersistenceSystem persistence = PersistenceSystem.GetInstance();
		foreach (string savedOwnerId : m_aOwnerIds)
		{
			if (!persistence.Save(m_Backend.GetLoadedWallet(savedOwnerId).m_State))
			{
				Print(string.Format("[MRX] Could not save wallet %1", savedOwnerId), LogLevel.ERROR);
				RestoreSnapshots();
				m_Result = MRX_TxResult.Create(MRX_ETxStatus.STORAGE_ERROR, m_Request.m_sTxId);
				Finish();
				return;
			}
		}

		m_Backend.RequestCommit(this);
	}

	//------------------------------------------------------------------------------------------------
	override void OnCommitFinished(bool success, bool timedOut)
	{
		if (!success)
		{
			if (!timedOut)
				RestoreSnapshots();

			m_Result = MRX_TxResult.Create(MRX_ETxStatus.STORAGE_ERROR, m_Request.m_sTxId);
		}

		Finish();
	}

	//------------------------------------------------------------------------------------------------
	override protected void Fail(MRX_ETxStatus status)
	{
		m_Result = MRX_TxResult.Create(status, m_Request.m_sTxId);
		Finish();
	}

	//------------------------------------------------------------------------------------------------
	protected void RestoreSnapshots()
	{
		PersistenceSystem persistence = PersistenceSystem.GetInstance();
		foreach (string ownerId, MRX_WalletRecord snapshot : m_mSnapshots)
		{
			MRX_NativeWallet wallet = m_Backend.GetLoadedWallet(ownerId);
			wallet.m_State.m_Record = snapshot;
			persistence.Save(wallet.m_State);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void Finish()
	{
		m_Backend.GetCallQueue().PostTx(m_Callback, m_Result);
		m_bDone = true;
	}
}

//------------------------------------------------------------------------------------------------
//! Loaded (or loading) stash of one owner. Kept for the session.
class MRX_NativeStash : Managed
{
	string m_sOwnerId;
	ref MRX_StashState m_State;
	ref PersistenceResultCallback m_LoadCallback;
	bool m_bIdSet;
	bool m_bLoading;
	bool m_bLoaded;
	ref array<ref MRX_NativeOp> m_aWaiting = {};
}

//------------------------------------------------------------------------------------------------
//! Weak reference to a stash, passed as persistence callback context. Internal.
class MRX_NativeStashLink : Managed
{
	MRX_NativeStash m_Stash;

	//------------------------------------------------------------------------------------------------
	static MRX_NativeStashLink Create(MRX_NativeStash stash)
	{
		MRX_NativeStashLink link = new MRX_NativeStashLink();
		link.m_Stash = stash;
		return link;
	}
}

//------------------------------------------------------------------------------------------------
class MRX_NativeLoadStashOp : MRX_NativeOp
{
	protected string m_sOwnerId;
	protected ref MRX_StashCallback m_Callback;

	//------------------------------------------------------------------------------------------------
	void MRX_NativeLoadStashOp(MRX_NativeBackend backend, string ownerId, MRX_StashCallback callback)
	{
		m_Backend = backend;
		m_sOwnerId = ownerId;
		m_Callback = callback;
	}

	//------------------------------------------------------------------------------------------------
	override void Start()
	{
		m_Backend.LoadStashState(m_sOwnerId, this);
	}

	//------------------------------------------------------------------------------------------------
	override void OnStashReady(MRX_EStashStatus status)
	{
		MRX_StashRecord record;
		if (status == MRX_EStashStatus.OK)
			record = m_Backend.GetLoadedStash(m_sOwnerId).m_State.m_Record.Copy();

		MRX_StashDelivery.Post(m_Backend.GetCallQueue(), m_Callback, status, record);
		m_bDone = true;
	}
}

//------------------------------------------------------------------------------------------------
//! Applies a stash request to the loaded stash, saves it and waits for the storage commit.
//! Same failure rules as MRX_NativeApplyOp: a failed commit restores the stash, a timed-out one does not.
class MRX_NativeStashApplyOp : MRX_NativeOp
{
	protected ref MRX_StashRequest m_Request;
	protected ref MRX_StashResultCallback m_Callback;
	protected ref MRX_StashResult m_Result;
	protected ref MRX_StashRecord m_Snapshot;

	//------------------------------------------------------------------------------------------------
	void MRX_NativeStashApplyOp(MRX_NativeBackend backend, MRX_StashRequest request, MRX_StashResultCallback callback)
	{
		m_Backend = backend;
		m_Request = request;
		m_Callback = callback;
	}

	//------------------------------------------------------------------------------------------------
	override void Start()
	{
		m_Backend.LoadStashState(m_Request.m_sOwnerId, this);
	}

	//------------------------------------------------------------------------------------------------
	override void OnStashReady(MRX_EStashStatus status)
	{
		if (status != MRX_EStashStatus.OK)
		{
			FinishStash(MRX_StashResult.Create(status, m_Request.m_sRequestId));
			return;
		}

		MRX_NativeStash stash = m_Backend.GetLoadedStash(m_Request.m_sOwnerId);
		m_Snapshot = stash.m_State.m_Record.Copy();
		m_Result = MRX_StashMath.Apply(stash.m_State.m_Record, m_Request, m_Backend.GetRules());
		if (m_Result.m_eStatus != MRX_EStashStatus.OK)
		{
			FinishStash(m_Result);
			return;
		}

		if (!PersistenceSystem.GetInstance().Save(stash.m_State))
		{
			Print(string.Format("[MRX] Could not save stash %1", m_Request.m_sOwnerId), LogLevel.ERROR);
			RestoreSnapshot();
			FinishStash(MRX_StashResult.Create(MRX_EStashStatus.STORAGE_ERROR, m_Request.m_sRequestId));
			return;
		}

		m_Backend.RequestCommit(this);
	}

	//------------------------------------------------------------------------------------------------
	override void OnCommitFinished(bool success, bool timedOut)
	{
		if (success)
		{
			FinishStash(m_Result);
			return;
		}

		if (!timedOut)
			RestoreSnapshot();

		FinishStash(MRX_StashResult.Create(MRX_EStashStatus.STORAGE_ERROR, m_Request.m_sRequestId));
	}

	//------------------------------------------------------------------------------------------------
	protected void RestoreSnapshot()
	{
		MRX_NativeStash stash = m_Backend.GetLoadedStash(m_Request.m_sOwnerId);
		stash.m_State.m_Record = m_Snapshot;
		PersistenceSystem.GetInstance().Save(stash.m_State);
	}

	//------------------------------------------------------------------------------------------------
	protected void FinishStash(notnull MRX_StashResult result)
	{
		MRX_StashResultDelivery.Post(m_Backend.GetCallQueue(), m_Callback, result);
		m_bDone = true;
	}
}
