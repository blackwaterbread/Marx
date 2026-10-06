//! Shared parts of the Marx arsenal UI. Internal.
class MRX_ArsenalUI
{
	protected static const string AVAILABLE_DISPLAY = "ResourceDisplayAvailable";
	protected static const string STORED_DISPLAY = "ResourceDisplayStored";

	//------------------------------------------------------------------------------------------------
	//! Hides the vanilla supply displays below root (a storage title or a whole panel): a Marx arsenal has no supplies to
	//! show. Vanilla rebuilds storage titles on every refresh, so the widgets are looked up each time.
	static void HideSupplies(Widget root)
	{
		if (!root)
			return;

		Widget stored = root.FindAnyWidget(STORED_DISPLAY);
		if (stored)
			stored.SetVisible(false);

		Widget available = root.FindAnyWidget(AVAILABLE_DISPLAY);
		if (available)
			available.SetVisible(false);
	}
}
