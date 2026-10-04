//! Remembers audited players, so Marx can pick up players that were audited before it started
//! (e.g. the host of a listen server).
modded class SCR_BaseGameMode
{
	protected ref set<int> m_MRX_aAuditedPlayerIds = new set<int>();

	//------------------------------------------------------------------------------------------------
	override void OnPlayerAuditSuccess(int iPlayerID)
	{
		m_MRX_aAuditedPlayerIds.Insert(iPlayerID);
		super.OnPlayerAuditSuccess(iPlayerID);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		m_MRX_aAuditedPlayerIds.RemoveItem(playerId);
		super.OnPlayerDisconnected(playerId, cause, timeout);
	}

	//------------------------------------------------------------------------------------------------
	//! Connected players whose audit succeeded.
	int MRX_GetAuditedPlayerIds(notnull array<int> outPlayerIds)
	{
		outPlayerIds.Clear();
		foreach (int playerId : m_MRX_aAuditedPlayerIds)
		{
			outPlayerIds.Insert(playerId);
		}

		return outPlayerIds.Count();
	}
}
