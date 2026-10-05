#ifdef WORKBENCH
// Tests of the stash rules (MRX_StashMath) and MRX_StashService with a fake world and the in-memory backend.

//------------------------------------------------------------------------------------------------
class MRX_StashTests
{
	static const ResourceName PREFAB_RIFLE = "Prefabs/Test/Rifle.et";
	static const ResourceName PREFAB_MAP = "Prefabs/Test/Map.et";

	//------------------------------------------------------------------------------------------------
	static void Register(notnull MRX_TestRunner runner)
	{
		runner.Add(new MRX_Test_StashMath());
		runner.Add(new MRX_Test_StashRoundTrip());
		runner.Add(new MRX_Test_StashDepositRules());
		runner.Add(new MRX_Test_StashWithdrawFailures());
		runner.Add(new MRX_Test_StashCommitFailures());
		runner.Add(new MRX_Test_StashConsumerApi());
		runner.Add(new MRX_Test_StashVanishedItems());
		runner.Add(new MRX_Test_StashDeathPolicies());
		runner.Add(new MRX_Test_StashRecovery());
	}

	//------------------------------------------------------------------------------------------------
	static MRX_StashRequest CreateRequest(string ownerId, string key)
	{
		MRX_StashRequest request = new MRX_StashRequest();
		request.m_sRequestId = "req-" + key;
		request.m_sOwnerId = ownerId;
		request.m_Context = MRX_TxContext.Create("test", "stash math", key);
		return request;
	}
}

//------------------------------------------------------------------------------------------------
class MRX_TestAssetItem : Managed
{
	ResourceName m_sPrefab;
	int m_iHolderPlayerId;
	int m_iAmmo = -1;
	bool m_bDeleted;
}

//------------------------------------------------------------------------------------------------
//! Items are held by players; the test keeps every item alive, deleted ones are flagged.
class MRX_TestAssetWorld : MRX_AssetWorld
{
	ref array<ref MRX_TestAssetItem> m_aItems = {};
	bool m_bNoSpace;
	bool m_bFailSpawn;

