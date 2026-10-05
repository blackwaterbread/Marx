// Personal stash container (work in progress): a hidden entity that holds a player's stashed items while the stash is open,
// shown through the vanilla inventory. These components report item moves and keep other characters out.

void MRX_StashContainerMoveDelegate(MRX_StashStorageComponent storage, IEntity item, bool added);
typedef func MRX_StashContainerMoveDelegate;

[ComponentEditorProps(category: "Marx", description: "Storage of the Marx stash container")]
class MRX_StashStorageComponentClass : SCR_UniversalInventoryStorageComponentClass
{
}

//! Storage of the stash container. Reports every item added or removed (after the move, on every machine that runs it).
class MRX_StashStorageComponent : SCR_UniversalInventoryStorageComponent
{
	protected static ref ScriptInvokerBase<MRX_StashContainerMoveDelegate> s_OnItemMoved;

	//------------------------------------------------------------------------------------------------
	static ScriptInvokerBase<MRX_StashContainerMoveDelegate> GetOnItemMoved()
	{
		if (!s_OnItemMoved)
			s_OnItemMoved = new ScriptInvokerBase<MRX_StashContainerMoveDelegate>();

		return s_OnItemMoved;
	}

	//------------------------------------------------------------------------------------------------
	protected override void OnAddedToSlot(IEntity item, int slotID)
	{
		super.OnAddedToSlot(item, slotID);
		if (s_OnItemMoved)
			s_OnItemMoved.Invoke(this, item, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnRemovedFromSlot(IEntity item, int slotID)
	{
		super.OnRemovedFromSlot(item, slotID);
		if (s_OnItemMoved)
			s_OnItemMoved.Invoke(this, item, false);
	}
}

[ComponentEditorProps(category: "Marx", description: "Inventory manager of the Marx stash container: only its user may take items out")]
class MRX_StashContainerManagerComponentClass : ScriptedInventoryStorageManagerComponentClass
{
}

//! Inventory manager of the stash container. Server: only the character set with SetUser() may take items out.
class MRX_StashContainerManagerComponent : ScriptedInventoryStorageManagerComponent
{
	//! Weak: the character of the player who opened the stash.
	protected IEntity m_User;

	//------------------------------------------------------------------------------------------------
	void SetUser(IEntity user)
	{
		m_User = user;
	}

	//------------------------------------------------------------------------------------------------
	IEntity GetUser()
	{
		return m_User;
	}

	//------------------------------------------------------------------------------------------------
	override protected bool ShouldForbidRemoveByInstigator(InventoryStorageManagerComponent instigatorManager, BaseInventoryStorageComponent fromStorage, IEntity item)
	{
		bool forbid = !instigatorManager || instigatorManager.GetOwner() != m_User;
		Print(string.Format("[MRX_STASH] remove check: instigator %1, user %2, forbid %3", instigatorManager, m_User, forbid));
		return forbid;
	}
}
