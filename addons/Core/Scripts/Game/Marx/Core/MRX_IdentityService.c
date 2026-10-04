void MRX_OwnerDelegate(int playerId, string ownerId);
typedef func MRX_OwnerDelegate;

//! Maps connected players to owner IDs (Bohemia identity UUIDs). Server only (API v0).
//! The only place in Marx that reads player identities, and only after OnPlayerAuditSuccess.
class MRX_IdentityService : Managed
{
	//! Prefix of the ID that SCR_PlayerIdentityUtils derives from the player name when no backend identity exists (PeerTool, listen servers).
	static const string NAME_DERIVED_ID_PREFIX = "00bbbddd-";

	protected ref map<int, string> m_mOwnerIds = new map<int, string>();
	protected ref map<string, int> m_mPlayerIds = new map<string, int>();
	protected ref set<int> m_aPlayersWithoutIdentity = new set<int>();
	protected ref ScriptInvokerBase<MRX_OwnerDelegate> m_OnOwnerReady;
	protected ref ScriptInvokerBase<MRX_OwnerDelegate> m_OnOwnerLeft;

	//------------------------------------------------------------------------------------------------
	void Attach(notnull SCR_BaseGameMode gameMode)
	{
		gameMode.GetOnPlayerAuditSuccess().Insert(OnPlayerAuditSuccess);
		gameMode.GetOnPlayerDisconnected().Insert(OnPlayerDisconnected);

		array<int> auditedPlayerIds = {};
		gameMode.MRX_GetAuditedPlayerIds(auditedPlayerIds);
		RegisterAuditedPlayers(auditedPlayerIds);
	}

	//------------------------------------------------------------------------------------------------
	void Detach(notnull SCR_BaseGameMode gameMode)
	{
		gameMode.GetOnPlayerAuditSuccess().Remove(OnPlayerAuditSuccess);
		gameMode.GetOnPlayerDisconnected().Remove(OnPlayerDisconnected);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Empty until the player's identity is resolved.
	string GetOwnerId(int playerId)
	{
		return m_mOwnerIds.Get(playerId);
	}

	//------------------------------------------------------------------------------------------------
	bool IsOwnerReady(int playerId)
	{
		return m_mOwnerIds.Contains(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! True when the player passed the audit but no identity was available (e.g. the backend was unreachable).
	bool IsIdentityMissing(int playerId)
	{
		return m_aPlayersWithoutIdentity.Contains(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! \return 0 when the owner is not connected.
	int GetPlayerId(string ownerId)
	{
		return m_mPlayerIds.Get(ownerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Invoked when a connected player's owner ID becomes known.
	ScriptInvokerBase<MRX_OwnerDelegate> GetOnOwnerReady()
	{
		if (!m_OnOwnerReady)
			m_OnOwnerReady = new ScriptInvokerBase<MRX_OwnerDelegate>();

		return m_OnOwnerReady;
	}

	//------------------------------------------------------------------------------------------------
	//! Invoked when a player with a known owner ID disconnects.
	ScriptInvokerBase<MRX_OwnerDelegate> GetOnOwnerLeft()
	{
		if (!m_OnOwnerLeft)
			m_OnOwnerLeft = new ScriptInvokerBase<MRX_OwnerDelegate>();

		return m_OnOwnerLeft;
	}

	//------------------------------------------------------------------------------------------------
	//! Picks up players audited before Attach() (the audit event already fired for them).
	protected void RegisterAuditedPlayers(notnull array<int> playerIds)
	{
		foreach (int playerId : playerIds)
		{
			OnPlayerAuditSuccess(playerId);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPlayerAuditSuccess(int playerId)
	{
		RegisterOwner(playerId, ResolveIdentity(playerId));
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		m_aPlayersWithoutIdentity.RemoveItem(playerId);
		UnregisterOwner(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Empty when the player has no identity.
	protected string ResolveIdentity(int playerId)
	{
		UUID identity = SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId);
		if (identity.IsNull())
			return string.Empty;

		return identity;
	}

	//------------------------------------------------------------------------------------------------
	protected void RegisterOwner(int playerId, string ownerId)
	{
		if (ownerId.IsEmpty())
		{
			Print(string.Format("[MRX] Player %1 has no identity, economy features stay disabled for this player", playerId), LogLevel.WARNING);
			m_aPlayersWithoutIdentity.Insert(playerId);
			return;
		}

		m_aPlayersWithoutIdentity.RemoveItem(playerId);

		string currentOwnerId = m_mOwnerIds.Get(playerId);
		if (currentOwnerId == ownerId)
			return;

		if (!currentOwnerId.IsEmpty())
			UnregisterOwner(playerId);

		int previousPlayerId = m_mPlayerIds.Get(ownerId);
		if (previousPlayerId != 0)
		{
			Print(string.Format("[MRX] Owner %1 moved from player %2 to player %3", ownerId, previousPlayerId, playerId), LogLevel.WARNING);
			UnregisterOwner(previousPlayerId);
		}

		if (ownerId.StartsWith(NAME_DERIVED_ID_PREFIX))
			Print(string.Format("[MRX] Owner ID of player %1 is derived from the player name (no backend identity). Do not rely on it outside of testing.", playerId), LogLevel.WARNING);

		m_mOwnerIds.Set(playerId, ownerId);
		m_mPlayerIds.Set(ownerId, playerId);

		if (m_OnOwnerReady)
			m_OnOwnerReady.Invoke(playerId, ownerId);
	}

	//------------------------------------------------------------------------------------------------
	protected void UnregisterOwner(int playerId)
	{
		string ownerId = m_mOwnerIds.Get(playerId);
		if (ownerId.IsEmpty())
			return;

		m_mOwnerIds.Remove(playerId);
		if (m_mPlayerIds.Get(ownerId) == playerId)
			m_mPlayerIds.Remove(ownerId);

		if (m_OnOwnerLeft)
			m_OnOwnerLeft.Invoke(playerId, ownerId);
	}
}
