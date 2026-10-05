#ifdef WORKBENCH
// End-to-end stash test with real entities: the Marx system's stash service and the local (host) player's character.

//------------------------------------------------------------------------------------------------
class MRX_StashEntityTests
{
	//------------------------------------------------------------------------------------------------
	static void Register(notnull MRX_TestRunner runner)
	{
		runner.Add(new MRX_Test_StashEntityRoundTrip());
		runner.Add(new MRX_Test_StashPointFlow());
	}
}

//------------------------------------------------------------------------------------------------
//! Grant a rifle -> withdraw it into the inventory -> change the magazine -> deposit -> withdraw again: same contents
//! and ammo. Cleans up by depositing and removing the asset.
class MRX_Test_StashEntityRoundTrip : MRX_TestCase
{
	static const ResourceName RIFLE_PREFAB = "{EF73725A81669E2C}Prefabs/Weapons/Rifles/M16/Rifle_M16A2_carbine_M203.et";
	protected static const int CHANGED_AMMO = 7;
	//! Lets inventory changes settle between steps.
	protected static const int STEP_DELAY_MS = 500;

	protected ref MRX_TestPossession m_Possession;
	protected MRX_StashService m_Stash;
	protected string m_sOwnerId;
	protected int m_iPlayerId;
	protected string m_sAssetId;
	protected int m_iItemCount;
	protected bool m_bHasMagazine;
	protected int m_iStep;
	protected ref MRX_StashResultCallback m_Callback;

	//------------------------------------------------------------------------------------------------
	override int GetTimeoutMs()
	{
		return 20000;
	}

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		m_Stash = MRX_Marx.GetStash();
		m_Possession = MRX_TestPossession.Acquire();
		if (!m_Stash || !m_Possession || !m_Possession.GetCharacter())
		{
			Skip("no stash service or no local character");
			return;
		}

		m_iPlayerId = m_Possession.GetController().GetPlayerId();
		m_sOwnerId = MRX_Marx.GetOwnerId(m_iPlayerId);
		if (m_sOwnerId.IsEmpty())
		{
			End("the local player has no owner (start with -" + MRX_TestRunner.TEST_IDENTITY_PARAM + ")", true);
			return;
		}

