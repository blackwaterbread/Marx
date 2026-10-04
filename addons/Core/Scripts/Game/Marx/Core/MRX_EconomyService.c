enum MRX_EEconomyServiceState
{
	CREATED,
	STARTING,
	READY,
	FAILED
}

//! Watched owner balances.
class MRX_BalanceCache : Managed
{
	bool m_bLoaded;
	ref map<string, int> m_mBalances = new map<string, int>();
}

//! Server-side wallet operations (API v0). Every balance change goes through this service.
//! - Callbacks always run on a later frame.
//! - Operations on the same owner run one at a time, in call order. A transfer waits for both owners.
//! - Calls made before Init() completes wait in the queue.
class MRX_EconomyService : Managed
{
	protected ref MRX_StorageBackend m_Backend;
	protected ref MRX_WalletRules m_Rules;
	protected MRX_EEconomyServiceState m_eState;
	protected ref MRX_StatusCallback m_InitCallback;
	protected ref MRX_CallQueue m_CallQueue = new MRX_CallQueue();
	protected ref array<ref MRX_TxValidator> m_aValidators = {};

	protected ref array<ref MRX_EconomyOp> m_aQueuedOps = {};
	protected ref array<ref MRX_EconomyOp> m_aRunningOps = {};
	protected ref set<string> m_aLockedOwnerIds = new set<string>();
	protected bool m_bPumping;
	protected bool m_bPumpRequested;

	protected ref map<string, ref MRX_BalanceCache> m_mWatched = new map<string, ref MRX_BalanceCache>();

	protected ref ScriptInvokerBase<MRX_TransactionCommittedDelegate> m_OnTransactionCommitted;
	protected ref ScriptInvokerBase<MRX_BalanceChangedDelegate> m_OnBalanceChanged;

	//------------------------------------------------------------------------------------------------
	void MRX_EconomyService(notnull MRX_StorageBackend backend, notnull MRX_WalletRules rules)
	{
		m_Backend = backend;
		m_Rules = rules;
	}

	//------------------------------------------------------------------------------------------------
	//! Starts the backend. Queued calls run once it reports OK, and fail with STORAGE_ERROR otherwise.
	void Init(MRX_StatusCallback callback = null)
	{
		if (m_eState != MRX_EEconomyServiceState.CREATED)
		{
			Print("[MRX] Economy service initialized twice", LogLevel.WARNING);
			return;
		}

		m_eState = MRX_EEconomyServiceState.STARTING;
		m_InitCallback = callback;
		MRX_StatusCallback backendCallback = new MRX_StatusCallback();
		backendCallback.GetOnResult().Insert(OnBackendInit);
		m_Backend.Init(m_Rules, backendCallback);
	}

	//------------------------------------------------------------------------------------------------
	MRX_EEconomyServiceState GetState()
	{
		return m_eState;
	}

	//------------------------------------------------------------------------------------------------
	MRX_WalletRules GetRules()
	{
		return m_Rules;
	}

