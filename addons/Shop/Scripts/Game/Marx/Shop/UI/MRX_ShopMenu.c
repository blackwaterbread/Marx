//! Minimal shop dialog (API v0): balance, buy rows from the catalog, sell rows from the local inventory.
//! Client side; every request is validated again by the server.
class MRX_ShopMenu : MRX_ScriptedDialog
{
	//! Weak: the shop entity may stream out while the dialog is open.
	protected IEntity m_ShopEntity;
	protected ref MRX_ShopDefinition m_Definition;

	protected TextWidget m_wBalance;
	protected TextWidget m_wStatus;
	//! Items offered for sale, by index in the "sell:<index>" actions.
	protected ref array<IEntity> m_aSellItems = {};

	//------------------------------------------------------------------------------------------------
	//! Opens the dialog for a shop. \return Null when the shop has no valid catalog.
	static MRX_ShopMenu Open(notnull MRX_ShopComponent shop)
	{
		MRX_ShopDefinition definition = shop.GetDefinition();
		if (!definition)
			return null;

		MRX_ShopMenu menu = new MRX_ShopMenu();
		menu.m_ShopEntity = shop.GetOwner();
		menu.m_Definition = definition;
		OpenDialog(menu, "Shop", "MRX_Shop");
		return menu;
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnDialogOpened()
	{
		m_wBalance = AddHeaderLine(string.Empty);
		m_wStatus = AddHeaderLine(string.Empty);

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (controller)
		{
			controller.MRX_GetOnShopResult().Insert(OnShopResult);
			controller.MRX_GetWallet().GetOnBalanceChanged().Insert(OnBalanceChanged);
		}

		UpdateBalance();
		BuildRows();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (controller)
		{
			controller.MRX_GetOnShopResult().Remove(OnShopResult);
			controller.MRX_GetWallet().GetOnBalanceChanged().Remove(OnBalanceChanged);
		}

		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildRows()
	{
		ClearRows();
		m_aSellItems.Clear();

		foreach (MRX_ShopItem item : m_Definition.m_Catalog.m_aItems)
		{
			if (item.m_iPrice > 0)
				AddRow(string.Format("%1  %2 %3", GetItemName(item), item.m_iPrice, item.m_sCurrency), "Buy", "buy:" + item.m_sId);
		}

		if (!m_Definition.m_bAllowSell)
			return;

		array<IEntity> carried = {};
		InventoryStorageManagerComponent manager = GetLocalStorageManager();
		if (manager)
			manager.GetItems(carried);

		foreach (IEntity entity : carried)
		{
			MRX_ShopItem sellable = m_Definition.m_Catalog.FindByPrefab(SCR_ResourceNameUtils.GetPrefabName(entity));
			if (!sellable)
				continue;

			int sellPrice = m_Definition.GetSellPrice(sellable);
			if (sellPrice <= 0)
				continue;

			int index = m_aSellItems.Insert(entity);
			AddRow(string.Format("%1  +%2 %3", GetItemName(sellable), sellPrice, sellable.m_sCurrency), "Sell", "sell:" + index.ToString());
		}
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnRowAction(string action)
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller || !m_ShopEntity)
			return;

		if (action.StartsWith("buy:"))
		{
			controller.MRX_RequestBuy(m_ShopEntity, action.Substring(4, action.Length() - 4));
			m_wStatus.SetText("...");
			return;
		}

		int sellIndex = action.Substring(5, action.Length() - 5).ToInt();
		if (sellIndex >= 0 && sellIndex < m_aSellItems.Count() && m_aSellItems[sellIndex])
		{
			controller.MRX_RequestSell(m_ShopEntity, m_aSellItems[sellIndex]);
			m_wStatus.SetText("...");
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnShopResult(MRX_EShopStatus status, MRX_ETxStatus txStatus, string itemId, int price, string currency)
	{
		if (status == MRX_EShopStatus.OK)
			m_wStatus.SetText(string.Format("Done: %1 (%2 %3)", itemId, price, currency));
		else if (status == MRX_EShopStatus.PAYMENT_FAILED)
			m_wStatus.SetText("Payment failed: " + typename.EnumToString(MRX_ETxStatus, txStatus));
		else
			m_wStatus.SetText("Failed: " + typename.EnumToString(MRX_EShopStatus, status));

		BuildRows();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBalanceChanged(string currency, int balance)
	{
		UpdateBalance();
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateBalance()
	{
		MRX_ClientWallet wallet = MRX_ClientWallet.GetLocal();
		if (!wallet || !m_wBalance)
			return;

		array<string> currencies = {};
		wallet.GetCurrencies(currencies);
		string text = "Balance:";
		foreach (string currency : currencies)
		{
			int balance;
			wallet.TryGetBalance(currency, balance);
			text += string.Format(" %1 %2", balance, currency);
		}

		m_wBalance.SetText(text);
	}

	//------------------------------------------------------------------------------------------------
	protected static string GetItemName(notnull MRX_ShopItem item)
	{
		if (!item.m_sName.IsEmpty())
			return item.m_sName;

		return GetPrefabDisplayName(item.m_sPrefab);
	}
}
