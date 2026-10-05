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
//! The stash wardrobe with the vanilla-inventory container: open, take a stashed item out, put it back, store a new
//! item, close, reopen and find both, then clean up. Item moves use the inventory manager like the vanilla UI does.
class MRX_Test_StashPointFlow : MRX_TestCase
{
	static const ResourceName STASH_PREFAB = "{66BACE8BD545B8C2}Prefabs/Marx/Stash/MRX_StashWardrobe.et";
	static const ResourceName ITEM_PREFAB = "{A81F501D3EF6F38E}Prefabs/Items/Medicine/FieldDressing_01/FieldDressing_US_01.et";
	static const ResourceName NEW_ITEM_PREFAB = "{61D4F80E49BF9B12}Prefabs/Items/Equipment/Compass/Compass_SY183.et";
	protected static const int STEP_MS = 700;
	protected static const int WAIT_LIMIT_MS = 6000;

	protected ref MRX_TestPossession m_Possession;
	protected SCR_PlayerController m_Controller;
	protected IEntity m_StashPoint;
	protected string m_sOwnerId;
	protected string m_sAssetId;
	protected IEntity m_Item;
	protected IEntity m_NewItem;
	protected int m_iWaitedMs;
	protected bool m_bReopened;
	protected ref MRX_StashResultCallback m_ServiceCallback;
	protected ref MRX_StashCallback m_ListCallback;
	protected ref array<string> m_aCleanupIds = {};

	//------------------------------------------------------------------------------------------------
	override int GetTimeoutMs()
	{
		return 30000;
	}

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		m_Possession = MRX_TestPossession.Acquire();
		if (!m_Possession || !m_Possession.GetCharacter() || !MRX_Marx.GetStash() || !MRX_StashSessions.Get())
		{
			Skip("no stash service, no stash sessions or no local character");
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
		if (!m_StashPoint)
		{
			Check(false, "stash prefab spawns");
			End(false, string.Empty);
			return;
		}

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
		m_aCleanupIds.Insert(m_sAssetId);
		Open();
	}

