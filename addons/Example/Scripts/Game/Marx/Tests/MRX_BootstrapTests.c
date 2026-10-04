#ifdef WORKBENCH
// Tests of identity mapping, settings and the running Marx system.

//------------------------------------------------------------------------------------------------
class MRX_BootstrapTests
{
	//------------------------------------------------------------------------------------------------
	static void Register(notnull MRX_TestRunner runner)
	{
		runner.Add(new MRX_Test_IdentityMapping());
		runner.Add(new MRX_Test_IdentityCatchUp());
		runner.Add(new MRX_Test_SettingsRules());
		runner.Add(new MRX_Test_SystemBootstrap());
	}
}

//------------------------------------------------------------------------------------------------
//! Identity service fed with fake identities instead of the backend.
class MRX_TestIdentityService : MRX_IdentityService
{
	ref map<int, string> m_mFakeIds = new map<int, string>();

	//------------------------------------------------------------------------------------------------
	override protected string ResolveIdentity(int playerId)
	{
		return m_mFakeIds.Get(playerId);
	}

	//------------------------------------------------------------------------------------------------
	void SimulateAudit(int playerId)
	{
		OnPlayerAuditSuccess(playerId);
	}

	//------------------------------------------------------------------------------------------------
	void SimulateDisconnect(int playerId)
	{
		OnPlayerDisconnected(playerId, KickCauseCode.NONE, -1);
	}

