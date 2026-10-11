//! One line of a cart checkout (API v0): an item that is delivered or sold as a whole, with everything it holds.
class MRX_ShopCartLine : Managed
{
	//! Weak: an entity in the engine implementation.
	Managed m_Item;
	//! Purchase: catalog IDs the line is paid for, one per paid item (the item itself and paid items inside it). Empty
	//! for a line that costs nothing, e.g. an item of the player's own that got into the cart. Sale: unused.
	ref array<string> m_aItemIds = {};
}

//! Cart of a checkout (API v0), e.g. the cart and the sale box of a Marx arsenal (MRX_ArsenalTradeSession). The cart
//! lists and moves the items; the shop service resolves the prices from the catalog.
class MRX_ShopCart : Managed
{
	//------------------------------------------------------------------------------------------------
	//! \param selling True: the items to sell, false: the items to buy, in the order they are delivered.
	//! \return Count.
	int GetLines(bool selling, notnull array<ref MRX_ShopCartLine> outLines)
	{
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Purchase: moves a paid line into the player's inventory and reports whether it went, possibly on a later frame.
	void Deliver(int playerId, notnull MRX_ShopCartLine line, notnull MRX_ShopDeliveryCallback callback)
	{
		callback.OnResult(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Sale: what a line holds, like MRX_ShopInventory.InspectForSale with contents (issued items: ISSUED, or an empty
	//! prefab among the contents), for an item of the cart instead of one the player carries.
	MRX_EShopStatus InspectForSale(notnull MRX_ShopCartLine line, out ResourceName prefab, notnull array<ResourceName> outContents)
	{
		return MRX_EShopStatus.NOT_IN_INVENTORY;
	}

	//------------------------------------------------------------------------------------------------
	//! Sale: gives a line that is not sold back to the player.
	void Return(int playerId, notnull MRX_ShopCartLine line)
	{
	}

	//------------------------------------------------------------------------------------------------
	//! Sale: deletes a sold line with everything it holds. True on success.
	bool Remove(notnull MRX_ShopCartLine line)
	{
		return false;
	}
}

//! Result of a checkout (API v0). m_iPrice and m_sCurrency hold the first currency.
class MRX_ShopCheckoutResult : MRX_ShopResult
{
	//! Amounts by currency: paid (purchase, after refunds) or received (sale).
	ref array<string> m_aCurrencies = {};
	ref array<int> m_aAmounts = {};
	//! Lines delivered (purchase) or sold, the ones worth nothing included.
	int m_iDoneCount;
	//! Lines left in the cart (purchase: refused or no room) or given back (sale: refused or not paid).
	int m_iLeftCount;

	//------------------------------------------------------------------------------------------------
	void AddAmount(string currency, int amount)
	{
		if (amount <= 0)
			return;

		int index = m_aCurrencies.Find(currency);
		if (index < 0)
		{
			m_aCurrencies.Insert(currency);
			m_aAmounts.Insert(amount);
		}
		else
		{
			m_aAmounts[index] = MRX_ShopCheckoutOp.AddCapped(m_aAmounts[index], amount);
		}

		m_sCurrency = m_aCurrencies[0];
		m_iPrice = m_aAmounts[0];
	}
}

//------------------------------------------------------------------------------------------------
//! One payment of a checkout, in one currency. Internal.
class MRX_ShopCheckoutPayment : Managed
{
	string m_sCurrency;
	int m_iAmount;
	bool m_bPaid;
	bool m_bRetried;
}

//------------------------------------------------------------------------------------------------
//! Shared part of the checkout requests: one payment per currency, each sent again once when its outcome is unknown.
//! Internal.
class MRX_ShopCheckoutOp : MRX_ShopOp
{
	protected ref MRX_ShopCart m_Cart;
	protected ref MRX_ShopCheckoutResult m_CheckoutResult;
	protected ref array<ref MRX_ShopCheckoutPayment> m_aPayments = {};
	protected int m_iPaymentIndex;
	//! Ledger reason: the catalog IDs of the checkout.
	protected string m_sItemIds;

	//------------------------------------------------------------------------------------------------
	//! a + b, capped at int.MAX.
	static int AddCapped(int a, int b)
	{
		if (a > int.MAX - b)
			return int.MAX;

		return a + b;
	}

	//------------------------------------------------------------------------------------------------
	protected void InitCheckout(MRX_ShopService service, int playerId, MRX_ShopDefinition shop, MRX_ShopCart cart, MRX_ShopCallback callback)
	{
		Init(service, playerId, shop, callback);
		m_Cart = cart;
		m_CheckoutResult = new MRX_ShopCheckoutResult();
		m_CheckoutResult.m_eStatus = MRX_EShopStatus.OK;
		m_CheckoutResult.m_sRequestId = m_Result.m_sRequestId;
		m_Result = m_CheckoutResult;
	}

	//------------------------------------------------------------------------------------------------
	protected void AddToPayment(string currency, int amount)
	{
		if (amount <= 0)
			return;

		foreach (MRX_ShopCheckoutPayment payment : m_aPayments)
		{
			if (payment.m_sCurrency == currency)
			{
				payment.m_iAmount = AddCapped(payment.m_iAmount, amount);
				return;
			}
		}

		MRX_ShopCheckoutPayment newPayment = new MRX_ShopCheckoutPayment();
		newPayment.m_sCurrency = currency;
		newPayment.m_iAmount = amount;
		m_aPayments.Insert(newPayment);
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_TxContext CreateCheckoutContext(string action, string currency)
	{
		return CreateContext(action, m_sItemIds, action + ":" + currency);
	}

	//------------------------------------------------------------------------------------------------
	//! True once per payment whose outcome is unknown (see MRX_ShopOp.ShouldRetryPayment).
	protected static bool ShouldRetry(notnull MRX_ShopCheckoutPayment payment, notnull MRX_TxResult result)
	{
		if (result.m_eStatus != MRX_ETxStatus.STORAGE_ERROR || payment.m_bRetried)
			return false;

		payment.m_bRetried = true;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_ShopCheckoutPayment GetCurrentPayment()
	{
		if (!m_aPayments.IsIndexValid(m_iPaymentIndex))
			return null;

		return m_aPayments[m_iPaymentIndex];
	}
}

//------------------------------------------------------------------------------------------------
//! Checkout of the items to buy: checks every line, takes the total of each currency in one payment, moves the lines into
//! the player's inventory and refunds the lines that did not fit (they stay in the cart). Internal.
class MRX_ShopCheckoutBuyOp : MRX_ShopCheckoutOp
{
	protected ref array<ref MRX_ShopCartLine> m_aLines = {};
	protected ref array<ref MRX_ShopDeliveryCallback> m_aDeliveries = {};
	//! Refunds of the lines that did not fit, or of every payment after a failed one.
	protected ref array<ref MRX_ShopCheckoutPayment> m_aRefunds = {};
	protected int m_iRefundIndex;
	protected int m_iPendingDeliveries;
	protected MRX_EShopStatus m_eFinalStatus = MRX_EShopStatus.OK;

	//------------------------------------------------------------------------------------------------
	void MRX_ShopCheckoutBuyOp(MRX_ShopService service, int playerId, MRX_ShopDefinition shop, MRX_ShopCart cart, MRX_ShopCallback callback)
	{
		InitCheckout(service, playerId, shop, cart, callback);
	}

	//------------------------------------------------------------------------------------------------
	override void Start(string ownerId)
	{
		m_sOwnerId = ownerId;
		array<ref MRX_ShopCartLine> lines = {};
		m_Cart.GetLines(false, lines);
		array<string> itemIds = {};
		foreach (MRX_ShopCartLine line : lines)
		{
			if (!CanBuyLine(line))
			{
				m_CheckoutResult.m_iLeftCount++;
				continue;
			}

			m_aLines.Insert(line);
			foreach (string itemId : line.m_aItemIds)
			{
				MRX_ShopItem item = m_Shop.m_Catalog.FindItem(itemId);
				AddToPayment(item.m_sCurrency, item.m_iPrice);
				itemIds.Insert(itemId);
			}
		}

		m_sItemIds = SCR_StringHelper.Join(",", itemIds);
		if (m_aLines.IsEmpty())
		{
			if (m_CheckoutResult.m_iLeftCount > 0)
				Finish(MRX_EShopStatus.REJECTED, null, true);
			else
				Finish(MRX_EShopStatus.OK, null, true);

			return;
		}

		PayNext();
	}

	//------------------------------------------------------------------------------------------------
	//! Every paid item is for sale and allowed by the validators.
	protected bool CanBuyLine(notnull MRX_ShopCartLine line)
	{
		foreach (string itemId : line.m_aItemIds)
		{
			MRX_ShopItem item = m_Shop.m_Catalog.FindItem(itemId);
			if (!item || item.m_iPrice <= 0 || item.m_Product)
				return false;

			if (!m_Service.IsAllowed(true, m_iPlayerId, m_sOwnerId, m_Shop, item))
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void PayNext()
	{
		MRX_ShopCheckoutPayment payment = GetCurrentPayment();
		if (!payment)
		{
			DeliverAll();
			return;
		}

		MRX_TxCallback callback = new MRX_TxCallback();
		callback.GetOnResult().Insert(OnPaid);
		m_Service.GetEconomy().Debit(m_sOwnerId, payment.m_sCurrency, payment.m_iAmount, CreateCheckoutContext("checkout", payment.m_sCurrency), callback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPaid(MRX_TxResult result)
	{
		MRX_ShopCheckoutPayment payment = GetCurrentPayment();
		if (ShouldRetry(payment, result))
		{
			PayNext();
			return;
		}

		if (!result.IsCommitted())
		{
			if (result.m_eStatus == MRX_ETxStatus.STORAGE_ERROR)
			{
				Print(string.Format("[MRX] Checkout payment of shop request %1 failed twice (STORAGE_ERROR), outcome unknown: owner %2 may have paid %3 %4 for '%5' without getting it",
					m_Result.m_sRequestId, m_sOwnerId, payment.m_iAmount, payment.m_sCurrency, m_sItemIds), LogLevel.ERROR);
			}

			// Nothing is delivered: the payments that went through are refunded.
			m_Result.m_eTxStatus = result.m_eStatus;
			m_eFinalStatus = MRX_EShopStatus.PAYMENT_FAILED;
			m_CheckoutResult.m_iLeftCount += m_aLines.Count();
			foreach (MRX_ShopCheckoutPayment paid : m_aPayments)
			{
				if (paid.m_bPaid)
					m_aRefunds.Insert(paid);
			}

			RefundNext();
			return;
		}

		payment.m_bPaid = true;
		m_iPaymentIndex++;
		PayNext();
	}

	//------------------------------------------------------------------------------------------------
	protected void DeliverAll()
	{
		m_iPendingDeliveries = m_aLines.Count();
		foreach (MRX_ShopCartLine line : m_aLines)
		{
			MRX_ShopCheckoutDelivery delivery = new MRX_ShopCheckoutDelivery(this, line);
			m_aDeliveries.Insert(delivery);
			m_Cart.Deliver(m_iPlayerId, line, delivery);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by MRX_ShopCheckoutDelivery once per line.
	void OnLineDelivered(notnull MRX_ShopCartLine line, bool success)
	{
		foreach (string itemId : line.m_aItemIds)
		{
			MRX_ShopItem item = m_Shop.m_Catalog.FindItem(itemId);
			if (!item)
				continue;

			if (!success)
			{
				AddRefund(item.m_sCurrency, item.m_iPrice);
				continue;
			}

			ScriptInvokerBase<MRX_ShopTradeDelegate> onPurchase = m_Service.GetOnPurchase();
			onPurchase.Invoke(m_iPlayerId, m_sOwnerId, m_Shop, item, item.m_iPrice);
		}

		if (success)
			m_CheckoutResult.m_iDoneCount++;
		else
			m_CheckoutResult.m_iLeftCount++;

		m_iPendingDeliveries--;
		if (m_iPendingDeliveries == 0)
			RefundNext();
	}

	//------------------------------------------------------------------------------------------------
	protected void AddRefund(string currency, int amount)
	{
		if (amount <= 0)
			return;

		foreach (MRX_ShopCheckoutPayment refund : m_aRefunds)
		{
			if (refund.m_sCurrency == currency)
			{
				refund.m_iAmount = AddCapped(refund.m_iAmount, amount);
				return;
			}
		}

		MRX_ShopCheckoutPayment newRefund = new MRX_ShopCheckoutPayment();
		newRefund.m_sCurrency = currency;
		newRefund.m_iAmount = amount;
		m_aRefunds.Insert(newRefund);
	}

	//------------------------------------------------------------------------------------------------
	protected void RefundNext()
	{
		if (!m_aRefunds.IsIndexValid(m_iRefundIndex))
		{
			FinishCheckout();
			return;
		}

		MRX_ShopCheckoutPayment refund = m_aRefunds[m_iRefundIndex];
		MRX_TxCallback callback = new MRX_TxCallback();
		callback.GetOnResult().Insert(OnRefunded);
		m_Service.GetEconomy().Credit(m_sOwnerId, refund.m_sCurrency, refund.m_iAmount, CreateCheckoutContext("checkout_refund", refund.m_sCurrency), callback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnRefunded(MRX_TxResult result)
	{
		MRX_ShopCheckoutPayment refund = m_aRefunds[m_iRefundIndex];
		if (ShouldRetry(refund, result))
		{
			RefundNext();
			return;
		}

		if (result.IsCommitted())
		{
			refund.m_bPaid = true;
		}
		else
		{
			Print(string.Format("[MRX] Checkout refund of shop request %1 failed (%2): owner %3 paid %4 %5 for items left in the cart",
				m_Result.m_sRequestId, typename.EnumToString(MRX_ETxStatus, result.m_eStatus), m_sOwnerId, refund.m_iAmount, refund.m_sCurrency), LogLevel.ERROR);
		}

		m_iRefundIndex++;
		RefundNext();
	}

	//------------------------------------------------------------------------------------------------
	//! Amounts paid, less what was refunded.
	protected void FinishCheckout()
	{
		foreach (MRX_ShopCheckoutPayment payment : m_aPayments)
		{
			if (!payment.m_bPaid)
				continue;

			int amount = payment.m_iAmount;
			foreach (MRX_ShopCheckoutPayment refund : m_aRefunds)
			{
				if (refund.m_bPaid && refund.m_sCurrency == payment.m_sCurrency)
					amount -= refund.m_iAmount;
			}

			m_CheckoutResult.AddAmount(payment.m_sCurrency, amount);
		}

		Finish(m_eFinalStatus, null, true);
	}
}

//------------------------------------------------------------------------------------------------
//! Reports the delivery of one checkout line to its op. Internal.
class MRX_ShopCheckoutDelivery : MRX_ShopDeliveryCallback
{
	//! Weak: the service keeps the op until it is done.
	protected MRX_ShopCheckoutBuyOp m_Op;
	protected ref MRX_ShopCartLine m_Line;
	protected bool m_bReported;

	//------------------------------------------------------------------------------------------------
	void MRX_ShopCheckoutDelivery(MRX_ShopCheckoutBuyOp op, MRX_ShopCartLine line)
	{
		m_Op = op;
		m_Line = line;
	}

	//------------------------------------------------------------------------------------------------
	override void OnResult(bool success)
	{
		super.OnResult(success);
		if (m_bReported || !m_Op)
			return;

		m_bReported = true;
		m_Op.OnLineDelivered(m_Line, success);
	}
}

//------------------------------------------------------------------------------------------------
//! One sold line of a sale checkout. Internal.
class MRX_ShopCheckoutSale : Managed
{
	ref MRX_ShopCartLine m_Line;
	MRX_ShopItem m_Item;
	ResourceName m_sPrefab;
	int m_iPrice;
	ref Managed m_ReturnCapture;
}

//------------------------------------------------------------------------------------------------
//! Checkout of the items to sell: prices every line with its contents (issued items and items the shop does not buy are
//! worth nothing), gives refused lines back, deletes the rest first and pays the total of each currency at once. When a
//! payment fails, its lines are given back. Internal.
class MRX_ShopCheckoutSellOp : MRX_ShopCheckoutOp
{
	protected ref array<ref MRX_ShopCheckoutSale> m_aSales = {};
	protected ref array<ref MRX_ShopDeliveryCallback> m_aReturns = {};

	//------------------------------------------------------------------------------------------------
	void MRX_ShopCheckoutSellOp(MRX_ShopService service, int playerId, MRX_ShopDefinition shop, MRX_ShopCart cart, MRX_ShopCallback callback)
	{
		InitCheckout(service, playerId, shop, cart, callback);
	}

	//------------------------------------------------------------------------------------------------
	override void Start(string ownerId)
	{
		m_sOwnerId = ownerId;
		if (!m_Shop.m_bAllowSell)
		{
			Finish(MRX_EShopStatus.SELL_DISABLED, null, false);
			return;
		}

		array<ref MRX_ShopCartLine> lines = {};
		m_Cart.GetLines(true, lines);
		array<string> itemIds = {};
		foreach (MRX_ShopCartLine line : lines)
		{
			MRX_ShopCheckoutSale sale = Price(line);
			if (!sale)
			{
				m_Cart.Return(m_iPlayerId, line);
				m_CheckoutResult.m_iLeftCount++;
				continue;
			}

			if (sale.m_iPrice > 0)
				sale.m_ReturnCapture = m_Service.GetInventory().CaptureForReturn(m_iPlayerId, line.m_Item);

			// Remove first: losing a payment is recoverable by admins, a duplicated item is not.
			if (!m_Cart.Remove(line))
			{
				m_CheckoutResult.m_iLeftCount++;
				continue;
			}

			m_CheckoutResult.m_iDoneCount++;
			if (sale.m_iPrice <= 0)
				continue;

			m_aSales.Insert(sale);
			AddToPayment(sale.m_Item.m_sCurrency, sale.m_iPrice);
			itemIds.Insert(sale.m_Item.m_sId);
		}

		m_sItemIds = SCR_StringHelper.Join(",", itemIds);
		PayNext();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The line with its price (0: disposed of), or null when it must go back to the player.
	protected MRX_ShopCheckoutSale Price(notnull MRX_ShopCartLine line)
	{
		MRX_ShopCheckoutSale sale = new MRX_ShopCheckoutSale();
		sale.m_Line = line;
		array<ResourceName> contents = {};
		MRX_EShopStatus status = m_Cart.InspectForSale(line, sale.m_sPrefab, contents);
		if (status == MRX_EShopStatus.ISSUED)
			return sale;

		if (status != MRX_EShopStatus.OK)
			return null;

		sale.m_Item = m_Shop.m_Catalog.FindByPrefab(sale.m_sPrefab);
		if (!sale.m_Item)
			return sale;

		sale.m_iPrice = MRX_ShopService.GetSellPriceWithContents(m_Shop, sale.m_sPrefab, contents);
		if (sale.m_iPrice > 0 && !m_Service.IsAllowed(false, m_iPlayerId, m_sOwnerId, m_Shop, sale.m_Item))
			return null;

		return sale;
	}

	//------------------------------------------------------------------------------------------------
	protected void PayNext()
	{
		MRX_ShopCheckoutPayment payment = GetCurrentPayment();
		if (!payment)
		{
			Finish(m_CheckoutResult.m_eStatus, null, false);
			return;
		}

		MRX_TxCallback callback = new MRX_TxCallback();
		callback.GetOnResult().Insert(OnPaid);
		m_Service.GetEconomy().Credit(m_sOwnerId, payment.m_sCurrency, payment.m_iAmount, CreateCheckoutContext("checkout_sell", payment.m_sCurrency), callback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPaid(MRX_TxResult result)
	{
		MRX_ShopCheckoutPayment payment = GetCurrentPayment();
		if (ShouldRetry(payment, result))
		{
			PayNext();
			return;
		}

		if (result.IsCommitted())
		{
			payment.m_bPaid = true;
			m_CheckoutResult.AddAmount(payment.m_sCurrency, payment.m_iAmount);
			foreach (MRX_ShopCheckoutSale sale : m_aSales)
			{
				if (sale.m_Item.m_sCurrency != payment.m_sCurrency)
					continue;

				ScriptInvokerBase<MRX_ShopTradeDelegate> onSale = m_Service.GetOnSale();
				onSale.Invoke(m_iPlayerId, m_sOwnerId, m_Shop, sale.m_Item, sale.m_iPrice);
			}
		}
		else
		{
			Print(string.Format("[MRX] Checkout payment for sold items failed (%1), request %2: returning the items sold for %3 to player %4",
				typename.EnumToString(MRX_ETxStatus, result.m_eStatus), m_Result.m_sRequestId, payment.m_sCurrency, m_iPlayerId), LogLevel.ERROR);
			m_Result.m_eTxStatus = result.m_eStatus;
			m_CheckoutResult.m_eStatus = MRX_EShopStatus.PAYMENT_FAILED;
			GiveBack(payment.m_sCurrency);
		}

		m_iPaymentIndex++;
		PayNext();
	}

	//------------------------------------------------------------------------------------------------
	//! Gives the lines sold for a currency back to the player, with what they held.
	protected void GiveBack(string currency)
	{
		foreach (MRX_ShopCheckoutSale sale : m_aSales)
		{
			if (sale.m_Item.m_sCurrency != currency)
				continue;

			m_CheckoutResult.m_iDoneCount--;
			m_CheckoutResult.m_iLeftCount++;
			MRX_ShopDeliveryCallback callback = new MRX_ShopDeliveryCallback();
			m_aReturns.Insert(callback);
			m_Service.GetInventory().GiveBack(m_iPlayerId, sale.m_sPrefab, sale.m_ReturnCapture, callback);
		}
	}
}
