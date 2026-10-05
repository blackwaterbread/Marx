#ifdef WORKBENCH
//! Test sessions only (-mrxTestIdentity): a player without a backend identity gets a stable owner ID derived from
//! the player name, so the economy tests also run while the Bohemia backend is unreachable.
modded class MRX_IdentityService
{
	//------------------------------------------------------------------------------------------------
	override protected string ResolveIdentity(int playerId)
	{
		string ownerId = super.ResolveIdentity(playerId);
		if (!ownerId.IsEmpty() || !System.IsCLIParam(MRX_TestRunner.TEST_IDENTITY_PARAM))
			return ownerId;

		ownerId = PersistenceIdUtils.FromString("marx-test-identity:" + GetGame().GetPlayerManager().GetPlayerName(playerId));
		Print(string.Format("[MRX_TEST] player %1 has no backend identity, using test owner %2", playerId, ownerId), LogLevel.WARNING);
		return ownerId;
	}
}
#endif
