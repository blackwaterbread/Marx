#ifdef WORKBENCH
// Runtime spike harness (Workbench only). Temporary - remove before release.
// Logs are prefixed with [MRX_SPIKE].

//------------------------------------------------------------------------------------------------
class MRX_Spike
{
	static const string TAG = "[MRX_SPIKE] ";
	static const string REST_URL = "http://127.0.0.1:8765/";
	static const string REST_URL_DEAD = "http://127.0.0.1:8799/";

	static const ResourceName PREFAB_RIFLE = "{3E413771E1834D2F}Prefabs/Weapons/Rifles/M16/Rifle_M16A2.et";
	static const ResourceName PREFAB_CAR = "{99F1610551D54D17}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_base.et";
	static const ResourceName PREFAB_BACKPACK = "{841162A79157C494}Prefabs/Items/Equipment/Backpacks/Backpack_ALICE_Medium_frame.et";

	protected static ref MRX_Spike s_Instance;

	protected ref array<ref MRX_SpikeRestProbe> m_aRestProbes = {};
	protected ref array<ref MRX_SpikeEntityProbe> m_aEntityProbes = {};
	protected ref PersistenceStatusCallback m_CommitCallback;
	protected ref array<ref MRX_SpikeSnapshotProbe> m_aSnapshotProbes = {};
	protected static ref array<ref MRX_SpikeWalletProbe> s_aWalletProbes = {};
	protected int m_iCommitStart;

	//------------------------------------------------------------------------------------------------
	static void Log(string msg)
	{
		Print(TAG + msg);
	}

	//------------------------------------------------------------------------------------------------
	static void LogLong(string label, string text)
	{
		int len = text.Length();
		Log(string.Format("%1 (length=%2)", label, len));
		for (int i = 0; i < len; i += 800)
		{
			int chunk = Math.Min(800, len - i);
			Log(string.Format("%1 [%2] %3", label, i, text.Substring(i, chunk)));
		}
	}

