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
//! Keeps the items on a grid of pages (MRX_StashGrid, keyed by the items' replication IDs): an item put in takes the
//! placement requested for it (SetPendingPlacement) or the first free one. The server sends its grid to the user's
//! client, which shows the items there.
class MRX_StashStorageComponent : SCR_UniversalInventoryStorageComponent
{
	//! Size of one page of the stash panel, in inventory cells.
	static const int PAGE_COLUMNS = 6;
	static const int PAGE_ROWS = 8;

	[Attribute("3", UIWidgets.EditBox, "Pages of the stash panel the items may fill. 0 = no limit.", params: "0 inf")]
	protected int m_iMaxPages;

	protected static ref ScriptInvokerBase<MRX_StashContainerMoveDelegate> s_OnItemMoved;

	//! Client: entity shown as the panel's preview (the stash point). Weak.
	protected IEntity m_PreviewEntity;
	//! Server: stashed items are being restored; they may go behind the page limit.
	protected bool m_bRestoring;
	protected ref MRX_StashGrid m_Grid;
	//! Placements requested for items about to be put in, by item key.
	protected ref map<string, ref MRX_StashPlacement> m_mPendingPlacements = new map<string, ref MRX_StashPlacement>();

	//------------------------------------------------------------------------------------------------
	static ScriptInvokerBase<MRX_StashContainerMoveDelegate> GetOnItemMoved()
	{
		if (!s_OnItemMoved)
			s_OnItemMoved = new ScriptInvokerBase<MRX_StashContainerMoveDelegate>();

		return s_OnItemMoved;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Key of the item on the grid (its replication ID), or empty.
	static string GetItemKey(IEntity item)
	{
		if (!item)
			return string.Empty;

		RplId id = SCR_EntityHelper.EntityToRplId(item);
		if (!id.IsValid())
			return string.Empty;

		return id.AsString();
	}

	//------------------------------------------------------------------------------------------------
	//! Cells the item takes, as laid out by the vanilla inventory (SCR_InventorySlotUI.SetSlotSize).
	static void GetCellSize(InventoryItemComponent itemComponent, out int width, out int height)
	{
		width = 1;
		height = 1;
		SCR_ItemAttributeCollection attributes;
		if (itemComponent)
			attributes = SCR_ItemAttributeCollection.Cast(itemComponent.GetAttributes());

		if (!attributes)
			return;

		switch (attributes.GetItemSize())
		{
			case ESlotSize.SLOT_2x1: { width = 2; height = 1; break; }
			case ESlotSize.SLOT_2x2: { width = 2; height = 2; break; }
			case ESlotSize.SLOT_3x3: { width = 3; height = 3; break; }
		}
	}

	//------------------------------------------------------------------------------------------------
	static void GetItemCellSize(IEntity item, out int width, out int height)
	{
		InventoryItemComponent itemComponent;
		if (item)
			itemComponent = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));

		GetCellSize(itemComponent, width, height);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Inventory cells the item takes.
	static int GetCellCount(InventoryItemComponent itemComponent)
	{
		int width, height;
		GetCellSize(itemComponent, width, height);
		return width * height;
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
		if (m_Grid)
			m_Grid.SetMaxPages(maxPages);
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
	//! Server: while true, items put in may go behind the page limit (already stashed items always show).
	void SetRestoring(bool restoring)
	{
		m_bRestoring = restoring;
	}

	//------------------------------------------------------------------------------------------------
	MRX_StashGrid GetGrid()
	{
		if (!m_Grid)
			m_Grid = new MRX_StashGrid(PAGE_COLUMNS, PAGE_ROWS, m_iMaxPages);

		return m_Grid;
	}

	//------------------------------------------------------------------------------------------------
	//! The next time this item is put in, it goes to this placement, or is refused when the cells are taken.
	void SetPendingPlacement(string key, MRX_StashPlacement placement)
	{
		if (key.IsEmpty())
			return;

		if (placement)
			m_mPendingPlacements.Set(key, placement);
		else
			m_mPendingPlacements.Remove(key);
	}

	//------------------------------------------------------------------------------------------------
	void ClearPendingPlacements()
	{
		m_mPendingPlacements.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! \return Where the item would go if put in now, or null when there is no room (or its requested cells are taken).
	MRX_StashPlacement FindPlacementFor(IEntity item)
	{
		string key = GetItemKey(item);
		int width, height;
		GetItemCellSize(item, width, height);
		MRX_StashPlacement pending = m_mPendingPlacements.Get(key);
		if (pending)
		{
			if (GetGrid().IsFree(pending.m_iPage, pending.m_iColumn, pending.m_iRow, width, height, key))
				return pending;

			return null;
		}

		return GetGrid().FindFree(width, height, 0, key);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the grid as text for the client, "key=page,column,row,width,height;..." (see ApplyPlacementsText).
	string GetPlacementsText()
	{
		array<string> entries = {};
		array<string> keys = {};
		GetGrid().GetKeys(keys);
		foreach (string key : keys)
		{
			MRX_StashGridEntry entry = GetGrid().GetEntry(key);
			entries.Insert(string.Format("%1=%2,%3,%4", key, entry.m_Placement.Format(), entry.m_iWidth, entry.m_iHeight));
		}

		return SCR_StringHelper.Join(";", entries);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: replaces the grid with the server's (see GetPlacementsText). Malformed entries are skipped.
	void ApplyPlacementsText(string text)
	{
		MRX_StashGrid grid = GetGrid();
		grid.Clear();
		array<string> entries = {};
		text.Split(";", entries, true);
		foreach (string entry : entries)
		{
			array<string> keyAndCells = {};
			entry.Split("=", keyAndCells, false);
			if (keyAndCells.Count() != 2)
				continue;

			array<string> numbers = {};
			keyAndCells[1].Split(",", numbers, false);
			if (numbers.Count() != 5)
				continue;

			MRX_StashPlacement placement = MRX_StashPlacement.Parse(string.Format("%1,%2,%3", numbers[0], numbers[1], numbers[2]));
			if (placement)
				grid.Place(keyAndCells[0], placement, numbers[3].ToInt(), numbers[4].ToInt(), true);
		}
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
	override protected bool ShouldHideInVicinity()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanStoreItem(IEntity item, int slotID)
	{
		if (!super.CanStoreItem(item, slotID))
			return false;

		// Room on the grid, on the server and on the client (which then refuses the drop right away).
		if (!m_bRestoring && !Contains(item) && !FindPlacementFor(item))
			return false;

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
		PlaceOnGrid(item);
		if (s_OnItemMoved)
			s_OnItemMoved.Invoke(this, item, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnRemovedFromSlot(IEntity item, int slotID)
	{
		super.OnRemovedFromSlot(item, slotID);
		GetGrid().Remove(GetItemKey(item));
		if (s_OnItemMoved)
			s_OnItemMoved.Invoke(this, item, false);
	}

	//------------------------------------------------------------------------------------------------
	//! An item put in takes its requested placement or the first free one; nothing put in is ever left off the grid.
	protected void PlaceOnGrid(IEntity item)
	{
		string key = GetItemKey(item);
		if (key.IsEmpty() || GetGrid().Contains(key))
			return;

		int width, height;
		GetItemCellSize(item, width, height);
		MRX_StashPlacement pending = m_mPendingPlacements.Get(key);
		m_mPendingPlacements.Remove(key);
		if (pending && GetGrid().Place(key, pending, width, height, m_bRestoring))
			return;

		MRX_StashPlacement free = GetGrid().FindFree(width, height, 0, key, true);
		if (free)
			GetGrid().Place(key, free, width, height, true);
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
