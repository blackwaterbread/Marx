//! Server: inventory changes of a character with an open stash re-check its container (contents of stashed bags).
modded class SCR_InventoryStorageManagerComponent
{
	//------------------------------------------------------------------------------------------------
	override protected void OnItemAdded(BaseInventoryStorageComponent storageOwner, IEntity item)
	{
		super.OnItemAdded(storageOwner, item);
		MRX_NotifyStashSessions();
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnItemRemoved(BaseInventoryStorageComponent storageOwner, IEntity item)
	{
		super.OnItemRemoved(storageOwner, item);
		MRX_NotifyStashSessions();
	}

	//------------------------------------------------------------------------------------------------
	protected void MRX_NotifyStashSessions()
	{
		if (!Replication.IsServer())
			return;

		MRX_StashSessionManager sessions = MRX_StashSessions.Get();
		if (sessions)
			sessions.OnCharacterInventoryChanged(GetOwner());
	}
}
