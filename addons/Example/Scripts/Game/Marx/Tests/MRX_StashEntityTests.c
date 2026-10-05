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
		runner.Add(new MRX_Test_StashPlacementFlow());
		runner.Add(new MRX_Test_StashBagMerge());
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
	protected string m_sNewAssetId;
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
		SCR_InventoryMenuUI menu = SCR_InventoryMenuUI.Cast(GetGame().GetMenuManager().FindMenuByPreset(ChimeraMenuPreset.Inventory20Menu));
		bool panelOpen = menu && session && menu.GetOpenedStorage(session.GetStorage()) != null;
		if (!session || !session.IsReady() || !panelOpen)
		{
			m_iWaitedMs += STEP_MS;
			if (m_iWaitedMs < WAIT_LIMIT_MS)
			{
				GetGame().GetCallqueue().CallLater(WaitOpen, STEP_MS);
				return;
			}

			Check(false, string.Format("stash opens as an inventory panel (session %1, inventory %2, panel %3)", session != null, menu != null, panelOpen));
			End(false, string.Empty);
			return;
		}

		// Assets are found by ID: the player's stash may hold other items of the same prefabs.
		if (m_bReopened)
		{
			Check(session.FindShownItem(m_sAssetId) != null, "stored item shown again after reopening");
			Check(!m_sNewAssetId.IsEmpty() && session.FindShownItem(m_sNewAssetId) != null, "new item shown again after reopening");
			Close();
			GetGame().GetCallqueue().CallLater(CheckClosedForCleanup, STEP_MS * 2);
			return;
		}

		m_Item = session.FindShownItem(m_sAssetId);
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
			MRX_StashSession session = GetSession();
			foreach (MRX_AssetRecord asset : record.m_aAssets)
			{
				if (m_NewItem && session && asset.m_eState == MRX_EAssetState.STASHED && session.FindShownItem(asset.m_sId) == m_NewItem)
					m_sNewAssetId = asset.m_sId;
			}

			Check(!m_sNewAssetId.IsEmpty() && m_sNewAssetId != m_sAssetId, "new item stored as a new asset");
			if (!m_sNewAssetId.IsEmpty() && m_sNewAssetId != m_sAssetId)
				m_aCleanupIds.Insert(m_sNewAssetId);
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

