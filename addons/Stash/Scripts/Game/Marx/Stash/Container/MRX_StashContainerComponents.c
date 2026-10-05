// Personal stash container: an entity without model that holds a player's stashed items while the stash is open, shown
// in its own panel of the vanilla inventory (see MRX_StashSession). These components report item moves, stay out of the
// vicinity list and keep other characters out.

void MRX_StashContainerMoveDelegate(MRX_StashStorageComponent storage, IEntity item, bool added);
typedef func MRX_StashContainerMoveDelegate;

[ComponentEditorProps(category: "Marx", description: "Storage of the Marx stash container")]
class MRX_StashStorageComponentClass : SCR_UniversalInventoryStorageComponentClass
{
}

//! Storage of the stash container. Reports every item added or removed (after the move, on every machine that runs it).
//! Never listed in the vicinity: the user's client opens it as its own inventory panel.
class MRX_StashStorageComponent : SCR_UniversalInventoryStorageComponent
{
	//! Size of one page of the stash panel, in inventory cells.
	static const int PAGE_COLUMNS = 6;
	static const int PAGE_ROWS = 8;

	[Attribute("3", UIWidgets.EditBox, "Pages of the stash panel the items may fill. Every item counts with its cell size, identical items too. 0 = no limit.", params: "0 inf")]
	protected int m_iMaxPages;

	protected static ref ScriptInvokerBase<MRX_StashContainerMoveDelegate> s_OnItemMoved;

	//! Client: entity shown as the panel's preview (the stash point). Weak.
	protected IEntity m_PreviewEntity;
	//! Server: stashed items are being restored; the page limit does not apply to them.
	protected bool m_bRestoring;

	//------------------------------------------------------------------------------------------------
	static ScriptInvokerBase<MRX_StashContainerMoveDelegate> GetOnItemMoved()
	{
		if (!s_OnItemMoved)
			s_OnItemMoved = new ScriptInvokerBase<MRX_StashContainerMoveDelegate>();

		return s_OnItemMoved;
	}

	//------------------------------------------------------------------------------------------------
	void SetPreviewEntity(IEntity entity)
	{
		m_PreviewEntity = entity;
	}

	//------------------------------------------------------------------------------------------------
	IEntity GetPreviewEntity()
	{
		return m_PreviewEntity;
	}

	//------------------------------------------------------------------------------------------------
	//! \param maxPages 0 = no limit.
	void SetMaxPages(int maxPages)
	{
		m_iMaxPages = maxPages;
	}

	//------------------------------------------------------------------------------------------------
	//! \return 0 = no limit.
	int GetMaxPages()
	{
		return m_iMaxPages;
	}

	//------------------------------------------------------------------------------------------------
	int GetMaxCells()
	{
		return m_iMaxPages * PAGE_COLUMNS * PAGE_ROWS;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: while true, items are put in regardless of the page limit (already stashed items always show).
	void SetRestoring(bool restoring)
	{
		m_bRestoring = restoring;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Cells taken by the items in the container.
	int GetUsedCells()
	{
		int cells;
		array<InventoryItemComponent> ownedItems = {};
		GetOwnedItems(ownedItems, false);
		foreach (InventoryItemComponent itemComponent : ownedItems)
		{
			cells += GetCellCount(itemComponent);
		}

		return cells;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Inventory cells the item takes, as laid out by the vanilla inventory (SCR_InventorySlotUI).
	static int GetCellCount(InventoryItemComponent itemComponent)
	{
		SCR_ItemAttributeCollection attributes;
		if (itemComponent)
			attributes = SCR_ItemAttributeCollection.Cast(itemComponent.GetAttributes());

		if (!attributes)
			return 1;

		switch (attributes.GetItemSize())
		{
			case ESlotSize.SLOT_2x1: return 2;
			case ESlotSize.SLOT_2x2: return 4;
			case ESlotSize.SLOT_3x3: return 9;
		}

		return 1;
	}

	//------------------------------------------------------------------------------------------------
	override protected bool ShouldHideInVicinity()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanStoreItem(IEntity item, int slotID)
	{
		if (!super.CanStoreItem(item, slotID))
			return false;

		// Page limit, on the server and on the client (which then refuses the drop right away).
		if (m_iMaxPages > 0 && !m_bRestoring && !Contains(item))
		{
			InventoryItemComponent itemComponent = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
			if (GetUsedCells() + GetCellCount(itemComponent) > GetMaxCells())
				return false;
		}

		// Server: items carried by another character may not be put in. Clients do not know the user.
		MRX_StashContainerManagerComponent manager = MRX_StashContainerManagerComponent.Cast(GetOwner().FindComponent(MRX_StashContainerManagerComponent));
		if (!manager || !manager.GetUser())
			return true;

		IEntity root = item.GetRootParent();
		return root == manager.GetUser() || !ChimeraCharacter.Cast(root);
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
		// Without a user (closing) nobody may take items out.
		return !m_User || !instigatorManager || instigatorManager.GetOwner() != m_User;
	}
}
