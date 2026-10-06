//! Stash container requests: the client asks to open the stash at a stash point, the server fills a container and
//! tells the client to show it as a panel of the vanilla inventory. Closing the inventory or the panel closes the stash.
modded class SCR_PlayerController
{
	protected static const int MRX_CONTAINER_WAIT_MS = 100;
	protected static const int MRX_CONTAINER_WAIT_LIMIT_MS = 3000;
	//! The vanilla inventory reports closing only for its own close action, so the menu is also watched.
	protected static const int MRX_INVENTORY_WATCH_MS = 500;

	protected RplId m_MRX_StashContainerId;
	protected int m_iMRX_StashContainerWaitMs;
	protected bool m_bMRX_StashInventoryOpen;
	//! Client: the stash point of the last open request. Weak.
	protected IEntity m_MRX_StashPoint;
	//! Client: the server's grid of the open stash (MRX_StashStorageComponent.GetPlacementsText), kept until the
	//! container has replicated.
	protected string m_sMRX_StashPlacements;
	//! Client: pages of the open stash (the owner's, 0 = no limit), kept until the container has replicated.
	protected int m_iMRX_StashPages = -1;

	//------------------------------------------------------------------------------------------------
	//! Client: asks the server to open the player's stash at the stash point.
	void MRX_RequestStashOpen(notnull IEntity stashPoint)
	{
		m_MRX_StashPoint = stashPoint;
		MRX_GetOnStashResult().Remove(MRX_OnStashResultHint);
		MRX_GetOnStashResult().Insert(MRX_OnStashResultHint);
		Rpc(MRX_RpcAsk_StashOpen, SCR_EntityHelper.EntityToRplId(stashPoint));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: tells the owning client to show the filled container. \param maxPages Pages of the owner's stash.
	void MRX_SendStashContainerOpen(IEntity container, int maxPages)
	{
		Rpc(MRX_RpcDo_StashContainerOpen, SCR_EntityHelper.EntityToRplId(container), maxPages);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: tells the owning client that the stash closed.
	void MRX_SendStashContainerClose()
	{
		Rpc(MRX_RpcDo_StashContainerClose);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sends the grid and the pages of the open stash to the owning client.
	void MRX_SendStashPlacements(string placements, int maxPages)
	{
		Rpc(MRX_RpcDo_StashPlacements, placements, maxPages);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: asks to put an item of the open stash, or one about to be put in, at a placement.
	void MRX_RequestStashPlacement(notnull IEntity item, notnull MRX_StashPlacement placement)
	{
		Rpc(MRX_RpcAsk_StashPlace, SCR_EntityHelper.EntityToRplId(item), placement.m_iPage, placement.m_iColumn, placement.m_iRow);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MRX_RpcAsk_StashPlace(RplId itemId, int page, int column, int row)
	{
		MRX_StashSessionManager sessions = MRX_StashSessions.Get();
		MRX_StashSession session;
		if (sessions)
			session = sessions.Find(GetPlayerId());

		// The grid checks the cells; out of range or taken cells are refused there.
		if (session)
			session.RequestPlacement(itemId, MRX_StashPlacement.Create(page, column, row));
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void MRX_RpcDo_StashPlacements(string placements, int maxPages)
	{
		m_sMRX_StashPlacements = placements;
		m_iMRX_StashPages = maxPages;
		MRX_ApplyStashPlacements();
	}

	//------------------------------------------------------------------------------------------------
	//! Client: shows the server's grid. A hosting server's grid is the same object and already up to date.
	protected void MRX_ApplyStashPlacements()
	{
		MRX_StashStorageComponent storage = MRX_GetStashStorage();
		if (!storage)
			return;

		if (!Replication.IsServer())
		{
			if (m_iMRX_StashPages >= 0)
				storage.SetMaxPages(m_iMRX_StashPages);

			storage.ApplyPlacementsText(m_sMRX_StashPlacements);
		}

		GetGame().GetCallqueue().Remove(MRX_RefreshStashPanel);
		GetGame().GetCallqueue().CallLater(MRX_RefreshStashPanel);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MRX_RpcAsk_StashOpen(RplId stashPointId)
	{
		MRX_EStashStatus status = MRX_CheckStashRequest(stashPointId);
		MRX_StashSessionManager sessions = MRX_StashSessions.Get();
		if (status == MRX_EStashStatus.OK && !sessions)
			status = MRX_EStashStatus.STORAGE_ERROR;

		if (status == MRX_EStashStatus.OK)
			status = sessions.Open(GetPlayerId(), SCR_EntityHelper.RplIdToEntity(stashPointId));

		if (status != MRX_EStashStatus.OK)
			MRX_SendStashResult(status, string.Empty);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MRX_RpcAsk_StashClose()
	{
		MRX_StashSessionManager sessions = MRX_StashSessions.Get();
		if (sessions)
			sessions.Close(GetPlayerId(), "client closed the inventory");
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void MRX_RpcDo_StashContainerOpen(RplId containerId, int maxPages)
	{
		m_MRX_StashContainerId = containerId;
		m_iMRX_StashPages = maxPages;
		m_sMRX_StashPlacements = string.Empty;
		m_iMRX_StashContainerWaitMs = 0;
		MRX_OpenStashContainer();
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void MRX_RpcDo_StashContainerClose()
	{
		GetGame().GetCallqueue().Remove(MRX_OpenStashContainer);
		GetGame().GetCallqueue().Remove(MRX_ShowStashPanel);
		MRX_StashStorageComponent storage = MRX_GetStashStorage();
		m_MRX_StashContainerId = RplId.Invalid();
		MRX_SetStashItemOfInterest(null);
		if (!m_bMRX_StashInventoryOpen)
			return;

		// Only the stash panel goes; the inventory stays open.
		MRX_StopStashInventory(false);
		SCR_InventoryMenuUI menu = MRX_GetInventoryMenu();
		if (menu && menu.GetOpenedStorage(storage))
			menu.GetOpenedStorage(storage).CloseStorage();
	}

	//------------------------------------------------------------------------------------------------
	//! Client: opens the vanilla inventory once the container has replicated.
	protected void MRX_OpenStashContainer()
	{
		MRX_StashStorageComponent storage = MRX_GetStashStorage();
		SCR_InventoryStorageManagerComponent manager = MRX_GetLocalInventoryManager();
		if (!storage || !manager)
		{
			if (!MRX_WaitForStash())
				return;

			GetGame().GetCallqueue().CallLater(MRX_OpenStashContainer, MRX_CONTAINER_WAIT_MS);
			return;
		}

		storage.SetPreviewEntity(m_MRX_StashPoint);
		if (!Replication.IsServer() && m_iMRX_StashPages >= 0)
			storage.SetMaxPages(m_iMRX_StashPages);

		MRX_SetStashItemOfInterest(storage.GetOwner());
		manager.m_OnInventoryOpenInvoker.Remove(MRX_OnInventoryOpenChanged);
		manager.m_OnInventoryOpenInvoker.Insert(MRX_OnInventoryOpenChanged);
		manager.OpenInventory();
		m_iMRX_StashContainerWaitMs = 0;
		GetGame().GetCallqueue().CallLater(MRX_ShowStashPanel, MRX_CONTAINER_WAIT_MS);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: adds the stash panel to the open inventory. The container stays out of the vicinity list.
	protected void MRX_ShowStashPanel()
	{
		MRX_StashStorageComponent storage = MRX_GetStashStorage();
		SCR_InventoryMenuUI menu = MRX_GetInventoryMenu();
		if (!storage || !menu)
		{
			if (MRX_WaitForStash())
				GetGame().GetCallqueue().CallLater(MRX_ShowStashPanel, MRX_CONTAINER_WAIT_MS);

			return;
		}

		if (!Replication.IsServer() && m_iMRX_StashPages >= 0)
			storage.SetMaxPages(m_iMRX_StashPages);

		if (!Replication.IsServer() && !m_sMRX_StashPlacements.IsEmpty())
			storage.ApplyPlacementsText(m_sMRX_StashPlacements);

		menu.MRX_OpenStashPanel(storage);
		m_bMRX_StashInventoryOpen = true;
		MRX_StashStorageComponent.GetOnItemMoved().Remove(MRX_OnStashItemMoved);
		MRX_StashStorageComponent.GetOnItemMoved().Insert(MRX_OnStashItemMoved);
		GetGame().GetCallqueue().Remove(MRX_WatchStashInventory);
		GetGame().GetCallqueue().CallLater(MRX_WatchStashInventory, MRX_INVENTORY_WATCH_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: counts the waiting time for the container or the menu; past the limit the stash is closed.
	//! \return True while waiting may continue.
	protected bool MRX_WaitForStash()
	{
		m_iMRX_StashContainerWaitMs += MRX_CONTAINER_WAIT_MS;
		if (m_iMRX_StashContainerWaitMs < MRX_CONTAINER_WAIT_LIMIT_MS)
			return true;

		MRX_StopStashInventory(true);
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void MRX_OnInventoryOpenChanged(bool open)
	{
		if (!open && m_bMRX_StashInventoryOpen)
			MRX_StopStashInventory(true);
	}

	//------------------------------------------------------------------------------------------------
	protected void MRX_WatchStashInventory()
	{
		SCR_InventoryMenuUI menu = MRX_GetInventoryMenu();
		if (m_bMRX_StashInventoryOpen && (!menu || !menu.GetOpenedStorage(MRX_GetStashStorage())))
			MRX_StopStashInventory(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the server moves items too (refused moves are undone), which the panel does not notice on its own.
	protected void MRX_OnStashItemMoved(MRX_StashStorageComponent storage, IEntity item, bool added)
	{
		if (storage != MRX_GetStashStorage())
			return;

		GetGame().GetCallqueue().Remove(MRX_RefreshStashPanel);
		GetGame().GetCallqueue().CallLater(MRX_RefreshStashPanel);
	}

	//------------------------------------------------------------------------------------------------
	protected void MRX_RefreshStashPanel()
	{
		SCR_InventoryMenuUI menu = MRX_GetInventoryMenu();
		if (menu && menu.GetOpenedStorage(MRX_GetStashStorage()))
			menu.GetOpenedStorage(MRX_GetStashStorage()).Refresh();
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the stash inventory is gone. \param notifyServer False when the server closed the stash itself.
	protected void MRX_StopStashInventory(bool notifyServer)
	{
		m_bMRX_StashInventoryOpen = false;
		GetGame().GetCallqueue().Remove(MRX_WatchStashInventory);
		GetGame().GetCallqueue().Remove(MRX_RefreshStashPanel);
		MRX_StashStorageComponent.GetOnItemMoved().Remove(MRX_OnStashItemMoved);
		MRX_SetStashItemOfInterest(null);
		SCR_InventoryStorageManagerComponent manager = MRX_GetLocalInventoryManager();
		if (manager)
			manager.m_OnInventoryOpenInvoker.Remove(MRX_OnInventoryOpenChanged);

		if (notifyServer)
			Rpc(MRX_RpcAsk_StashClose);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: makes the container the vicinity's item of interest, as the vanilla loot action does with a body, so
	//! that its items replicate to a remote client (the container stays out of the vicinity list).
	//! \param container Null clears the item of interest if it is a stash container.
	protected void MRX_SetStashItemOfInterest(IEntity container)
	{
		IEntity character = GetControlledEntity();
		if (!character)
			return;

		CharacterVicinityComponent vicinity = CharacterVicinityComponent.Cast(character.FindComponent(CharacterVicinityComponent));
		if (!vicinity)
			return;

		if (container)
		{
			vicinity.SetItemOfInterest(container);
			return;
		}

		IEntity current = vicinity.GetItemOfInterest();
		if (current && current.FindComponent(MRX_StashStorageComponent))
			vicinity.SetItemOfInterest(null);
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_StashStorageComponent MRX_GetStashStorage()
	{
		IEntity container = SCR_EntityHelper.RplIdToEntity(m_MRX_StashContainerId);
		if (!container)
			return null;

		return MRX_StashStorageComponent.Cast(container.FindComponent(MRX_StashStorageComponent));
	}

	//------------------------------------------------------------------------------------------------
	protected static SCR_InventoryMenuUI MRX_GetInventoryMenu()
	{
		return SCR_InventoryMenuUI.Cast(GetGame().GetMenuManager().FindMenuByPreset(ChimeraMenuPreset.Inventory20Menu));
	}

	//------------------------------------------------------------------------------------------------
	protected void MRX_OnStashResultHint(MRX_EStashStatus status, string assetId)
	{
		if (status != MRX_EStashStatus.OK)
			SCR_HintManagerComponent.ShowCustomHint(MRX_GetStashFailureText(status), "#MRX-Stash_Title", 4);
	}

	//------------------------------------------------------------------------------------------------
	protected static string MRX_GetStashFailureText(MRX_EStashStatus status)
	{
		switch (status)
		{
			case MRX_EStashStatus.OWNER_NOT_READY: return "#MRX-Common_OwnerNotReady";
			case MRX_EStashStatus.STASH_FULL: return "#MRX-Stash_Full";
			case MRX_EStashStatus.NO_SPACE: return "#MRX-Common_NoSpace";
			case MRX_EStashStatus.BUSY: return "#MRX-Common_PleaseWait";
			case MRX_EStashStatus.REJECTED: return "#MRX-Stash_Rejected";
			case MRX_EStashStatus.NOT_OWNED: return "#MRX-Stash_NotOwned";
			case MRX_EStashStatus.NOT_IN_INVENTORY: return "#MRX-Stash_NotStorable";
		}

		return WidgetManager.Translate("#MRX-Common_Failed", typename.EnumToString(MRX_EStashStatus, status));
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_InventoryStorageManagerComponent MRX_GetLocalInventoryManager()
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(GetControlledEntity());
		if (!character || !character.GetCharacterController())
			return null;

		return SCR_InventoryStorageManagerComponent.Cast(character.GetCharacterController().GetInventoryStorageManager());
	}
}
