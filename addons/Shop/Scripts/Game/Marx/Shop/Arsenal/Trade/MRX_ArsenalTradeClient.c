void MRX_ArsenalTradeResultDelegate(bool sell, MRX_EShopStatus status, MRX_ETxStatus txStatus, array<string> currencies, array<int> amounts, int doneCount, int leftCount);
typedef func MRX_ArsenalTradeResultDelegate;

void MRX_ArsenalTradeFailureDelegate(MRX_EShopStatus status);
typedef func MRX_ArsenalTradeFailureDelegate;

//! Client: the local player's trade at a Marx arsenal with a cart (see MRX_ArsenalTradeSession). Opens it, waits for the
//! trade container to replicate (its contents replicate while it is the vicinity's item of interest), keeps the server's
//! totals and sale prices and sends the player's requests. Owned by the local player controller
//! (SCR_PlayerController.MRX_GetArsenalTrade).
class MRX_ArsenalTradeClient : Managed
{
	protected static const int WAIT_MS = 100;
	protected static const int WAIT_LIMIT_MS = 3000;
	//! A checkout without an answer by then counts as unanswered.
	protected static const int ANSWER_LIMIT_MS = 15000;

	//! Weak: the controller owns this.
	protected SCR_PlayerController m_Controller;
	//! Weak: the arsenal of the open trade.
	protected IEntity m_Arsenal;
	protected RplId m_ContainerId = RplId.Invalid();
	//! Number of the last opening; answers to older ones are ignored.
	protected int m_iRequest;
	protected int m_iWaitMs;
	//! Requested and not closed.
	protected bool m_bOpen;
	//! The container replicated.
	protected bool m_bReady;
	protected bool m_bSaleTab;
	protected bool m_bCheckingOut;
	protected ref array<string> m_aCartCurrencies = {};
	protected ref array<int> m_aCartTotals = {};
	protected ref map<string, int> m_mSalePrices = new map<string, int>();
	protected ref map<string, string> m_mSaleCurrencies = new map<string, string>();
	//! Weak: where the player's items put up for sale were, to give them back there.
	protected ref map<EntityID, BaseInventoryStorageComponent> m_mSaleOrigins = new map<EntityID, BaseInventoryStorageComponent>();
	protected ref ScriptInvokerVoid m_OnChanged;
	protected ref ScriptInvokerBase<MRX_ArsenalTradeResultDelegate> m_OnResult;
	protected ref ScriptInvokerVoid m_OnNoAnswer;
	protected ref ScriptInvokerBase<MRX_ArsenalTradeFailureDelegate> m_OnFailure;

	//------------------------------------------------------------------------------------------------
	void MRX_ArsenalTradeClient(SCR_PlayerController controller)
	{
		m_Controller = controller;
	}

	//------------------------------------------------------------------------------------------------
	void ~MRX_ArsenalTradeClient()
	{
		GetGame().GetCallqueue().Remove(WaitForContainer);
		GetGame().GetCallqueue().Remove(OnNoAnswer);
	}

	//------------------------------------------------------------------------------------------------
	//! The local player's trade, or null without a local player controller.
	static MRX_ArsenalTradeClient GetLocal()
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
			return null;

