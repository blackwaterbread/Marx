//! Extra stash pages of an owner (API v0), e.g. bought in a shop: the stash panel shows the container's own pages
//! (MRX_StashStorageComponent) plus these. Kept as a stash property. Server.
class MRX_StashPages
{
	static const string PROPERTY = "marx.stash.extraPages";

	protected static int s_iBasePages = -1;

	//------------------------------------------------------------------------------------------------
	//! \return Pages of the stash container itself (0 = no limit).
	static int GetBasePages()
	{
		if (s_iBasePages >= 0)
			return s_iBasePages;

		s_iBasePages = 0;
		IEntityComponentSource storage = SCR_BaseContainerTools.FindComponentSource(Resource.Load(MRX_StashSessionManager.CONTAINER_PREFAB), MRX_StashStorageComponent);
		if (storage)
			storage.Get("m_iMaxPages", s_iBasePages);

		return s_iBasePages;
	}

	//------------------------------------------------------------------------------------------------
	static int GetExtraPages(MRX_StashRecord record)
	{
		if (!record)
			return 0;

		string value = record.GetProperty(PROPERTY);
		if (value.IsEmpty())
			return 0;

		return Math.Max(0, value.ToInt());
	}

	//------------------------------------------------------------------------------------------------
	//! \return Pages the owner's stash panel shows (0 = no limit).
	static int GetPages(MRX_StashRecord record)
	{
		int basePages = GetBasePages();
		if (basePages <= 0)
			return 0;

		return basePages + GetExtraPages(record);
	}

	//------------------------------------------------------------------------------------------------
	//! Adds pages, up to maxPages pages in all (base pages included). The callback gets OK, LIMIT (the stash already has
	//! maxPages or would get more) or the stash status of a failure.
	static void AddPages(string ownerId, int count, int maxPages, notnull MRX_TxContext context, notnull MRX_StashPagesCallback callback)
	{
		MRX_StashPagesChange change = new MRX_StashPagesChange(ownerId, count, maxPages, context, callback);
		change.Start();
	}
}

//------------------------------------------------------------------------------------------------
//! Result of MRX_StashPages.AddPages (API v0). Override OnResult.
class MRX_StashPagesCallback : Managed
{
	//------------------------------------------------------------------------------------------------
	//! \param limitReached True when the pages were not added because of maxPages.
	//! \param pages Pages after the change (or now, on failure).
	void OnResult(MRX_EStashStatus status, bool limitReached, int pages)
	{
	}
}

//------------------------------------------------------------------------------------------------
//! Reads the extra pages, then sets them only if they did not change meanwhile. Receives the stash listing itself, so the
//! stash service keeps it alive until it answers. Internal.
class MRX_StashPagesChange : MRX_StashCallback
{
	protected string m_sOwnerId;
	protected int m_iCount;
	protected int m_iMaxPages;
	protected ref MRX_TxContext m_Context;
	protected ref MRX_StashPagesCallback m_Callback;
	protected int m_iPages;

	//------------------------------------------------------------------------------------------------
	void MRX_StashPagesChange(string ownerId, int count, int maxPages, MRX_TxContext context, MRX_StashPagesCallback callback)
	{
		m_sOwnerId = ownerId;
		m_iCount = count;
		m_iMaxPages = maxPages;
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

		int extra = MRX_StashPages.GetExtraPages(record);
		m_iPages = MRX_StashPages.GetBasePages() + extra;
		if (m_iCount <= 0 || m_iPages + m_iCount > m_iMaxPages)
		{
			m_Callback.OnResult(MRX_EStashStatus.OK, true, m_iPages);
			return;
		}

		string current = record.GetProperty(MRX_StashPages.PROPERTY);
		MRX_PropertyChange change = MRX_PropertyChange.Create(MRX_StashPages.PROPERTY, (extra + m_iCount).ToString()).Expecting(current);
		MRX_Marx.GetStash().SetProperty(m_sOwnerId, change, m_Context, new MRX_StashPagesChangeReply(this));
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by MRX_StashPagesChangeReply.
	void OnChanged(MRX_StashResult result)
	{
		if (result.m_eStatus != MRX_EStashStatus.OK)
		{
			m_Callback.OnResult(result.m_eStatus, false, m_iPages);
			return;
		}

		m_Callback.OnResult(MRX_EStashStatus.OK, false, m_iPages + m_iCount);
	}
}

//------------------------------------------------------------------------------------------------
//! Keeps the change alive until the stash service answers. Internal.
class MRX_StashPagesChangeReply : MRX_StashResultCallback
{
	protected ref MRX_StashPagesChange m_Change;

	//------------------------------------------------------------------------------------------------
	void MRX_StashPagesChangeReply(MRX_StashPagesChange change)
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
