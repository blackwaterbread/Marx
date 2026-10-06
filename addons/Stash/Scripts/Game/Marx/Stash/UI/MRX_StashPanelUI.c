//! Stash panel of the vanilla inventory: shows every page the stash may fill from the start, and each item on its own
//! slot at its cell of the stash grid (MRX_StashStorageComponent.GetGrid). An item dropped on an empty cell goes there:
//! moved inside the stash, or put in from elsewhere (the move is refused when the cells are taken).
class MRX_StashPanelUI : SCR_InventoryOpenedStorageUI
{
	protected ref MRX_LoadoutBar m_LoadoutBar;
	protected ref MRX_BalancePanel m_BalancePanel;
	protected MRX_ClientWallet m_Wallet;

	//------------------------------------------------------------------------------------------------
	//! Below the items: the loadout slots and the balance (the vicinity panel, which shows it otherwise, is hidden).
	override void Init()
	{
		super.Init();
		if (!MRX_StashStorageComponent.Cast(m_Storage) || !m_widget)
			return;

		m_LoadoutBar = MRX_LoadoutBar.Create(m_widget);
		m_Wallet = MRX_ClientWallet.GetLocal();
		if (!m_Wallet || !MRX_BalancePanel.IsShownInInventory())
			return;

		m_BalancePanel = MRX_BalancePanel.Create(m_widget, null);
		if (m_BalancePanel)
			m_Wallet.GetOnBalanceChanged().Insert(OnBalanceChanged);
	}

	//------------------------------------------------------------------------------------------------
	override event void HandlerDeattached(Widget w)
	{
		if (m_Wallet)
			m_Wallet.GetOnBalanceChanged().Remove(OnBalanceChanged);

		if (m_BalancePanel)
			m_BalancePanel.Stop();

		if (m_LoadoutBar)
			m_LoadoutBar.Stop();

		super.HandlerDeattached(w);
	}

