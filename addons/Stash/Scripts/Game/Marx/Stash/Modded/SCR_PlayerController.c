//! Stash container requests: the client asks to open the stash at a stash point, the server fills a container and
//! tells the client to show it in the vanilla inventory. Closing the inventory closes the stash.
modded class SCR_PlayerController
{
	protected static const int MRX_CONTAINER_WAIT_MS = 100;
	protected static const int MRX_CONTAINER_WAIT_LIMIT_MS = 3000;
	//! The vanilla inventory reports closing only for its own close action, so the menu is also watched.
	protected static const int MRX_INVENTORY_WATCH_MS = 500;

	protected RplId m_MRX_StashContainerId;
	protected int m_iMRX_StashContainerWaitMs;
	protected bool m_bMRX_StashInventoryOpen;

	//------------------------------------------------------------------------------------------------
	//! Client: asks the server to open the player's stash at the stash point.
	void MRX_RequestStashOpen(notnull IEntity stashPoint)
	{
		MRX_GetOnStashResult().Remove(MRX_OnStashResultHint);
		MRX_GetOnStashResult().Insert(MRX_OnStashResultHint);
		Rpc(MRX_RpcAsk_StashOpen, SCR_EntityHelper.EntityToRplId(stashPoint));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: tells the owning client to show the filled container.
	void MRX_SendStashContainerOpen(IEntity container)
	{
		Rpc(MRX_RpcDo_StashContainerOpen, SCR_EntityHelper.EntityToRplId(container));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: tells the owning client that the stash closed.
	void MRX_SendStashContainerClose()
	{
		Rpc(MRX_RpcDo_StashContainerClose);
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
	protected void MRX_RpcDo_StashContainerOpen(RplId containerId)
	{
		m_MRX_StashContainerId = containerId;
		m_iMRX_StashContainerWaitMs = 0;
		MRX_OpenStashContainer();
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void MRX_RpcDo_StashContainerClose()
	{
		GetGame().GetCallqueue().Remove(MRX_OpenStashContainer);
		m_MRX_StashContainerId = RplId.Invalid();
		if (!m_bMRX_StashInventoryOpen)
			return;

		MRX_StopStashInventory(false);
		SCR_InventoryStorageManagerComponent manager = MRX_GetLocalInventoryManager();
		if (manager)
			manager.CloseInventory();
	}

	//------------------------------------------------------------------------------------------------
	//! Client: opens the vanilla inventory on the container once it has replicated.
	protected void MRX_OpenStashContainer()
	{
		IEntity container = SCR_EntityHelper.RplIdToEntity(m_MRX_StashContainerId);
		SCR_InventoryStorageManagerComponent manager = MRX_GetLocalInventoryManager();
		if (!container || !manager)
		{
			m_iMRX_StashContainerWaitMs += MRX_CONTAINER_WAIT_MS;
			if (m_iMRX_StashContainerWaitMs < MRX_CONTAINER_WAIT_LIMIT_MS)
				GetGame().GetCallqueue().CallLater(MRX_OpenStashContainer, MRX_CONTAINER_WAIT_MS);
			else
				Rpc(MRX_RpcAsk_StashClose);

			return;
		}

		manager.m_OnInventoryOpenInvoker.Remove(MRX_OnInventoryOpenChanged);
		manager.m_OnInventoryOpenInvoker.Insert(MRX_OnInventoryOpenChanged);
		manager.SetStorageToOpen(container);
		manager.OpenInventory();
		m_bMRX_StashInventoryOpen = true;
		GetGame().GetCallqueue().Remove(MRX_WatchStashInventory);
		GetGame().GetCallqueue().CallLater(MRX_WatchStashInventory, MRX_INVENTORY_WATCH_MS, true);
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
		if (m_bMRX_StashInventoryOpen && !GetGame().GetMenuManager().FindMenuByPreset(ChimeraMenuPreset.Inventory20Menu))
			MRX_StopStashInventory(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the stash inventory is gone. \param notifyServer False when the server closed the stash itself.
	protected void MRX_StopStashInventory(bool notifyServer)
	{
		m_bMRX_StashInventoryOpen = false;
		GetGame().GetCallqueue().Remove(MRX_WatchStashInventory);
		SCR_InventoryStorageManagerComponent manager = MRX_GetLocalInventoryManager();
		if (manager)
			manager.m_OnInventoryOpenInvoker.Remove(MRX_OnInventoryOpenChanged);

		if (notifyServer)
			Rpc(MRX_RpcAsk_StashClose);
	}

	//------------------------------------------------------------------------------------------------
	protected void MRX_OnStashResultHint(MRX_EStashStatus status, string assetId)
	{
		if (status != MRX_EStashStatus.OK)
			SCR_HintManagerComponent.ShowCustomHint(typename.EnumToString(MRX_EStashStatus, status), "Stash", 4);
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
