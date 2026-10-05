//! Cell of the stash grid where an item's top left corner lies. Stored as "page,column,row" (MRX_AssetRecord.m_sPlacement).
class MRX_StashPlacement : Managed
{
	int m_iPage;
	int m_iColumn;
	int m_iRow;

	//------------------------------------------------------------------------------------------------
	static MRX_StashPlacement Create(int page, int column, int row)
	{
		MRX_StashPlacement placement = new MRX_StashPlacement();
		placement.m_iPage = page;
		placement.m_iColumn = column;
		placement.m_iRow = row;
		return placement;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Null when the text is not "page,column,row" with numbers of zero or more.
	static MRX_StashPlacement Parse(string text)
	{
		array<string> parts = {};
		text.Split(",", parts, false);
		if (parts.Count() != 3)
			return null;

		array<int> values = {};
		foreach (string part : parts)
		{
			part.TrimInPlace();
			if (part.IsEmpty() || !SCR_StringHelper.IsFormat(SCR_EStringFormat.DIGITS_ONLY, part))
				return null;

			values.Insert(part.ToInt());
		}

		return Create(values[0], values[1], values[2]);
	}

	//------------------------------------------------------------------------------------------------
	string Format()
	{
		return string.Format("%1,%2,%3", m_iPage, m_iColumn, m_iRow);
	}

	//------------------------------------------------------------------------------------------------
	bool Equals(MRX_StashPlacement other)
	{
		return other && other.m_iPage == m_iPage && other.m_iColumn == m_iColumn && other.m_iRow == m_iRow;
	}
}

//------------------------------------------------------------------------------------------------
//! Item of the stash grid: its placement and size in cells.
class MRX_StashGridEntry : Managed
{
	ref MRX_StashPlacement m_Placement;
	int m_iWidth;
	int m_iHeight;
}

//------------------------------------------------------------------------------------------------
//! Pages of columns x rows cells holding items of a stash by key. Every item covers width x height cells from its
//! placement; items never overlap. Pages up to the page limit are regular; pages behind it only take overflow
//! (items already stashed that no longer fit, e.g. after the limit was lowered).
class MRX_StashGrid : Managed
{
	//! Bound for overflow pages.
	static const int MAX_OVERFLOW_PAGES = 100;

	protected int m_iColumns;
	protected int m_iRows;
	//! 0 = no limit.
	protected int m_iMaxPages;
	protected ref map<string, ref MRX_StashGridEntry> m_mEntries = new map<string, ref MRX_StashGridEntry>();

	//------------------------------------------------------------------------------------------------
	void MRX_StashGrid(int columns, int rows, int maxPages)
	{
		m_iColumns = Math.Max(1, columns);
		m_iRows = Math.Max(1, rows);
		m_iMaxPages = Math.Max(0, maxPages);
	}

	//------------------------------------------------------------------------------------------------
	int GetColumns()
	{
		return m_iColumns;
	}

	//------------------------------------------------------------------------------------------------
	int GetRows()
	{
		return m_iRows;
	}

	//------------------------------------------------------------------------------------------------
	//! \return 0 = no limit.
	int GetMaxPages()
	{
		return m_iMaxPages;
	}