	//------------------------------------------------------------------------------------------------
	MRX_LoadoutBar GetLoadoutBar()
	{
		return m_LoadoutBar;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBalanceChanged(string currency, int balance)
	{
		if (m_BalancePanel)
			m_BalancePanel.OnBalanceChanged(currency, balance);
	}

	//------------------------------------------------------------------------------------------------
	int GetShownPage()
	{
		return m_iLastShownPage;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The cell under the mouse on the shown page, or null when the mouse is not over the grid.
	MRX_StashPlacement GetPlacementUnderMouse()
	{
		int x, y, column, row;
		WidgetManager.GetMousePos(x, y);
		if (!GetCellAt(x, y, column, row))
			return null;

		return MRX_StashPlacement.Create(m_iLastShownPage, column, row);
	}

	//------------------------------------------------------------------------------------------------
	//! Cell under a screen point: the empty cell widget there, or else the grid's rectangle.
	bool GetCellAt(int x, int y, out int column, out int row)
	{
		if (!m_wGrid)
			return false;

		if (m_aEmptySlotWidget)
		{
			array<Widget> widgets = {};
			WidgetManager.TraceWidgets(x, y, m_wGrid, widgets);
			foreach (Widget widget : widgets)
			{
				Widget cell = widget;
				while (cell && !m_aEmptySlotWidget.Contains(cell))
				{
					cell = cell.GetParent();
				}

				if (cell)
				{
					column = GridSlot.GetColumn(cell);
					row = GridSlot.GetRow(cell);
					return true;
				}
			}
		}

		float gridX, gridY, width, height;
		m_wGrid.GetScreenPos(gridX, gridY);
		m_wGrid.GetScreenSize(width, height);
		if (width <= 0 || height <= 0 || x < gridX || y < gridY || x >= gridX + width || y >= gridY + height)
			return false;

		column = (x - gridX) * m_iMaxColumns / width;
		row = (y - gridY) * m_iMaxRows / height;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Every item gets its own slot: its cell is its own.
	override protected int FindItem(notnull InventoryItemComponent pInvItem)
	{
		if (MRX_StashStorageComponent.Cast(m_Storage))
			return -1;

		return super.FindItem(pInvItem);
	}

	//------------------------------------------------------------------------------------------------
	override protected void SortSlots()
	{
		MRX_StashStorageComponent stash = MRX_StashStorageComponent.Cast(m_Storage);
		if (!stash)
		{
			super.SortSlots();
			return;
		}

		// The grid's placements; items not on it yet (before the server's grid arrives) at free cells for now.
		MRX_StashGrid grid = stash.GetGrid();
		MRX_StashGrid layout = new MRX_StashGrid(m_iMaxColumns, m_iMaxRows, stash.GetMaxPages());
		array<SCR_InventorySlotUI> unplaced = {};
		foreach (SCR_InventorySlotUI slot : m_aSlots)
		{
			if (!slot || !slot.GetWidget() || !slot.GetInventoryItemComponent())
				continue;

			string key = MRX_StashStorageComponent.GetItemKey(slot.GetInventoryItemComponent().GetOwner());
			MRX_StashPlacement placement = grid.Get(key);
			if (placement && layout.Place(key, placement, GetSlotWidth(slot), GetSlotHeight(slot), true))
				SetSlotCell(slot, placement);
			else
				unplaced.Insert(slot);
		}

		foreach (SCR_InventorySlotUI unplacedSlot : unplaced)
		{
			string unplacedKey = MRX_StashStorageComponent.GetItemKey(unplacedSlot.GetInventoryItemComponent().GetOwner());
			MRX_StashPlacement free = layout.FindFree(GetSlotWidth(unplacedSlot), GetSlotHeight(unplacedSlot), 0, unplacedKey, true);
			if (free && layout.Place(unplacedKey, free, GetSlotWidth(unplacedSlot), GetSlotHeight(unplacedSlot), true))
				SetSlotCell(unplacedSlot, free);
		}

		m_iNrOfPages = layout.GetPageCount();
	}

	//------------------------------------------------------------------------------------------------
	//! Called by the vanilla menu when an item is dropped on the panel outside item slots, before it moves the item.
	override bool OnDrop(SCR_InventorySlotUI slot)
	{
		bool result = super.OnDrop(slot);
		MRX_StashStorageComponent stash = MRX_StashStorageComponent.Cast(m_Storage);
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		MRX_StashPlacement placement = GetPlacementUnderMouse();
		if (!stash || !controller || !placement || !slot || !slot.GetInventoryItemComponent())
			return result;

		IEntity item = slot.GetInventoryItemComponent().GetOwner();
		string key = MRX_StashStorageComponent.GetItemKey(item);
		if (slot.GetStorageUI() == this)
		{
			// Moved inside the stash, which vanilla ignores: shown at once, the server confirms or puts it back.
			if (!stash.GetGrid().Place(key, placement, GetSlotWidth(slot), GetSlotHeight(slot)))
			{
				SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_INV_DROP_ERROR);
				return result;
			}

			Refresh();
			controller.MRX_RequestStashPlacement(item, placement);
			return result;
		}

		// Put in from elsewhere: the move that follows goes to this cell, or is refused when it is taken.
		stash.SetPendingPlacement(key, placement);
		controller.MRX_RequestStashPlacement(item, placement);
		return result;
	}

	//------------------------------------------------------------------------------------------------
	protected void SetSlotCell(notnull SCR_InventorySlotUI slot, notnull MRX_StashPlacement placement)
	{
		Widget widget = slot.GetWidget();
		GridSlot.SetColumn(widget, placement.m_iColumn);
		GridSlot.SetRow(widget, placement.m_iRow);
		GridSlot.SetColumnSpan(widget, GetSlotWidth(slot));
		GridSlot.SetRowSpan(widget, GetSlotHeight(slot));
		slot.SetPage(placement.m_iPage);
	}

	//------------------------------------------------------------------------------------------------
	protected static int GetSlotWidth(notnull SCR_InventorySlotUI slot)
	{
		return Math.Max(1, slot.GetColumnSize());
	}

	//------------------------------------------------------------------------------------------------
	protected static int GetSlotHeight(notnull SCR_InventorySlotUI slot)
	{
		return Math.Max(1, slot.GetRowSize());
	}

	//------------------------------------------------------------------------------------------------
	void MRX_StashPanelUI(
		BaseInventoryStorageComponent storage,
		LoadoutAreaType slotID = null,
		SCR_InventoryMenuUI menuManager = null,
		int iPage = 0,
		array<BaseInventoryStorageComponent> aTraverseStorage = null,
		int cols = 6,
		int rows = 3,
		bool fromVicinity = false)
	{
	}
}
