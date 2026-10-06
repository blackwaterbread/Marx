//! The panel of a Marx arsenal shows the local balance and the outcome of trades below its items
//! (MRX_ArsenalBalanceBar), and updates its slots when the balance changes.
modded class SCR_InventoryOpenedStorageArsenalUI
{
	protected MRX_ClientWallet m_MRX_Wallet;
	protected SCR_PlayerController m_MRX_Controller;
	protected ref MRX_ArsenalBalanceBar m_MRX_BalanceBar;

	//------------------------------------------------------------------------------------------------
	protected MRX_ArsenalShopComponent MRX_GetArsenal()
	{
		if (!m_Storage)
			return null;

		return MRX_ArsenalShopComponent.Find(m_Storage.GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	override void Init()
	{
		super.Init();
		if (!MRX_GetArsenal())
			return;

		m_MRX_BalanceBar = MRX_ArsenalBalanceBar.Create(m_widget, MRX_GetArsenal());
		m_MRX_Wallet = MRX_ClientWallet.GetLocal();
		if (m_MRX_Wallet)
			m_MRX_Wallet.GetOnBalanceChanged().Insert(MRX_OnBalanceChanged);

		m_MRX_Controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (m_MRX_Controller)
			m_MRX_Controller.MRX_GetOnShopResult().Insert(MRX_OnShopResult);

		RefreshResources();
	}

	//------------------------------------------------------------------------------------------------
	override event void HandlerDeattached(Widget w)
	{
		if (m_MRX_Wallet)
			m_MRX_Wallet.GetOnBalanceChanged().Remove(MRX_OnBalanceChanged);

		if (m_MRX_Controller)
			m_MRX_Controller.MRX_GetOnShopResult().Remove(MRX_OnShopResult);

		if (m_MRX_BalanceBar)
			m_MRX_BalanceBar.Stop();

		super.HandlerDeattached(w);
	}

	//------------------------------------------------------------------------------------------------
	override void RefreshResources()
	{
		MRX_ArsenalShopComponent arsenal = MRX_GetArsenal();
		if (!arsenal)
		{
			super.RefreshResources();
			return;
		}

		MRX_ArsenalUI.HideSupplies(m_widget);
	}

	//------------------------------------------------------------------------------------------------
	//! Balance text of the panel, or null.
	TextWidget MRX_GetBalanceText()
	{
		if (!m_MRX_BalanceBar)
			return null;

		return m_MRX_BalanceBar.GetAmountText();
	}

	//------------------------------------------------------------------------------------------------
	protected void MRX_OnBalanceChanged(string currency, int balance)
	{
		if (m_MRX_BalanceBar)
			m_MRX_BalanceBar.OnBalanceChanged(currency, balance);

		foreach (SCR_InventorySlotUI slot : m_aSlots)
		{
			if (slot)
				slot.Refresh();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Failures and details of a sale; the balance change itself shows from the wallet.
	protected void MRX_OnShopResult(MRX_EShopStatus status, MRX_ETxStatus txStatus, string itemId, int price, string currency, int itemCount, int unpaidCount)
	{
		if (!m_MRX_BalanceBar)
			return;

		if (status != MRX_EShopStatus.OK)
		{
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_INV_DROP_ERROR);
			m_MRX_BalanceBar.ShowInfo(MRX_ShopMenu.GetFailureText(status, txStatus), true);
			return;
		}

		if (itemCount <= 1 && unpaidCount == 0)
			return;

		string text = string.Format("%1 items sold", itemCount);
		if (unpaidCount > 0)
			text += string.Format(", %1 not bought back", unpaidCount);

		m_MRX_BalanceBar.ShowInfo(text, false);
	}
}