	//------------------------------------------------------------------------------------------------
	//! Items already behind a lowered limit stay where they are.
	void SetMaxPages(int maxPages)
	{
		m_iMaxPages = Math.Max(0, maxPages);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Pages to show: the page limit, or more when items lie behind it.
	int GetPageCount()
	{
		int pages = m_iMaxPages;
		foreach (string key, MRX_StashGridEntry entry : m_mEntries)
		{
			pages = Math.Max(pages, entry.m_Placement.m_iPage + 1);
		}

		return Math.Max(1, pages);
	}

	//------------------------------------------------------------------------------------------------
	MRX_StashPlacement Get(string key)
	{
		MRX_StashGridEntry entry = m_mEntries.Get(key);
		if (!entry)
			return null;

		return entry.m_Placement;
	}

	//------------------------------------------------------------------------------------------------
	MRX_StashGridEntry GetEntry(string key)
	{
		return m_mEntries.Get(key);
	}

	//------------------------------------------------------------------------------------------------
	bool Contains(string key)
	{
		return m_mEntries.Contains(key);
	}

	//------------------------------------------------------------------------------------------------
	int GetKeys(notnull out array<string> keys)
	{
		keys.Clear();
		foreach (string key, MRX_StashGridEntry entry : m_mEntries)
		{
			keys.Insert(key);
		}

		return keys.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! \param ignoreKey An item whose own cells count as free (when moving it).
	//! \param allowOverflow Also pages behind the page limit.
	bool IsFree(int page, int column, int row, int width, int height, string ignoreKey = string.Empty, bool allowOverflow = false)
	{
		if (page < 0 || column < 0 || row < 0 || width < 1 || height < 1 || column + width > m_iColumns || row + height > m_iRows)
			return false;

		if (m_iMaxPages > 0 && page >= m_iMaxPages && (!allowOverflow || page >= m_iMaxPages + MAX_OVERFLOW_PAGES))
			return false;

		foreach (string key, MRX_StashGridEntry entry : m_mEntries)
		{
			if (key == ignoreKey || entry.m_Placement.m_iPage != page)
				continue;

			bool apartX = column + width <= entry.m_Placement.m_iColumn || entry.m_Placement.m_iColumn + entry.m_iWidth <= column;
			bool apartY = row + height <= entry.m_Placement.m_iRow || entry.m_Placement.m_iRow + entry.m_iHeight <= row;
			if (!apartX && !apartY)
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the item at the placement, replacing an earlier placement of the same key.
	//! \return False (and nothing changes) when the cells are not free.
	bool Place(string key, notnull MRX_StashPlacement placement, int width, int height, bool allowOverflow = false)
	{
		if (!IsFree(placement.m_iPage, placement.m_iColumn, placement.m_iRow, width, height, key, allowOverflow))
			return false;

		MRX_StashGridEntry entry = new MRX_StashGridEntry();
		entry.m_Placement = MRX_StashPlacement.Create(placement.m_iPage, placement.m_iColumn, placement.m_iRow);
		entry.m_iWidth = width;
		entry.m_iHeight = height;
		m_mEntries.Set(key, entry);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! First free placement for an item of this size: on the preferred page first, then on the other pages from the
	//! first one, then (with allowOverflow) behind the page limit. Without a page limit, new pages follow the last one.
	//! \return Null when there is no room.
	MRX_StashPlacement FindFree(int width, int height, int preferredPage = 0, string ignoreKey = string.Empty, bool allowOverflow = false)
	{
		int pages = m_iMaxPages;
		if (pages == 0)
			pages = GetPageCount() + 1;

		MRX_StashPlacement placement;
		if (preferredPage >= 0 && preferredPage < pages)
		{
			placement = FindFreeOnPage(preferredPage, width, height, ignoreKey, allowOverflow);
			if (placement)
				return placement;
		}

		int lastPage = pages;
		if (allowOverflow && m_iMaxPages > 0)
			lastPage = m_iMaxPages + MAX_OVERFLOW_PAGES;

		for (int page = 0; page < lastPage; page++)
		{
			if (page == preferredPage)
				continue;

			placement = FindFreeOnPage(page, width, height, ignoreKey, allowOverflow);
			if (placement)
				return placement;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	bool Remove(string key)
	{
		if (!m_mEntries.Contains(key))
			return false;

		m_mEntries.Remove(key);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void Clear()
	{
		m_mEntries.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_StashPlacement FindFreeOnPage(int page, int width, int height, string ignoreKey, bool allowOverflow)
	{
		for (int row = 0; row + height <= m_iRows; row++)
		{
			for (int column = 0; column + width <= m_iColumns; column++)
			{
				if (IsFree(page, column, row, width, height, ignoreKey, allowOverflow))
					return MRX_StashPlacement.Create(page, column, row);
			}
		}

		return null;
	}
}