		return controller.MRX_GetArsenalTrade();
	}

	//------------------------------------------------------------------------------------------------
	//! Invoked when the trade opens, closes, switches tabs or gets new totals.
	ScriptInvokerVoid GetOnChanged()
	{
		if (!m_OnChanged)
			m_OnChanged = new ScriptInvokerVoid();

		return m_OnChanged;
	}

	//------------------------------------------------------------------------------------------------
	//! Invoked with the result of a checkout.
	ScriptInvokerBase<MRX_ArsenalTradeResultDelegate> GetOnResult()
	{
		if (!m_OnResult)
			m_OnResult = new ScriptInvokerBase<MRX_ArsenalTradeResultDelegate>();

		return m_OnResult;
	}

	//------------------------------------------------------------------------------------------------
	//! Invoked when a checkout got no answer within ANSWER_LIMIT_MS (its outcome is unknown).
	ScriptInvokerVoid GetOnNoAnswer()
	{
		if (!m_OnNoAnswer)
			m_OnNoAnswer = new ScriptInvokerVoid();

		return m_OnNoAnswer;
	}

	//------------------------------------------------------------------------------------------------
	//! Invoked when the server refuses a change of the cart or the trade.
	ScriptInvokerBase<MRX_ArsenalTradeFailureDelegate> GetOnFailure()
	{
		if (!m_OnFailure)
			m_OnFailure = new ScriptInvokerBase<MRX_ArsenalTradeFailureDelegate>();

		return m_OnFailure;
	}

	//------------------------------------------------------------------------------------------------
	//! Opens the trade at the arsenal (a trade at another one closes).
	void Open(notnull IEntity arsenal)
	{
		if (m_bOpen && m_Arsenal == arsenal)
			return;

		if (m_bOpen)
			Close(true);

		m_Arsenal = arsenal;
		m_bOpen = true;
		m_bReady = false;
		m_bSaleTab = false;
		m_iRequest++;
		ClearState();
		if (m_Controller)
			m_Controller.MRX_RequestTradeOpen(arsenal, m_iRequest);

		Changed();
	}

	//------------------------------------------------------------------------------------------------
	//! \param notifyServer False when the server closed the trade itself.
	void Close(bool notifyServer)
	{
		if (!m_bOpen)
			return;

		m_bOpen = false;
		m_bReady = false;
		m_bCheckingOut = false;
		GetGame().GetCallqueue().Remove(WaitForContainer);
		GetGame().GetCallqueue().Remove(OnNoAnswer);
		SetItemOfInterest(null);
		m_ContainerId = RplId.Invalid();
		ClearState();
		if (notifyServer && m_Controller)
			m_Controller.MRX_RequestTradeClose();

		Changed();
	}

	//------------------------------------------------------------------------------------------------
	bool IsOpen()
	{
		return m_bOpen;
	}

	//------------------------------------------------------------------------------------------------
	//! The trade is open at this arsenal.
	bool IsTradingAt(MRX_ArsenalShopComponent arsenal)
	{
		return m_bOpen && arsenal && arsenal.GetOwner() == m_Arsenal;
	}

	//------------------------------------------------------------------------------------------------
	//! The container replicated: the cart and the sale box can be shown.
	bool IsReady()
	{
		return m_bReady && GetCartStorage() && GetSaleStorage();
	}

	//------------------------------------------------------------------------------------------------
	IEntity GetArsenal()
	{
		return m_Arsenal;
	}

	//------------------------------------------------------------------------------------------------
	IEntity GetContainer()
	{
		return SCR_EntityHelper.RplIdToEntity(m_ContainerId);
	}

	//------------------------------------------------------------------------------------------------
	MRX_ArsenalTradeStorageComponent GetCartStorage()
	{
		return MRX_ArsenalTradeStorageComponent.Find(GetContainer(), MRX_EArsenalTradeRole.CART);
	}

	//------------------------------------------------------------------------------------------------
	MRX_ArsenalTradeStorageComponent GetSaleStorage()
	{
		return MRX_ArsenalTradeStorageComponent.Find(GetContainer(), MRX_EArsenalTradeRole.SALE);
	}

	//------------------------------------------------------------------------------------------------
	//! The sale tab is shown (the arsenal lists what it buys back).
	bool IsSaleTab()
	{
		return m_bSaleTab;
	}

	//------------------------------------------------------------------------------------------------
	void SetSaleTab(bool sale)
	{
		if (m_bSaleTab == sale)
			return;

		m_bSaleTab = sale;
		Changed();
	}

	//------------------------------------------------------------------------------------------------
	bool IsCheckingOut()
	{
		return m_bCheckingOut;
	}

	//------------------------------------------------------------------------------------------------
	//! An arsenal item taken by any input: into the cart, into the storage when it belongs to an item in the cart (e.g.
	//! a scope dropped on a rifle there), otherwise into the cart's cells.
	//! \return False when this trade cannot take it (not open at the arsenal, the sale tab, a checkout running).
	bool TakeFromArsenal(notnull MRX_ArsenalShopComponent arsenal, MRX_ShopItem item, BaseInventoryStorageComponent storage)
	{
		if (!item || !IsTradingAt(arsenal) || !IsReady() || m_bSaleTab || m_bCheckingOut)
			return false;

		BaseInventoryStorageComponent target;
		if (storage && storage != GetCartStorage() && MRX_ArsenalTradeStorageComponent.IsInCart(storage.GetOwner()))
			target = storage;

		Add(item.m_sId, target);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! An item given to the arsenal by any input (X, dragged onto it): an item of the cart leaves the cart, an item of the
	//! player's goes into the sale box.
	//! \return False when this trade cannot take it.
	bool GiveToArsenal(notnull MRX_ArsenalShopComponent arsenal, IEntity item)
	{
		if (!item || !IsTradingAt(arsenal) || !IsReady() || m_bCheckingOut)
			return false;

		MRX_ArsenalTradeStorageComponent holder = MRX_ArsenalTradeStorageComponent.FindHolder(item);
		if (holder && holder == GetCartStorage())
		{
			RemoveFromCart(item);
			return true;
		}

		if (holder)
			return false;

		return PutUpForSale(item);
	}

	//------------------------------------------------------------------------------------------------
	//! Moves an item the player carries into the sale box and shows the sale tab (the server checks the move).
	bool PutUpForSale(notnull IEntity item)
	{
		MRX_ArsenalTradeStorageComponent sale = GetSaleStorage();
		IEntity character;
		if (m_Controller)
			character = m_Controller.GetControlledEntity();

		SCR_InventoryStorageManagerComponent manager;
		if (character)
			manager = SCR_InventoryStorageManagerComponent.Cast(character.FindComponent(SCR_InventoryStorageManagerComponent));

		MRX_ArsenalShopComponent arsenal = MRX_ArsenalShopComponent.Find(m_Arsenal);
		MRX_ShopDefinition shop;
		if (arsenal)
			shop = arsenal.GetShop();

		if (!sale || !manager || !shop || !shop.m_bAllowSell || item.GetRootParent() != character || !manager.CanMoveItem(item))
			return false;

		InventoryItemComponent itemComponent = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
		BaseInventoryStorageComponent origin;
		if (itemComponent && itemComponent.GetParentSlot())
			origin = itemComponent.GetParentSlot().GetStorage();

		// Into the sale box itself, not into an item already lying there.
		if (!manager.TryMoveItemToStorage(item, sale))
			return false;

		if (origin)
			m_mSaleOrigins.Set(item.GetID(), origin);

		SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_INV_CONTAINER_DIFR_DROP);
		SetSaleTab(true);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Storage the item was in when it was put up for sale (PutUpForSale), or null.
	BaseInventoryStorageComponent GetSaleOrigin(notnull IEntity item)
	{
		return m_mSaleOrigins.Get(item.GetID());
	}

	//------------------------------------------------------------------------------------------------
	//! Asks to put a new item of the arsenal into the cart's cells (storage null) or into a storage of an item in the cart.
	void Add(string itemId, BaseInventoryStorageComponent storage, int slotId = -1)
	{
		if (!m_bReady || !m_Controller)
			return;

		RplId storageId = RplId.Invalid();
		if (storage)
			storageId = Replication.FindItemId(storage);

		m_Controller.MRX_RequestTradeAdd(itemId, storageId, slotId);
	}

	//------------------------------------------------------------------------------------------------
	//! Asks to take an item out of the cart.
	void RemoveFromCart(notnull IEntity item)
	{
		if (m_bReady && m_Controller)
			m_Controller.MRX_RequestTradeRemove(item);
	}

	//------------------------------------------------------------------------------------------------
	//! Asks to move an item of the cart to the cells (storage null) or into a storage of another item in the cart.
	void MoveInCart(notnull IEntity item, BaseInventoryStorageComponent storage, int slotId = -1)
	{
		if (!m_bReady || !m_Controller)
			return;

		RplId storageId = RplId.Invalid();
		if (storage)
			storageId = Replication.FindItemId(storage);

		m_Controller.MRX_RequestTradeMove(item, storageId, slotId);
	}

	//------------------------------------------------------------------------------------------------
	//! Pays for the cart or sells the sale box, by the shown tab.
	void Checkout()
	{
		if (!m_bReady || m_bCheckingOut || !m_Controller)
			return;

		m_bCheckingOut = true;
		m_Controller.MRX_RequestTradeCheckout(m_bSaleTab);
		GetGame().GetCallqueue().Remove(OnNoAnswer);
		GetGame().GetCallqueue().CallLater(OnNoAnswer, ANSWER_LIMIT_MS);
		Changed();
	}

	//------------------------------------------------------------------------------------------------
	//! Totals of the cart by currency (the server's).
	void GetCartTotals(notnull array<string> outCurrencies, notnull array<int> outAmounts)
	{
		outCurrencies.Copy(m_aCartCurrencies);
		outAmounts.Copy(m_aCartTotals);
	}

	//------------------------------------------------------------------------------------------------
	int GetCartTotal(string currency)
	{
		int index = m_aCartCurrencies.Find(currency);
		if (index < 0)
			return 0;

		return m_aCartTotals[index];
	}

	//------------------------------------------------------------------------------------------------
	//! The local balance less the cart's total. \return False when the balance is not known.
	bool GetBalanceAfter(string currency, out int balance)
	{
		MRX_ClientWallet wallet = MRX_ClientWallet.GetLocal();
		if (!wallet || !wallet.TryGetBalance(currency, balance))
			return false;

		balance -= GetCartTotal(currency);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Sale price of an item in the sale box, with what it holds (the server's). \return False when not known yet.
	bool GetSalePrice(IEntity item, out int price, out string currency)
	{
		if (!item)
			return false;

		string key = SCR_EntityHelper.EntityToRplId(item).AsString();
		if (!m_mSalePrices.Contains(key))
			return false;

		price = m_mSalePrices.Get(key);
		currency = m_mSaleCurrencies.Get(key);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Totals of the sale box by currency.
	void GetSaleTotals(notnull array<string> outCurrencies, notnull array<int> outAmounts)
	{
		foreach (string key, int price : m_mSalePrices)
		{
			string currency = m_mSaleCurrencies.Get(key);
			if (price <= 0 || currency.IsEmpty())
				continue;

			int index = outCurrencies.Find(currency);
			if (index < 0)
			{
				outCurrencies.Insert(currency);
				outAmounts.Insert(price);
			}
			else
			{
				outAmounts[index] = MRX_ShopCheckoutOp.AddCapped(outAmounts[index], price);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by the RPC of SCR_PlayerController.
	void OnOpened(RplId containerId, int request)
	{
		if (request != m_iRequest)
			return;

		// Closed meanwhile (e.g. the inventory closed before the answer): the server's trade closes too.
		if (!m_bOpen)
		{
			if (m_Controller)
				m_Controller.MRX_RequestTradeClose();

			return;
		}

		m_ContainerId = containerId;
		m_iWaitMs = 0;
		GetGame().GetCallqueue().Remove(WaitForContainer);
		WaitForContainer();
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by the RPC of SCR_PlayerController.
	void OnClosed(int droppedCount, int request)
	{
		if (request == m_iRequest)
			Close(false);

		if (droppedCount > 0)
			SCR_HintManagerComponent.ShowCustomHint(WidgetManager.Translate("#MRX-Cart_Dropped", droppedCount), "#MRX-Cart_Title", 8);
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by the RPC of SCR_PlayerController.
	void OnState(array<string> cartCurrencies, array<int> cartTotals, array<string> saleKeys, array<int> salePrices, array<string> saleCurrencies)
	{
		if (!m_bOpen)
			return;

		m_aCartCurrencies.Copy(cartCurrencies);
		m_aCartTotals.Copy(cartTotals);
		m_mSalePrices.Clear();
		m_mSaleCurrencies.Clear();
		foreach (int i, string key : saleKeys)
		{
			if (!salePrices.IsIndexValid(i) || !saleCurrencies.IsIndexValid(i))
				break;

			m_mSalePrices.Set(key, salePrices[i]);
			m_mSaleCurrencies.Set(key, saleCurrencies[i]);
		}

		Changed();
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by the RPC of SCR_PlayerController.
	void OnFailure(MRX_EShopStatus status)
	{
		SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_INV_DROP_ERROR);
		if (m_OnFailure)
			m_OnFailure.Invoke(status);
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by the RPC of SCR_PlayerController.
	void OnResult(bool sell, MRX_EShopStatus status, MRX_ETxStatus txStatus, array<string> currencies, array<int> amounts, int doneCount, int leftCount)
	{
		m_bCheckingOut = false;
		GetGame().GetCallqueue().Remove(OnNoAnswer);
		if (status != MRX_EShopStatus.OK)
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_INV_DROP_ERROR);

		if (m_OnResult)
			m_OnResult.Invoke(sell, status, txStatus, currencies, amounts, doneCount, leftCount);

		Changed();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnNoAnswer()
	{
		if (!m_bCheckingOut)
			return;

		m_bCheckingOut = false;
		SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_INV_DROP_ERROR);
		if (m_OnNoAnswer)
			m_OnNoAnswer.Invoke();

		Changed();
	}

	//------------------------------------------------------------------------------------------------
	protected void WaitForContainer()
	{
		if (!m_bOpen)
			return;

		IEntity container = GetContainer();
		if (!container || !GetCartStorage() || !GetSaleStorage())
		{
			m_iWaitMs += WAIT_MS;
			if (m_iWaitMs >= WAIT_LIMIT_MS)
			{
				OnFailure(MRX_EShopStatus.UNKNOWN_SHOP);
				Close(true);
				return;
			}

			GetGame().GetCallqueue().CallLater(WaitForContainer, WAIT_MS);
			return;
		}

		SetItemOfInterest(container);
		m_bReady = true;
		Changed();
	}

	//------------------------------------------------------------------------------------------------
	//! Makes the container the vicinity's item of interest, as the vanilla loot action does with a body, so that its
	//! items replicate to a remote client (the container stays out of the vicinity list).
	//! \param container Null clears the item of interest if it is a trade container.
	protected void SetItemOfInterest(IEntity container)
	{
		IEntity character;
		if (m_Controller)
			character = m_Controller.GetControlledEntity();

		CharacterVicinityComponent vicinity;
		if (character)
			vicinity = CharacterVicinityComponent.Cast(character.FindComponent(CharacterVicinityComponent));

		if (!vicinity)
			return;

		if (container)
		{
			vicinity.SetItemOfInterest(container);
			return;
		}

		IEntity current = vicinity.GetItemOfInterest();
		if (current && current.FindComponent(MRX_ArsenalTradeManagerComponent))
			vicinity.SetItemOfInterest(null);
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearState()
	{
		m_aCartCurrencies.Clear();
		m_aCartTotals.Clear();
		m_mSalePrices.Clear();
		m_mSaleCurrencies.Clear();
		m_mSaleOrigins.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected void Changed()
	{
		if (m_OnChanged)
			m_OnChanged.Invoke();
	}
}
