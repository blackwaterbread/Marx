//! Server: trades at Marx arsenals with a cart, one per player (API v0). Created by the Marx system; use
//! MRX_ArsenalTrades.Get(). A trade closes when its player closes the inventory, leaves the character, dies, walks away
//! from the arsenal or disconnects, or when a mod calls Close().
class MRX_ArsenalTradeSessionManager : Managed
{
	//! Trade container: MRX_ArsenalTradeManagerComponent, two MRX_ArsenalTradeStorageComponent (cart and sale box),
	//! RplComponent, no persistence.
	static const ResourceName CONTAINER_PREFAB = "{C9FC0E965C247F46}Prefabs/Marx/Shop/MRX_ArsenalTradeContainer.et";
	protected static const int CHECK_INTERVAL_MS = 1000;

	protected ref map<int, ref MRX_ArsenalTradeSession> m_mSessions = new map<int, ref MRX_ArsenalTradeSession>();
	//! Closed sessions, released on a later frame (never inside their own call stack).
	protected ref array<ref MRX_ArsenalTradeSession> m_aClosed = {};

	//------------------------------------------------------------------------------------------------
	void MRX_ArsenalTradeSessionManager()
	{
		GetGame().GetCallqueue().CallLater(CheckSessions, CHECK_INTERVAL_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	void ~MRX_ArsenalTradeSessionManager()
	{
		GetGame().GetCallqueue().Remove(CheckSessions);
		GetGame().GetCallqueue().Remove(ReleaseClosed);
	}

	//------------------------------------------------------------------------------------------------
	//! Opens a trade of the player at the arsenal (the caller checked the player may use it). A trade at the same arsenal
	//! that is still open is reused; one at another arsenal starts closing and BUSY is answered.
	//! \param request The client's number of the opening, sent back with the answers; -1 when no client asked (the
	//! client then ignores them).
	MRX_EShopStatus Open(int playerId, notnull IEntity arsenal, int request)
	{
		MRX_ArsenalShopComponent arsenalShop = MRX_ArsenalShopComponent.Find(arsenal);
		if (!arsenalShop || !arsenalShop.UsesCart())
			return MRX_EShopStatus.UNKNOWN_SHOP;

		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!IsAlive(character))
			return MRX_EShopStatus.TOO_FAR;

		MRX_ArsenalTradeSession current = m_mSessions.Get(playerId);
		if (current)
		{
			if (current.GetArsenal() == arsenal && current.GetCharacter() == character && !current.IsClosing())
			{
				current.SetRequest(request);
				OnSessionReady(current);
				return MRX_EShopStatus.OK;
			}

			current.Close("opened at another arsenal");
			return MRX_EShopStatus.BUSY;
		}

		MRX_ArsenalTradeSession session = new MRX_ArsenalTradeSession(this, playerId, character, arsenal);
		session.SetRequest(request);
		if (!session.Start(CONTAINER_PREFAB))
			return MRX_EShopStatus.DELIVERY_FAILED;

		m_mSessions.Set(playerId, session);
		OnSessionReady(session);
		return MRX_EShopStatus.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Closes the player's trade right away (API v0): the cart is emptied and the sale box goes back to the character,
	//! at its feet when its inventory is full. Mods that save a leaving player's gear call this first, so the items put up
	//! for sale are saved with it. A running checkout finishes first.
	void Close(int playerId, string reason)
	{
		MRX_ArsenalTradeSession session = m_mSessions.Get(playerId);
		if (session)
			session.Close(reason);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Open trade of the player, or null.
	MRX_ArsenalTradeSession Find(int playerId)
	{
		return m_mSessions.Get(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by MRX_ArsenalTradeSession when it starts closing: tells the player, with the number of items put
	//! at the character's feet.
	void OnSessionClosing(notnull MRX_ArsenalTradeSession session, int droppedCount)
	{
		SCR_PlayerController controller = GetController(session.GetPlayerId());
		if (controller)
			controller.MRX_SendTradeClosed(droppedCount, session.GetRequest());
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by MRX_ArsenalTradeSession once the container is gone.
	void OnSessionClosed(notnull MRX_ArsenalTradeSession session)
	{
		if (m_mSessions.Get(session.GetPlayerId()) == session)
			m_mSessions.Remove(session.GetPlayerId());

		if (m_aClosed.IsEmpty())
			GetGame().GetCallqueue().CallLater(ReleaseClosed);

		m_aClosed.Insert(session);
	}

	//------------------------------------------------------------------------------------------------
	//! Internal: sends the cart's totals and the sale prices to the player when they changed since the last time.
	void SendState(notnull MRX_ArsenalTradeSession session)
	{
		SCR_PlayerController controller = GetController(session.GetPlayerId());
		if (!controller)
			return;

		array<string> cartCurrencies = {};
		array<int> cartTotals = {};
		array<string> saleKeys = {};
		array<int> salePrices = {};
		array<string> saleCurrencies = {};
		session.GetState(cartCurrencies, cartTotals, saleKeys, salePrices, saleCurrencies);
		array<string> parts = {};
		foreach (int i, string cartCurrency : cartCurrencies)
		{
			parts.Insert(string.Format("%1=%2", cartCurrency, cartTotals[i]));
		}

		foreach (int j, string key : saleKeys)
		{
			parts.Insert(string.Format("%1:%2%3", key, salePrices[j], saleCurrencies[j]));
		}

		string state = SCR_StringHelper.Join(";", parts);
		if (!session.SetSentState(state))
			return;

		controller.MRX_SendTradeState(cartCurrencies, cartTotals, saleKeys, salePrices, saleCurrencies);
	}

	//------------------------------------------------------------------------------------------------
	//! Internal: tells the player about a refused or failed change of the cart.
	void ReportFailure(notnull MRX_ArsenalTradeSession session, MRX_EShopStatus status)
	{
		SCR_PlayerController controller = GetController(session.GetPlayerId());
		if (controller)
			controller.MRX_SendTradeFailure(status);
	}

	//------------------------------------------------------------------------------------------------
	//! Internal: sends the result of a checkout to the player.
	void SendCheckoutResult(notnull MRX_ArsenalTradeSession session, bool sell, notnull MRX_ShopResult result)
	{
		SCR_PlayerController controller = GetController(session.GetPlayerId());
		if (!controller)
			return;

		MRX_ShopCheckoutResult checkout = MRX_ShopCheckoutResult.Cast(result);
		array<string> currencies = {};
		array<int> amounts = {};
		int doneCount;
		int leftCount;
		if (checkout)
		{
			currencies.Copy(checkout.m_aCurrencies);
			amounts.Copy(checkout.m_aAmounts);
			doneCount = checkout.m_iDoneCount;
			leftCount = checkout.m_iLeftCount;
		}

		controller.MRX_SendTradeResult(sell, result.m_eStatus, result.m_eTxStatus, currencies, amounts, doneCount, leftCount);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSessionReady(notnull MRX_ArsenalTradeSession session)
	{
		SCR_PlayerController controller = GetController(session.GetPlayerId());
		if (controller)
			controller.MRX_SendTradeOpened(session.GetContainer(), session.GetRequest());

		session.ScheduleState();
	}

	//------------------------------------------------------------------------------------------------
	//! Closes trades whose player left, died or walked away from the arsenal; refreshes the state of the others (items
	//! may change inside items, e.g. a magazine taken out of a rifle in the sale box).
	protected void CheckSessions()
	{
		array<MRX_ArsenalTradeSession> toClose = {};
		array<string> reasons = {};
		foreach (int playerId, MRX_ArsenalTradeSession session : m_mSessions)
		{
			if (session.IsClosing())
				continue;

			IEntity character = session.GetCharacter();
			IEntity arsenal = session.GetArsenal();
			float maxDistance = MRX_ArsenalShopComponent.MAX_DISTANCE;
			string reason;
			if (GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId) != character)
				reason = "player no longer controls the character";
			else if (!IsAlive(character))
				reason = "character dead";
			else if (!arsenal || !session.GetContainer())
				reason = "arsenal gone";
			else if (vector.DistanceSq(character.GetOrigin(), arsenal.GetOrigin()) > maxDistance * maxDistance)
				reason = "out of range";

			if (reason.IsEmpty())
			{
				session.ScheduleState();
				continue;
			}

			toClose.Insert(session);
			reasons.Insert(reason);
		}

		foreach (int i, MRX_ArsenalTradeSession closing : toClose)
		{
			closing.Close(reasons[i]);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void ReleaseClosed()
	{
		m_aClosed.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected static SCR_PlayerController GetController(int playerId)
	{
		return SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsAlive(IEntity character)
	{
		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if (!chimera || !chimera.GetCharacterController())
			return false;

		return chimera.GetCharacterController().GetLifeState() == ECharacterLifeState.ALIVE;
	}
}

//! Static access to the arsenal trades (API v0). Server only; null on clients.
class MRX_ArsenalTrades
{
	//------------------------------------------------------------------------------------------------
	static MRX_ArsenalTradeSessionManager Get()
	{
		MRX_MarxSystem system = MRX_MarxSystem.GetInstance();
		if (!system)
			return null;

		return system.MRX_GetArsenalTrades();
	}
}
