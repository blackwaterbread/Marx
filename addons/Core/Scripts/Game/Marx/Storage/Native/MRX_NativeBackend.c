//! Wallets as per-owner scripted states in the "MarxWallets" collection, committed with CommitStorage(GamemodeStorage)
//! after every transaction. Needs a persistence config with that collection and MRX_WalletStateSerializer
//! (Marx_Core ships reference configs). Without it, the backend falls back to in-memory storage.
class MRX_NativeBackend : MRX_StorageBackend
{
	//! Save data names allow only letters, digits, hyphen and dot (no underscore).
	static const string COLLECTION_NAME = "MarxWallets";
	static const string ID_PREFIX = "marx:wallet:";
	protected static const int READY_POLL_MS = 250;
	protected static const int READY_TIMEOUT_MS = 60000;
	protected static const int COMMIT_TIMEOUT_MS = 10000;

	protected ref MRX_WalletRules m_Rules;
	protected ref MRX_StatusCallback m_InitCallback;
	protected int m_iWaitedMs;
	//! Set when the persistence config has no Marx wallet collection.
	protected ref MRX_InMemoryBackend m_Fallback;

	protected ref map<string, ref MRX_NativeWallet> m_mWallets = new map<string, ref MRX_NativeWallet>();
	protected ref array<ref MRX_NativeOp> m_aOps = {};

	protected ref array<ref MRX_NativeApplyOp> m_aCommitQueue = {};
	protected ref array<ref MRX_NativeApplyOp> m_aCommitting = {};
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
	override void Init(notnull MRX_WalletRules rules, notnull MRX_StatusCallback callback)
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
	MRX_WalletRules GetRules()
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
		wallet.m_LoadCallback = new PersistenceResultCallback(OnWalletStateLoaded, wallet);
		persistence.RequestLoad(request, wallet.m_LoadCallback);
	}

	//------------------------------------------------------------------------------------------------
	//! Internal: commits the staged wallet states of the op, batched with other ops waiting for a commit.
	void RequestCommit(notnull MRX_NativeApplyOp op)
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
		array<ref MRX_NativeApplyOp> committed = m_aCommitting;
		m_aCommitting = {};
		foreach (MRX_NativeApplyOp op : committed)
		{
			op.OnCommitFinished(success, timedOut);
		}

		if (!m_aCommitQueue.IsEmpty())
			StartCommit();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnWalletStateLoaded(EPersistenceStatusCode statusCode, Managed result, bool isLast, Managed context)
	{
		MRX_NativeWallet wallet = MRX_NativeWallet.Cast(context);
		if (!wallet)
			return;

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
		Print(string.Format("[MRX] NATIVE storage unavailable (%1). Wallets are kept in memory only and are lost when the server stops. Select a systems config that uses the Marx persistence config.", reason), LogLevel.ERROR);
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
	//! Internal, called by the backend when the commit that contains this op finished.
	void OnCommitFinished(bool success, bool timedOut)
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