	//------------------------------------------------------------------------------------------------
	MRX_TestAssetItem AddItem(int holderPlayerId, ResourceName prefab, int ammo = -1)
	{
		MRX_TestAssetItem item = new MRX_TestAssetItem();
		item.m_iHolderPlayerId = holderPlayerId;
		item.m_sPrefab = prefab;
		item.m_iAmmo = ammo;
		m_aItems.Insert(item);
		return item;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Last live item of the player with that prefab.
	MRX_TestAssetItem FindLive(int holderPlayerId, ResourceName prefab)
	{
		for (int i = m_aItems.Count() - 1; i >= 0; i--)
		{
			MRX_TestAssetItem item = m_aItems[i];
			if (!item.m_bDeleted && item.m_iHolderPlayerId == holderPlayerId && item.m_sPrefab == prefab)
				return item;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	int CountLive()
	{
		int count;
		foreach (MRX_TestAssetItem item : m_aItems)
		{
			if (!item.m_bDeleted)
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	override MRX_EStashStatus CheckDeposit(int playerId, Managed item)
	{
		MRX_TestAssetItem testItem = MRX_TestAssetItem.Cast(item);
		if (!testItem || testItem.m_bDeleted || testItem.m_iHolderPlayerId != playerId)
			return MRX_EStashStatus.NOT_IN_INVENTORY;

		return MRX_EStashStatus.OK;
	}

	//------------------------------------------------------------------------------------------------
	override MRX_ItemSnapshot Capture(Managed item)
	{
		MRX_TestAssetItem testItem = MRX_TestAssetItem.Cast(item);
		if (!testItem)
			return null;

		MRX_ItemSnapshot snapshot = MRX_ItemSnapshot.Create(testItem.m_sPrefab);
		snapshot.m_iAmmo = testItem.m_iAmmo;
		return snapshot;
	}

	//------------------------------------------------------------------------------------------------
	override bool Delete(Managed item)
	{
		MRX_TestAssetItem testItem = MRX_TestAssetItem.Cast(item);
		if (!testItem || testItem.m_bDeleted)
			return false;

		testItem.m_bDeleted = true;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanDeliver(int playerId, ResourceName prefab)
	{
		return !m_bNoSpace;
	}

	//------------------------------------------------------------------------------------------------
	override void Deliver(int playerId, ResourceName prefab, MRX_ItemSnapshot snapshot, notnull MRX_AssetSpawnCallback callback)
	{
		if (m_bFailSpawn)
		{
			callback.OnSpawned(null);
			return;
		}

		int ammo = -1;
		if (snapshot)
			ammo = snapshot.m_iAmmo;

		callback.OnSpawned(AddItem(playerId, prefab, ammo));
	}

	//------------------------------------------------------------------------------------------------
	override bool IsAlive(Managed item)
	{
		MRX_TestAssetItem testItem = MRX_TestAssetItem.Cast(item);
		return testItem && !testItem.m_bDeleted;
	}

	//------------------------------------------------------------------------------------------------
	override bool IsCarriedBy(Managed item, Managed holder)
	{
		MRX_TestAssetItem testItem = MRX_TestAssetItem.Cast(item);
		MRX_TestAssetHolder testHolder = MRX_TestAssetHolder.Cast(holder);
		return testItem && testHolder && testItem.m_iHolderPlayerId == testHolder.m_iPlayerId;
	}
}

//------------------------------------------------------------------------------------------------
//! Stands in for a character in the fake world.
class MRX_TestAssetHolder : Managed
{
	int m_iPlayerId;

	//------------------------------------------------------------------------------------------------
	static MRX_TestAssetHolder Create(int playerId)
	{
		MRX_TestAssetHolder holder = new MRX_TestAssetHolder();
		holder.m_iPlayerId = playerId;
		return holder;
	}
}

//------------------------------------------------------------------------------------------------
class MRX_TestStashBackend : MRX_InMemoryBackend
{
	bool m_bFailApply;

	//------------------------------------------------------------------------------------------------
	override void ApplyStash(notnull MRX_StashRequest request, notnull MRX_StashResultCallback callback)
	{
		if (m_bFailApply)
		{
			MRX_StashResultDelivery.Post(m_CallQueue, callback, MRX_StashResult.Create(MRX_EStashStatus.STORAGE_ERROR, request.m_sRequestId));
			return;
		}

		super.ApplyStash(request, callback);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_TestRejectPrefabValidator : MRX_StashValidator
{
	ResourceName m_sRejected;

	//------------------------------------------------------------------------------------------------
	override bool CanDeposit(int playerId, string ownerId, notnull MRX_ItemSnapshot snapshot)
	{
		return snapshot.m_sPrefab != m_sRejected;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanWithdraw(int playerId, notnull MRX_AssetRecord asset)
	{
		return asset.m_sPrefab != m_sRejected;
	}
}

//------------------------------------------------------------------------------------------------
//! Transition table and atomic apply, synchronously.
class MRX_Test_StashMath : MRX_TestCase
{
	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		Check(MRX_AssetStates.CanTransition(MRX_EAssetState.STASHED, MRX_EAssetState.DEPLOYED), "STASHED -> DEPLOYED");
		Check(MRX_AssetStates.CanTransition(MRX_EAssetState.DEPLOYED, MRX_EAssetState.STASHED), "DEPLOYED -> STASHED");
		Check(MRX_AssetStates.CanTransition(MRX_EAssetState.DEPLOYED, MRX_EAssetState.LOST), "DEPLOYED -> LOST");
		Check(MRX_AssetStates.CanTransition(MRX_EAssetState.DEPLOYED, MRX_EAssetState.CONSUMED), "DEPLOYED -> CONSUMED");
		Check(!MRX_AssetStates.CanTransition(MRX_EAssetState.STASHED, MRX_EAssetState.LOST), "STASHED -> LOST refused");
		Check(!MRX_AssetStates.CanTransition(MRX_EAssetState.LOST, MRX_EAssetState.STASHED), "LOST is final");

		MRX_StorageRules rules = MRX_TestUtils.CreateRules();
		rules.m_iMaxStashAssets = 2;
		MRX_StashRecord record = MRX_StashRecord.Create("math-owner");

		MRX_StashRequest add = MRX_StashTests.CreateRequest("math-owner", "add-1");
		add.m_aChanges.Insert(MRX_AssetChange.Add(MRX_AssetRecord.Create("a1", MRX_StashTests.PREFAB_RIFLE)));
		add.m_aChanges.Insert(MRX_AssetChange.Add(MRX_AssetRecord.Create("a2", MRX_StashTests.PREFAB_MAP)));
		MRX_StashResult result = MRX_StashMath.Apply(record, add, rules);
		CheckStashStatus(result.m_eStatus, MRX_EStashStatus.OK, "add two");
		CheckInt(record.m_aAssets.Count(), 2, "assets after add");
		CheckString(record.m_aAssets[0].m_sOwnerId, "math-owner", "owner set on add");
		CheckString(record.m_aAssets[0].m_sSource, "test", "source set on add");

		result = MRX_StashMath.Apply(record, add, rules);
		CheckStashStatus(result.m_eStatus, MRX_EStashStatus.DUPLICATE, "same key again");
		CheckString(result.m_sRequestId, "req-add-1", "duplicate reports the original request");
		CheckInt(record.m_aAssets.Count(), 2, "assets after duplicate");

		MRX_StashRequest full = MRX_StashTests.CreateRequest("math-owner", "add-2");
		full.m_aChanges.Insert(MRX_AssetChange.Add(MRX_AssetRecord.Create("a3", MRX_StashTests.PREFAB_MAP)));
		CheckStashStatus(MRX_StashMath.Apply(record, full, rules).m_eStatus, MRX_EStashStatus.STASH_FULL, "third asset over the limit");

		// A failed request is atomic and does not consume its key.
		MRX_StashRequest mixed = MRX_StashTests.CreateRequest("math-owner", "mixed");
		mixed.m_aChanges.Insert(MRX_AssetChange.Update("a1", MRX_EAssetState.STASHED, MRX_EAssetState.DEPLOYED, null, "session"));
		mixed.m_aChanges.Insert(MRX_AssetChange.Update("a2", MRX_EAssetState.DEPLOYED, MRX_EAssetState.STASHED));
		CheckStashStatus(MRX_StashMath.Apply(record, mixed, rules).m_eStatus, MRX_EStashStatus.INVALID_STATE, "second change has the wrong state");
		CheckInt(record.m_aAssets[0].m_eState, MRX_EAssetState.STASHED, "first change rolled back");

		MRX_StashRequest deploy = MRX_StashTests.CreateRequest("math-owner", "mixed");
		deploy.m_aChanges.Insert(MRX_AssetChange.Update("a1", MRX_EAssetState.STASHED, MRX_EAssetState.DEPLOYED, null, "session"));
		CheckStashStatus(MRX_StashMath.Apply(record, deploy, rules).m_eStatus, MRX_EStashStatus.OK, "deploy with the key of the failed request");
		CheckString(record.FindAsset("a1").m_sDeploySession, "session", "deploy session stored");

		MRX_StashRequest lost = MRX_StashTests.CreateRequest("math-owner", "lost");
		lost.m_aChanges.Insert(MRX_AssetChange.Update("a1", MRX_EAssetState.DEPLOYED, MRX_EAssetState.LOST));
		result = MRX_StashMath.Apply(record, lost, rules);
		CheckStashStatus(result.m_eStatus, MRX_EStashStatus.OK, "deployed asset lost");
		Check(record.FindAsset("a1") == null, "lost asset left the stash");
		Check(result.GetAsset() && result.GetAsset().m_eState == MRX_EAssetState.LOST, "result reports the final state");

		MRX_StashRequest unknown = MRX_StashTests.CreateRequest("math-owner", "unknown");
		unknown.m_aChanges.Insert(MRX_AssetChange.Remove("a1", MRX_EAssetState.STASHED));
		CheckStashStatus(MRX_StashMath.Apply(record, unknown, rules).m_eStatus, MRX_EStashStatus.UNKNOWN_ASSET, "remove of a gone asset");

		MRX_StashRequest noContext = MRX_StashTests.CreateRequest("math-owner", "no-context");
		noContext.m_Context = null;
		noContext.m_aChanges.Insert(MRX_AssetChange.Remove("a2", MRX_EAssetState.STASHED));
		CheckStashStatus(MRX_StashMath.Apply(record, noContext, rules).m_eStatus, MRX_EStashStatus.INVALID_CONTEXT, "missing context");

		Finish();
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckStashStatus(MRX_EStashStatus actual, MRX_EStashStatus expected, string message)
	{
		if (actual != expected)
			Check(false, string.Format("%1: expected %2, got %3", message, typename.EnumToString(MRX_EStashStatus, expected), typename.EnumToString(MRX_EStashStatus, actual)));
	}
}

//------------------------------------------------------------------------------------------------
enum MRX_EStashStepKind
{
	GRANT,
	DEPOSIT,
	WITHDRAW,
	REMOVE,
	CONSUME,
	LIST,
	CUSTOM
}

//------------------------------------------------------------------------------------------------
class MRX_StashStep : Managed
{
	MRX_EStashStepKind m_eKind;
	int m_iPlayerId;
	ResourceName m_sPrefab;
	//! Index into the scenario's asset IDs (WITHDRAW, REMOVE) or items (DEPOSIT, CONSUME).
	int m_iRef = -1;
	string m_sKey;
	MRX_EStashStatus m_eExpected = MRX_EStashStatus.OK;
	//! LIST: expected STASHED and DEPLOYED counts.
	int m_iExpectedStashed;
	int m_iExpectedDeployed;

	//------------------------------------------------------------------------------------------------
	MRX_StashStep Expect(MRX_EStashStatus status)
	{
		m_eExpected = status;
		return this;
	}

	//------------------------------------------------------------------------------------------------
	string Describe(int index)
	{
		return string.Format("step %1 %2 player=%3 ref=%4", index, typename.EnumToString(MRX_EStashStepKind, m_eKind), m_iPlayerId, m_iRef);
	}
}

//------------------------------------------------------------------------------------------------
//! Players 1 and 2 have owners ("stash-a", "stash-b"), player 9 has none. Max 3 assets per stash.
//! Assets created by OK steps are numbered in order (asset refs); world items are numbered by the fake world (item refs).
class MRX_StashScenarioTest : MRX_TestCase
{
	protected ref MRX_TestIdentityService m_Identity;
	protected ref MRX_TestStashBackend m_Backend;
	protected ref MRX_EconomyService m_Economy;
	protected ref MRX_TestAssetWorld m_World;
	protected ref MRX_StashService m_Stash;
	protected ref array<ref MRX_StashStep> m_aSteps = {};
	protected ref array<string> m_aAssetIds = {};
	protected ref MRX_StashResultCallback m_ResultCallback;
	protected ref MRX_StashCallback m_ListCallback;
	protected ref MRX_StashResult m_LastResult;
	protected int m_iStep = -1;
	protected int m_iEvents;

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		m_Identity = new MRX_TestIdentityService();
		m_Identity.m_mFakeIds.Set(1, "stash-a");
		m_Identity.m_mFakeIds.Set(2, "stash-b");
		m_Identity.SimulateAudit(1);
		m_Identity.SimulateAudit(2);

		MRX_StorageRules rules = MRX_TestUtils.CreateRules();
		rules.m_iMaxStashAssets = 3;
		m_Backend = new MRX_TestStashBackend();
		m_Economy = new MRX_EconomyService(m_Backend, rules);
		m_Economy.Init();

		m_World = new MRX_TestAssetWorld();
		m_Stash = new MRX_StashService(m_Economy, m_Identity, m_World);
		m_Stash.GetOnAssetStateChanged().Insert(OnAssetStateChanged);

		m_ResultCallback = new MRX_StashResultCallback();
		m_ResultCallback.GetOnResult().Insert(OnResult);
		m_ListCallback = new MRX_StashCallback();
		m_ListCallback.GetOnResult().Insert(OnList);

		DefineSteps();
		NextStep();
	}

	//------------------------------------------------------------------------------------------------
	protected void DefineSteps()
	{
	}

	//------------------------------------------------------------------------------------------------
	//! Runs before step index. Override for setup between steps (CUSTOM steps).
	protected void OnCustomStep(int index)
	{
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckEnd()
	{
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_StashStep AddStep(MRX_EStashStepKind kind, int playerId)
	{
		MRX_StashStep step = new MRX_StashStep();
		step.m_eKind = kind;
		step.m_iPlayerId = playerId;
		m_aSteps.Insert(step);
		return step;
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_StashStep Grant(int playerId, ResourceName prefab, string key)
	{
		MRX_StashStep step = AddStep(MRX_EStashStepKind.GRANT, playerId);
		step.m_sPrefab = prefab;
		step.m_sKey = key;
		return step;
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_StashStep Deposit(int playerId, int itemRef)
	{
		MRX_StashStep step = AddStep(MRX_EStashStepKind.DEPOSIT, playerId);
		step.m_iRef = itemRef;
		return step;
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_StashStep Withdraw(int playerId, int assetRef)
	{
		MRX_StashStep step = AddStep(MRX_EStashStepKind.WITHDRAW, playerId);
		step.m_iRef = assetRef;
		return step;
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_StashStep RemoveAsset(int playerId, int assetRef, string key)
	{
		MRX_StashStep step = AddStep(MRX_EStashStepKind.REMOVE, playerId);
		step.m_iRef = assetRef;
		step.m_sKey = key;
		return step;
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_StashStep Consume(int itemRef, string key)
	{
		MRX_StashStep step = AddStep(MRX_EStashStepKind.CONSUME, 0);
		step.m_iRef = itemRef;
		step.m_sKey = key;
		return step;
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_StashStep List(int playerId, int stashed, int deployed)
	{
		MRX_StashStep step = AddStep(MRX_EStashStepKind.LIST, playerId);
		step.m_iExpectedStashed = stashed;
		step.m_iExpectedDeployed = deployed;
		return step;
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_StashStep Custom()
	{
		return AddStep(MRX_EStashStepKind.CUSTOM, 0);
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_TxContext CreateContext(string key)
	{
		if (key.IsEmpty())
			return null;

		return MRX_TxContext.Create("test", "stash scenario", key + ":" + PersistenceIdUtils.Generate());
	}

	//------------------------------------------------------------------------------------------------
	protected string GetAssetId(int assetRef)
	{
		if (assetRef < 0 || assetRef >= m_aAssetIds.Count())
			return "missing-asset";

		return m_aAssetIds[assetRef];
	}

	//------------------------------------------------------------------------------------------------
	protected void NextStep()
	{
		m_iStep++;
		if (m_iStep >= m_aSteps.Count())
		{
			CheckEnd();
			Finish();
			return;
		}

		MRX_StashStep step = m_aSteps[m_iStep];
		string ownerId = m_Identity.GetOwnerId(step.m_iPlayerId);
		switch (step.m_eKind)
		{
			case MRX_EStashStepKind.GRANT:
				m_Stash.Grant(ownerId, step.m_sPrefab, CreateContext(step.m_sKey), m_ResultCallback);
				break;

			case MRX_EStashStepKind.DEPOSIT:
				m_Stash.Deposit(step.m_iPlayerId, m_World.m_aItems[step.m_iRef], m_ResultCallback);
				break;

			case MRX_EStashStepKind.WITHDRAW:
				m_Stash.Withdraw(step.m_iPlayerId, GetAssetId(step.m_iRef), m_ResultCallback);
				break;

			case MRX_EStashStepKind.REMOVE:
				m_Stash.Remove(ownerId, GetAssetId(step.m_iRef), CreateContext(step.m_sKey), m_ResultCallback);
				break;

			case MRX_EStashStepKind.CONSUME:
				m_Stash.MarkConsumed(m_World.m_aItems[step.m_iRef], CreateContext(step.m_sKey), m_ResultCallback);
				break;

			case MRX_EStashStepKind.LIST:
				m_Stash.List(ownerId, m_ListCallback);
				break;

			case MRX_EStashStepKind.CUSTOM:
				OnCustomStep(m_iStep);
				GetGame().GetCallqueue().CallLater(NextStep);
				break;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnResult(MRX_StashResult result)
	{
		MRX_StashStep step = m_aSteps[m_iStep];
		if (result.m_eStatus != step.m_eExpected)
			Check(false, string.Format("%1: expected %2, got %3", step.Describe(m_iStep), typename.EnumToString(MRX_EStashStatus, step.m_eExpected), typename.EnumToString(MRX_EStashStatus, result.m_eStatus)));

		m_LastResult = result;
		bool creates = step.m_eKind == MRX_EStashStepKind.GRANT || step.m_eKind == MRX_EStashStepKind.DEPOSIT;
		MRX_AssetRecord asset = result.GetAsset();
		if (result.m_eStatus == MRX_EStashStatus.OK && creates && asset && !m_aAssetIds.Contains(asset.m_sId))
			m_aAssetIds.Insert(asset.m_sId);

		NextStep();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnList(MRX_EStashStatus status, MRX_StashRecord record)
	{
		MRX_StashStep step = m_aSteps[m_iStep];
		if (status != step.m_eExpected)
			Check(false, string.Format("%1: expected %2, got %3", step.Describe(m_iStep), typename.EnumToString(MRX_EStashStatus, step.m_eExpected), typename.EnumToString(MRX_EStashStatus, status)));

		if (record)
		{
			CheckInt(record.CountInState(MRX_EAssetState.STASHED), step.m_iExpectedStashed, step.Describe(m_iStep) + " stashed");
			CheckInt(record.CountInState(MRX_EAssetState.DEPLOYED), step.m_iExpectedDeployed, step.Describe(m_iStep) + " deployed");
		}

		NextStep();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnAssetStateChanged(MRX_AssetRecord asset, MRX_EAssetState oldState)
	{
		m_iEvents++;
	}
}

//------------------------------------------------------------------------------------------------
//! Grant -> withdraw -> change the item -> deposit it back -> withdraw again: the change survives.
class MRX_Test_StashRoundTrip : MRX_StashScenarioTest
{
	//------------------------------------------------------------------------------------------------
	override protected void DefineSteps()
	{
		Grant(1, MRX_StashTests.PREFAB_RIFLE, "grant-rifle");	// asset 0
		List(1, 1, 0);
		Withdraw(1, 0);											// item 0
		List(1, 0, 1);
		Custom();												// item 0 ammo -> 7
		Deposit(1, 0);
		List(1, 1, 0);
		Withdraw(1, 0);											// item 1, ammo 7
		Custom();												// checks
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnCustomStep(int index)
	{
		if (index == 4)
		{
			MRX_TestAssetItem item = m_World.m_aItems[0];
			CheckString(m_Stash.GetAssetId(item), GetAssetId(0), "withdrawn item is bound to the asset");
			CheckString(m_Stash.GetOwnerOf(item), "stash-a", "owner of the withdrawn item");
			item.m_iAmmo = 7;
			return;
		}

		Check(m_World.m_aItems[0].m_bDeleted, "deposited item removed from the world");
		CheckInt(m_World.m_aItems.Count(), 2, "items spawned");
		MRX_TestAssetItem again = m_World.m_aItems[1];
		CheckInt(again.m_iAmmo, 7, "ammo restored from the deposit snapshot");
		CheckString(m_Stash.GetAssetId(again), GetAssetId(0), "second withdraw bound to the same asset");
		CheckString(m_Stash.GetAssetId(m_World.m_aItems[0]), string.Empty, "old item unbound");
	}

	//------------------------------------------------------------------------------------------------
	override protected void CheckEnd()
	{
		// grant, withdraw, deposit, withdraw
		CheckInt(m_iEvents, 4, "state change events");
	}
}

//------------------------------------------------------------------------------------------------
//! Deposits of unbound items, other players' items and bound items of another owner.
class MRX_Test_StashDepositRules : MRX_StashScenarioTest
{
	//------------------------------------------------------------------------------------------------
	override protected void DefineSteps()
	{
		m_World.AddItem(1, MRX_StashTests.PREFAB_MAP);	// item 0, held by player 1
		m_World.AddItem(2, MRX_StashTests.PREFAB_MAP);	// item 1, held by player 2

		Deposit(2, 0).Expect(MRX_EStashStatus.NOT_IN_INVENTORY);
		Deposit(9, 0).Expect(MRX_EStashStatus.OWNER_NOT_READY);
		Deposit(1, 0);											// asset 0 (new)
		List(1, 1, 0);
		Withdraw(2, 0).Expect(MRX_EStashStatus.UNKNOWN_ASSET);	// not in player 2's stash
		Withdraw(1, 0);											// item 2
		Custom();												// item 2 handed to player 2
		Deposit(2, 2).Expect(MRX_EStashStatus.NOT_OWNED);
		Custom();												// rejecting validator
		Deposit(2, 1).Expect(MRX_EStashStatus.REJECTED);
		List(2, 0, 0);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnCustomStep(int index)
	{
		if (index == 6)
		{
			m_World.m_aItems[2].m_iHolderPlayerId = 2;
			return;
		}

		MRX_TestRejectPrefabValidator validator = new MRX_TestRejectPrefabValidator();
		validator.m_sRejected = MRX_StashTests.PREFAB_MAP;
		m_Stash.AddValidator(validator);
	}

	//------------------------------------------------------------------------------------------------
	override protected void CheckEnd()
	{
		Check(!m_World.m_aItems[1].m_bDeleted, "rejected item stays in the world");
		Check(!m_World.m_aItems[2].m_bDeleted, "item of another owner stays in the world");
	}
}

//------------------------------------------------------------------------------------------------
//! No room, spawn failure, wrong states and the stash limit.
class MRX_Test_StashWithdrawFailures : MRX_StashScenarioTest
{
	//------------------------------------------------------------------------------------------------
	override protected void DefineSteps()
	{
		Grant(1, MRX_StashTests.PREFAB_RIFLE, "g1");		// asset 0
		Grant(1, MRX_StashTests.PREFAB_RIFLE, "g2");		// asset 1
		Grant(1, MRX_StashTests.PREFAB_RIFLE, "g3");		// asset 2
		Grant(1, MRX_StashTests.PREFAB_RIFLE, "g4").Expect(MRX_EStashStatus.STASH_FULL);
		Custom();											// no space
		Withdraw(1, 0).Expect(MRX_EStashStatus.NO_SPACE);
		Custom();											// spawn fails
		Withdraw(1, 0).Expect(MRX_EStashStatus.SPAWN_FAILED);
		List(1, 3, 0);
		Custom();											// world works again
		Withdraw(1, 0);										// item 0
		Withdraw(1, 0).Expect(MRX_EStashStatus.INVALID_STATE);
		Withdraw(1, 7).Expect(MRX_EStashStatus.UNKNOWN_ASSET);
		Grant(1, string.Empty, "g5").Expect(MRX_EStashStatus.INVALID_PREFAB);
		Grant(1, MRX_StashTests.PREFAB_RIFLE, string.Empty).Expect(MRX_EStashStatus.INVALID_CONTEXT);
		Custom();											// map item for player 1
		Deposit(1, 1).Expect(MRX_EStashStatus.STASH_FULL);	// 3 assets, one deployed
		List(1, 2, 1);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnCustomStep(int index)
	{
		if (index == 4)
			m_World.m_bNoSpace = true;
		else if (index == 6)
		{
			m_World.m_bNoSpace = false;
			m_World.m_bFailSpawn = true;
		}
		else if (index == 9)
			m_World.m_bFailSpawn = false;
		else
			m_World.AddItem(1, MRX_StashTests.PREFAB_MAP);
	}

	//------------------------------------------------------------------------------------------------
	override protected void CheckEnd()
	{
		Check(!m_World.m_aItems[1].m_bDeleted, "item of a full stash stays in the world");
	}
}

//------------------------------------------------------------------------------------------------
//! Storage failures after the world changed: deposit gives the item back, withdraw removes the spawned item.
class MRX_Test_StashCommitFailures : MRX_StashScenarioTest
{
	//------------------------------------------------------------------------------------------------
	override protected void DefineSteps()
	{
		m_World.AddItem(1, MRX_StashTests.PREFAB_MAP, 3);	// item 0
		Grant(1, MRX_StashTests.PREFAB_RIFLE, "g1");		// asset 0
		Custom();											// commits fail
		Deposit(1, 0).Expect(MRX_EStashStatus.STORAGE_ERROR);	// item 1 = returned map
		Withdraw(1, 0).Expect(MRX_EStashStatus.STORAGE_ERROR);	// item 2 spawned and removed
		Custom();											// commits work, checks
		List(1, 1, 0);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnCustomStep(int index)
	{
		if (index == 1)
		{
			m_Backend.m_bFailApply = true;
			return;
		}

		m_Backend.m_bFailApply = false;
		Check(m_World.m_aItems[0].m_bDeleted, "deposited item was removed before the commit");
		MRX_TestAssetItem returned = m_World.FindLive(1, MRX_StashTests.PREFAB_MAP);
		Check(returned != null, "map returned after the failed deposit");
		if (returned)
			CheckInt(returned.m_iAmmo, 3, "returned map keeps its state");

		Check(m_World.FindLive(1, MRX_StashTests.PREFAB_RIFLE) == null, "rifle of the failed withdraw removed");
		CheckInt(m_World.CountLive(), 1, "live items");
		if (m_World.m_aItems.Count() > 2)
			CheckString(m_Stash.GetAssetId(m_World.m_aItems[2]), string.Empty, "failed withdraw unbound");
	}
}

//------------------------------------------------------------------------------------------------
//! MarkConsumed and Remove for consumer mods, with their context rules.
class MRX_Test_StashConsumerApi : MRX_StashScenarioTest
{
	//------------------------------------------------------------------------------------------------
	override protected void DefineSteps()
	{
		Grant(1, MRX_StashTests.PREFAB_RIFLE, "g1");			// asset 0
		Grant(1, MRX_StashTests.PREFAB_MAP, "g2");				// asset 1
		Withdraw(1, 0);											// item 0
		Consume(0, string.Empty).Expect(MRX_EStashStatus.INVALID_CONTEXT);
		Consume(0, "used");
		Consume(0, "used-again").Expect(MRX_EStashStatus.UNKNOWN_ASSET);
		RemoveAsset(1, 1, string.Empty).Expect(MRX_EStashStatus.INVALID_CONTEXT);
		RemoveAsset(1, 1, "sold");
		RemoveAsset(1, 1, "sold-again").Expect(MRX_EStashStatus.UNKNOWN_ASSET);
		List(1, 0, 0);
	}

	//------------------------------------------------------------------------------------------------
	override protected void CheckEnd()
	{
		CheckString(m_Stash.GetAssetId(m_World.m_aItems[0]), string.Empty, "consumed item unbound");
		Check(!m_World.m_aItems[0].m_bDeleted, "MarkConsumed leaves the item to the caller");
		// grant, grant, withdraw, consume, remove
		CheckInt(m_iEvents, 5, "state change events");
	}
}

//------------------------------------------------------------------------------------------------
//! A vanished item is LOST while its owner is online and restored to the stash while the owner is offline.
class MRX_Test_StashVanishedItems : MRX_StashScenarioTest
{
	//------------------------------------------------------------------------------------------------
	override protected void DefineSteps()
	{
		Grant(1, MRX_StashTests.PREFAB_RIFLE, "g1");	// asset 0
		Grant(1, MRX_StashTests.PREFAB_MAP, "g2");		// asset 1
		Withdraw(1, 0);									// item 0
		Withdraw(1, 1);									// item 1
		Custom();										// item 0 destroyed, owner online
		List(1, 0, 1);
		Custom();										// owner leaves, item 1 deleted with the character
		AddStep(MRX_EStashStepKind.LIST, 0).m_sKey = "stash-a";
		Custom();										// checks
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnCustomStep(int index)
	{
		if (index == 4)
		{
			m_World.m_aItems[0].m_bDeleted = true;
			m_Stash.CheckBindings();
			m_Stash.CheckBindings();	// reported once
			return;
		}

		if (index == 6)
		{
			m_Identity.SimulateDisconnect(1);
			m_World.m_aItems[1].m_bDeleted = true;
			m_Stash.CheckBindings();
			return;
		}

		CheckString(m_Stash.GetAssetId(m_World.m_aItems[1]), string.Empty, "restored asset unbound");
	}

	//------------------------------------------------------------------------------------------------
	override protected void NextStep()
	{
		// LIST by owner ID for a player who left.
		int next = m_iStep + 1;
		if (next < m_aSteps.Count() && m_aSteps[next].m_eKind == MRX_EStashStepKind.LIST && !m_aSteps[next].m_sKey.IsEmpty())
		{
			m_iStep = next;
			m_aSteps[next].m_iExpectedStashed = 1;
			m_Stash.List(m_aSteps[next].m_sKey, m_ListCallback);
			return;
		}

		super.NextStep();
	}
}

//------------------------------------------------------------------------------------------------
//! KEEP marks the items on the body (lost when they vanish, even offline), LOSE loses at once, RETURN stores the current state.
class MRX_Test_StashDeathPolicies : MRX_StashScenarioTest
{
	//------------------------------------------------------------------------------------------------
	override protected void DefineSteps()
	{
		Grant(1, MRX_StashTests.PREFAB_RIFLE, "g1");	// asset 0
		Grant(1, MRX_StashTests.PREFAB_RIFLE, "g2");	// asset 1
		Grant(1, MRX_StashTests.PREFAB_MAP, "g3");		// asset 2
		Withdraw(1, 0);									// item 0
		Custom();										// KEEP death, then the body is cleaned up offline
		List(1, 2, 0);
		Withdraw(1, 1);									// item 1
		Custom();										// LOSE death
		List(1, 1, 0);
		Withdraw(1, 2);									// item 2
		Custom();										// RETURN death with a changed item
		List(1, 1, 0);
		Withdraw(1, 2);									// item 3
		Custom();										// checks
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnCustomStep(int index)
	{
		MRX_TestAssetHolder body = MRX_TestAssetHolder.Create(1);
		if (index == 4)
		{
			m_Stash.SetLossPolicy(MRX_LossPolicy.Create(MRX_EDeathAction.KEEP));
			m_Stash.OnCarrierKilled(body);
			m_Identity.SimulateDisconnect(1);
			m_World.m_aItems[0].m_bDeleted = true;
			m_Stash.CheckBindings();
			m_Identity.SimulateAudit(1);
			return;
		}

		if (index == 7)
		{
			m_Stash.SetLossPolicy(MRX_LossPolicy.Create(MRX_EDeathAction.LOSE));
			m_Stash.OnCarrierKilled(body);
			return;
		}

		if (index == 10)
		{
			Check(!m_World.m_aItems[1].m_bDeleted, "LOSE leaves the item as loot");
			CheckString(m_Stash.GetAssetId(m_World.m_aItems[1]), string.Empty, "LOSE unbinds the item");
			m_World.m_aItems[2].m_iAmmo = 4;
			m_Stash.SetLossPolicy(MRX_LossPolicy.Create(MRX_EDeathAction.RETURN));
			m_Stash.OnCarrierKilled(body);
			return;
		}

		Check(m_World.m_aItems[2].m_bDeleted, "RETURN removes the item from the body");
		CheckInt(m_World.m_aItems[3].m_iAmmo, 4, "RETURN stored the state at death");
	}
}

//------------------------------------------------------------------------------------------------
//! Assets deployed in an earlier session: RESTORE puts them back, LOSE removes them; the current session is untouched.
class MRX_Test_StashRecovery : MRX_StashScenarioTest
{
	protected ref MRX_StashResultCallback m_SeedCallback;

	//------------------------------------------------------------------------------------------------
	override protected void DefineSteps()
	{
		Custom();										// one asset deployed in an old session
		Grant(1, MRX_StashTests.PREFAB_MAP, "g1");		// asset 0
		Withdraw(1, 0);									// deployed in this session
		Custom();										// RESTORE recovery
		List(1, 1, 1);
		Custom();										// LOSE recovery of another old-session asset
		List(1, 1, 1);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnCustomStep(int index)
	{
		if (index == 0)
		{
			Seed("old-1", "seed-1");
			return;
		}

		if (index == 3)
		{
			m_Stash.SetLossPolicy(MRX_LossPolicy.Create(MRX_EDeathAction.KEEP, MRX_ERecoveryAction.RESTORE));
			m_Stash.RecoverOwner("stash-a");
			return;
		}

		Seed("old-2", "seed-2");
		m_Stash.SetLossPolicy(MRX_LossPolicy.Create(MRX_EDeathAction.KEEP, MRX_ERecoveryAction.LOSE));
		m_Stash.RecoverOwner("stash-a");
	}

	//------------------------------------------------------------------------------------------------
	//! Writes a DEPLOYED asset of another session straight into the backend (as left by a crash).
	protected void Seed(string assetId, string key)
	{
		MRX_AssetRecord asset = MRX_AssetRecord.Create(assetId, MRX_StashTests.PREFAB_RIFLE);
		asset.m_eState = MRX_EAssetState.DEPLOYED;
		asset.m_sDeploySession = "old-session";
		MRX_StashRequest request = MRX_StashTests.CreateRequest("stash-a", key);
		request.m_aChanges.Insert(MRX_AssetChange.Add(asset));
		m_SeedCallback = new MRX_StashResultCallback();
		m_Backend.ApplyStash(request, m_SeedCallback);
	}
}
#endif
