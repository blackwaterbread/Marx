//! Adds the stash container sessions to the Marx system (server).
modded class MRX_MarxSystem
{
	protected ref MRX_StashSessionManager m_MRX_StashSessions;

	//------------------------------------------------------------------------------------------------
	override event protected void OnInit()
	{
		super.OnInit();
		if (GetStash())
			m_MRX_StashSessions = new MRX_StashSessionManager();
	}

	//------------------------------------------------------------------------------------------------
	override event protected void OnCleanup()
	{
		m_MRX_StashSessions = null;
		super.OnCleanup();
	}

	//------------------------------------------------------------------------------------------------
	MRX_StashSessionManager MRX_GetStashSessions()
	{
		return m_MRX_StashSessions;
	}
}

//! Static access to the stash container sessions (API v0). Server only; null on clients.
class MRX_StashSessions
{
	//------------------------------------------------------------------------------------------------
	static MRX_StashSessionManager Get()
	{
		MRX_MarxSystem system = MRX_MarxSystem.GetInstance();
		if (!system)
			return null;

		return system.MRX_GetStashSessions();
	}
}