	//------------------------------------------------------------------------------------------------
	static void LogIdentity(string stage, int playerId)
	{
		if (!Replication.IsServer())
			return;

		PlayerManager pm = GetGame().GetPlayerManager();
		UUID uid = SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId);
		Log(string.Format("identity stage=%1 player=%2 name=%3 uuid='%4' null=%5 platform=%6 rplMode=%7",
			stage, playerId, pm.GetPlayerName(playerId), uid, uid.IsNull(),
			typename.EnumToString(PlatformKind, pm.GetPlatformKind(playerId)),
			typename.EnumToString(RplMode, RplSession.Mode())));
	}

	//------------------------------------------------------------------------------------------------
	static void Run()
	{
		s_Instance = new MRX_Spike();
		s_Instance.RunPersistenceInfo();
		s_Instance.RunJsonDto();
		// Entity probes disabled: DeserializeSpawn and DeserializeLoad with script-provided json crash the engine (1.8.0.13).
		s_Instance.RunSnapshotProbes();
		s_Instance.RunRestProbes();
	}

	//------------------------------------------------------------------------------------------------
	static void StartWalletProbe(int playerId)
	{
		UUID identity = SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId);
		if (identity.IsNull())
		{
			Log(string.Format("wallet: skipped player=%1, identity is null", playerId));
			return;
		}

		MRX_SpikeWalletProbe probe = new MRX_SpikeWalletProbe();
		s_aWalletProbes.Insert(probe);
		probe.Start(identity);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunSnapshotProbes()
	{
		BaseWorld world = GetGame().GetWorld();
		float x = 2880;
		float z = 1600;
		AddSnapshotProbe("car", PREFAB_CAR, Vector(x + 10, world.GetSurfaceY(x + 10, z) + 1, z));
		AddSnapshotProbe("rifle", PREFAB_RIFLE, Vector(x, world.GetSurfaceY(x, z) + 1, z));
	}

	//------------------------------------------------------------------------------------------------
	protected void AddSnapshotProbe(string label, ResourceName prefab, vector pos)
	{
		MRX_SpikeSnapshotProbe probe = new MRX_SpikeSnapshotProbe(label, prefab, pos);
		m_aSnapshotProbes.Insert(probe);
		probe.Start();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunPersistenceInfo()
	{
		PersistenceSystem sys = PersistenceSystem.GetInstance();
		if (!sys)
		{
			Log("persistence: no PersistenceSystem instance");
			return;
		}

		SaveGameManager sgm = GetGame().GetSaveGameManager();
		bool savingEnabled = sgm && sgm.IsSavingEnabled();
		Log(string.Format("persistence[reload-test-1]: state=%1 dataLoaded=%2 hive=%3 savingEnabled=%4",
			typename.EnumToString(EPersistenceSystemState, sys.GetState()), sys.WasDataLoaded(),
			PersistenceIdUtils.GetCurrentHiveId(), savingEnabled));

		UUID generated = PersistenceIdUtils.Generate();
		UUID derived = PersistenceIdUtils.FromString("marx:test");
		Log(string.Format("persistence: generated=%1 derived=%2 derivedAgain=%3", generated, derived, PersistenceIdUtils.FromString("marx:test")));

		SCR_PersistenceSystem scripted = SCR_PersistenceSystem.Cast(sys);
		if (scripted)
		{
			scripted.GetOnBeforeSave().Insert(OnBeforeSave);
			scripted.GetOnAfterSave().Insert(OnAfterSave);
		}

		m_CommitCallback = new PersistenceStatusCallback(OnCommitResult);
		m_iCommitStart = System.GetTickCount();
		sys.CommitStorage(PersistenceSessionStorage, m_CommitCallback);
		Log("persistence: CommitStorage(PersistenceSessionStorage) requested");

		GetGame().GetCallqueue().CallLater(LogCommitPending, 10000, false, 10);
		GetGame().GetCallqueue().CallLater(LogCommitPending, 30000, false, 30);

	}

	//------------------------------------------------------------------------------------------------
	protected void OnCommitResult(EPersistenceStatusCode statusCode, Managed context)
	{
		Log(string.Format("persistence: CommitStorage result=%1 ms=%2",
			typename.EnumToString(EPersistenceStatusCode, statusCode), System.GetTickCount() - m_iCommitStart));
		m_iCommitStart = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected void LogCommitPending(int seconds)
	{
		if (m_iCommitStart != 0)
			Log(string.Format("persistence: CommitStorage still pending after %1s", seconds));
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBeforeSave(ESaveGameType saveType)
	{
		Log(string.Format("persistence: OnBeforeSave type=%1 ms=%2", typename.EnumToString(ESaveGameType, saveType), System.GetTickCount() - m_iCommitStart));
	}

	//------------------------------------------------------------------------------------------------
	protected void OnAfterSave(ESaveGameType saveType, bool success)
	{
		Log(string.Format("persistence: OnAfterSave type=%1 success=%2", typename.EnumToString(ESaveGameType, saveType), success));
	}

	//------------------------------------------------------------------------------------------------
	protected void RunJsonDto()
	{
		MRX_SpikeDto dto = new MRX_SpikeDto();
		dto.m_iAmount = 2147483647;
		dto.m_sOwner = "owner-1";
		dto.m_aTags = {"a", "b"};

		JsonSaveContext save = new JsonSaveContext();
		save.WriteValue("", dto);
		string json = save.SaveToString();

		JsonLoadContext load = new JsonLoadContext();
		bool loaded = load.LoadFromString(json);
		MRX_SpikeDto back = new MRX_SpikeDto();
		load.ReadValue("", back);
		Log(string.Format("json: out=%1 loaded=%2 amount=%3 owner=%4 tags=%5", json, loaded, back.m_iAmount, back.m_sOwner, back.m_aTags.Count()));

		JsonLoadContext broken = new JsonLoadContext();
		Log("json: LoadFromString(garbage)=" + broken.LoadFromString("{not json"));
	}

	//------------------------------------------------------------------------------------------------
	protected void RunEntityProbes()
	{
		// Run 1 crashed the engine in DeserializeSpawn with the original ids still in the json.
		// Probes now run one after another so a crash keeps the earlier results in the log.
		BaseWorld world = GetGame().GetWorld();
		float x = 2880;
		float z = 1600;
		AddEntityProbe("car_load", PREFAB_CAR, Vector(x + 10, world.GetSurfaceY(x + 10, z) + 1, z), MRX_ESpikeRespawnMode.SPAWN_PREFAB_THEN_LOAD, 0);
		AddEntityProbe("rifle_load", PREFAB_RIFLE, Vector(x, world.GetSurfaceY(x, z) + 1, z), MRX_ESpikeRespawnMode.SPAWN_PREFAB_THEN_LOAD, 6000);
		AddEntityProbe("rifle_noid", PREFAB_RIFLE, Vector(x, world.GetSurfaceY(x, z + 3) + 1, z + 3), MRX_ESpikeRespawnMode.DESERIALIZE_SPAWN_STRIPPED_IDS, 12000);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddEntityProbe(string label, ResourceName prefab, vector pos, MRX_ESpikeRespawnMode mode, int delayMs)
	{
		MRX_SpikeEntityProbe probe = new MRX_SpikeEntityProbe(label, prefab, mode);
		m_aEntityProbes.Insert(probe);
		GetGame().GetCallqueue().CallLater(probe.Start, delayMs, false, pos);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunRestProbes()
	{
		RestApi api = GetGame().GetRestApi();
		RestContext ctx = api.GetContext(REST_URL);
		RestContext ctxAgain = api.GetContext(REST_URL);
		Log(string.Format("rest: contextCount=%1 sameContext=%2", api.GetContextCount(), ctx == ctxAgain));

		Log("rest: SetHeaders=" + ctx.SetHeaders("Content-Type,application/json,X-Mrx-Test,hello"));
		ctx.SetTimeout(3);

		SendRest(ctx, "get200", "GET", "status/200", "");
		SendRest(ctx, "post201", "POST", "status/201", "{\"amount\":5}");
		SendRest(ctx, "get404", "GET", "status/404", "");
		SendRest(ctx, "get500", "GET", "status/500", "");
		SendRest(ctx, "timeout3_delay6000", "GET", "delay/6000", "");
		SendRest(ctx, "parallel_a_delay1000", "GET", "delay/1000", "");
		SendRest(ctx, "parallel_b_delay1000", "GET", "delay/1000", "");
		SendRest(ctx, "parallel_c_delay1000", "GET", "delay/1000", "");

		RestContext dead = api.GetContext(REST_URL_DEAD);
		dead.SetTimeout(3);
		SendRest(dead, "dead_port", "GET", "status/200", "");
	}

	//------------------------------------------------------------------------------------------------
	protected void SendRest(RestContext ctx, string name, string method, string path, string body)
	{
		MRX_SpikeRestProbe probe = new MRX_SpikeRestProbe(name);
		m_aRestProbes.Insert(probe);
		probe.Send(ctx, method, path, body);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_SpikeDto
{
	int m_iAmount;
	string m_sOwner;
	ref array<string> m_aTags;
}

//------------------------------------------------------------------------------------------------
class MRX_SpikeRestProbe
{
	protected string m_sName;
	protected int m_iStart;
	protected ref RestCallback m_Callback;

	//------------------------------------------------------------------------------------------------
	void MRX_SpikeRestProbe(string name)
	{
		m_sName = name;
	}

	//------------------------------------------------------------------------------------------------
	void Send(RestContext ctx, string method, string path, string body)
	{
		m_Callback = new RestCallback();
		m_Callback.SetOnSuccess(OnSuccess);
		m_Callback.SetOnError(OnError);
		m_iStart = System.GetTickCount();

		int requestId;
		if (method == "POST")
			requestId = ctx.POST(m_Callback, path, body);
		else
			requestId = ctx.GET(m_Callback, path);

		MRX_Spike.Log(string.Format("rest send %1 %2 %3 returned=%4", m_sName, method, path, requestId));
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSuccess(RestCallback cb)
	{
		Report("OnSuccess", cb);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnError(RestCallback cb)
	{
		Report("OnError", cb);
	}

	//------------------------------------------------------------------------------------------------
	protected void Report(string handler, RestCallback cb)
	{
		int http = cb.GetHttpCode();
		string data = cb.GetData();
		if (data.Length() > 600)
			data = data.Substring(0, 600) + "...";

		MRX_Spike.Log(string.Format("rest %1 %2 ms=%3 result=%4 http=%5 httpEnum=%6 data=%7",
			handler, m_sName, System.GetTickCount() - m_iStart,
			typename.EnumToString(ERestResult, cb.GetRestResult()), http,
			typename.EnumToString(HttpCode, cb.GetHttpCode()), data));
	}
}

//------------------------------------------------------------------------------------------------
enum MRX_ESpikeRespawnMode
{
	DESERIALIZE_SPAWN,
	DESERIALIZE_SPAWN_STRIPPED_IDS,
	SPAWN_PREFAB_THEN_LOAD
}

//------------------------------------------------------------------------------------------------
class MRX_SpikeEntityProbe
{
	protected string m_sLabel;
	protected ResourceName m_sPrefab;
	protected MRX_ESpikeRespawnMode m_eMode;
	protected IEntity m_Entity;
	protected IEntity m_Restored;
	protected string m_sJson;
	protected UUID m_sOriginalId;
	protected vector m_vPosition;

	//------------------------------------------------------------------------------------------------
	void MRX_SpikeEntityProbe(string label, ResourceName prefab, MRX_ESpikeRespawnMode mode)
	{
		m_sLabel = label;
		m_sPrefab = prefab;
		m_eMode = mode;
	}

	//------------------------------------------------------------------------------------------------
	//! Removes every "id":"<uuid>", pair so the system has to assign new ids.
	protected static string StripIds(string json)
	{
		string key = "\"id\":\"";
		int start = json.IndexOf(key);
		while (start != -1)
		{
			int valueEnd = json.IndexOfFrom(start + key.Length(), "\"");
			int cut = valueEnd + 1;
			if (cut < json.Length() && json.Substring(cut, 1) == ",")
				cut++;

			json = json.Substring(0, start) + json.Substring(cut, json.Length() - cut);
			start = json.IndexOf(key);
		}

		return json;
	}

	//------------------------------------------------------------------------------------------------
	void Start(vector pos)
	{
		m_vPosition = pos;
		Resource res = Resource.Load(m_sPrefab);
		if (!res || !res.IsValid())
		{
			Log("prefab load failed: " + m_sPrefab);
			return;
		}

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = pos;
		m_Entity = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!m_Entity)
		{
			Log("spawn failed");
			return;
		}

		Log("spawned at " + pos.ToString());
		GetGame().GetCallqueue().CallLater(Mutate, 1500);
	}

	//------------------------------------------------------------------------------------------------
	protected void Log(string msg)
	{
		MRX_Spike.Log(string.Format("entity[%1] %2", m_sLabel, msg));
	}

	//------------------------------------------------------------------------------------------------
	protected void Mutate()
	{
		BaseWeaponComponent weapon = BaseWeaponComponent.Cast(m_Entity.FindComponent(BaseWeaponComponent));
		if (weapon && weapon.GetCurrentMagazine())
			weapon.GetCurrentMagazine().SetAmmoCount(7);

		SCR_FuelManagerComponent fuel = SCR_FuelManagerComponent.Cast(m_Entity.FindComponent(SCR_FuelManagerComponent));
		if (fuel)
			fuel.SetTotalFuelPercentage(0.5);

		HitZoneContainerComponent hitZones = HitZoneContainerComponent.Cast(m_Entity.FindComponent(HitZoneContainerComponent));
		if (hitZones)
		{
			array<HitZone> zones = {};
			hitZones.GetAllHitZones(zones);
			if (!zones.IsEmpty())
				zones[0].SetHealth(zones[0].GetMaxHealth() * 0.5);
		}

		Log("before " + Describe(m_Entity));
		GetGame().GetCallqueue().CallLater(SerializeAndDelete, 500);
	}

	//------------------------------------------------------------------------------------------------
	protected void SerializeAndDelete()
	{
		PersistenceSystem sys = PersistenceSystem.GetInstance();
		if (!sys)
		{
			Log("no PersistenceSystem");
			return;
		}

		bool trackedBefore = sys.IsTracked(m_Entity);
		UUID idBefore = sys.GetId(m_Entity);
		bool started = sys.StartTracking(m_Entity, false);
		m_sOriginalId = sys.GetId(m_Entity);

		SCR_PersistenceJsonSaveContext ctx = new SCR_PersistenceJsonSaveContext();
		int t0 = System.GetTickCount();
		ESerializeResult result = sys.Serialize(m_Entity, ctx);
		int serializeMs = System.GetTickCount() - t0;
		m_sJson = ctx.SaveToString();

		Log(string.Format("serialize trackedBefore=%1 idBefore=%2 startTracking=%3 id=%4 result=%5 ms=%6",
			trackedBefore, idBefore, started, m_sOriginalId, typename.EnumToString(ESerializeResult, result), serializeMs));
		MRX_Spike.LogLong(string.Format("entity[%1] json", m_sLabel), m_sJson);

		bool stopped = sys.StopTracking(m_Entity, true);
		SCR_EntityHelper.DeleteEntityAndChildren(m_Entity);
		Log("deleted stopTracking=" + stopped);
		GetGame().GetCallqueue().CallLater(Respawn, 500);
	}

	//------------------------------------------------------------------------------------------------
	protected void Respawn()
	{
		PersistenceSystem sys = PersistenceSystem.GetInstance();
		string json = m_sJson;
		if (m_eMode == MRX_ESpikeRespawnMode.DESERIALIZE_SPAWN_STRIPPED_IDS)
		{
			json = StripIds(json);
			MRX_Spike.LogLong(string.Format("entity[%1] stripped json", m_sLabel), json);
		}

		SCR_PersistenceJsonLoadContext ctx = new SCR_PersistenceJsonLoadContext();
		bool loaded = ctx.LoadFromString(json);

		Log("respawn mode=" + typename.EnumToString(MRX_ESpikeRespawnMode, m_eMode) + " (about to call engine)");
		int t0 = System.GetTickCount();
		if (m_eMode == MRX_ESpikeRespawnMode.SPAWN_PREFAB_THEN_LOAD)
		{
			EntitySpawnParams params = new EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;
			params.Transform[3] = m_vPosition;
			IEntity fresh = GetGame().SpawnEntityPrefab(Resource.Load(m_sPrefab), GetGame().GetWorld(), params);
			Log("fresh spawn id=" + sys.GetId(fresh) + " " + Describe(fresh));
			Managed applied = sys.DeserializeLoad(fresh, ctx);
			m_Restored = IEntity.Cast(applied);
			Log("DeserializeLoad returnedSameInstance=" + (applied == fresh));
		}
		else
		{
			Managed spawned = sys.DeserializeSpawn(ctx);
			m_Restored = IEntity.Cast(spawned);
		}

		UUID restoredId;
		if (m_Restored)
			restoredId = sys.GetId(m_Restored);

		Log(string.Format("respawn loaded=%1 spawned=%2 ms=%3 restoredId=%4 sameId=%5",
			loaded, m_Restored != null, System.GetTickCount() - t0, restoredId, restoredId == m_sOriginalId));

		if (m_Restored)
		{
			Log("restored immediately " + Describe(m_Restored));
			GetGame().GetCallqueue().CallLater(Verify, 1500);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void Verify()
	{
		Log("restored after 1.5s " + Describe(m_Restored));
	}

	//------------------------------------------------------------------------------------------------
	protected string Describe(IEntity entity)
	{
		if (!entity)
			return "<null>";

		string text = string.Format("prefab=%1 pos=%2", entity.GetPrefabData().GetPrefabName(), entity.GetOrigin().ToString());

		BaseWeaponComponent weapon = BaseWeaponComponent.Cast(entity.FindComponent(BaseWeaponComponent));
		if (weapon)
		{
			BaseMagazineComponent magazine = weapon.GetCurrentMagazine();
			if (magazine)
				text += string.Format(" ammo=%1/%2", magazine.GetAmmoCount(), magazine.GetMaxAmmoCount());
			else
				text += " ammo=<no magazine>";
		}

		SCR_FuelManagerComponent fuel = SCR_FuelManagerComponent.Cast(entity.FindComponent(SCR_FuelManagerComponent));
		if (fuel)
			text += string.Format(" fuel=%1/%2", fuel.GetTotalFuel(), fuel.GetTotalMaxFuel());

		HitZoneContainerComponent hitZones = HitZoneContainerComponent.Cast(entity.FindComponent(HitZoneContainerComponent));
		if (hitZones)
		{
			array<HitZone> zones = {};
			hitZones.GetAllHitZones(zones);
			if (!zones.IsEmpty())
				text += string.Format(" hz0=%1:%2/%3 hzCount=%4", zones[0].GetName(), zones[0].GetHealth(), zones[0].GetMaxHealth(), zones.Count());
		}

		BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(entity.FindComponent(BaseInventoryStorageComponent));
		if (storage)
		{
			array<IEntity> items = {};
			storage.GetAll(items);
			text += string.Format(" storageItems=%1", items.Count());
		}

		int children;
		IEntity child = entity.GetChildren();
		while (child)
		{
			children++;
			child = child.GetSibling();
		}

		text += string.Format(" children=%1", children);
		return text;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_BaseGameMode
{
	//------------------------------------------------------------------------------------------------
	override void OnPlayerConnected(int playerId)
	{
		super.OnPlayerConnected(playerId);
		MRX_Spike.LogIdentity("connected", playerId);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnPlayerRegistered(int playerId)
	{
		super.OnPlayerRegistered(playerId);
		MRX_Spike.LogIdentity("registered", playerId);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerAuditSuccess(int iPlayerID)
	{
		super.OnPlayerAuditSuccess(iPlayerID);
		MRX_Spike.LogIdentity("auditSuccess", iPlayerID);
		if (Replication.IsServer())
			MRX_Spike.StartWalletProbe(iPlayerID);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnGameStart()
	{
		super.OnGameStart();
		if (Replication.IsServer())
			GetGame().GetCallqueue().CallLater(MRX_Spike.Run, 5000);
	}
}
#endif
