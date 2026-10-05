#ifdef WORKBENCH
// Tests of the stash grid (MRX_StashGrid): placements, overlaps, bounds, page limit, overflow, free cell search.

//------------------------------------------------------------------------------------------------
class MRX_StashGridTests
{
	//------------------------------------------------------------------------------------------------
	static void Register(notnull MRX_TestRunner runner)
	{
		runner.Add(new MRX_Test_StashPlacementText());
		runner.Add(new MRX_Test_StashGridPlace());
		runner.Add(new MRX_Test_StashGridFindFree());
	}

	//------------------------------------------------------------------------------------------------
	static bool Is(MRX_StashPlacement placement, int page, int column, int row)
	{
		return placement && placement.Equals(MRX_StashPlacement.Create(page, column, row));
	}
}

//------------------------------------------------------------------------------------------------
class MRX_Test_StashPlacementText : MRX_TestCase
{
	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		MRX_StashPlacement placement = MRX_StashPlacement.Parse("1,2,3");
		Check(placement && placement.m_iPage == 1 && placement.m_iColumn == 2 && placement.m_iRow == 3, "parse page, column, row");
		Check(MRX_StashPlacement.Parse(" 1, 2 ,3") != null, "spaces allowed");
		CheckString(MRX_StashPlacement.Create(2, 0, 7).Format(), "2,0,7", "format");
		Check(MRX_StashPlacement.Parse(string.Empty) == null, "empty refused");
		Check(MRX_StashPlacement.Parse("1,2") == null, "two numbers refused");
		Check(MRX_StashPlacement.Parse("1,2,3,4") == null, "four numbers refused");
		Check(MRX_StashPlacement.Parse("a,b,c") == null, "letters refused");
		Check(MRX_StashPlacement.Parse("-1,0,0") == null, "negative refused");
		Finish();
	}
}

//------------------------------------------------------------------------------------------------
class MRX_Test_StashGridPlace : MRX_TestCase
{
	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		MRX_StashGrid grid = new MRX_StashGrid(6, 8, 3);
		Check(grid.Place("rifle", MRX_StashPlacement.Create(0, 0, 0), 2, 1), "2x1 at the first cell");
		Check(!grid.Place("dressing", MRX_StashPlacement.Create(0, 1, 0), 1, 1), "overlap refused");
		Check(!grid.Contains("dressing"), "refused item not added");
		Check(grid.Place("dressing", MRX_StashPlacement.Create(0, 2, 0), 1, 1), "next to the rifle");
		Check(grid.Place("dressing2", MRX_StashPlacement.Create(0, 0, 1), 1, 1), "below the rifle");

		Check(!grid.IsFree(0, 5, 0, 2, 1), "past the last column refused");
		Check(!grid.IsFree(0, 0, 7, 2, 2), "past the last row refused");
		Check(!grid.IsFree(0, -1, 0, 1, 1), "negative column refused");
		Check(grid.IsFree(2, 5, 7, 1, 1), "last cell of the last page");
		Check(!grid.IsFree(3, 0, 0, 1, 1), "page behind the limit refused");
		Check(grid.IsFree(3, 0, 0, 1, 1, string.Empty, true), "page behind the limit as overflow");

		// Moving an item onto cells it already covers.
		Check(grid.Place("rifle", MRX_StashPlacement.Create(0, 1, 2), 2, 1), "rifle moved");
		Check(grid.Place("rifle", MRX_StashPlacement.Create(0, 0, 2), 2, 1), "rifle moved onto its own cell");
		Check(MRX_StashGridTests.Is(grid.Get("rifle"), 0, 0, 2), "rifle placement updated");
		Check(grid.IsFree(0, 0, 0, 2, 1), "old rifle cells free");
		Check(!grid.Place("rifle", MRX_StashPlacement.Create(0, 2, 0), 2, 1), "move onto another item refused");
		Check(MRX_StashGridTests.Is(grid.Get("rifle"), 0, 0, 2), "refused move keeps the placement");

		CheckInt(grid.GetPageCount(), 3, "pages = limit");
		Check(grid.Place("old", MRX_StashPlacement.Create(4, 0, 0), 1, 1, true), "overflow item behind the limit");
		CheckInt(grid.GetPageCount(), 5, "pages include overflow");
		Check(grid.Remove("old"), "remove");
		Check(!grid.Remove("old"), "remove twice");
		CheckInt(grid.GetPageCount(), 3, "pages after remove");
		Finish();
	}
}

//------------------------------------------------------------------------------------------------
class MRX_Test_StashGridFindFree : MRX_TestCase
{
	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		MRX_StashGrid grid = new MRX_StashGrid(6, 8, 2);
		Check(MRX_StashGridTests.Is(grid.FindFree(1, 1), 0, 0, 0), "empty grid: first cell");
		Check(MRX_StashGridTests.Is(grid.FindFree(1, 1, 1), 1, 0, 0), "preferred page");

		grid.Place("a", MRX_StashPlacement.Create(1, 0, 0), 1, 1);
		Check(MRX_StashGridTests.Is(grid.FindFree(2, 1, 1), 1, 1, 0), "first free cell of the preferred page");
		Check(MRX_StashGridTests.Is(grid.FindFree(2, 2, 1, "a"), 1, 0, 0), "ignored item's cells are free");

		FillPage(grid, 1, "p1");
		Check(MRX_StashGridTests.Is(grid.FindFree(1, 1, 1), 0, 0, 0), "full preferred page: first page");

		FillPage(grid, 0, "p0");
		Check(grid.FindFree(1, 1) == null, "all pages full");
		MRX_StashPlacement overflow = grid.FindFree(1, 1, 0, string.Empty, true);
		Check(overflow && overflow.m_iPage == 2, "overflow behind the limit");

		// A 2x1 item needs two free cells side by side.
		MRX_StashGrid gaps = new MRX_StashGrid(2, 1, 1);
		gaps.Place("left", MRX_StashPlacement.Create(0, 0, 0), 1, 1);
		Check(gaps.FindFree(1, 1) != null, "one cell free");
		Check(gaps.FindFree(2, 1) == null, "no room for 2x1");

		MRX_StashGrid unlimited = new MRX_StashGrid(6, 8, 0);
		FillPage(unlimited, 0, "u");
		MRX_StashPlacement next = unlimited.FindFree(1, 1);
		Check(next && next.m_iPage == 1, "without a limit a new page follows");
		Finish();
	}

	//------------------------------------------------------------------------------------------------
	protected static void FillPage(MRX_StashGrid grid, int page, string prefix)
	{
		for (int row = 0; row < grid.GetRows(); row++)
		{
			for (int column = 0; column < grid.GetColumns(); column++)
			{
				grid.Place(string.Format("%1-%2-%3", prefix, column, row), MRX_StashPlacement.Create(page, column, row), 1, 1);
			}
		}
	}
}
#endif
