//! Server-side owner of the Marx services. Registered through the systems config, so consumers need no setup.
//! Use MRX_Marx for access.
class MRX_MarxSystem : GameSystem
{
	[Attribute(params: "conf class=MRX_Settings", desc: "Marx settings. Empty uses built-in defaults (native storage with in-memory fallback, 'cash' currency).")]
	protected ResourceName m_sSettingsConfig;

	protected ref MRX_Settings m_Settings;
	protected ref MRX_EconomyService m_Economy;
	protected ref MRX_IdentityService m_Identity;

	//------------------------------------------------------------------------------------------------
	override static void InitInfo(WorldSystemInfo outInfo)
	{
		super.InitInfo(outInfo);
		outInfo.SetAbstract(false).SetUnique(true).SetLocation(ESystemLocation.Server);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Null on clients or when the system is not registered.
	static MRX_MarxSystem GetInstance()
	{
		World world = GetGame().GetWorld();
		if (!world)
			return null;

		return MRX_MarxSystem.Cast(world.FindSystem(MRX_MarxSystem));
	}

	//------------------------------------------------------------------------------------------------
	MRX_EconomyService GetEconomy()
	{
		return m_Economy;
	}

	//------------------------------------------------------------------------------------------------
	MRX_IdentityService GetIdentity()
	{
		return m_Identity;
	}

	//------------------------------------------------------------------------------------------------
	MRX_Settings GetSettings()
	{
		return m_Settings;
	}

	//------------------------------------------------------------------------------------------------
	override event protected void OnInit()
	{
		super.OnInit();
		if (!GetGame().InPlayMode())
			return;

		m_Settings = LoadSettings();
		m_Economy = new MRX_EconomyService(CreateBackend(), m_Settings.CreateRules());
		m_Economy.Init();

		m_Identity = new MRX_IdentityService();
		m_Identity.GetOnOwnerReady().Insert(OnOwnerReady);
		m_Identity.GetOnOwnerLeft().Insert(OnOwnerLeft);

		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (gameMode)
			m_Identity.Attach(gameMode);
		else
			Print("[MRX] No SCR_BaseGameMode in this world, player identities are not available", LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	override event protected void OnCleanup()
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (gameMode && m_Identity)
			m_Identity.Detach(gameMode);

		m_Identity = null;
		m_Economy = null;
		m_Settings = null;

		super.OnCleanup();
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_Settings LoadSettings()
	{
		if (m_sSettingsConfig.IsEmpty())
		{
			Print("[MRX] No settings config set, using defaults", LogLevel.NORMAL);
			return MRX_Settings.CreateDefault();
		}

		MRX_Settings settings = SCR_ConfigHelperT<MRX_Settings>.GetConfigObject(m_sSettingsConfig);
		if (!settings)
		{
			Print(string.Format("[MRX] Could not load settings %1, using defaults", m_sSettingsConfig), LogLevel.ERROR);
			return MRX_Settings.CreateDefault();
		}

		return settings;
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_StorageBackend CreateBackend()
	{
		if (m_Settings.m_eBackend == MRX_EBackendType.NATIVE)
			return new MRX_NativeBackend();

		Print("[MRX] IN_MEMORY storage selected: wallets are lost when the server stops", LogLevel.WARNING);
		return new MRX_InMemoryBackend();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnOwnerReady(int playerId, string ownerId)
	{
		m_Economy.WatchOwner(ownerId);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnOwnerLeft(int playerId, string ownerId)
	{
		m_Economy.UnwatchOwner(ownerId);
	}
}
