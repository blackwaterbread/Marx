#ifdef WORKBENCH
// Tests of MRX_ShopService with a fake inventory and the in-memory economy.

//------------------------------------------------------------------------------------------------
class MRX_ShopTests
{
	static const ResourceName PREFAB_RIFLE = "Prefabs/Test/Rifle.et";
	static const ResourceName PREFAB_AMMO = "Prefabs/Test/Ammo.et";
	static const ResourceName PREFAB_MAP = "Prefabs/Test/Map.et";
	static const ResourceName PREFAB_RADIO = "Prefabs/Test/Radio.et";
	static const ResourceName PREFAB_UNKNOWN = "Prefabs/Test/Unknown.et";
	static const ResourceName PREFAB_GEM = "Prefabs/Test/Gem.et";

	//------------------------------------------------------------------------------------------------
	static void Register(notnull MRX_TestRunner runner)
	{
		runner.Add(new MRX_Test_ShopPricing());
		runner.Add(new MRX_Test_ShopCatalog());
		runner.Add(new MRX_Test_ShopBuy());
		runner.Add(new MRX_Test_ShopBuyRefused());
		runner.Add(new MRX_Test_ShopNoSpace());
		runner.Add(new MRX_Test_ShopDeliveryRefund());
		runner.Add(new MRX_Test_ShopBusy());
		runner.Add(new MRX_Test_ShopSell());
		runner.Add(new MRX_Test_ShopSellDisabled());
		runner.Add(new MRX_Test_ShopValidator());
		runner.Add(new MRX_Test_ShopSellPriceWithContents());
		runner.Add(new MRX_Test_ShopBuyTarget());
		runner.Add(new MRX_Test_ShopSellContents());
		runner.Add(new MRX_Test_ShopSellGiveBack());
		runner.Add(new MRX_Test_ShopSellIssuedContents());
		runner.Add(new MRX_Test_ShopProduct());
		runner.Add(new MRX_Test_ShopProductRefund());
		runner.Add(new MRX_Test_ShopProductStates());
	}

	//------------------------------------------------------------------------------------------------
	//! rifle 100 (sold back at the shop percentage), ammo 10 (sold back at 3), map not for sale, radio not bought back.
	static MRX_ShopCatalog CreateCatalog()
	{
		MRX_ShopCatalog catalog = new MRX_ShopCatalog();
		catalog.m_aItems = {};
		catalog.m_aItems.Insert(MRX_ShopItem.Create("rifle", PREFAB_RIFLE, 100));
		catalog.m_aItems.Insert(MRX_ShopItem.Create("ammo", PREFAB_AMMO, 10, "cash", 3));
		catalog.m_aItems.Insert(MRX_ShopItem.Create("map", PREFAB_MAP, 0));
		catalog.m_aItems.Insert(MRX_ShopItem.Create("radio", PREFAB_RADIO, 40, "cash", 0));
		return catalog;
	}
}

//------------------------------------------------------------------------------------------------
class MRX_TestSaleItem : Managed
{
	int m_iHolderPlayerId;
	ResourceName m_sPrefab;
	bool m_bEmpty = true;
	bool m_bRemoved;
	//! What the item holds, recursively (sales with contents).
	ref array<ResourceName> m_aContents = {};
	bool m_bRefundable = true;
}

//------------------------------------------------------------------------------------------------
class MRX_TestShopInventory : MRX_ShopInventory
{
	int m_iFreeSlots = 10;
	bool m_bFailDelivery;
	//! A delivery target given to CanGive() has no room.
	bool m_bTargetFull;
	//! "playerId:prefab" of every delivered item.
	ref array<string> m_aGiven = {};
	//! Delivery target of every delivered item (null: anywhere).
	ref array<Managed> m_aGivenTargets = {};
	//! "playerId:prefab:content count" of every item given back.
	ref array<string> m_aGivenBack = {};

	//------------------------------------------------------------------------------------------------
	override bool CanGive(int playerId, ResourceName prefab, Managed target = null)
	{
		if (target)
			return !m_bTargetFull;

		return m_iFreeSlots > 0;
	}

	//------------------------------------------------------------------------------------------------
	override void Give(int playerId, ResourceName prefab, notnull MRX_ShopDeliveryCallback callback, Managed target = null)
	{
		if (!m_bFailDelivery)
		{
			m_iFreeSlots--;
			m_aGiven.Insert(playerId.ToString() + ":" + prefab);
			m_aGivenTargets.Insert(target);
		}

		callback.OnResult(!m_bFailDelivery);
	}

	//------------------------------------------------------------------------------------------------
	override Managed CaptureForReturn(int playerId, Managed item)
	{
		return item;
	}

	//------------------------------------------------------------------------------------------------
	override void GiveBack(int playerId, ResourceName prefab, Managed capture, notnull MRX_ShopDeliveryCallback callback)
	{
		MRX_TestSaleItem saleItem = MRX_TestSaleItem.Cast(capture);
		int contents = -1;
		if (saleItem)
		{
			contents = saleItem.m_aContents.Count();
			saleItem.m_bRemoved = false;
		}

		m_aGivenBack.Insert(string.Format("%1:%2:%3", playerId, prefab, contents));
		callback.OnResult(true);
	}

