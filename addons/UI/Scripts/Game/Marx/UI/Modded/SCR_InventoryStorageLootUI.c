//! The inventory's vicinity panel shows the local player's balances below its items (MRX_BalancePanel), unless a mod
//! turned it off with MRX_BalancePanel.SetShownInInventory(false).
modded class SCR_InventoryStorageLootUI
{
	protected ref MRX_BalancePanel m_MRX_BalancePanel;
	protected MRX_ClientWallet m_MRX_Wallet;

	//------------------------------------------------------------------------------------------------
	override void Init()
	{
		super.Init();
		if (!m_widget || !MRX_BalancePanel.IsShownInInventory())
			return;

		// No wallet: Marx is off or the player has no owner yet.
		m_MRX_Wallet = MRX_ClientWallet.GetLocal();
		if (!m_MRX_Wallet)
			return;

		m_MRX_BalancePanel = MRX_BalancePanel.Create(m_widget, null);
		if (m_MRX_BalancePanel)
			m_MRX_Wallet.GetOnBalanceChanged().Insert(MRX_OnBalanceChanged);
	}

	//------------------------------------------------------------------------------------------------
	override event void HandlerDeattached(Widget w)
	{
		if (m_MRX_Wallet)
			m_MRX_Wallet.GetOnBalanceChanged().Remove(MRX_OnBalanceChanged);

		if (m_MRX_BalancePanel)
			m_MRX_BalancePanel.Stop();

		super.HandlerDeattached(w);
	}

	//------------------------------------------------------------------------------------------------
	protected void MRX_OnBalanceChanged(string currency, int balance)
	{
		if (m_MRX_BalancePanel)
			m_MRX_BalancePanel.OnBalanceChanged(currency, balance);
	}
}