//------------------------------------------------------------------------------------------------
//! Stash cells through the session: an item moved inside the open stash and an item put in at a requested cell keep
//! their cells in the records and after reopening; a taken cell refuses an item.
class MRX_Test_StashPlacementFlow : MRX_TestCase
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
	protected string m_sNewAssetId;
	protected IEntity m_NewItem;
	protected IEntity m_Extra;
	protected int m_iWaitedMs;
	protected bool m_bReopened;
	protected ref MRX_StashResultCallback m_ServiceCallback;
	protected ref MRX_StashCallback m_ListCallback;
	protected ref array<string> m_aCleanupIds = {};

	//------------------------------------------------------------------------------------------------
	override int GetTimeoutMs()
	{
		return 40000;
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
		m_ServiceCallback = new MRX_StashResultCallback();
		m_ServiceCallback.GetOnResult().Insert(OnGranted);
		MRX_Marx.GetStash().Grant(m_sOwnerId, ITEM_PREFAB, MRX_TxContext.Create("test", "stash placement flow", MRX_NetworkTests.UniqueKey("stash-place")), m_ServiceCallback);
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
		MRX_StashPanelUI panel = GetPanel();
		if (!session || !session.IsReady() || !panel)
		{
			m_iWaitedMs += STEP_MS;
			if (m_iWaitedMs < WAIT_LIMIT_MS)
			{
				GetGame().GetCallqueue().CallLater(WaitOpen, STEP_MS);
				return;
			}

			Check(false, "stash opens as an inventory panel");
			End(false, string.Empty);
			return;
		}

		if (m_bReopened)
		{
			CheckReopened(session, panel);
			return;
		}

		IEntity item = session.FindShownItem(m_sAssetId);
		Check(item != null, "granted item shown");
		if (!item)
		{
			End(false, string.Empty);
			return;
		}

		// As the panel does for an item dropped inside the stash.
		m_Controller.MRX_RequestStashPlacement(item, MRX_StashPlacement.Create(2, 4, 5));
		GetCharacterManager().TrySpawnPrefabToStorage(NEW_ITEM_PREFAB);
		GetCharacterManager().TrySpawnPrefabToStorage(ITEM_PREFAB);
		GetGame().GetCallqueue().CallLater(PutNewItem, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void PutNewItem()
	{
		MRX_StashSession session = GetSession();
		MRX_StashStorageComponent storage = session.GetStorage();
		Check(Is(storage.GetGrid().Get(GetKey(session.FindShownItem(m_sAssetId))), 2, 4, 5), "item moved to page 3, column 5, row 6");

		array<IEntity> items = {};
		m_Possession.GetItems(items);
		foreach (IEntity item : items)
		{
			string prefab = SCR_ResourceNameUtils.GetPrefabName(item);
			if (prefab == NEW_ITEM_PREFAB)
				m_NewItem = item;
			else if (prefab == ITEM_PREFAB)
				m_Extra = item;
		}

		Check(m_NewItem && m_Extra, "items in the character inventory");
		if (!m_NewItem || !m_Extra)
		{
			End(false, string.Empty);
			return;
		}

		// As the panel does for an item dropped on a cell from elsewhere.
		MRX_StashPlacement requested = MRX_StashPlacement.Create(1, 0, 3);
		storage.SetPendingPlacement(GetKey(m_NewItem), requested);
		m_Controller.MRX_RequestStashPlacement(m_NewItem, requested);
		GetCharacterManager().TryMoveItemToStorage(m_NewItem, storage);

		storage.SetPendingPlacement(GetKey(m_Extra), MRX_StashPlacement.Create(2, 4, 5));
		Check(!GetCharacterManager().CanMoveItemToStorage(m_Extra, storage), "a taken cell refuses an item");
		storage.SetPendingPlacement(GetKey(m_Extra), null);
		GetGame().GetCallqueue().CallLater(ListPlacements, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void ListPlacements()
	{
		MRX_StashStorageComponent storage = GetSession().GetStorage();
		Check(storage.Contains(m_NewItem), "new item put in");
		Check(Is(storage.GetGrid().Get(GetKey(m_NewItem)), 1, 0, 3), "new item at the requested cell");

		m_ListCallback = new MRX_StashCallback();
		m_ListCallback.GetOnResult().Insert(OnListed);
		MRX_Marx.GetStash().List(m_sOwnerId, m_ListCallback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnListed(MRX_EStashStatus status, MRX_StashRecord record)
	{
		Check(record != null, "stash listed");
		MRX_StashSession session = GetSession();
		if (record && session)
		{
			MRX_AssetRecord moved = record.FindAsset(m_sAssetId);
			CheckString(moved.m_sPlacement, "2,4,5", "moved item's cell stored");
			foreach (MRX_AssetRecord asset : record.m_aAssets)
			{
				if (session.FindShownItem(asset.m_sId) == m_NewItem)
					m_sNewAssetId = asset.m_sId;
			}

			Check(!m_sNewAssetId.IsEmpty(), "new item stored");
			if (!m_sNewAssetId.IsEmpty())
			{
				m_aCleanupIds.Insert(m_sNewAssetId);
				CheckString(record.FindAsset(m_sNewAssetId).m_sPlacement, "1,0,3", "new item's cell stored");
			}
		}

		Close();
		m_iWaitedMs = 0;
		GetGame().GetCallqueue().CallLater(WaitClosed, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void WaitClosed()
	{
		m_iWaitedMs += STEP_MS;
		if (GetSession() && m_iWaitedMs < WAIT_LIMIT_MS)
		{
			GetGame().GetCallqueue().CallLater(WaitClosed, STEP_MS);
			return;
		}

		Check(GetSession() == null, "stash closed");
		m_bReopened = true;
		Open();
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckReopened(MRX_StashSession session, MRX_StashPanelUI panel)
	{
		IEntity moved = session.FindShownItem(m_sAssetId);
		IEntity putIn = session.FindShownItem(m_sNewAssetId);
		MRX_StashGrid grid = session.GetStorage().GetGrid();
		Check(moved && Is(grid.Get(GetKey(moved)), 2, 4, 5), "moved item back at its cell after reopening");
		Check(putIn && Is(grid.Get(GetKey(putIn)), 1, 0, 3), "put in item back at its cell after reopening");

		array<SCR_InventorySlotUI> slots = {};
		panel.GetSlots(slots);
		foreach (SCR_InventorySlotUI slot : slots)
		{
			if (!slot || !slot.GetInventoryItemComponent() || slot.GetInventoryItemComponent().GetOwner() != moved)
				continue;

			Widget widget = slot.GetWidget();
			Check(slot.GetPage() == 2 && GridSlot.GetColumn(widget) == 4 && GridSlot.GetRow(widget) == 5, "panel shows the item at its cell");
		}

		Close();
		GetGame().GetCallqueue().CallLater(CleanUp, STEP_MS * 2);
	}

	//------------------------------------------------------------------------------------------------
	protected void CleanUp()
	{
		m_ServiceCallback = new MRX_StashResultCallback();
		m_ServiceCallback.GetOnResult().Insert(OnCleanedUp);
		foreach (string assetId : m_aCleanupIds)
		{
			MRX_Marx.GetStash().Remove(m_sOwnerId, assetId, MRX_TxContext.Create("test", "stash placement cleanup", MRX_NetworkTests.UniqueKey("stash-place")), m_ServiceCallback);
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
	protected void Close()
	{
		GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.Inventory20Menu);
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_StashSession GetSession()
	{
		return MRX_StashSessions.Get().Find(m_Controller.GetPlayerId());
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_StashPanelUI GetPanel()
	{
		SCR_InventoryMenuUI menu = SCR_InventoryMenuUI.Cast(GetGame().GetMenuManager().FindMenuByPreset(ChimeraMenuPreset.Inventory20Menu));
		MRX_StashSession session = GetSession();
		if (!menu || !session)
			return null;

		return MRX_StashPanelUI.Cast(menu.GetOpenedStorage(session.GetStorage()));
	}

	//------------------------------------------------------------------------------------------------
	protected InventoryStorageManagerComponent GetCharacterManager()
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(m_Possession.GetCharacter());
		return character.GetCharacterController().GetInventoryStorageManager();
	}

	//------------------------------------------------------------------------------------------------
	protected static string GetKey(IEntity item)
	{
		return MRX_StashStorageComponent.GetItemKey(item);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Is(MRX_StashPlacement placement, int page, int column, int row)
	{
		return placement && placement.Equals(MRX_StashPlacement.Create(page, column, row));
	}

	//------------------------------------------------------------------------------------------------
	protected void End(bool skip, string reason)
	{
		Close();
		if (m_Extra && !m_Extra.IsDeleted())
			SCR_EntityHelper.DeleteEntityAndChildren(m_Extra);

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

//------------------------------------------------------------------------------------------------
//! Items going into a bag in the open stash become part of the bag: a stashed dressing moved into it and a deployed
//! compass put into it lose their own assets, the bag's snapshot holds them, and they are in the bag after reopening.
//! Items coming out become assets again: the dressing moved to the stash is STASHED, the compass taken is DEPLOYED.
class MRX_Test_StashBagMerge : MRX_TestCase
{
	static const ResourceName STASH_PREFAB = "{66BACE8BD545B8C2}Prefabs/Marx/Stash/MRX_StashWardrobe.et";
	static const ResourceName BAG_PREFAB = "{06B68C58B72EAAC6}Prefabs/Items/Equipment/Backpacks/Backpack_ALICE_Medium.et";
	static const ResourceName DRESSING_PREFAB = "{A81F501D3EF6F38E}Prefabs/Items/Medicine/FieldDressing_01/FieldDressing_US_01.et";
	static const ResourceName COMPASS_PREFAB = "{61D4F80E49BF9B12}Prefabs/Items/Equipment/Compass/Compass_SY183.et";
	protected static const int STEP_MS = 700;
	protected static const int WAIT_LIMIT_MS = 6000;

	protected ref MRX_TestPossession m_Possession;
	protected SCR_PlayerController m_Controller;
	protected IEntity m_StashPoint;
	protected string m_sOwnerId;
	protected string m_sBagId;
	protected string m_sDressingId;
	protected string m_sCompassId;
	protected IEntity m_Compass;
	protected IEntity m_Dressing;
	protected int m_iWaitedMs;
	protected bool m_bReopened;
	protected ref MRX_StashResultCallback m_ServiceCallback;
	protected ref MRX_StashCallback m_ListCallback;
	protected ref array<string> m_aCleanupIds = {};

	//------------------------------------------------------------------------------------------------
	override int GetTimeoutMs()
	{
		return 45000;
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
		m_ServiceCallback = new MRX_StashResultCallback();
		m_ServiceCallback.GetOnResult().Insert(OnBagGranted);
		MRX_Marx.GetStash().Grant(m_sOwnerId, BAG_PREFAB, CreateContext(), m_ServiceCallback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBagGranted(MRX_StashResult result)
	{
		Check(result.m_eStatus == MRX_EStashStatus.OK && result.GetAsset(), "bag granted");
		if (!result.GetAsset())
		{
			End(false, string.Empty);
			return;
		}

		m_sBagId = result.GetAsset().m_sId;
		m_ServiceCallback = new MRX_StashResultCallback();
		m_ServiceCallback.GetOnResult().Insert(OnDressingGranted);
		MRX_Marx.GetStash().Grant(m_sOwnerId, DRESSING_PREFAB, CreateContext(), m_ServiceCallback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnDressingGranted(MRX_StashResult result)
	{
		Check(result.m_eStatus == MRX_EStashStatus.OK && result.GetAsset(), "dressing granted");
		if (!result.GetAsset())
		{
			End(false, string.Empty);
			return;
		}

		m_sDressingId = result.GetAsset().m_sId;
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
		if (!session || !session.IsReady())
		{
			m_iWaitedMs += STEP_MS;
			if (m_iWaitedMs < WAIT_LIMIT_MS)
			{
				GetGame().GetCallqueue().CallLater(WaitOpen, STEP_MS);
				return;
			}

			Check(false, "stash opens");
			End(false, string.Empty);
			return;
		}

		IEntity bag = session.FindShownItem(m_sBagId);
		if (m_bReopened)
		{
			m_Dressing = FindInside(bag, DRESSING_PREFAB);
			m_Compass = FindInside(bag, COMPASS_PREFAB);
			Check(m_Dressing && m_Compass, "bag holds both items after reopening");
			if (!m_Dressing || !m_Compass)
			{
				Close();
				GetGame().GetCallqueue().CallLater(CleanUp, STEP_MS * 2);
				return;
			}

			// Out of the bag: the dressing into the stash, the compass into the character inventory.
			Check(GetCharacterManager().TryMoveItemToStorage(m_Dressing, session.GetStorage()), "dressing moved out of the bag into the stash");
			BaseInventoryStorageComponent target = GetCharacterManager().FindStorageForItem(m_Compass);
			Check(target && GetCharacterManager().TryMoveItemToStorage(m_Compass, target), "compass taken out of the bag");
			GetGame().GetCallqueue().CallLater(ListSplit, STEP_MS);
			return;
		}

		IEntity dressing = session.FindShownItem(m_sDressingId);
		Check(bag && dressing, "bag and dressing shown");
		if (!bag || !dressing)
		{
			End(false, string.Empty);
			return;
		}

		Check(GetCharacterManager().TryMoveItemToStorage(dressing, GetBagStorage(bag)), "dressing moved into the bag");
		GetCharacterManager().TrySpawnPrefabToStorage(COMPASS_PREFAB);
		GetGame().GetCallqueue().CallLater(StoreCompass, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void StoreCompass()
	{
		array<IEntity> items = {};
		m_Possession.GetItems(items);
		foreach (IEntity item : items)
		{
			if (SCR_ResourceNameUtils.GetPrefabName(item) == COMPASS_PREFAB)
				m_Compass = item;
		}

		Check(m_Compass != null, "compass in the character inventory");
		if (!m_Compass)
		{
			End(false, string.Empty);
			return;
		}

		GetCharacterManager().TryMoveItemToStorage(m_Compass, GetSession().GetStorage());
		GetGame().GetCallqueue().CallLater(TakeCompass, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void TakeCompass()
	{
		InventoryStorageManagerComponent manager = GetCharacterManager();
		BaseInventoryStorageComponent target = manager.FindStorageForItem(m_Compass);
		Check(target && manager.TryMoveItemToStorage(m_Compass, target), "compass taken out of the stash");
		GetGame().GetCallqueue().CallLater(PutCompassIntoBag, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void PutCompassIntoBag()
	{
		m_sCompassId = MRX_Marx.GetStash().GetAssetId(m_Compass);
		Check(!m_sCompassId.IsEmpty(), "taken compass is a deployed asset");
		IEntity bag = GetSession().FindShownItem(m_sBagId);
		Check(bag && GetCharacterManager().TryMoveItemToStorage(m_Compass, GetBagStorage(bag)), "deployed compass put into the bag");
		GetGame().GetCallqueue().CallLater(ListMerged, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void ListMerged()
	{
		CheckString(MRX_Marx.GetStash().GetAssetId(m_Compass), string.Empty, "compass no longer bound");
		m_ListCallback = new MRX_StashCallback();
		m_ListCallback.GetOnResult().Insert(OnListedMerged);
		MRX_Marx.GetStash().List(m_sOwnerId, m_ListCallback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnListedMerged(MRX_EStashStatus status, MRX_StashRecord record)
	{
		Check(record != null, "stash listed");
		if (record)
		{
			Check(record.FindAsset(m_sDressingId) == null, "stashed dressing merged into the bag");
			Check(m_sCompassId.IsEmpty() || record.FindAsset(m_sCompassId) == null, "deployed compass merged into the bag");
			MRX_AssetRecord bag = record.FindAsset(m_sBagId);
			Check(bag && bag.m_eState == MRX_EAssetState.STASHED, "bag still stashed");
			Check(bag && SnapshotHolds(bag.m_Snapshot, DRESSING_PREFAB) && SnapshotHolds(bag.m_Snapshot, COMPASS_PREFAB), "bag snapshot holds both items");
		}

		Close();
		m_iWaitedMs = 0;
		GetGame().GetCallqueue().CallLater(WaitClosed, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void WaitClosed()
	{
		m_iWaitedMs += STEP_MS;
		if (GetSession() && m_iWaitedMs < WAIT_LIMIT_MS)
		{
			GetGame().GetCallqueue().CallLater(WaitClosed, STEP_MS);
			return;
		}

		m_bReopened = true;
		Open();
	}

	//------------------------------------------------------------------------------------------------
	protected void ListSplit()
	{
		m_ListCallback = new MRX_StashCallback();
		m_ListCallback.GetOnResult().Insert(OnListedSplit);
		MRX_Marx.GetStash().List(m_sOwnerId, m_ListCallback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnListedSplit(MRX_EStashStatus status, MRX_StashRecord record)
	{
		Check(record != null, "stash listed after taking items out");
		MRX_StashSession session = GetSession();
		if (record && session)
		{
			MRX_AssetRecord bag = record.FindAsset(m_sBagId);
			Check(bag && !SnapshotHolds(bag.m_Snapshot, DRESSING_PREFAB) && !SnapshotHolds(bag.m_Snapshot, COMPASS_PREFAB), "bag snapshot without the items taken out");

			string dressingId;
			foreach (MRX_AssetRecord asset : record.m_aAssets)
			{
				if (session.FindShownItem(asset.m_sId) == m_Dressing)
					dressingId = asset.m_sId;
			}

			Check(!dressingId.IsEmpty() && record.FindAsset(dressingId).m_eState == MRX_EAssetState.STASHED, "dressing out of the bag is a stashed asset");
			if (!dressingId.IsEmpty())
				m_aCleanupIds.Insert(dressingId);

			string compassId = MRX_Marx.GetStash().GetAssetId(m_Compass);
			MRX_AssetRecord compass = record.FindAsset(compassId);
			Check(compass && compass.m_eState == MRX_EAssetState.DEPLOYED, "compass taken out of the bag is a deployed asset");
		}

		// The compass is used up: its asset goes, then the entity.
		MRX_Marx.GetStash().MarkConsumed(m_Compass, CreateContext());
		SCR_EntityHelper.DeleteEntityAndChildren(m_Compass);
		Close();
		GetGame().GetCallqueue().CallLater(CleanUp, STEP_MS * 2);
	}

	//------------------------------------------------------------------------------------------------
	protected void CleanUp()
	{
		m_aCleanupIds.Insert(m_sBagId);
		m_ServiceCallback = new MRX_StashResultCallback();
		m_ServiceCallback.GetOnResult().Insert(OnCleanedUp);
		foreach (string assetId : m_aCleanupIds)
		{
			MRX_Marx.GetStash().Remove(m_sOwnerId, assetId, CreateContext(), m_ServiceCallback);
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
	protected static bool SnapshotHolds(MRX_ItemSnapshot snapshot, ResourceName prefab)
	{
		if (!snapshot)
			return false;

		foreach (MRX_ItemSnapshot child : snapshot.m_aChildren)
		{
			if (child.m_sPrefab == prefab || SnapshotHolds(child, prefab))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity FindInside(IEntity bag, ResourceName prefab)
	{
		BaseInventoryStorageComponent storage;
		if (bag)
			storage = GetBagStorage(bag);

		if (!storage)
			return null;

		array<IEntity> items = {};
		storage.GetAll(items);
		foreach (IEntity item : items)
		{
			if (SCR_ResourceNameUtils.GetPrefabName(item) == prefab)
				return item;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static BaseInventoryStorageComponent GetBagStorage(IEntity bag)
	{
		return SCR_UniversalInventoryStorageComponent.Cast(bag.FindComponent(SCR_UniversalInventoryStorageComponent));
	}

	//------------------------------------------------------------------------------------------------
	protected static MRX_TxContext CreateContext()
	{
		return MRX_TxContext.Create("test", "stash bag merge", MRX_NetworkTests.UniqueKey("stash-bag"));
	}

	//------------------------------------------------------------------------------------------------
	protected void Close()
	{
		GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.Inventory20Menu);
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