	//------------------------------------------------------------------------------------------------
	override MRX_EShopStatus InspectForSale(int playerId, Managed item, out ResourceName prefab, array<ResourceName> outContents = null)
	{
		MRX_TestSaleItem saleItem = MRX_TestSaleItem.Cast(item);
		if (!saleItem || saleItem.m_bRemoved || saleItem.m_iHolderPlayerId != playerId)
			return MRX_EShopStatus.NOT_IN_INVENTORY;

		if (outContents)
		{
			if (!saleItem.m_bRefundable)
				return MRX_EShopStatus.NOT_BUYABLE;

			outContents.InsertAll(saleItem.m_aContents);
		}
		else if (!saleItem.m_bEmpty)
		{
			return MRX_EShopStatus.NOT_EMPTY;
		}

		prefab = saleItem.m_sPrefab;
		return MRX_EShopStatus.OK;
	}

	//------------------------------------------------------------------------------------------------
	override bool Remove(Managed item)
	{
		MRX_TestSaleItem saleItem = MRX_TestSaleItem.Cast(item);
		if (!saleItem || saleItem.m_bRemoved)
			return false;

		saleItem.m_bRemoved = true;
		return true;
	}
}

//------------------------------------------------------------------------------------------------
class MRX_TestRejectItemValidator : MRX_ShopValidator
{
	string m_sRejectedItemId;

	//------------------------------------------------------------------------------------------------
	override bool CanBuy(int playerId, string ownerId, MRX_ShopDefinition shop, MRX_ShopItem item)
	{
		return item.m_sId != m_sRejectedItemId;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanSell(int playerId, string ownerId, MRX_ShopDefinition shop, MRX_ShopItem item)
	{
		return item.m_sId != m_sRejectedItemId;
	}
}

//------------------------------------------------------------------------------------------------
enum MRX_EShopStepKind
{
	BUY,
	SELL,
	BALANCE
}

//------------------------------------------------------------------------------------------------
class MRX_ShopStep : Managed
{
	MRX_EShopStepKind m_eKind;
	int m_iPlayerId;
	string m_sItemId;
	int m_iSaleItem = -1;
	bool m_bNullShop;
	bool m_bWithContents;
	ref Managed m_Target;
	int m_iExpectedPrice = -1;
	int m_iExpectedItemCount = -1;
	int m_iExpectedUnpaidCount = -1;
	MRX_EShopStatus m_eExpected = MRX_EShopStatus.OK;
	bool m_bCheckTx;
	MRX_ETxStatus m_eExpectedTx;
	int m_iExpectedBalance;

	//------------------------------------------------------------------------------------------------
	MRX_ShopStep Expect(MRX_EShopStatus status)
	{
		m_eExpected = status;
		return this;
	}

	//------------------------------------------------------------------------------------------------
	MRX_ShopStep ExpectTx(MRX_ETxStatus status)
	{
		m_bCheckTx = true;
		m_eExpectedTx = status;
		return this;
	}

	//------------------------------------------------------------------------------------------------
	MRX_ShopStep WithoutShop()
	{
		m_bNullShop = true;
		return this;
	}

	//------------------------------------------------------------------------------------------------
	MRX_ShopStep WithContents()
	{
		m_bWithContents = true;
		return this;
	}

	//------------------------------------------------------------------------------------------------
	MRX_ShopStep To(Managed target)
	{
		m_Target = target;
		return this;
	}

	//------------------------------------------------------------------------------------------------
	MRX_ShopStep ExpectPrice(int price)
	{
		m_iExpectedPrice = price;
		return this;
	}

	//------------------------------------------------------------------------------------------------
	MRX_ShopStep ExpectCounts(int itemCount, int unpaidCount)
	{
		m_iExpectedItemCount = itemCount;
		m_iExpectedUnpaidCount = unpaidCount;
		return this;
	}

	//------------------------------------------------------------------------------------------------
	string Describe(int index)
	{
		return string.Format("step %1 %2 player=%3 item=%4 sale=%5", index, typename.EnumToString(MRX_EShopStepKind, m_eKind), m_iPlayerId, m_sItemId, m_iSaleItem);
	}
}

//------------------------------------------------------------------------------------------------
//! Players 1 and 2 have owners ("shop-a", "shop-b"), player 9 has none. Everyone starts with 100 cash.
class MRX_ShopScenarioTest : MRX_TestCase
{
	protected ref MRX_TestIdentityService m_Identity;
	protected ref MRX_EconomyService m_Economy;
	protected ref MRX_TestShopInventory m_Inventory;
	protected ref MRX_ShopService m_Shop;
	protected ref MRX_ShopDefinition m_ShopDef;
	protected ref array<ref MRX_TestSaleItem> m_aSaleItems = {};
	protected ref array<ref MRX_ShopStep> m_aSteps = {};
	protected ref MRX_ShopCallback m_ShopCallback;
	protected ref MRX_BalanceCallback m_BalanceCallback;
	protected int m_iStep = -1;
	protected int m_iPurchaseEvents;
	protected int m_iSaleEvents;

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		m_Identity = new MRX_TestIdentityService();
		m_Identity.m_mFakeIds.Set(1, "shop-a");
		m_Identity.m_mFakeIds.Set(2, "shop-b");
		m_Identity.SimulateAudit(1);
		m_Identity.SimulateAudit(2);

