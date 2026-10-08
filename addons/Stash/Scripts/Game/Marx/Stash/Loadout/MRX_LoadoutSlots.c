//! Loadout slots of an owner (API v0): MRX_Settings.m_iLoadoutSlots unlocked for everyone plus extra slots, e.g. bought
//! in a shop, up to GetMaxSlots() (MRX_Settings.m_iMaxLoadoutSlots), which the loadout window shows, the locked ones
//! greyed out. The extra slots are kept as a stash property. Server.
class MRX_LoadoutSlots
{
	static const string PROPERTY = "marx.loadout.extraSlots";
	//! Most slots any setting allows.
	static const int MAX_SLOTS = 10;

	//------------------------------------------------------------------------------------------------
	//! \return Slots every owner has unlocked (0 = loadouts are off).
	static int GetBaseSlots()
	{
		MRX_MarxSystem system = MRX_MarxSystem.GetInstance();
		if (!system || !system.GetSettings())
			return 0;

		return Math.Clamp(system.GetSettings().m_iLoadoutSlots, 0, MAX_SLOTS);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Slots an owner can have unlocked, which the loadout window shows (0 = loadouts are off).
	static int GetMaxSlots()
	{
		int baseSlots = GetBaseSlots();
		MRX_MarxSystem system = MRX_MarxSystem.GetInstance();
		if (baseSlots <= 0 || !system || !system.GetSettings())
			return 0;

		return Math.Clamp(system.GetSettings().m_iMaxLoadoutSlots, baseSlots, MAX_SLOTS);
	}

	//------------------------------------------------------------------------------------------------
	static int GetExtraSlots(MRX_StashRecord record)
	{
		if (!record)
			return 0;

		string value = record.GetProperty(PROPERTY);
		if (value.IsEmpty())
			return 0;

		return Math.Max(0, value.ToInt());
	}

	//------------------------------------------------------------------------------------------------
	//! \return Unlocked slots of the owner (0 = loadouts are off).
	static int GetSlots(MRX_StashRecord record)
	{
		int baseSlots = GetBaseSlots();
		if (baseSlots <= 0)
			return 0;

		return Math.Min(GetMaxSlots(), baseSlots + GetExtraSlots(record));
	}

	//------------------------------------------------------------------------------------------------
	//! Unlocks slots, up to GetMaxSlots() in all. The callback gets OK, the limit (the owner already has the most slots or
	//! would get more) or the stash status of a failure.
	static void AddSlots(string ownerId, int count, notnull MRX_TxContext context, notnull MRX_LoadoutSlotsCallback callback)
	{
		MRX_LoadoutSlotsChange change = new MRX_LoadoutSlotsChange(ownerId, count, GetMaxSlots(), context, callback);
		change.Start();
	}
}

//------------------------------------------------------------------------------------------------
//! Result of MRX_LoadoutSlots.AddSlots (API v0). Override OnResult.
class MRX_LoadoutSlotsCallback : Managed
{
	//------------------------------------------------------------------------------------------------
	//! \param limitReached True when the slots were not added because of GetMaxSlots() (or when loadouts are off).
	//! \param slots Slots after the change (or now, on failure).
	void OnResult(MRX_EStashStatus status, bool limitReached, int slots)
	{
	}
}

//------------------------------------------------------------------------------------------------
//! Reads the extra slots, then sets them only if they did not change meanwhile. Receives the stash listing itself, so
//! the stash service keeps it alive until it answers. Internal.
class MRX_LoadoutSlotsChange : MRX_StashCallback
{
	protected string m_sOwnerId;
	protected int m_iCount;
	protected int m_iMaxSlots;
	protected ref MRX_TxContext m_Context;
	protected ref MRX_LoadoutSlotsCallback m_Callback;
	protected int m_iSlots;

	//------------------------------------------------------------------------------------------------
	void MRX_LoadoutSlotsChange(string ownerId, int count, int maxSlots, MRX_TxContext context, MRX_LoadoutSlotsCallback callback)
	{
		m_sOwnerId = ownerId;
		m_iCount = count;
		m_iMaxSlots = maxSlots;
		m_Context = context;
		m_Callback = callback;
	}

	//------------------------------------------------------------------------------------------------
	void Start()
	{
		MRX_StashService stash = MRX_Marx.GetStash();
		if (!stash)
		{
			m_Callback.OnResult(MRX_EStashStatus.STORAGE_ERROR, false, 0);
			return;
		}

		stash.List(m_sOwnerId, this);
	}

	//------------------------------------------------------------------------------------------------
	override void OnResult(MRX_EStashStatus status, MRX_StashRecord record)
	{
		super.OnResult(status, record);
		if (status != MRX_EStashStatus.OK || !record)
		{
			m_Callback.OnResult(status, false, 0);
			return;
		}

		int extra = MRX_LoadoutSlots.GetExtraSlots(record);
		m_iSlots = MRX_LoadoutSlots.GetSlots(record);
		if (m_iCount <= 0 || m_iSlots <= 0 || m_iSlots + m_iCount > m_iMaxSlots)
		{
			m_Callback.OnResult(MRX_EStashStatus.OK, true, m_iSlots);
			return;
		}

		string current = record.GetProperty(MRX_LoadoutSlots.PROPERTY);
		MRX_PropertyChange change = MRX_PropertyChange.Create(MRX_LoadoutSlots.PROPERTY, (extra + m_iCount).ToString()).Expecting(current);
		MRX_Marx.GetStash().SetProperty(m_sOwnerId, change, m_Context, new MRX_LoadoutSlotsChangeReply(this));
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by MRX_LoadoutSlotsChangeReply.
	void OnChanged(MRX_StashResult result)
	{
		if (result.m_eStatus != MRX_EStashStatus.OK)
		{
			m_Callback.OnResult(result.m_eStatus, false, m_iSlots);
			return;
		}

		m_Callback.OnResult(MRX_EStashStatus.OK, false, m_iSlots + m_iCount);
	}
}

//------------------------------------------------------------------------------------------------
//! Keeps the change alive until the stash service answers. Internal.
class MRX_LoadoutSlotsChangeReply : MRX_StashResultCallback
{
	protected ref MRX_LoadoutSlotsChange m_Change;

	//------------------------------------------------------------------------------------------------
	void MRX_LoadoutSlotsChangeReply(MRX_LoadoutSlotsChange change)
	{
		m_Change = change;
	}

	//------------------------------------------------------------------------------------------------
	override void OnResult(MRX_StashResult result)
	{
		super.OnResult(result);
		m_Change.OnChanged(result);
	}
}
