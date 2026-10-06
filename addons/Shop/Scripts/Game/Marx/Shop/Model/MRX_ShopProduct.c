void MRX_ShopProductDelegate(MRX_EShopStatus status);
typedef func MRX_ShopProductDelegate;

void MRX_ShopProductStateDelegate(MRX_ShopProductState state);
typedef func MRX_ShopProductStateDelegate;

//------------------------------------------------------------------------------------------------
//! Answer of a product to a check or a delivery (API v0).
class MRX_ShopProductCallback : Managed
{
	protected ref ScriptInvokerBase<MRX_ShopProductDelegate> m_OnResult;

	//------------------------------------------------------------------------------------------------
	ScriptInvokerBase<MRX_ShopProductDelegate> GetOnResult()
	{
		if (!m_OnResult)
			m_OnResult = new ScriptInvokerBase<MRX_ShopProductDelegate>();

		return m_OnResult;
	}

	//------------------------------------------------------------------------------------------------
	void OnResult(MRX_EShopStatus status)
	{
		if (m_OnResult)
			m_OnResult.Invoke(status);
	}
}

//------------------------------------------------------------------------------------------------
//! State of a product for one player, shown in the shop window (API v0).
class MRX_ShopProductState : Managed
{
	//! False: the window shows the product but its Buy button is off.
	bool m_bAvailable = true;
	//! E.g. "4 / 8 pages". Empty: nothing shown.
	string m_sText;

	//------------------------------------------------------------------------------------------------
	static MRX_ShopProductState Create(bool available, string text)
	{
		MRX_ShopProductState state = new MRX_ShopProductState();
		state.m_bAvailable = available;
		state.m_sText = text;
		return state;
	}
}

//------------------------------------------------------------------------------------------------
class MRX_ShopProductStateCallback : Managed
{
	protected ref ScriptInvokerBase<MRX_ShopProductStateDelegate> m_OnResult;

	//------------------------------------------------------------------------------------------------
	ScriptInvokerBase<MRX_ShopProductStateDelegate> GetOnResult()
	{
		if (!m_OnResult)
			m_OnResult = new ScriptInvokerBase<MRX_ShopProductStateDelegate>();

		return m_OnResult;
	}

	//------------------------------------------------------------------------------------------------
	void OnResult(MRX_ShopProductState state)
	{
		if (m_OnResult)
			m_OnResult.Invoke(state);
	}
}

//------------------------------------------------------------------------------------------------
//! Receives the product states of a shop (MRX_ShopService.GetProductStates), as parallel arrays.
class MRX_ShopStatesCallback : Managed
{
	//------------------------------------------------------------------------------------------------
	//! Override.
	void OnResult(array<string> itemIds, array<bool> available, array<string> texts)
	{
	}
}

//------------------------------------------------------------------------------------------------
//! Collects the states of several products and delivers them together on a later frame. Internal.
class MRX_ShopStatesRequest : Managed
{
	protected ref MRX_ShopStatesCallback m_Callback;
	protected ref MRX_CallQueue m_CallQueue;
	protected ref array<string> m_aItemIds = {};
	protected ref array<bool> m_aAvailable = {};
	protected ref array<string> m_aTexts = {};
	protected int m_iPending;
	protected bool m_bClosed;
	protected bool m_bDelivered;

	//------------------------------------------------------------------------------------------------
	void MRX_ShopStatesRequest(MRX_ShopStatesCallback callback, MRX_CallQueue callQueue)
	{
		m_Callback = callback;
		m_CallQueue = callQueue;
	}

	//------------------------------------------------------------------------------------------------
	void Add(int playerId, string ownerId, notnull MRX_ShopItem item)
	{
		int index = m_aItemIds.Insert(item.m_sId);
		m_aAvailable.Insert(true);
		m_aTexts.Insert(string.Empty);
		m_iPending++;
		item.m_Product.GetState(playerId, ownerId, item, new MRX_ShopStateReply(this, index));
	}

	//------------------------------------------------------------------------------------------------
	//! No more products follow.
	void Close()
	{
		m_bClosed = true;
		TryDeliver();
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by MRX_ShopStateReply.
	void OnState(int index, MRX_ShopProductState state)
	{
		if (state)
		{
			m_aAvailable[index] = state.m_bAvailable;
			m_aTexts[index] = state.m_sText;
		}

		m_iPending--;
		TryDeliver();
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by MRX_ShopStatesDelivery.
	void Deliver()
	{
		m_Callback.OnResult(m_aItemIds, m_aAvailable, m_aTexts);
	}

	//------------------------------------------------------------------------------------------------
	protected void TryDeliver()
	{
		if (!m_bClosed || m_iPending > 0 || m_bDelivered)
			return;

		m_bDelivered = true;
		m_CallQueue.Post(new MRX_ShopStatesDelivery(this));
	}
}

//------------------------------------------------------------------------------------------------
//! Keeps its request alive while the product answers. Internal.
class MRX_ShopStateReply : MRX_ShopProductStateCallback
{
	protected ref MRX_ShopStatesRequest m_Request;
	protected int m_iIndex;
	protected bool m_bDone;

	//------------------------------------------------------------------------------------------------
	void MRX_ShopStateReply(MRX_ShopStatesRequest request, int index)
	{
		m_Request = request;
		m_iIndex = index;
	}

	//------------------------------------------------------------------------------------------------
	override void OnResult(MRX_ShopProductState state)
	{
		super.OnResult(state);
		if (m_bDone || !m_Request)
			return;

		m_bDone = true;
		m_Request.OnState(m_iIndex, state);
	}
}

//------------------------------------------------------------------------------------------------
//! Internal.
class MRX_ShopStatesDelivery : MRX_DeferredCall
{
	protected ref MRX_ShopStatesRequest m_Request;

	//------------------------------------------------------------------------------------------------
	void MRX_ShopStatesDelivery(MRX_ShopStatesRequest request)
	{
		m_Request = request;
	}

	//------------------------------------------------------------------------------------------------
	override void Run()
	{
		m_Request.Deliver();
	}
}

//------------------------------------------------------------------------------------------------
//! A product that is not an inventory item (API v0), e.g. a stash upgrade or a service. Set it on a catalog item
//! (MRX_ShopItem.m_Product) and sell it in the shop window: the shop checks it, takes the payment, has it delivered
//! and refunds the payment when the delivery fails. Products are never bought back. Server side; callbacks may run
//! right away or on a later frame.
[BaseContainerProps()]
class MRX_ShopProduct
{
	//------------------------------------------------------------------------------------------------
	//! Before the payment: whether the player may buy the product now. Answer OK, or e.g. LIMIT_REACHED.
	void Check(int playerId, string ownerId, notnull MRX_ShopItem item, notnull MRX_ShopProductCallback callback)
	{
		callback.OnResult(MRX_EShopStatus.OK);
	}

	//------------------------------------------------------------------------------------------------
	//! After the payment: grants the product. Answer OK, or the failure (the payment is then refunded). Check the
	//! conditions again here: other requests may have run since Check().
	void Deliver(int playerId, string ownerId, notnull MRX_ShopItem item, notnull MRX_ShopProductCallback callback)
	{
		callback.OnResult(MRX_EShopStatus.DELIVERY_FAILED);
	}

	//------------------------------------------------------------------------------------------------
	//! State for the shop window, e.g. the current level and whether it can be bought now.
	void GetState(int playerId, string ownerId, notnull MRX_ShopItem item, notnull MRX_ShopProductStateCallback callback)
	{
		callback.OnResult(MRX_ShopProductState.Create(true, string.Empty));
	}
}