		m_Economy = MRX_TestUtils.CreateService();
		m_Inventory = new MRX_TestShopInventory();
		m_Shop = new MRX_ShopService(m_Economy, m_Identity, m_Inventory);
		m_Shop.GetOnPurchase().Insert(OnPurchase);
		m_Shop.GetOnSale().Insert(OnSale);
		m_ShopDef = MRX_ShopDefinition.Create("test_shop", MRX_ShopTests.CreateCatalog());

		m_ShopCallback = new MRX_ShopCallback();
		m_ShopCallback.GetOnResult().Insert(OnShopResult);
		m_BalanceCallback = new MRX_BalanceCallback();
		m_BalanceCallback.GetOnResult().Insert(OnBalance);

		DefineSteps();
		NextStep();
	}

	//------------------------------------------------------------------------------------------------
	protected void DefineSteps()
	{
	}

	//------------------------------------------------------------------------------------------------
	//! Item held by a player, to be sold. \return Index for Sell().
	protected int AddSaleItem(int holderPlayerId, ResourceName prefab, bool empty = true, array<ResourceName> contents = null)
	{
		MRX_TestSaleItem item = new MRX_TestSaleItem();
		item.m_iHolderPlayerId = holderPlayerId;
		item.m_sPrefab = prefab;
		item.m_bEmpty = empty;
		if (contents)
		{
			item.m_aContents.InsertAll(contents);
			item.m_bEmpty = contents.IsEmpty();
		}

		return m_aSaleItems.Insert(item);
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_ShopStep AddStep(MRX_EShopStepKind kind, int playerId)
	{
		MRX_ShopStep step = new MRX_ShopStep();
		step.m_eKind = kind;
		step.m_iPlayerId = playerId;
		m_aSteps.Insert(step);
		return step;
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_ShopStep Buy(int playerId, string itemId)
	{
		MRX_ShopStep step = AddStep(MRX_EShopStepKind.BUY, playerId);
		step.m_sItemId = itemId;
		return step;
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_ShopStep Sell(int playerId, int saleItem)
	{
		MRX_ShopStep step = AddStep(MRX_EShopStepKind.SELL, playerId);
		step.m_iSaleItem = saleItem;
		return step;
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_ShopStep Balance(int playerId, int expected)
	{
		MRX_ShopStep step = AddStep(MRX_EShopStepKind.BALANCE, playerId);
		step.m_iExpectedBalance = expected;
		return step;
	}

	//------------------------------------------------------------------------------------------------
	//! Called after the last step.
	protected void CheckEnd()
	{
	}

	//------------------------------------------------------------------------------------------------
	protected void NextStep()
	{
		m_iStep++;
		if (m_iStep >= m_aSteps.Count())
		{
			CheckEnd();
			Finish();
			return;
		}

		MRX_ShopStep step = m_aSteps[m_iStep];
		MRX_ShopDefinition shop = m_ShopDef;
		if (step.m_bNullShop)
			shop = null;

		switch (step.m_eKind)
		{
			case MRX_EShopStepKind.BUY:
				m_Shop.Buy(step.m_iPlayerId, shop, step.m_sItemId, m_ShopCallback, step.m_Target);
				break;

			case MRX_EShopStepKind.SELL:
				m_Shop.Sell(step.m_iPlayerId, shop, m_aSaleItems[step.m_iSaleItem], m_ShopCallback, step.m_bWithContents);
				break;

			case MRX_EShopStepKind.BALANCE:
				m_Economy.GetBalance(m_Identity.GetOwnerId(step.m_iPlayerId), "cash", m_BalanceCallback);
				break;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnShopResult(MRX_ShopResult result)
	{
		MRX_ShopStep step = m_aSteps[m_iStep];
		string label = step.Describe(m_iStep);
		if (result.m_eStatus != step.m_eExpected)
			Check(false, string.Format("%1: expected %2, got %3", label, typename.EnumToString(MRX_EShopStatus, step.m_eExpected), typename.EnumToString(MRX_EShopStatus, result.m_eStatus)));

		if (step.m_bCheckTx)
			CheckStatus(result.m_eTxStatus, step.m_eExpectedTx, label + " economy status");

		if (step.m_iExpectedPrice >= 0)
			CheckInt(result.m_iPrice, step.m_iExpectedPrice, label + " price");

		if (step.m_iExpectedItemCount >= 0)
			CheckInt(result.m_iItemCount, step.m_iExpectedItemCount, label + " item count");

		if (step.m_iExpectedUnpaidCount >= 0)
			CheckInt(result.m_iUnpaidCount, step.m_iExpectedUnpaidCount, label + " unpaid count");

		NextStep();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBalance(MRX_ETxStatus status, int balance)
	{
		MRX_ShopStep step = m_aSteps[m_iStep];
		CheckInt(balance, step.m_iExpectedBalance, step.Describe(m_iStep) + " balance");
		NextStep();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPurchase(int playerId, string ownerId, MRX_ShopDefinition shop, MRX_ShopItem item, int price)
	{
		m_iPurchaseEvents++;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSale(int playerId, string ownerId, MRX_ShopDefinition shop, MRX_ShopItem item, int price)
	{
		m_iSaleEvents++;
	}
}

//------------------------------------------------------------------------------------------------
class MRX_Test_ShopPricing : MRX_TestCase
{
	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		CheckInt(MRX_ShopDefinition.Percent(100, 50), 50, "50% of 100");
		CheckInt(MRX_ShopDefinition.Percent(99, 50), 49, "rounds down");
		CheckInt(MRX_ShopDefinition.Percent(int.MAX, 100), int.MAX, "100% of int.MAX");
		CheckInt(MRX_ShopDefinition.Percent(int.MAX, 50), 1073741823, "50% of int.MAX without overflow");
		CheckInt(MRX_ShopDefinition.Percent(-5, 50), 0, "negative value");
		CheckInt(MRX_ShopDefinition.Percent(100, 150), 100, "percent above 100 is clamped");

		MRX_ShopCatalog catalog = MRX_ShopTests.CreateCatalog();
		MRX_ShopDefinition shop = MRX_ShopDefinition.Create("s", catalog, 25);
		CheckInt(shop.GetSellPrice(catalog.FindItem("rifle")), 25, "shop percentage");
		CheckInt(shop.GetSellPrice(catalog.FindItem("ammo")), 3, "item sell price wins");
		CheckInt(shop.GetSellPrice(catalog.FindItem("radio")), 0, "item not bought back");

		MRX_ShopDefinition closed = MRX_ShopDefinition.Create("s", catalog, 25, false);
		CheckInt(closed.GetSellPrice(catalog.FindItem("rifle")), 0, "shop without selling");
		Finish();
	}
}

//------------------------------------------------------------------------------------------------
class MRX_Test_ShopCatalog : MRX_TestCase
{
	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		MRX_ShopCatalog catalog = new MRX_ShopCatalog();
		catalog.m_aItems = {};
		catalog.m_aItems.Insert(MRX_ShopItem.Create("a", "{ABCDEF0123456789}Prefabs/Test/A.et", 10));
		catalog.m_aItems.Insert(MRX_ShopItem.Create("", "Prefabs/Test/B.et", 10));
		catalog.m_aItems.Insert(MRX_ShopItem.Create("c", "", 10));
		catalog.m_aItems.Insert(MRX_ShopItem.Create("d", "Prefabs/Test/D.et", 10, "gold"));
		catalog.m_aItems.Insert(MRX_ShopItem.Create("a", "Prefabs/Test/Other.et", 10));
		catalog.m_aItems.Insert(MRX_ShopItem.Create("e", "Prefabs/Test/E.et", 10));

		CheckInt(catalog.Validate(MRX_TestUtils.CreateRules().m_Currencies, "test"), 2, "valid entries");
		MRX_ShopItem first = catalog.FindItem("a");
		Check(first != null, "duplicate ID kept once");
		if (first)
			CheckString(first.m_sPrefab, "{ABCDEF0123456789}Prefabs/Test/A.et", "first duplicate wins");
		Check(catalog.FindItem("e") != null, "valid entry kept");

		// The GUID identifies a prefab even when the path differs in case.
		Check(catalog.FindByPrefab("{ABCDEF0123456789}prefabs/test/a.et") != null, "lookup by GUID");
		Check(catalog.FindByPrefab("prefabs/test/e.et") != null, "lookup by path ignores case");
		Check(catalog.FindByPrefab("Prefabs/Test/B.et") == null, "removed entry not found");
		Finish();
	}
}

//------------------------------------------------------------------------------------------------
class MRX_Test_ShopBuy : MRX_ShopScenarioTest
{
	//------------------------------------------------------------------------------------------------
	override protected void DefineSteps()
	{
		Buy(1, "rifle");
		Balance(1, 0);
		Buy(1, "ammo").Expect(MRX_EShopStatus.PAYMENT_FAILED).ExpectTx(MRX_ETxStatus.INSUFFICIENT_FUNDS);
		Buy(2, "ammo");
		Balance(2, 90);
	}

	//------------------------------------------------------------------------------------------------
	override protected void CheckEnd()
	{
		CheckInt(m_Inventory.m_aGiven.Count(), 2, "delivered items");
		if (m_Inventory.m_aGiven.Count() == 2)
		{
			CheckString(m_Inventory.m_aGiven[0], "1:" + MRX_ShopTests.PREFAB_RIFLE, "first delivery");
			CheckString(m_Inventory.m_aGiven[1], "2:" + MRX_ShopTests.PREFAB_AMMO, "second delivery");
		}

		CheckInt(m_iPurchaseEvents, 2, "purchase events");
	}
}

//------------------------------------------------------------------------------------------------
class MRX_Test_ShopBuyRefused : MRX_ShopScenarioTest
{
	//------------------------------------------------------------------------------------------------
	override protected void DefineSteps()
	{
		Buy(1, "nothing").Expect(MRX_EShopStatus.UNKNOWN_ITEM);
		Buy(1, "map").Expect(MRX_EShopStatus.NOT_FOR_SALE);
		Buy(9, "rifle").Expect(MRX_EShopStatus.OWNER_NOT_READY);
		Buy(1, "rifle").WithoutShop().Expect(MRX_EShopStatus.UNKNOWN_SHOP);
		Balance(1, 100);
	}
}

//------------------------------------------------------------------------------------------------
//! No free slot: refused before any payment.
class MRX_Test_ShopNoSpace : MRX_ShopScenarioTest
{
	//------------------------------------------------------------------------------------------------
	override protected void DefineSteps()
	{
		m_Inventory.m_iFreeSlots = 0;
		Buy(1, "rifle").Expect(MRX_EShopStatus.NO_SPACE);
		Balance(1, 100);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_Test_ShopDeliveryRefund : MRX_ShopScenarioTest
{
	//------------------------------------------------------------------------------------------------
	override protected void DefineSteps()
	{
		m_Inventory.m_bFailDelivery = true;
		Buy(1, "rifle").Expect(MRX_EShopStatus.DELIVERY_FAILED);
		Balance(1, 100);
	}

	//------------------------------------------------------------------------------------------------
	override protected void CheckEnd()
	{
		CheckInt(m_iPurchaseEvents, 0, "purchase events");
	}
}

//------------------------------------------------------------------------------------------------
//! A second request of the same player while the first one runs is answered with BUSY.
class MRX_Test_ShopBusy : MRX_ShopScenarioTest
{
	protected ref MRX_ShopCallback m_FirstCallback;
	protected ref MRX_ShopCallback m_SecondCallback;
	protected int m_iAnswers;

	//------------------------------------------------------------------------------------------------
	override protected void DefineSteps()
	{
		m_FirstCallback = new MRX_ShopCallback();
		m_FirstCallback.GetOnResult().Insert(OnFirst);
		m_SecondCallback = new MRX_ShopCallback();
		m_SecondCallback.GetOnResult().Insert(OnSecond);
		m_Shop.Buy(1, m_ShopDef, "ammo", m_FirstCallback);
		m_Shop.Buy(1, m_ShopDef, "ammo", m_SecondCallback);
	}

	//------------------------------------------------------------------------------------------------
	override protected void NextStep()
	{
		// Steps are not used; the two answers finish the test.
	}

	//------------------------------------------------------------------------------------------------
	protected void OnFirst(MRX_ShopResult result)
	{
		Check(result.m_eStatus == MRX_EShopStatus.OK, "first request should succeed");
		Answered();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSecond(MRX_ShopResult result)
	{
		Check(result.m_eStatus == MRX_EShopStatus.BUSY, "second request should be BUSY");
		Answered();
	}

	//------------------------------------------------------------------------------------------------
	protected void Answered()
	{
		m_iAnswers++;
		if (m_iAnswers == 2)
			Finish();
	}
}

//------------------------------------------------------------------------------------------------
class MRX_Test_ShopSell : MRX_ShopScenarioTest
{
	protected int m_iRifle;

	//------------------------------------------------------------------------------------------------
	override protected void DefineSteps()
	{
		m_iRifle = AddSaleItem(1, MRX_ShopTests.PREFAB_RIFLE);
		int ammo = AddSaleItem(1, MRX_ShopTests.PREFAB_AMMO);
		int radio = AddSaleItem(1, MRX_ShopTests.PREFAB_RADIO);
		int unknown = AddSaleItem(1, MRX_ShopTests.PREFAB_UNKNOWN);
		int full = AddSaleItem(1, MRX_ShopTests.PREFAB_RIFLE, false);
		int foreign = AddSaleItem(2, MRX_ShopTests.PREFAB_RIFLE);

		Sell(1, m_iRifle);
		Balance(1, 150);
		Sell(1, m_iRifle).Expect(MRX_EShopStatus.NOT_IN_INVENTORY);
		Sell(1, ammo);
		Balance(1, 153);
		Sell(1, radio).Expect(MRX_EShopStatus.NOT_BUYABLE);
		Sell(1, unknown).Expect(MRX_EShopStatus.NOT_BUYABLE);
		Sell(1, full).Expect(MRX_EShopStatus.NOT_EMPTY);
		Sell(1, foreign).Expect(MRX_EShopStatus.NOT_IN_INVENTORY);
		Balance(1, 153);
	}

	//------------------------------------------------------------------------------------------------
	override protected void CheckEnd()
	{
		Check(m_aSaleItems[m_iRifle].m_bRemoved, "sold rifle removed");
		CheckInt(m_iSaleEvents, 2, "sale events");
	}
}

//------------------------------------------------------------------------------------------------
class MRX_Test_ShopSellDisabled : MRX_ShopScenarioTest
{
	//------------------------------------------------------------------------------------------------
	override protected void DefineSteps()
	{
		m_ShopDef.m_bAllowSell = false;
		Sell(1, AddSaleItem(1, MRX_ShopTests.PREFAB_RIFLE)).Expect(MRX_EShopStatus.SELL_DISABLED);
		Balance(1, 100);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_Test_ShopValidator : MRX_ShopScenarioTest
{
	protected ref MRX_TestRejectItemValidator m_Validator;

	//------------------------------------------------------------------------------------------------
	override protected void DefineSteps()
	{
		m_Validator = new MRX_TestRejectItemValidator();
		m_Validator.m_sRejectedItemId = "rifle";
		m_Shop.AddValidator(m_Validator);

		Buy(1, "rifle").Expect(MRX_EShopStatus.REJECTED);
		Buy(1, "ammo");
		Sell(1, AddSaleItem(1, MRX_ShopTests.PREFAB_RIFLE)).Expect(MRX_EShopStatus.REJECTED);
		Balance(1, 90);
	}
}
//------------------------------------------------------------------------------------------------
class MRX_Test_ShopSellPriceWithContents : MRX_TestCase
{
	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		MRX_ShopCatalog catalog = MRX_ShopTests.CreateCatalog();
		catalog.m_aItems.Insert(MRX_ShopItem.Create("gem", MRX_ShopTests.PREFAB_GEM, 1000, "gold"));
		MRX_ShopDefinition shop = MRX_ShopDefinition.Create("s", catalog, 50);
		int unpaid;

		array<ResourceName> contents = { MRX_ShopTests.PREFAB_AMMO, MRX_ShopTests.PREFAB_AMMO, MRX_ShopTests.PREFAB_UNKNOWN };
		CheckInt(MRX_ShopService.GetSellPriceWithContents(shop, MRX_ShopTests.PREFAB_RIFLE, contents, unpaid), 56, "rifle 50 + ammo 3 + ammo 3, unknown 0");
		CheckInt(unpaid, 1, "unknown content unpaid");

		contents = { MRX_ShopTests.PREFAB_RADIO, MRX_ShopTests.PREFAB_GEM, MRX_ShopTests.PREFAB_MAP };
		CheckInt(MRX_ShopService.GetSellPriceWithContents(shop, MRX_ShopTests.PREFAB_RIFLE, contents, unpaid), 50, "not bought back, other currency and not for sale count 0");
		CheckInt(unpaid, 3, "all three unpaid");

		contents = { MRX_ShopTests.PREFAB_AMMO };
		CheckInt(MRX_ShopService.GetSellPriceWithContents(shop, MRX_ShopTests.PREFAB_UNKNOWN, contents, unpaid), 0, "unknown item is not bought, whatever it holds");
		CheckInt(MRX_ShopService.GetSellPriceWithContents(shop, MRX_ShopTests.PREFAB_RADIO, contents, unpaid), 0, "item not bought back is not bought, whatever it holds");

		ResourceName crate = "Prefabs/Test/Crate.et";
		catalog.m_aItems.Insert(MRX_ShopItem.Create("crate", crate, int.MAX, "cash", int.MAX));
		contents = { crate };
		CheckInt(MRX_ShopService.GetSellPriceWithContents(shop, crate, contents, unpaid), int.MAX, "sum capped at int.MAX");
		Finish();
	}
}

//------------------------------------------------------------------------------------------------
class MRX_Test_ShopBuyTarget : MRX_ShopScenarioTest
{
	protected ref MRX_TestSaleItem m_Target = new MRX_TestSaleItem();

	//------------------------------------------------------------------------------------------------
	override protected void DefineSteps()
	{
		Buy(1, "ammo").To(m_Target);
		Buy(1, "ammo");
		Balance(1, 80);
		Buy(2, "rifle").To(m_Target).Expect(MRX_EShopStatus.NO_SPACE);
		Balance(2, 100);
	}

	//------------------------------------------------------------------------------------------------
	override protected void NextStep()
	{
		// The target has no room from the fourth step on.
		m_Inventory.m_bTargetFull = m_iStep >= 2;
		super.NextStep();
	}

	//------------------------------------------------------------------------------------------------
	override protected void CheckEnd()
	{
		CheckInt(m_Inventory.m_aGivenTargets.Count(), 2, "deliveries");
		if (m_Inventory.m_aGivenTargets.Count() == 2)
		{
			Check(m_Inventory.m_aGivenTargets[0] == m_Target, "first purchase delivered to its target");
			Check(m_Inventory.m_aGivenTargets[1] == null, "second purchase delivered anywhere");
		}
	}
}

//------------------------------------------------------------------------------------------------
class MRX_Test_ShopSellContents : MRX_ShopScenarioTest
{
	protected int m_iLoaded;

	//------------------------------------------------------------------------------------------------
	override protected void DefineSteps()
	{
		m_iLoaded = AddSaleItem(1, MRX_ShopTests.PREFAB_RIFLE, true, { MRX_ShopTests.PREFAB_AMMO, MRX_ShopTests.PREFAB_AMMO, MRX_ShopTests.PREFAB_UNKNOWN, MRX_ShopTests.PREFAB_RADIO });
		int full = AddSaleItem(1, MRX_ShopTests.PREFAB_RIFLE, true, { MRX_ShopTests.PREFAB_AMMO });
		int unknownRoot = AddSaleItem(1, MRX_ShopTests.PREFAB_UNKNOWN, true, { MRX_ShopTests.PREFAB_AMMO });
		int radioRoot = AddSaleItem(1, MRX_ShopTests.PREFAB_RADIO, true, { MRX_ShopTests.PREFAB_AMMO });
		int mission = AddSaleItem(1, MRX_ShopTests.PREFAB_RIFLE);
		m_aSaleItems[mission].m_bRefundable = false;
		int single = AddSaleItem(1, MRX_ShopTests.PREFAB_AMMO);

		Sell(1, m_iLoaded).WithContents().ExpectPrice(56).ExpectCounts(5, 2);
		Balance(1, 156);
		Sell(1, full).Expect(MRX_EShopStatus.NOT_EMPTY);
		Sell(1, unknownRoot).WithContents().Expect(MRX_EShopStatus.NOT_BUYABLE);
		Sell(1, radioRoot).WithContents().Expect(MRX_EShopStatus.NOT_BUYABLE);
		Sell(1, mission).WithContents().Expect(MRX_EShopStatus.NOT_BUYABLE);
		Sell(1, single).WithContents().ExpectPrice(3).ExpectCounts(1, 0);
		Balance(1, 159);
	}

	//------------------------------------------------------------------------------------------------
	override protected void CheckEnd()
	{
		Check(m_aSaleItems[m_iLoaded].m_bRemoved, "sold item removed with its contents");
		CheckInt(m_iSaleEvents, 2, "sale events");
	}
}

//------------------------------------------------------------------------------------------------
//! The payment of a sale fails (currency unknown to the economy): the item is given back with its contents.
class MRX_Test_ShopSellGiveBack : MRX_ShopScenarioTest
{
	protected int m_iGem;

	//------------------------------------------------------------------------------------------------
	override protected void DefineSteps()
	{
		m_ShopDef.m_Catalog.m_aItems.Insert(MRX_ShopItem.Create("gem", MRX_ShopTests.PREFAB_GEM, 1000, "gold"));
		m_iGem = AddSaleItem(1, MRX_ShopTests.PREFAB_GEM, true, { MRX_ShopTests.PREFAB_AMMO });
		Sell(1, m_iGem).WithContents().Expect(MRX_EShopStatus.PAYMENT_FAILED).ExpectTx(MRX_ETxStatus.UNKNOWN_CURRENCY);
		Balance(1, 100);
	}

	//------------------------------------------------------------------------------------------------
	override protected void CheckEnd()
	{
		CheckInt(m_Inventory.m_aGivenBack.Count(), 1, "items given back");
		if (!m_Inventory.m_aGivenBack.IsEmpty())
			Check(m_Inventory.m_aGivenBack[0] == "1:" + MRX_ShopTests.PREFAB_GEM + ":1", "given back with its contents: " + m_Inventory.m_aGivenBack[0]);

		Check(!m_aSaleItems[m_iGem].m_bRemoved, "item back in the inventory");
		CheckInt(m_iSaleEvents, 0, "no sale event");
	}
}

//------------------------------------------------------------------------------------------------
//! Issued contents (listed without prefab by the inventory) are worth nothing but go with the item.
class MRX_Test_ShopSellIssuedContents : MRX_ShopScenarioTest
{
	//------------------------------------------------------------------------------------------------
	override protected void DefineSteps()
	{
		int rifle = AddSaleItem(1, MRX_ShopTests.PREFAB_RIFLE, true, { MRX_ShopTests.PREFAB_AMMO, ResourceName.Empty });
		Sell(1, rifle).WithContents().ExpectPrice(53).ExpectCounts(3, 1);
		Balance(1, 153);
	}
}

//------------------------------------------------------------------------------------------------
//! Product with levels up to a maximum. LIMIT_REACHED at the maximum; a failing delivery also answers LIMIT_REACHED.
class MRX_TestShopProduct : MRX_ShopProduct
{
	int m_iLevel;
	int m_iMax = 2;
	bool m_bFailDelivery;
	int m_iDelivered;

	//------------------------------------------------------------------------------------------------
	override void Check(int playerId, string ownerId, notnull MRX_ShopItem item, notnull MRX_ShopProductCallback callback)
	{
		if (m_iLevel >= m_iMax)
			callback.OnResult(MRX_EShopStatus.LIMIT_REACHED);
		else
			callback.OnResult(MRX_EShopStatus.OK);
	}

	//------------------------------------------------------------------------------------------------
	override void Deliver(int playerId, string ownerId, notnull MRX_ShopItem item, notnull MRX_ShopProductCallback callback)
	{
		if (m_bFailDelivery || m_iLevel >= m_iMax)
		{
			callback.OnResult(MRX_EShopStatus.LIMIT_REACHED);
			return;
		}

		m_iLevel++;
		m_iDelivered++;
		callback.OnResult(MRX_EShopStatus.OK);
	}

	//------------------------------------------------------------------------------------------------
	override void GetState(int playerId, string ownerId, notnull MRX_ShopItem item, notnull MRX_ShopProductStateCallback callback)
	{
		callback.OnResult(MRX_ShopProductState.Create(m_iLevel < m_iMax, string.Format("%1 / %2", m_iLevel, m_iMax)));
	}
}

//------------------------------------------------------------------------------------------------
class MRX_ShopProductTests
{
	//------------------------------------------------------------------------------------------------
	//! Adds an "upgrade" product (price 30, no prefab) to the catalog.
	static MRX_TestShopProduct AddProduct(notnull MRX_ShopCatalog catalog)
	{
		MRX_TestShopProduct product = new MRX_TestShopProduct();
		MRX_ShopItem item = MRX_ShopItem.Create("upgrade", ResourceName.Empty, 30);
		item.m_Product = product;
		catalog.m_aItems.Insert(item);
		return product;
	}
}

//------------------------------------------------------------------------------------------------
//! Paid, delivered, refused at the maximum before any payment; never bought back; valid without a prefab.
class MRX_Test_ShopProduct : MRX_ShopScenarioTest
{
	protected MRX_TestShopProduct m_Product;

	//------------------------------------------------------------------------------------------------
	override protected void DefineSteps()
	{
		m_Product = MRX_ShopProductTests.AddProduct(m_ShopDef.m_Catalog);
		Buy(1, "upgrade").ExpectPrice(30);
		Balance(1, 70);
		Buy(1, "upgrade");
		Buy(1, "upgrade").Expect(MRX_EShopStatus.LIMIT_REACHED);
		Balance(1, 40);
	}

	//------------------------------------------------------------------------------------------------
	override protected void CheckEnd()
	{
		CheckInt(m_Product.m_iDelivered, 2, "deliveries");
		CheckInt(m_iPurchaseEvents, 2, "purchase events");
		CheckInt(m_Inventory.m_aGiven.Count(), 0, "no inventory item given");
		Check(m_ShopDef.m_Catalog.FindByPrefab(ResourceName.Empty) == null, "products are not found by prefab");
		CheckInt(m_ShopDef.m_Catalog.Validate(null, "test"), 5, "product without prefab is valid");
	}
}

//------------------------------------------------------------------------------------------------
//! A failed delivery refunds the payment and reports the status of the product.
class MRX_Test_ShopProductRefund : MRX_ShopScenarioTest
{
	//------------------------------------------------------------------------------------------------
	override protected void DefineSteps()
	{
		MRX_ShopProductTests.AddProduct(m_ShopDef.m_Catalog).m_bFailDelivery = true;
		Buy(1, "upgrade").Expect(MRX_EShopStatus.LIMIT_REACHED);
		Balance(1, 100);
	}

	//------------------------------------------------------------------------------------------------
	override protected void CheckEnd()
	{
		CheckInt(m_iPurchaseEvents, 0, "purchase events");
	}
}

//------------------------------------------------------------------------------------------------
//! The shop window's product states: one per product, in catalog order; none without an owner.
class MRX_Test_ShopProductStates : MRX_TestCase
{
	protected ref MRX_ShopService m_Shop;
	protected ref MRX_ShopDefinition m_ShopDef;
	protected ref MRX_TestShopStatesReply m_Reply;
	protected ref MRX_TestShopStatesReply m_NoOwnerReply;

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		MRX_TestIdentityService identity = new MRX_TestIdentityService();
		identity.m_mFakeIds.Set(1, "states-a");
		identity.SimulateAudit(1);
		m_Shop = new MRX_ShopService(MRX_TestUtils.CreateService(), identity, new MRX_TestShopInventory());
		m_ShopDef = MRX_ShopDefinition.Create("states", MRX_ShopTests.CreateCatalog());
		MRX_ShopProductTests.AddProduct(m_ShopDef.m_Catalog).m_iLevel = 2;

		m_NoOwnerReply = new MRX_TestShopStatesReply();
		m_Shop.GetProductStates(9, m_ShopDef, m_NoOwnerReply);
		m_Reply = new MRX_TestShopStatesReply();
		m_Reply.m_Test = this;
		m_Shop.GetProductStates(1, m_ShopDef, m_Reply);
	}

	//------------------------------------------------------------------------------------------------
	void OnStates(MRX_TestShopStatesReply reply)
	{
		Check(m_NoOwnerReply.m_bAnswered && m_NoOwnerReply.m_aItemIds.IsEmpty(), "no states without an owner");
		CheckInt(reply.m_aItemIds.Count(), 1, "one product state");
		if (reply.m_aItemIds.Count() == 1)
		{
			CheckString(reply.m_aItemIds[0], "upgrade", "product ID");
			Check(!reply.m_aAvailable[0], "product at its maximum is not available");
			CheckString(reply.m_aTexts[0], "2 / 2", "product state text");
		}

		Finish();
	}
}

//------------------------------------------------------------------------------------------------
class MRX_TestShopStatesReply : MRX_ShopStatesCallback
{
	MRX_Test_ShopProductStates m_Test;
	bool m_bAnswered;
	ref array<string> m_aItemIds = {};
	ref array<bool> m_aAvailable = {};
	ref array<string> m_aTexts = {};

	//------------------------------------------------------------------------------------------------
	override void OnResult(array<string> itemIds, array<bool> available, array<string> texts)
	{
		m_bAnswered = true;
		m_aItemIds.Copy(itemIds);
		m_aAvailable.Copy(available);
		m_aTexts.Copy(texts);
		if (m_Test)
			m_Test.OnStates(this);
	}
}
#endif
