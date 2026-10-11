//! Trades at Marx arsenals with a cart (MRX_ArsenalTradeSession): the client sends IDs only (catalog item, items and
//! storages of the trade container); the server checks the player like the arsenal requests and resolves the rest.
modded class SCR_PlayerController
{
	//! Client: the local player's trade.
	protected ref MRX_ArsenalTradeClient m_MRX_ArsenalTrade;

	//------------------------------------------------------------------------------------------------
	//! Client: the local player's trade at a Marx arsenal (created on first use).
	MRX_ArsenalTradeClient MRX_GetArsenalTrade()
	{
		if (!m_MRX_ArsenalTrade)
			m_MRX_ArsenalTrade = new MRX_ArsenalTradeClient(this);

		return m_MRX_ArsenalTrade;
	}

	//------------------------------------------------------------------------------------------------
	//! \param request Number of this opening, echoed by the answers (older answers are ignored).
	void MRX_RequestTradeOpen(notnull IEntity arsenal, int request)
	{
		Rpc(MRX_RpcAsk_TradeOpen, SCR_EntityHelper.EntityToRplId(arsenal), request);
	}

	//------------------------------------------------------------------------------------------------
	void MRX_RequestTradeClose()
	{
		Rpc(MRX_RpcAsk_TradeClose);
	}

	//------------------------------------------------------------------------------------------------
	//! \param storageId Invalid: the cart's cells.
	void MRX_RequestTradeAdd(string itemId, RplId storageId, int slotId)
	{
		Rpc(MRX_RpcAsk_TradeAdd, itemId, storageId, slotId);
	}

	//------------------------------------------------------------------------------------------------
	void MRX_RequestTradeRemove(notnull IEntity item)
	{
		Rpc(MRX_RpcAsk_TradeRemove, SCR_EntityHelper.EntityToRplId(item));
	}

	//------------------------------------------------------------------------------------------------
	//! \param storageId Invalid: the cart's cells.
	void MRX_RequestTradeMove(notnull IEntity item, RplId storageId, int slotId)
	{
		Rpc(MRX_RpcAsk_TradeMove, SCR_EntityHelper.EntityToRplId(item), storageId, slotId);
	}

	//------------------------------------------------------------------------------------------------
	//! \param sell True: the sale box, false: the cart.
	void MRX_RequestTradeCheckout(bool sell)
	{
		Rpc(MRX_RpcAsk_TradeCheckout, sell);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the trade container to show. \param request The client's number of the opening.
	void MRX_SendTradeOpened(IEntity container, int request)
	{
		Rpc(MRX_RpcDo_TradeOpened, SCR_EntityHelper.EntityToRplId(container), request);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the trade closed. \param droppedCount Items put at the character's feet.
	//! \param request The client's number of the opening.
	void MRX_SendTradeClosed(int droppedCount, int request)
	{
		Rpc(MRX_RpcDo_TradeClosed, droppedCount, request);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the cart's totals by currency and the sale price of each item in the sale box (by its RplId as text).
	void MRX_SendTradeState(array<string> cartCurrencies, array<int> cartTotals, array<string> saleKeys, array<int> salePrices, array<string> saleCurrencies)
	{
		Rpc(MRX_RpcDo_TradeState, cartCurrencies, cartTotals, saleKeys, salePrices, saleCurrencies);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a change of the trade was refused or failed.
	void MRX_SendTradeFailure(MRX_EShopStatus status)
	{
		Rpc(MRX_RpcDo_TradeFailure, status);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the result of a checkout.
	void MRX_SendTradeResult(bool sell, MRX_EShopStatus status, MRX_ETxStatus txStatus, array<string> currencies, array<int> amounts, int doneCount, int leftCount)
	{
		Rpc(MRX_RpcDo_TradeResult, sell, status, txStatus, currencies, amounts, doneCount, leftCount);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MRX_RpcAsk_TradeOpen(RplId arsenalId, int request)
	{
		IEntity arsenal = SCR_EntityHelper.RplIdToEntity(arsenalId);
		MRX_EShopStatus status = MRX_ArsenalRequests.CheckPlayer(GetPlayerId(), arsenal);
		MRX_ArsenalTradeSessionManager trades = MRX_ArsenalTrades.Get();
		if (status == MRX_EShopStatus.OK && !trades)
			status = MRX_EShopStatus.UNKNOWN_SHOP;

		if (status == MRX_EShopStatus.OK)
			status = trades.Open(GetPlayerId(), arsenal, request);

		if (status == MRX_EShopStatus.OK)
			return;

		MRX_SendTradeFailure(status);
		MRX_SendTradeClosed(0, request);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MRX_RpcAsk_TradeClose()
	{
		MRX_ArsenalTradeSessionManager trades = MRX_ArsenalTrades.Get();
		if (trades)
			trades.Close(GetPlayerId(), "client closed the inventory");
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MRX_RpcAsk_TradeAdd(string itemId, RplId storageId, int slotId)
	{
		MRX_ArsenalTradeSession session;
		MRX_EShopStatus status = MRX_ResolveTrade(session);
		if (status == MRX_EShopStatus.OK)
			status = session.Add(itemId, MRX_FindStorage(storageId), slotId);

		if (status != MRX_EShopStatus.OK)
			MRX_SendTradeFailure(status);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MRX_RpcAsk_TradeRemove(RplId itemId)
	{
		MRX_ArsenalTradeSession session;
		MRX_EShopStatus status = MRX_ResolveTrade(session);
		if (status == MRX_EShopStatus.OK)
			status = session.RemoveFromCart(SCR_EntityHelper.RplIdToEntity(itemId));

		if (status != MRX_EShopStatus.OK)
			MRX_SendTradeFailure(status);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MRX_RpcAsk_TradeMove(RplId itemId, RplId storageId, int slotId)
	{
		MRX_ArsenalTradeSession session;
		MRX_EShopStatus status = MRX_ResolveTrade(session);
		if (status == MRX_EShopStatus.OK)
			status = session.MoveInCart(SCR_EntityHelper.RplIdToEntity(itemId), MRX_FindStorage(storageId), slotId);

		if (status != MRX_EShopStatus.OK)
			MRX_SendTradeFailure(status);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MRX_RpcAsk_TradeCheckout(bool sell)
	{
		MRX_ArsenalTradeSession session;
		MRX_EShopStatus status = MRX_ResolveTrade(session);
		if (status == MRX_EShopStatus.OK)
			status = session.Checkout(sell);

		if (status != MRX_EShopStatus.OK)
			MRX_SendTradeResult(sell, status, MRX_ETxStatus.OK, new array<string>(), new array<int>(), 0, 0);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void MRX_RpcDo_TradeOpened(RplId containerId, int request)
	{
		MRX_GetArsenalTrade().OnOpened(containerId, request);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void MRX_RpcDo_TradeClosed(int droppedCount, int request)
	{
		MRX_GetArsenalTrade().OnClosed(droppedCount, request);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void MRX_RpcDo_TradeState(array<string> cartCurrencies, array<int> cartTotals, array<string> saleKeys, array<int> salePrices, array<string> saleCurrencies)
	{
		MRX_GetArsenalTrade().OnState(cartCurrencies, cartTotals, saleKeys, salePrices, saleCurrencies);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void MRX_RpcDo_TradeFailure(MRX_EShopStatus status)
	{
		MRX_GetArsenalTrade().OnFailure(status);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void MRX_RpcDo_TradeResult(bool sell, MRX_EShopStatus status, MRX_ETxStatus txStatus, array<string> currencies, array<int> amounts, int doneCount, int leftCount)
	{
		MRX_GetArsenalTrade().OnResult(sell, status, txStatus, currencies, amounts, doneCount, leftCount);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the player's open trade, with the checks of the arsenal requests.
	protected MRX_EShopStatus MRX_ResolveTrade(out MRX_ArsenalTradeSession session)
	{
		MRX_ArsenalTradeSessionManager trades = MRX_ArsenalTrades.Get();
		if (trades)
			session = trades.Find(GetPlayerId());

		if (!session || session.IsClosing())
			return MRX_EShopStatus.UNKNOWN_SHOP;

		if (session.GetCharacter() != GetControlledEntity())
			return MRX_EShopStatus.TOO_FAR;

		return MRX_ArsenalRequests.CheckPlayer(GetPlayerId(), session.GetArsenal());
	}

	//------------------------------------------------------------------------------------------------
	protected static BaseInventoryStorageComponent MRX_FindStorage(RplId storageId)
	{
		if (!storageId.IsValid())
			return null;

		return BaseInventoryStorageComponent.Cast(Replication.FindItem(storageId));
	}
}