	//------------------------------------------------------------------------------------------------
	MRX_StorageBackend GetBackend()
	{
		return m_Backend;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds amount to the owner's balance. The owner does not need to be online.
	void Credit(string ownerId, string currency, int amount, MRX_TxContext context, MRX_TxCallback callback = null)
	{
		MRX_TxRequest request = CreateRequest(context);
		request.m_aPostings.Insert(MRX_Posting.Create(ownerId, currency, amount));
		Submit(request, amount, callback);
	}

	//------------------------------------------------------------------------------------------------
	//! Subtracts amount from the owner's balance. The owner does not need to be online.
	void Debit(string ownerId, string currency, int amount, MRX_TxContext context, MRX_TxCallback callback = null)
	{
		MRX_TxRequest request = CreateRequest(context);
		request.m_aPostings.Insert(MRX_Posting.Create(ownerId, currency, -amount));
		Submit(request, amount, callback);
	}

	//------------------------------------------------------------------------------------------------
	//! Moves amount between two owners. Both balances change or neither does.
	void Transfer(string fromOwnerId, string toOwnerId, string currency, int amount, MRX_TxContext context, MRX_TxCallback callback = null)
	{
		MRX_TxRequest request = CreateRequest(context);
		request.m_aPostings.Insert(MRX_Posting.Create(fromOwnerId, currency, -amount));
		request.m_aPostings.Insert(MRX_Posting.Create(toOwnerId, currency, amount));
		Submit(request, amount, callback);
	}

	//------------------------------------------------------------------------------------------------
	void GetBalance(string ownerId, string currency, MRX_BalanceCallback callback)
	{
		SubmitOp(new MRX_BalanceOp(this, ownerId, currency, callback), ValidateRead(ownerId, currency, false));
	}

	//------------------------------------------------------------------------------------------------
	//! Recent ledger entries, newest first. Local backends only keep the last MRX_WalletRules.m_iMaxRecentEntries.
	//! \param currency Empty for all currencies.
	//! \param limit 0 or less for all available entries.
	void GetHistory(string ownerId, string currency, int limit, MRX_HistoryCallback callback)
	{
		SubmitOp(new MRX_HistoryOp(this, ownerId, currency, limit, callback), ValidateRead(ownerId, currency, true));
	}

	//------------------------------------------------------------------------------------------------
	//! Keeps the owner's balances cached (e.g. while the player is online). See TryGetCachedBalance().
	void WatchOwner(string ownerId)
	{
		if (ownerId.IsEmpty() || m_mWatched.Contains(ownerId))
			return;

		m_mWatched.Insert(ownerId, new MRX_BalanceCache());
		Enqueue(new MRX_WatchOp(this, ownerId));
	}

	//------------------------------------------------------------------------------------------------
	void UnwatchOwner(string ownerId)
	{
		m_mWatched.Remove(ownerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Synchronous read for UI and condition checks. Server-side truth is still the backend.
	//! \return False when the owner is not watched, not loaded yet, or the currency is unknown.
	bool TryGetCachedBalance(string ownerId, string currency, out int balance)
	{
		MRX_BalanceCache cache = m_mWatched.Get(ownerId);
		if (!cache || !cache.m_bLoaded)
			return false;

		return cache.m_mBalances.Find(currency, balance);
	}

	//------------------------------------------------------------------------------------------------
	void AddValidator(notnull MRX_TxValidator validator)
	{
		if (!m_aValidators.Contains(validator))
			m_aValidators.Insert(validator);
	}

	//------------------------------------------------------------------------------------------------
	void RemoveValidator(MRX_TxValidator validator)
	{
		m_aValidators.RemoveItem(validator);
	}

	//------------------------------------------------------------------------------------------------
	//! Invoked once per ledger entry of every committed transaction (not for DUPLICATE).
	ScriptInvokerBase<MRX_TransactionCommittedDelegate> GetOnTransactionCommitted()
	{
		if (!m_OnTransactionCommitted)
			m_OnTransactionCommitted = new ScriptInvokerBase<MRX_TransactionCommittedDelegate>();

		return m_OnTransactionCommitted;
	}

	//------------------------------------------------------------------------------------------------
	//! Invoked after a committed change, and for every currency when a watched owner is first loaded.
	ScriptInvokerBase<MRX_BalanceChangedDelegate> GetOnBalanceChanged()
	{
		if (!m_OnBalanceChanged)
			m_OnBalanceChanged = new ScriptInvokerBase<MRX_BalanceChangedDelegate>();

		return m_OnBalanceChanged;
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by MRX_EconomyOp when its backend call returns.
	void OnOpFinished(notnull MRX_EconomyOp op)
	{
		foreach (string ownerId : op.m_aOwnerIds)
		{
			m_aLockedOwnerIds.RemoveItem(ownerId);
		}

		Pump();
		op.Complete();
		m_aRunningOps.RemoveItem(op);
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by MRX_ApplyOp before its callback.
	void OnTransactionCompleted(notnull MRX_TxResult result)
	{
		if (result.m_eStatus != MRX_ETxStatus.OK)
			return;

		foreach (MRX_LedgerEntry entry : result.m_aEntries)
		{
			MRX_BalanceCache cache = m_mWatched.Get(entry.m_sOwnerId);
			if (cache && cache.m_bLoaded)
				cache.m_mBalances.Set(entry.m_sCurrency, entry.m_iBalanceAfter);

			if (m_OnTransactionCommitted)
				m_OnTransactionCommitted.Invoke(entry);

			if (m_OnBalanceChanged)
				m_OnBalanceChanged.Invoke(entry.m_sOwnerId, entry.m_sCurrency, entry.m_iBalanceAfter);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by MRX_WatchOp.
	void OnWatchLoaded(string ownerId, MRX_ETxStatus status, MRX_WalletRecord record)
	{
		MRX_BalanceCache cache = m_mWatched.Get(ownerId);
		if (!cache)
			return;

		if (status != MRX_ETxStatus.OK || !record)
		{
			Print(string.Format("[MRX] Could not load wallet of %1: %2", ownerId, typename.EnumToString(MRX_ETxStatus, status)), LogLevel.ERROR);
			m_mWatched.Remove(ownerId);
			return;
		}

		array<string> currencyIds = {};
		m_Rules.m_Currencies.GetIds(currencyIds);
		foreach (string currencyId : currencyIds)
		{
			cache.m_mBalances.Set(currencyId, MRX_WalletMath.GetBalance(record, m_Rules.m_Currencies.Find(currencyId)));
		}

		cache.m_bLoaded = true;

		if (!m_OnBalanceChanged)
			return;

		foreach (string changedId, int balance : cache.m_mBalances)
		{
			m_OnBalanceChanged.Invoke(ownerId, changedId, balance);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBackendInit(MRX_ETxStatus status)
	{
		if (status == MRX_ETxStatus.OK)
		{
			m_eState = MRX_EEconomyServiceState.READY;
			Print(string.Format("[MRX] Economy service ready, backend %1", m_Backend.GetName()), LogLevel.NORMAL);
			Pump();
		}
		else
		{
			m_eState = MRX_EEconomyServiceState.FAILED;
			Print(string.Format("[MRX] Storage backend %1 failed to start: %2", m_Backend.GetName(), typename.EnumToString(MRX_ETxStatus, status)), LogLevel.ERROR);
			FailQueuedOps(MRX_ETxStatus.STORAGE_ERROR);
		}

		if (m_InitCallback)
			m_InitCallback.OnResult(status);
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_TxRequest CreateRequest(MRX_TxContext context)
	{
		MRX_TxRequest request = new MRX_TxRequest();
		UUID txId = PersistenceIdUtils.Generate();
		if (txId.IsNull())
			txId = UUID.GenV4();

		request.m_sTxId = txId;
		request.m_iTimestamp = System.GetUnixTime();
		if (context)
			request.m_Context = context.Copy();

		return request;
	}

	//------------------------------------------------------------------------------------------------
	protected void Submit(notnull MRX_TxRequest request, int amount, MRX_TxCallback callback)
	{
		SubmitOp(new MRX_ApplyOp(this, request, callback), ValidateRequest(request, amount));
	}

	//------------------------------------------------------------------------------------------------
	//! Queues the op, or completes it with the validation failure on a later frame.
	protected void SubmitOp(notnull MRX_EconomyOp op, MRX_ETxStatus status)
	{
		if (status != MRX_ETxStatus.OK)
		{
			op.Fail(status);
			m_CallQueue.Post(new MRX_DeferredOpCompletion(op));
			return;
		}

		Enqueue(op);
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_ETxStatus ValidateRequest(notnull MRX_TxRequest request, int amount)
	{
		if (m_eState == MRX_EEconomyServiceState.FAILED)
			return MRX_ETxStatus.STORAGE_ERROR;

		foreach (MRX_Posting ownerPosting : request.m_aPostings)
		{
			if (ownerPosting.m_sOwnerId.IsEmpty())
				return MRX_ETxStatus.OWNER_NOT_READY;
		}

		array<string> ownerIds = {};
		if (request.GetOwnerIds(ownerIds) < request.m_aPostings.Count())
			return MRX_ETxStatus.INVALID_OWNER;

		if (!request.m_Context || !request.m_Context.IsValid())
			return MRX_ETxStatus.INVALID_CONTEXT;

		foreach (MRX_Posting currencyPosting : request.m_aPostings)
		{
			if (!m_Rules.m_Currencies.Find(currencyPosting.m_sCurrency))
				return MRX_ETxStatus.UNKNOWN_CURRENCY;
		}

		if (amount <= 0)
			return MRX_ETxStatus.INVALID_AMOUNT;

		foreach (MRX_TxValidator validator : m_aValidators)
		{
			if (!validator.Validate(request))
				return MRX_ETxStatus.REJECTED;
		}

		return MRX_ETxStatus.OK;
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_ETxStatus ValidateRead(string ownerId, string currency, bool allowAnyCurrency)
	{
		if (m_eState == MRX_EEconomyServiceState.FAILED)
			return MRX_ETxStatus.STORAGE_ERROR;

		if (ownerId.IsEmpty())
			return MRX_ETxStatus.OWNER_NOT_READY;

		if (allowAnyCurrency && currency.IsEmpty())
			return MRX_ETxStatus.OK;

		if (!m_Rules.m_Currencies.Find(currency))
			return MRX_ETxStatus.UNKNOWN_CURRENCY;

		return MRX_ETxStatus.OK;
	}

	//------------------------------------------------------------------------------------------------
	protected void Enqueue(notnull MRX_EconomyOp op)
	{
		if (m_eState == MRX_EEconomyServiceState.FAILED)
		{
			op.Fail(MRX_ETxStatus.STORAGE_ERROR);
			m_CallQueue.Post(new MRX_DeferredOpCompletion(op));
			return;
		}

		m_aQueuedOps.Insert(op);
		Pump();
	}

	//------------------------------------------------------------------------------------------------
	//! Starts every queued op whose owners are free. An op never overtakes an earlier queued op on the same owner.
	protected void Pump()
	{
		if (m_eState != MRX_EEconomyServiceState.READY)
			return;

		if (m_bPumping)
		{
			m_bPumpRequested = true;
			return;
		}

		m_bPumping = true;
		m_bPumpRequested = true;
		while (m_bPumpRequested)
		{
			m_bPumpRequested = false;
			set<string> blockedOwnerIds = new set<string>();
			int index = 0;
			while (index < m_aQueuedOps.Count())
			{
				MRX_EconomyOp op = m_aQueuedOps[index];
				if (IsBlocked(op, blockedOwnerIds))
				{
					foreach (string blockedId : op.m_aOwnerIds)
					{
						blockedOwnerIds.Insert(blockedId);
					}

					index++;
					continue;
				}

				foreach (string lockedId : op.m_aOwnerIds)
				{
					m_aLockedOwnerIds.Insert(lockedId);
				}

				m_aRunningOps.Insert(op);
				m_aQueuedOps.RemoveOrdered(index);
				op.Execute(m_Backend);
			}
		}

		m_bPumping = false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsBlocked(notnull MRX_EconomyOp op, notnull set<string> blockedOwnerIds)
	{
		foreach (string ownerId : op.m_aOwnerIds)
		{
			if (m_aLockedOwnerIds.Contains(ownerId) || blockedOwnerIds.Contains(ownerId))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void FailQueuedOps(MRX_ETxStatus status)
	{
		array<ref MRX_EconomyOp> ops = m_aQueuedOps;
		m_aQueuedOps = {};
		foreach (MRX_EconomyOp op : ops)
		{
			op.Fail(status);
			op.Complete();
		}
	}
}