	//------------------------------------------------------------------------------------------------
	void SimulateAttach(notnull array<int> auditedPlayerIds)
	{
		RegisterAuditedPlayers(auditedPlayerIds);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_Test_IdentityMapping : MRX_TestCase
{
	protected ref array<string> m_aEvents = {};

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		MRX_TestIdentityService identity = new MRX_TestIdentityService();
		identity.GetOnOwnerReady().Insert(OnOwnerReady);
		identity.GetOnOwnerLeft().Insert(OnOwnerLeft);
		identity.m_mFakeIds.Set(1, "owner-a");

		identity.SimulateAudit(1);
		identity.SimulateAudit(1);
		CheckString(identity.GetOwnerId(1), "owner-a", "owner of player 1");
		CheckInt(identity.GetPlayerId("owner-a"), 1, "player of owner-a");

		// No identity: the player stays without owner.
		identity.SimulateAudit(3);
		Check(!identity.IsOwnerReady(3), "player without identity is not ready");
		CheckString(identity.GetOwnerId(3), "", "owner of player without identity");

		// The same owner joins as another player before the old session is gone.
		identity.m_mFakeIds.Set(4, "owner-a");
		identity.SimulateAudit(4);
		CheckString(identity.GetOwnerId(1), "", "old player unbound");
		CheckInt(identity.GetPlayerId("owner-a"), 4, "owner-a rebound");

		identity.SimulateDisconnect(1);
		identity.SimulateDisconnect(4);
		CheckInt(identity.GetPlayerId("owner-a"), 0, "owner-a offline");
		Check(!identity.IsOwnerReady(4), "disconnected player is not ready");

		string events;
		foreach (string entry : m_aEvents)
		{
			events += entry + ";";
		}

		CheckString(events, "ready:1:owner-a;left:1:owner-a;ready:4:owner-a;left:4:owner-a;", "events");
		Finish();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnOwnerReady(int playerId, string ownerId)
	{
		m_aEvents.Insert(string.Format("ready:%1:%2", playerId, ownerId));
	}

	//------------------------------------------------------------------------------------------------
	protected void OnOwnerLeft(int playerId, string ownerId)
	{
		m_aEvents.Insert(string.Format("left:%1:%2", playerId, ownerId));
	}
}

//------------------------------------------------------------------------------------------------
//! Players audited before the service attached are registered once, later audit events change nothing.
class MRX_Test_IdentityCatchUp : MRX_TestCase
{
	protected int m_iReadyEvents;

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		MRX_TestIdentityService identity = new MRX_TestIdentityService();
		identity.GetOnOwnerReady().Insert(OnOwnerReady);
		identity.m_mFakeIds.Set(1, "owner-a");

		array<int> auditedPlayerIds = {1, 2};
		identity.SimulateAttach(auditedPlayerIds);
		CheckString(identity.GetOwnerId(1), "owner-a", "player audited before attach");
		Check(!identity.IsOwnerReady(2), "audited player without identity");

		identity.SimulateAudit(1);
		CheckInt(m_iReadyEvents, 1, "ready events");
		Finish();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnOwnerReady(int playerId, string ownerId)
	{
		m_iReadyEvents++;
	}
}

//------------------------------------------------------------------------------------------------
class MRX_Test_SettingsRules : MRX_TestCase
{
	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		MRX_Settings defaultSettings = MRX_Settings.CreateDefault();
		MRX_WalletRules defaults = defaultSettings.CreateRules();
		Check(defaults.m_Currencies.Find(MRX_Settings.DEFAULT_CURRENCY) != null, "default currency");
		CheckInt(defaults.m_iMaxRecentEntries, 50, "default entries");
		CheckInt(defaults.m_iMaxRecentKeys, 200, "default keys");

		MRX_Settings custom = MRX_Settings.CreateDefault();
		custom.m_aCurrencies = new array<ref MRX_CurrencyDef>();
		custom.m_aCurrencies.Insert(MRX_CurrencyDef.Create("gold", 10, 100));
		custom.m_aCurrencies.Insert(MRX_CurrencyDef.Create("gold", 0, 5));
		custom.m_aCurrencies.Insert(MRX_CurrencyDef.Create("bad", 500, 100));
		MRX_WalletRules customRules = custom.CreateRules();

		array<string> ids = {};
		CheckInt(customRules.m_Currencies.GetIds(ids), 1, "valid currencies");
		Check(customRules.m_Currencies.Find(MRX_Settings.DEFAULT_CURRENCY) == null, "no default when a currency is valid");
		MRX_CurrencyDef gold = customRules.m_Currencies.Find("gold");
		if (gold)
			CheckInt(gold.m_iMaxBalance, 100, "first definition wins");

		MRX_Settings invalid = MRX_Settings.CreateDefault();
		invalid.m_aCurrencies = new array<ref MRX_CurrencyDef>();
		invalid.m_aCurrencies.Insert(MRX_CurrencyDef.Create("", 0, 10));
		Check(invalid.CreateRules().m_Currencies.Find(MRX_Settings.DEFAULT_CURRENCY) != null, "fallback when no currency is valid");
		Finish();
	}
}

//------------------------------------------------------------------------------------------------
//! Needs MRX_MarxSystem in the world systems config (Marx_Core override of ChimeraSystemsConfig).
//! Waits until the local player has an owner ID and its balances are cached.
class MRX_Test_SystemBootstrap : MRX_TestCase
{
	protected static const int POLL_MS = 250;
	protected static const int MAX_POLLS = 16;

	protected int m_iPolls;

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		if (!MRX_MarxSystem.GetInstance())
		{
			Check(false, "MRX_MarxSystem not found: the systems config override is missing");
			Finish();
			return;
		}

		Poll();
	}

	//------------------------------------------------------------------------------------------------
	protected void Poll()
	{
		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);

		string ownerId;
		if (!playerIds.IsEmpty())
			ownerId = MRX_Marx.GetOwnerId(playerIds[0]);

		int balance;
		bool cached;
		if (!ownerId.IsEmpty())
			cached = MRX_Marx.GetEconomy().TryGetCachedBalance(ownerId, MRX_Settings.DEFAULT_CURRENCY, balance);

		if (MRX_Marx.IsReady() && cached)
		{
			Print(string.Format("[MRX_TEST]   player %1 owner %2 cash %3", playerIds[0], ownerId, balance));
			Finish();
			return;
		}

		m_iPolls++;
		if (m_iPolls < MAX_POLLS)
		{
			GetGame().GetCallqueue().CallLater(Poll, POLL_MS);
			return;
		}

		Check(MRX_Marx.IsReady(), "economy ready");
		Check(!playerIds.IsEmpty(), "a player is connected");
		Check(!ownerId.IsEmpty(), "player has an owner ID");
		Check(cached, "owner balances cached");
		Finish();
	}
}
#endif