		m_Callback = new MRX_StashResultCallback();
		m_Callback.GetOnResult().Insert(OnResult);
		m_Stash.Grant(m_sOwnerId, RIFLE_PREFAB, MRX_TxContext.Create("test", "stash entity round trip", MRX_NetworkTests.UniqueKey("stash-entity")), m_Callback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnResult(MRX_StashResult result)
	{
		string step = string.Format("step %1", m_iStep);
		if (result.m_eStatus != MRX_EStashStatus.OK)
		{
			Check(false, step + " failed: " + typename.EnumToString(MRX_EStashStatus, result.m_eStatus));
			End(string.Empty, false);
			return;
		}

		m_iStep++;
		switch (m_iStep)
		{
			case 1:		// granted
				m_sAssetId = result.GetAsset().m_sId;
				m_Stash.Withdraw(m_iPlayerId, m_sAssetId, m_Callback);
				break;

			case 2:		// withdrawn the first time
				GetGame().GetCallqueue().CallLater(ChangeAndDeposit, STEP_DELAY_MS);
				break;

			case 3:		// deposited with the change
				CheckDepositSnapshot(result.GetAsset());
				m_Stash.Withdraw(m_iPlayerId, m_sAssetId, m_Callback);
				break;

			case 4:		// withdrawn again
				GetGame().GetCallqueue().CallLater(CheckRestoredAndDeposit, STEP_DELAY_MS);
				break;

			case 5:		// deposited for cleanup
				m_Stash.Remove(m_sOwnerId, m_sAssetId, MRX_TxContext.Create("test", "stash entity cleanup", MRX_NetworkTests.UniqueKey("stash-entity")), m_Callback);
				break;

			default:	// removed
				End(string.Empty, false);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void ChangeAndDeposit()
	{
		IEntity rifle = FindBoundRifle();
		Check(rifle != null, "withdrawn rifle is in the inventory and bound to the asset");
		if (!rifle)
		{
			End(string.Empty, false);
			return;
		}

		MRX_ItemSnapshot before = MRX_EntitySnapshots.Capture(rifle);
		m_iItemCount = before.CountItems();
		Print(string.Format("[MRX_TEST]   withdrawn rifle holds %1 items (with itself)", m_iItemCount));
		Check(m_iItemCount > 1, "a granted rifle keeps its default attachments");

		BaseMagazineComponent magazine = GetMagazine(rifle);
		m_bHasMagazine = magazine != null;
		if (magazine)
			magazine.SetAmmoCount(CHANGED_AMMO);
		else
			Print("[MRX_TEST]   the rifle prefab has no magazine, ammo is not checked");

		m_Stash.Deposit(m_iPlayerId, rifle, m_Callback);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckDepositSnapshot(MRX_AssetRecord asset)
	{
		Check(asset && asset.m_eState == MRX_EAssetState.STASHED, "asset STASHED after deposit");
		if (!asset || !asset.m_Snapshot)
		{
			Check(false, "deposit stored a snapshot");
			return;
		}

		CheckInt(asset.m_Snapshot.CountItems(), m_iItemCount, "items in the deposit snapshot");
		Check(FindBoundRifle() == null, "deposited rifle left the inventory");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckRestoredAndDeposit()
	{
		IEntity rifle = FindBoundRifle();
		Check(rifle != null, "rifle withdrawn again");
		if (!rifle)
		{
			End(string.Empty, false);
			return;
		}

		CheckInt(MRX_EntitySnapshots.Capture(rifle).CountItems(), m_iItemCount, "items after the second withdraw");
		BaseMagazineComponent magazine = GetMagazine(rifle);
		if (m_bHasMagazine)
		{
			Check(magazine != null, "magazine restored");
			if (magazine)
				CheckInt(magazine.GetAmmoCount(), CHANGED_AMMO, "ammo restored");
		}

		m_Stash.Deposit(m_iPlayerId, rifle, m_Callback);
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity FindBoundRifle()
	{
		array<IEntity> items = {};
		m_Possession.GetItems(items);
		foreach (IEntity item : items)
		{
			if (m_Stash.GetAssetId(item) == m_sAssetId)
				return item;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static BaseMagazineComponent GetMagazine(notnull IEntity weapon)
	{
		BaseWeaponComponent weaponComponent = BaseWeaponComponent.Cast(weapon.FindComponent(BaseWeaponComponent));
		if (!weaponComponent)
			return null;

		return weaponComponent.GetCurrentMagazine();
	}

	//------------------------------------------------------------------------------------------------
	protected void End(string skipReason, bool skip)
	{
		if (m_Possession)
			m_Possession.Release();

		if (skip)
			Skip(skipReason);
		else
			Finish();
	}
}

//------------------------------------------------------------------------------------------------
//! The stash wardrobe prefab with the player controller RPCs: open and close the menu, list, withdraw and deposit.
class MRX_Test_StashPointFlow : MRX_TestCase
{
	static const ResourceName STASH_PREFAB = "{66BACE8BD545B8C2}Prefabs/Marx/Stash/MRX_StashWardrobe.et";
	static const ResourceName ITEM_PREFAB = "{A81F501D3EF6F38E}Prefabs/Items/Medicine/FieldDressing_01/FieldDressing_US_01.et";
	//! Longer than the controller's request interval.
	protected static const int REQUEST_GAP_MS = 400;

	protected ref MRX_TestPossession m_Possession;
	protected SCR_PlayerController m_Controller;
	protected IEntity m_StashPoint;
	protected string m_sOwnerId;
	protected string m_sAssetId;
	protected int m_iStep;
	//! The menu sends its own list request; only the answer to ours counts.
	protected bool m_bAwaitingList;
	protected ref MRX_StashResultCallback m_ServiceCallback;

	//------------------------------------------------------------------------------------------------
	override int GetTimeoutMs()
	{
		return 20000;
	}

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		m_Possession = MRX_TestPossession.Acquire();
		if (!m_Possession || !m_Possession.GetCharacter() || !MRX_Marx.GetStash())
		{
			Skip("no stash service or no local character");
			return;
		}

		m_Controller = m_Possession.GetController();
		m_sOwnerId = MRX_Marx.GetOwnerId(m_Controller.GetPlayerId());
		if (m_sOwnerId.IsEmpty())
		{
			End(true, "the local player has no owner (start with -" + MRX_TestRunner.TEST_IDENTITY_PARAM + ")");
			return;
		}

		m_StashPoint = MRX_TestWorldUtils.SpawnPrefab(STASH_PREFAB, m_Possession.GetCharacter().GetOrigin() + "1 0 0");
		MRX_StashPointComponent stashPoint;
		if (m_StashPoint)
			stashPoint = MRX_StashPointComponent.Cast(m_StashPoint.FindComponent(MRX_StashPointComponent));

		Check(stashPoint != null, "stash prefab has MRX_StashPointComponent");
		if (!stashPoint)
		{
			End(false, string.Empty);
			return;
		}

		Check(SCR_EntityHelper.EntityToRplId(m_StashPoint) != RplId.Invalid(), "stash point is replicated");
		MRX_StashMenu menu = MRX_StashMenu.Open(stashPoint);
		Check(menu != null, "stash menu opens");
		if (menu)
			menu.Close();

		m_Controller.MRX_GetOnStashList().Insert(OnStashList);
		m_Controller.MRX_GetOnStashResult().Insert(OnStashResult);

		m_ServiceCallback = new MRX_StashResultCallback();
		m_ServiceCallback.GetOnResult().Insert(OnGranted);
		MRX_Marx.GetStash().Grant(m_sOwnerId, ITEM_PREFAB, MRX_TxContext.Create("test", "stash point flow", MRX_NetworkTests.UniqueKey("stash-point")), m_ServiceCallback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnGranted(MRX_StashResult result)
	{
		if (result.m_eStatus != MRX_EStashStatus.OK || !result.GetAsset())
		{
			Check(false, "grant failed: " + typename.EnumToString(MRX_EStashStatus, result.m_eStatus));
			End(false, string.Empty);
			return;
		}

		m_sAssetId = result.GetAsset().m_sId;
		// The menu above already sent a list request.
		GetGame().GetCallqueue().CallLater(RequestList, REQUEST_GAP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void RequestList()
	{
		m_bAwaitingList = true;
		m_Controller.MRX_RequestStashList(m_StashPoint);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnStashList(MRX_EStashStatus status, array<string> assetIds, array<string> prefabs, array<int> states)
	{
		if (!m_bAwaitingList)
			return;

		m_bAwaitingList = false;
		if (status != MRX_EStashStatus.OK)
		{
			Check(false, "list failed: " + typename.EnumToString(MRX_EStashStatus, status));
			End(false, string.Empty);
			return;
		}

		int index = assetIds.Find(m_sAssetId);
		Check(index >= 0, "granted asset in the list");
		if (index >= 0)
			CheckInt(states[index], MRX_EAssetState.STASHED, "listed state");

		GetGame().GetCallqueue().CallLater(RequestWithdraw, REQUEST_GAP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void RequestWithdraw()
	{
		m_Controller.MRX_RequestStashWithdraw(m_StashPoint, m_sAssetId);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnStashResult(MRX_EStashStatus status, string assetId)
	{
		m_iStep++;
		string step = "withdraw";
		if (m_iStep > 1)
			step = "deposit";

		if (status != MRX_EStashStatus.OK)
		{
			Check(false, step + " failed: " + typename.EnumToString(MRX_EStashStatus, status));
			End(false, string.Empty);
			return;
		}

		CheckString(assetId, m_sAssetId, step + " reports the asset");
		if (m_iStep == 1)
		{
			GetGame().GetCallqueue().CallLater(RequestDeposit, REQUEST_GAP_MS);
			return;
		}

		m_ServiceCallback = new MRX_StashResultCallback();
		m_ServiceCallback.GetOnResult().Insert(OnRemoved);
		MRX_Marx.GetStash().Remove(m_sOwnerId, m_sAssetId, MRX_TxContext.Create("test", "stash point cleanup", MRX_NetworkTests.UniqueKey("stash-point")), m_ServiceCallback);
	}

	//------------------------------------------------------------------------------------------------
	protected void RequestDeposit()
	{
		IEntity item;
		array<IEntity> items = {};
		m_Possession.GetItems(items);
		foreach (IEntity candidate : items)
		{
			if (MRX_Marx.GetStash().GetAssetId(candidate) == m_sAssetId)
				item = candidate;
		}

		Check(item != null, "withdrawn item in the inventory");
		if (!item)
		{
			End(false, string.Empty);
			return;
		}

		m_Controller.MRX_RequestStashDeposit(m_StashPoint, item);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnRemoved(MRX_StashResult result)
	{
		CheckInt(result.m_eStatus, MRX_EStashStatus.OK, "cleanup remove");
		End(false, string.Empty);
	}

	//------------------------------------------------------------------------------------------------
	protected void End(bool skip, string reason)
	{
		if (m_Controller)
		{
			m_Controller.MRX_GetOnStashList().Remove(OnStashList);
			m_Controller.MRX_GetOnStashResult().Remove(OnStashResult);
		}

		if (m_StashPoint)
			SCR_EntityHelper.DeleteEntityAndChildren(m_StashPoint);

		if (m_Possession)
			m_Possession.Release();

		if (skip)
			Skip(reason);
		else
			Finish();
	}
}
#endif
