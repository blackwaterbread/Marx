//! Debug actions of the "Marx" page: the player, the character and the Marx services.

//------------------------------------------------------------------------------------------------
//! Player and owner ID, identity, storage backend and economy state.
class MRX_DebugMarxInfo : MRX_DebugAction
{
	//------------------------------------------------------------------------------------------------
	void MRX_DebugMarxInfo()
	{
		Setup("marx.info", "Marx", "Info");
	}

	//------------------------------------------------------------------------------------------------
	override bool HasServerPart()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void RunServer(notnull MRX_DebugContext context, notnull MRX_DebugReply reply)
	{
		MRX_MarxSystem system = MRX_MarxSystem.GetInstance();
		if (!system || !system.GetEconomy() || !system.GetIdentity())
		{
			reply.Done(MRX_DebugResult.Failed("The Marx system is not running (systems config?)"));
			return;
		}

		int playerId = context.m_iPlayerId;
		string ownerId = system.GetIdentity().GetOwnerId(playerId);
		string owner = ownerId;
		if (ownerId.IsEmpty())
			owner = "(none)";

		string note = MRX_DebugRegistry.Get().DescribeOwner(playerId, ownerId);
		if (!note.IsEmpty())
			owner += " (" + note + ")";

		string identity = "resolved";
		if (system.GetIdentity().IsIdentityMissing(playerId))
			identity = "missing (no backend identity)";
		else if (ownerId.IsEmpty())
			identity = "not resolved yet";

		string backend = typename.EnumToString(MRX_EBackendType, system.GetSettings().m_eBackend);
		string economy = typename.EnumToString(MRX_EEconomyServiceState, system.GetEconomy().GetState());
		reply.Done(MRX_DebugResult.Ok(string.Format("player %1, owner %2, identity %3, backend %4, economy %5, stash %6", playerId, owner, identity, backend, economy, system.GetStash() != null)));
	}
}