	//------------------------------------------------------------------------------------------------
	protected void Open()
	{
		m_iWaitedMs = 0;
		m_Controller.MRX_RequestStashOpen(m_StashPoint);
		GetGame().GetCallqueue().CallLater(WaitOpen, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void WaitOpen()
	{
		MRX_StashSession session = GetSession();
		bool inventoryOpen = GetGame().GetMenuManager().FindMenuByPreset(ChimeraMenuPreset.Inventory20Menu) != null;
		if (!session || !session.IsReady() || !inventoryOpen)
		{
			m_iWaitedMs += STEP_MS;
			if (m_iWaitedMs < WAIT_LIMIT_MS)
			{
				GetGame().GetCallqueue().CallLater(WaitOpen, STEP_MS);
				return;
			}

			Check(false, string.Format("stash opens (session %1, inventory %2)", session != null, inventoryOpen));
			End(false, string.Empty);
			return;
		}

		if (m_bReopened)
		{
			Check(FindInContainer(ITEM_PREFAB) != null, "stored item shown again after reopening");
			Check(FindInContainer(NEW_ITEM_PREFAB) != null, "new item shown again after reopening");
			Close();
			GetGame().GetCallqueue().CallLater(CheckClosedForCleanup, STEP_MS * 2);
			return;
		}

		m_Item = FindInContainer(ITEM_PREFAB);
		Check(m_Item != null, "granted item shown in the container");
		if (!m_Item)
		{
			End(false, string.Empty);
			return;
		}

		MoveToCharacter(m_Item);
		GetGame().GetCallqueue().CallLater(CheckTaken, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckTaken()
	{
		CheckString(MRX_Marx.GetStash().GetAssetId(m_Item), m_sAssetId, "item taken out is bound to its asset (DEPLOYED)");
		InventoryStorageManagerComponent manager = GetCharacterManager();
		manager.TryMoveItemToStorage(m_Item, GetSession().GetStorage());
		GetGame().GetCallqueue().CallLater(CheckPutBack, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckPutBack()
	{
		CheckString(MRX_Marx.GetStash().GetAssetId(m_Item), string.Empty, "item put back is unbound (STASHED)");
		GetCharacterManager().TrySpawnPrefabToStorage(NEW_ITEM_PREFAB);
		GetGame().GetCallqueue().CallLater(PutNewItem, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void PutNewItem()
	{
		array<IEntity> items = {};
		m_Possession.GetItems(items);
		foreach (IEntity item : items)
		{
			if (SCR_ResourceNameUtils.GetPrefabName(item) == NEW_ITEM_PREFAB)
				m_NewItem = item;
		}

		Check(m_NewItem != null, "new item in the character inventory");
		if (m_NewItem)
			GetCharacterManager().TryMoveItemToStorage(m_NewItem, GetSession().GetStorage());

		GetGame().GetCallqueue().CallLater(ListAfterStore, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void ListAfterStore()
	{
		m_ListCallback = new MRX_StashCallback();
		m_ListCallback.GetOnResult().Insert(OnListedAfterStore);
		MRX_Marx.GetStash().List(m_sOwnerId, m_ListCallback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnListedAfterStore(MRX_EStashStatus status, MRX_StashRecord record)
	{
		Check(record != null, "stash listed");
		if (record)
		{
			MRX_AssetRecord granted = record.FindAsset(m_sAssetId);
			Check(granted && granted.m_eState == MRX_EAssetState.STASHED, "granted asset STASHED after put back");
			foreach (MRX_AssetRecord asset : record.m_aAssets)
			{
				if (asset.m_sPrefab == NEW_ITEM_PREFAB && asset.m_eState == MRX_EAssetState.STASHED && !m_aCleanupIds.Contains(asset.m_sId))
					m_aCleanupIds.Insert(asset.m_sId);
			}

			CheckInt(m_aCleanupIds.Count(), 2, "new item stored as a new asset");
		}

		Close();
		m_iWaitedMs = 0;
		GetGame().GetCallqueue().CallLater(CheckClosed, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckClosed()
	{
		m_iWaitedMs += STEP_MS;
		if (GetSession() && m_iWaitedMs < WAIT_LIMIT_MS)
		{
			GetGame().GetCallqueue().CallLater(CheckClosed, STEP_MS);
			return;
		}

		Print(string.Format("[MRX_TEST]   session closed after %1 ms", m_iWaitedMs));
		Check(GetSession() == null, "session closed after the inventory closed");
		Check(!m_Item || m_Item.IsDeleted(), "container contents removed with the container");
		m_bReopened = true;
		Open();
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckClosedForCleanup()
	{
		Check(GetSession() == null, "session closed again");
		m_ServiceCallback = new MRX_StashResultCallback();
		m_ServiceCallback.GetOnResult().Insert(OnCleanedUp);
		foreach (string assetId : m_aCleanupIds)
		{
			MRX_Marx.GetStash().Remove(m_sOwnerId, assetId, MRX_TxContext.Create("test", "stash point cleanup", MRX_NetworkTests.UniqueKey("stash-point")), m_ServiceCallback);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCleanedUp(MRX_StashResult result)
	{
		CheckInt(result.m_eStatus, MRX_EStashStatus.OK, "cleanup remove");
		m_aCleanupIds.RemoveOrdered(0);
		if (m_aCleanupIds.IsEmpty())
			End(false, string.Empty);
	}

	//------------------------------------------------------------------------------------------------
	//! Like closing the inventory window: the client asks the server to close the stash.
	protected void Close()
	{
		GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.Inventory20Menu);
	}

	//------------------------------------------------------------------------------------------------
	protected void MoveToCharacter(IEntity item)
	{
		InventoryStorageManagerComponent manager = GetCharacterManager();
		BaseInventoryStorageComponent target = manager.FindStorageForItem(item);
		Check(target && manager.TryMoveItemToStorage(item, target), "move out of the container accepted");
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity FindInContainer(ResourceName prefab)
	{
		MRX_StashSession session = GetSession();
		if (!session || !session.GetStorage())
			return null;

		array<IEntity> items = {};
		session.GetStorage().GetAll(items);
		foreach (IEntity item : items)
		{
			if (SCR_ResourceNameUtils.GetPrefabName(item) == prefab)
				return item;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_StashSession GetSession()
	{
		return MRX_StashSessions.Get().Find(m_Controller.GetPlayerId());
	}

	//------------------------------------------------------------------------------------------------
	protected InventoryStorageManagerComponent GetCharacterManager()
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(m_Possession.GetCharacter());
		return character.GetCharacterController().GetInventoryStorageManager();
	}

	//------------------------------------------------------------------------------------------------
	protected void End(bool skip, string reason)
	{
		Close();
		if (m_NewItem && !m_NewItem.IsDeleted())
			SCR_EntityHelper.DeleteEntityAndChildren(m_NewItem);

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
