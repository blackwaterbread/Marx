//! Storage panels that replace the vicinity panel (API v0): while one of them is open, the vicinity panel (items on the
//! ground) is hidden. Addons mark their panels by overriding MRX_ReplacesVicinity().
//! The local player's balances show below the quick slots (MRX_BalancePanel), unless a mod turned them off with
//! MRX_BalancePanel.SetShownInInventory(false).
modded class SCR_InventoryMenuUI
{
	//! Column of the weapon slots and the quick slots, right of the character.
	protected static const string MRX_BALANCE_PARENT = "Weaponstuff";
	//! Space below the balance panel: the column ends at the bottom edge of the menu content, which cuts off the last
	//! pixel row otherwise.
	protected static const float MRX_BALANCE_BOTTOM_SPACE = 4;

	protected ref MRX_BalancePanel m_MRX_BalancePanel;
	protected MRX_ClientWallet m_MRX_Wallet;
	protected bool m_bMRX_BalanceCreated;

	//------------------------------------------------------------------------------------------------
	//! The balance panel below the quick slots (API v0), created on first use, e.g. by storage panels opened while the
	//! menu opens. Null when there is no wallet (Marx off, no owner yet), it is turned off or the layout lacks the slots.
	MRX_BalancePanel MRX_GetBalancePanel()
	{
		if (m_bMRX_BalanceCreated)
			return m_MRX_BalancePanel;

		m_bMRX_BalanceCreated = true;
		Widget root = GetRootWidget();
		m_MRX_Wallet = MRX_ClientWallet.GetLocal();
		if (!root || !m_MRX_Wallet || !MRX_BalancePanel.IsShownInInventory())
			return null;

		Widget parent = root.FindAnyWidget(MRX_BALANCE_PARENT);
		if (!parent)
			return null;

		m_MRX_BalancePanel = MRX_BalancePanel.CreateIn(parent, null);
		AlignableSlot.SetPadding(m_MRX_BalancePanel.GetRootWidget(), 0, 8, 0, MRX_BALANCE_BOTTOM_SPACE);
		m_MRX_Wallet.GetOnBalanceChanged().Insert(MRX_OnBalanceChanged);
		return m_MRX_BalancePanel;
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();
		MRX_GetBalancePanel();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		if (m_MRX_Wallet)
			m_MRX_Wallet.GetOnBalanceChanged().Remove(MRX_OnBalanceChanged);

		if (m_MRX_BalancePanel)
			m_MRX_BalancePanel.Remove();

		m_MRX_BalancePanel = null;
		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------------------------
	protected void MRX_OnBalanceChanged(string currency, int balance)
	{
		if (m_MRX_BalancePanel)
			m_MRX_BalancePanel.OnBalanceChanged(currency, balance);
	}

	//------------------------------------------------------------------------------------------------
	//! Override (call super): true when the open storage panel takes the place of the vicinity panel.
	protected bool MRX_ReplacesVicinity(notnull SCR_InventoryOpenedStorageUI panel)
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	bool MRX_IsVicinityShown()
	{
		return m_wLootStorage && m_wLootStorage.IsVisible();
	}

	//------------------------------------------------------------------------------------------------
	//! The vicinity panel is only hidden, not removed: vanilla code uses it without null checks.
	protected void MRX_UpdateVicinityVisibility()
	{
		if (!m_wLootStorage)
			return;

		bool replaced;
		foreach (SCR_InventoryOpenedStorageUI panel : m_aOpenedStoragesUI)
		{
			if (panel && MRX_ReplacesVicinity(panel))
			{
				replaced = true;
				break;
			}
		}

		m_wLootStorage.SetVisible(!replaced);
	}

	//------------------------------------------------------------------------------------------------
	//! Vanilla rebuilds the vicinity panel when other storages are opened or the last one is closed.
	override void ShowVicinity(bool compact = false)
	{
		super.ShowVicinity(compact);
		MRX_UpdateVicinityVisibility();
	}

	//------------------------------------------------------------------------------------------------
	override void RemoveOpenStorage(SCR_InventoryOpenedStorageUI openedStorage)
	{
		super.RemoveOpenStorage(openedStorage);
		MRX_UpdateVicinityVisibility();
	}

	//------------------------------------------------------------------------------------------------
	//! Other panels closed meanwhile may have shown the vicinity panel again.
	override void RefreshLootUIListener()
	{
		super.RefreshLootUIListener();
		MRX_UpdateVicinityVisibility();
	}
}
