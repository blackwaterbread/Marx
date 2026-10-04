//! Static entry point for consumers (API v0). Server only: on clients, or when the Marx system is not running,
//! getters return null or empty values.
class MRX_Marx
{
	//------------------------------------------------------------------------------------------------
	//! True when the economy service accepts calls without queueing them.
	static bool IsReady()
	{
		MRX_EconomyService economy = GetEconomy();
		if (!economy)
			return false;

		return economy.GetState() == MRX_EEconomyServiceState.READY;
	}

	//------------------------------------------------------------------------------------------------
	//! Calls made before the service is ready are queued and run once it is.
	static MRX_EconomyService GetEconomy()
	{
		MRX_MarxSystem system = MRX_MarxSystem.GetInstance();
		if (!system)
			return null;

		return system.GetEconomy();
	}

	//------------------------------------------------------------------------------------------------
	static MRX_IdentityService GetIdentity()
	{
		MRX_MarxSystem system = MRX_MarxSystem.GetInstance();
		if (!system)
			return null;

		return system.GetIdentity();
	}

	//------------------------------------------------------------------------------------------------
	//! \return Empty until the player's identity is resolved. Pass it as is: economy calls with an empty owner fail with OWNER_NOT_READY.
	static string GetOwnerId(int playerId)
	{
		MRX_IdentityService identity = GetIdentity();
		if (!identity)
			return string.Empty;

		return identity.GetOwnerId(playerId);
	}
}
