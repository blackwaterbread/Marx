#ifdef WORKBENCH
// Tests of an arsenal trade on the local (host) player. Server: cart prices with default parts, unpaid items kept in the
// cart (inventory, hands, attachments taken off, the player's own items kept out), removal, purchase checkout, the sale
// box (only its owner takes items out) and its checkout, closing, items put into a backpack in the cart. Client: the
// trade panel in the inventory's middle, an inspection taking the middle, an item storage (middle mouse, double click)
// below the cart, the sale tab with the buy-back list and sale prices, X on an item of the sale box, closing with the
// inventory.

//------------------------------------------------------------------------------------------------
class MRX_ArsenalTradeTests
{
	//------------------------------------------------------------------------------------------------
	static void Register(notnull MRX_TestRunner runner)
	{
		runner.Add(new MRX_Test_ArsenalTradeFlow());
		runner.Add(new MRX_Test_ArsenalTradeNested());
		runner.Add(new MRX_Test_ArsenalTradePanel());
	}

	//------------------------------------------------------------------------------------------------
	//! Skip reason when the trade cannot run in this session, or empty.
	static string GetSkipReason(string ownerId)
	{
		Resource container = Resource.Load(MRX_ArsenalTradeSessionManager.CONTAINER_PREFAB);
		if (ownerId.IsEmpty() || !MRX_Shop.GetService() || !MRX_ArsenalTrades.Get() || !container || !container.IsValid())
			return "no owner, shop service, trade manager or trade container prefab";

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsPrefab(notnull IEntity item, ResourceName prefab)
	{
		return MRX_ShopCatalog.GetPrefabKey(SCR_ResourceNameUtils.GetPrefabName(item)) == MRX_ShopCatalog.GetPrefabKey(prefab);
	}

	//------------------------------------------------------------------------------------------------
	static bool IsInside(notnull IEntity item, IEntity ancestor)
	{
		IEntity parent = item.GetParent();
		while (parent)
		{
			if (parent == ancestor)
				return true;

			parent = parent.GetParent();
		}

		return false;
	}
}

//------------------------------------------------------------------------------------------------
class MRX_Test_ArsenalTradeFlow : MRX_TestCase
{
	protected static const int STEP_MS = 500;
	protected static const int FUNDS = 5000;

	protected ref MRX_TestPossession m_Possession;
	protected IEntity m_Character;
	protected IEntity m_Arsenal;
	protected ref MRX_ShopDefinition m_Shop;
	protected string m_sOwnerId;
	protected int m_iPlayerId;
	protected int m_iBalance;
	//! Weak: the trade manager owns the session.
	protected MRX_ArsenalTradeSession m_Session;
	protected IEntity m_Container;
	protected MRX_ArsenalTradeStorageComponent m_Cart;
	protected MRX_ArsenalTradeStorageComponent m_Sale;
	protected IEntity m_CartRifle;
	protected IEntity m_CartBandage;
	protected IEntity m_DefaultPart;
	protected bool m_bHadPart;
	//! The player's own bandage, which must not go into the cart.
	protected IEntity m_OwnItem;
	//! Rifle tried into the hands (weapon slots): false when they were full, so the check is skipped.
	protected bool m_bTriedHands;
	//! Another character that tries to take an item out of the sale box.
	protected IEntity m_Other;
	protected IEntity m_SoldItem;
	protected ref MRX_ShopCallback m_Callback;
	protected ref MRX_TxCallback m_TxCallback;
	protected ref MRX_BalanceCallback m_BalanceCallback;

	//------------------------------------------------------------------------------------------------
	override int GetTimeoutMs()
	{
		return 30000;
	}

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		m_Possession = MRX_TestPossession.Acquire();
		if (!m_Possession || !m_Possession.GetCharacter())
		{
			Skip("no local character");
			return;
		}

		m_Character = m_Possession.GetCharacter();
		m_iPlayerId = m_Possession.GetController().GetPlayerId();
		MRX_IdentityService identity = MRX_Marx.GetIdentity();
		if (identity)
			m_sOwnerId = identity.GetOwnerId(m_iPlayerId);

		string skipReason = MRX_ArsenalTradeTests.GetSkipReason(m_sOwnerId);
		if (!skipReason.IsEmpty())
		{
			Skip(skipReason);
			End(false);
			return;
		}

		m_Arsenal = MRX_TestWorldUtils.SpawnPrefab(MRX_Test_ArsenalShopFlow.ARSENAL_PREFAB, m_Character.GetOrigin() + "1.5 0 0");
		MRX_ShopComponent shopComponent;
		if (m_Arsenal)
			shopComponent = MRX_ShopComponent.Cast(m_Arsenal.FindComponent(MRX_ShopComponent));

		MRX_ArsenalShopComponent arsenal = MRX_ArsenalShopComponent.Find(m_Arsenal);
		if (!shopComponent || !arsenal || !arsenal.UsesCart())
		{
			Check(false, "arsenal prefab spawns with a shop and a cart");
			End(true);
			return;
		}

		MRX_ShopCatalog catalog = new MRX_ShopCatalog();
		catalog.m_aItems = {};
		catalog.m_aItems.Insert(MRX_ShopItem.Create("rifle", MRX_Test_ArsenalShopFlow.RIFLE, 1000, MRX_Settings.DEFAULT_CURRENCY));
		catalog.m_aItems.Insert(MRX_ShopItem.Create("scope", MRX_Test_ArsenalShopFlow.OPTIC, 400, MRX_Settings.DEFAULT_CURRENCY));
		catalog.m_aItems.Insert(MRX_ShopItem.Create("bandage", MRX_Test_ArsenalShopFlow.BANDAGE, 10, MRX_Settings.DEFAULT_CURRENCY));
		m_Shop = MRX_ShopDefinition.Create("test_trade", catalog, 50);
		shopComponent.SetDefinition(m_Shop);

		// The player's own item, which must stay out of the cart, and another character, which must not take items out of
		// the sale box.
		GetCharacterManager().TrySpawnPrefabToStorage(MRX_Test_ArsenalShopFlow.BANDAGE);
		m_Other = MRX_TestWorldUtils.SpawnPrefab(MRX_TestWorldUtils.CHARACTER_PREFAB, m_Character.GetOrigin() + "0 0 1.5");

		m_TxCallback = new MRX_TxCallback();
		m_TxCallback.GetOnResult().Insert(OnFunded);
		MRX_Marx.GetEconomy().Credit(m_sOwnerId, MRX_Settings.DEFAULT_CURRENCY, FUNDS, MRX_TxContext.Create("test", "arsenal trade", MRX_NetworkTests.UniqueKey("trade")), m_TxCallback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnFunded(MRX_TxResult result)
	{
		CheckStatus(result.m_eStatus, MRX_ETxStatus.OK, "fund the buyer");
		m_BalanceCallback = new MRX_BalanceCallback();
		m_BalanceCallback.GetOnResult().Insert(OnBalanceBefore);
		MRX_Marx.GetEconomy().GetBalance(m_sOwnerId, MRX_Settings.DEFAULT_CURRENCY, m_BalanceCallback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBalanceBefore(MRX_ETxStatus status, int balance)
	{
		m_iBalance = balance;
		// No client request: the local client ignores the answers (an earlier test may have left its own request 1).
		CheckShopStatus(MRX_ArsenalTrades.Get().Open(m_iPlayerId, m_Arsenal, -1), MRX_EShopStatus.OK, "open the trade");
		m_Session = MRX_ArsenalTrades.Get().Find(m_iPlayerId);
		if (m_Session)
			m_Container = m_Session.GetContainer();

		m_Cart = MRX_ArsenalTradeStorageComponent.Find(m_Container, MRX_EArsenalTradeRole.CART);
		m_Sale = MRX_ArsenalTradeStorageComponent.Find(m_Container, MRX_EArsenalTradeRole.SALE);
		if (!m_Session || !m_Cart || !m_Sale)
		{
			Check(false, "trade container with a cart and a sale box");
			End(true);
			return;
		}

		CheckShopStatus(m_Session.Add("rifle", null, -1), MRX_EShopStatus.OK, "put a rifle into the cart");
		CheckShopStatus(m_Session.Add("bandage", null, -1), MRX_EShopStatus.OK, "put a bandage into the cart");
		CheckShopStatus(m_Session.Add("not_listed", null, -1), MRX_EShopStatus.UNKNOWN_ITEM, "only listed items go into the cart");
		GetGame().GetCallqueue().CallLater(CheckCart, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	//! The rifle's parts are in its price. The player tries to take unpaid items (into the inventory, into the hands, an
	//! attachment off the rifle) and to put an own item into the cart.
	protected void CheckCart()
	{
		if (IsTradeLost())
			return;

		array<IEntity> roots = {};
		CheckInt(m_Cart.GetRootItems(roots), 2, "two items in the cart");
		foreach (IEntity root : roots)
		{
			if (MRX_ArsenalTradeTests.IsPrefab(root, MRX_Test_ArsenalShopFlow.RIFLE))
				m_CartRifle = root;
			else if (MRX_ArsenalTradeTests.IsPrefab(root, MRX_Test_ArsenalShopFlow.BANDAGE))
				m_CartBandage = root;
		}

		if (!m_CartRifle || !m_CartBandage)
		{
			Check(false, "rifle and bandage in the cart");
			End(true);
			return;
		}

		CheckInt(GetCartTotal(), 1010, "cart total without the rifle's own parts");

		WeaponAttachmentsStorageComponent attachments = WeaponAttachmentsStorageComponent.Cast(m_CartRifle.FindComponent(WeaponAttachmentsStorageComponent));
		array<IEntity> parts = {};
		if (attachments)
			attachments.GetAll(parts, true);

		if (!parts.IsEmpty())
		{
			m_DefaultPart = parts[0];
			m_bHadPart = true;
		}
		else
		{
			Print("[MRX_TEST]   the rifle came without parts; default part checks skipped");
		}

		InventoryStorageManagerComponent manager = GetCharacterManager();
		TryTake(manager, m_CartBandage, EStoragePurpose.PURPOSE_ANY);
		m_bTriedHands = TryTake(manager, m_CartRifle, EStoragePurpose.PURPOSE_WEAPON_PROXY);
		if (!m_bTriedHands)
			Print("[MRX_TEST]   no free weapon slot; the hands check is skipped");

		if (m_DefaultPart)
			TryTake(manager, m_DefaultPart, EStoragePurpose.PURPOSE_DEPOSIT);

		m_OwnItem = FindCarried(MRX_Test_ArsenalShopFlow.BANDAGE);
		Check(m_OwnItem != null, "own bandage carried");
		if (m_OwnItem)
			manager.TryMoveItemToStorage(m_OwnItem, m_Cart);

		GetGame().GetCallqueue().CallLater(CheckBlocked, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	//! The trade closed under the test (e.g. the player no longer controls the character): the test ends.
	protected bool IsTradeLost()
	{
		if (m_Session && !m_Session.IsClosing() && m_Container)
			return false;

		Check(false, "trade still open");
		End(true);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asks the character's manager to move an item where it would fit. \return False when nothing would take it.
	protected bool TryTake(notnull InventoryStorageManagerComponent manager, notnull IEntity item, EStoragePurpose purpose)
	{
		BaseInventoryStorageComponent target = manager.FindStorageForItem(item, purpose);
		if (!target)
			return false;

		manager.TryMoveItemToStorage(item, target);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckBlocked()
	{
		if (IsTradeLost())
			return;

		Check(MRX_ArsenalTradeStorageComponent.IsInCart(m_CartBandage), "the player cannot take an unpaid item out of the cart");
		if (m_bTriedHands)
			Check(m_CartRifle && MRX_ArsenalTradeStorageComponent.IsInCart(m_CartRifle), "the player cannot take an unpaid weapon into the hands");

		if (m_DefaultPart)
			Check(MRX_ArsenalTradeTests.IsInside(m_DefaultPart, m_CartRifle), "the player cannot take an attachment off an unpaid weapon");

		if (m_OwnItem)
			Check(m_OwnItem.GetRootParent() == m_Character && !MRX_ArsenalTradeStorageComponent.IsInCart(m_OwnItem), "the player's own item stays out of the cart");

		if (m_DefaultPart)
			CheckShopStatus(m_Session.MoveInCart(m_DefaultPart, null, -1), MRX_EShopStatus.OK, "move a default part to the cells");

		GetGame().GetCallqueue().CallLater(CheckKept, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckKept()
	{
		if (IsTradeLost())
			return;

		if (m_DefaultPart)
		{
			Check(m_DefaultPart.GetParent() == m_Container, "default part lies in the cells");
			CheckInt(GetCartTotal(), 1010, "default part in the cells costs nothing");
		}

		CheckShopStatus(m_Session.RemoveFromCart(m_CartRifle), MRX_EShopStatus.OK, "take the rifle out of the cart");
		GetGame().GetCallqueue().CallLater(CheckRemoved, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckRemoved()
	{
		if (IsTradeLost())
			return;

		array<IEntity> roots = {};
		CheckInt(m_Cart.GetRootItems(roots), 1, "only the bandage left");
		if (m_bHadPart)
			Check(!m_DefaultPart || !MRX_ArsenalTradeStorageComponent.IsInCart(m_DefaultPart), "the rifle's default part in the cells goes with it");

		CheckInt(GetCartTotal(), 10, "cart total without the rifle");
		CheckShopStatus(m_Session.Add("scope", null, -1), MRX_EShopStatus.OK, "put a scope into the cart");
		GetGame().GetCallqueue().CallLater(BuyCart, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void BuyCart()
	{
		if (IsTradeLost())
			return;

		CheckInt(GetCartTotal(), 410, "cart total of bandage and scope");
		m_Callback = new MRX_ShopCallback();
		m_Callback.GetOnResult().Insert(OnBought);
		MRX_Shop.GetService().CheckoutBuy(m_iPlayerId, m_Shop, m_Session, m_Callback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBought(MRX_ShopResult result)
	{
		MRX_ShopCheckoutResult checkout = MRX_ShopCheckoutResult.Cast(result);
		CheckShopStatus(result.m_eStatus, MRX_EShopStatus.OK, "checkout of the cart");
		if (checkout)
		{
			CheckInt(checkout.m_iDoneCount, 2, "both items delivered");
			CheckInt(checkout.m_iLeftCount, 0, "nothing left in the cart");
			CheckInt(checkout.m_iPrice, 410, "paid once for the cart");
		}

		GetGame().GetCallqueue().CallLater(CheckBought, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckBought()
	{
		array<IEntity> roots = {};
		CheckInt(m_Cart.GetRootItems(roots), 0, "cart empty after the checkout");
		Check(FindCarried(MRX_Test_ArsenalShopFlow.OPTIC) != null, "bought scope carried");
		m_BalanceCallback = new MRX_BalanceCallback();
		m_BalanceCallback.GetOnResult().Insert(OnBalanceAfterBuy);
		MRX_Marx.GetEconomy().GetBalance(m_sOwnerId, MRX_Settings.DEFAULT_CURRENCY, m_BalanceCallback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBalanceAfterBuy(MRX_ETxStatus status, int balance)
	{
		if (IsTradeLost())
			return;

		CheckInt(m_iBalance - balance, 410, "balance less the cart");
		m_iBalance = balance;
		m_SoldItem = FindCarried(MRX_Test_ArsenalShopFlow.BANDAGE);
		InventoryStorageManagerComponent manager = GetCharacterManager();
		if (!m_SoldItem || !manager.TryMoveItemToStorage(m_SoldItem, m_Sale))
		{
			Check(false, "bandage goes into the sale box");
			End(true);
			return;
		}

		GetGame().GetCallqueue().CallLater(TakeByOther, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	//! Another character tries to take the item out of the sale box.
	protected void TakeByOther()
	{
		if (IsTradeLost())
			return;

		Check(m_Sale.Contains(m_SoldItem), "own item in the sale box");
		InventoryStorageManagerComponent otherManager;
		if (m_Other)
			otherManager = InventoryStorageManagerComponent.Cast(m_Other.FindComponent(SCR_InventoryStorageManagerComponent));

		BaseInventoryStorageComponent target;
		if (otherManager)
			target = otherManager.FindStorageForItem(m_SoldItem);

		if (target)
			otherManager.TryMoveItemToStorage(m_SoldItem, target);
		else
			Print("[MRX_TEST]   no other character with room; the sale box owner check is skipped");

		GetGame().GetCallqueue().CallLater(SellBox, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void SellBox()
	{
		if (IsTradeLost())
			return;

		Check(m_SoldItem && m_Sale.Contains(m_SoldItem), "another character cannot take an item out of the sale box");
		array<string> currencies = {};
		array<int> totals = {};
		array<string> keys = {};
		array<int> prices = {};
		array<string> saleCurrencies = {};
		m_Session.GetState(currencies, totals, keys, prices, saleCurrencies);
		CheckInt(prices.Count(), 1, "one sale price");
		if (prices.Count() == 1)
			CheckInt(prices[0], 5, "bandage bought back at half");

		m_Callback = new MRX_ShopCallback();
		m_Callback.GetOnResult().Insert(OnSold);
		MRX_Shop.GetService().CheckoutSell(m_iPlayerId, m_Shop, m_Session, m_Callback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSold(MRX_ShopResult result)
	{
		MRX_ShopCheckoutResult checkout = MRX_ShopCheckoutResult.Cast(result);
		CheckShopStatus(result.m_eStatus, MRX_EShopStatus.OK, "checkout of the sale box");
		if (checkout)
		{
			CheckInt(checkout.m_iDoneCount, 1, "one item sold");
			CheckInt(checkout.m_iPrice, 5, "paid for the sale box");
		}

		GetGame().GetCallqueue().CallLater(PutUpScope, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	//! Closing gives the sale box back.
	protected void PutUpScope()
	{
		if (IsTradeLost())
			return;

		array<IEntity> items = {};
		CheckInt(m_Sale.GetRootItems(items), 0, "sale box empty after the checkout");
		m_SoldItem = FindCarried(MRX_Test_ArsenalShopFlow.OPTIC);
		if (!m_SoldItem || !GetCharacterManager().TryMoveItemToStorage(m_SoldItem, m_Sale))
		{
			Check(false, "scope goes into the sale box");
			End(true);
			return;
		}

		GetGame().GetCallqueue().CallLater(CloseTrade, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void CloseTrade()
	{
		if (IsTradeLost())
			return;

		Check(m_Sale.Contains(m_SoldItem), "scope in the sale box");
		MRX_ArsenalTrades.Get().Close(m_iPlayerId, "test");
		GetGame().GetCallqueue().CallLater(CheckClosed, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckClosed()
	{
		Check(MRX_ArsenalTrades.Get().Find(m_iPlayerId) == null, "trade closed");
		Check(!m_Container, "trade container deleted");
		Check(m_SoldItem && m_SoldItem.GetRootParent() == m_Character, "the sale box goes back to the player");
		End(true);
	}

	//------------------------------------------------------------------------------------------------
	protected void End(bool finish)
	{
		if (MRX_ArsenalTrades.Get())
			MRX_ArsenalTrades.Get().Close(m_iPlayerId, "test end");

		if (m_SoldItem && m_SoldItem.GetRootParent() == m_Character)
			SCR_EntityHelper.DeleteEntityAndChildren(m_SoldItem);

		if (m_OwnItem && m_OwnItem.GetRootParent() == m_Character)
			SCR_EntityHelper.DeleteEntityAndChildren(m_OwnItem);

		if (m_Other)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Other);

		if (m_Arsenal)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Arsenal);

		if (m_Possession)
			m_Possession.Release();

		if (finish)
			Finish();
	}

	//------------------------------------------------------------------------------------------------
	protected int GetCartTotal()
	{
		array<string> currencies = {};
		array<int> totals = {};
		array<string> keys = {};
		array<int> prices = {};
		array<string> saleCurrencies = {};
		m_Session.GetState(currencies, totals, keys, prices, saleCurrencies);
		int index = currencies.Find(MRX_Settings.DEFAULT_CURRENCY);
		if (index < 0)
			return 0;

		return totals[index];
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckShopStatus(MRX_EShopStatus actual, MRX_EShopStatus expected, string message)
	{
		Check(actual == expected, string.Format("%1: expected %2, got %3", message, typename.EnumToString(MRX_EShopStatus, expected), typename.EnumToString(MRX_EShopStatus, actual)));
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity FindCarried(ResourceName prefab)
	{
		array<IEntity> items = {};
		m_Possession.GetItems(items);
		foreach (IEntity item : items)
		{
			if (MRX_ArsenalTradeTests.IsPrefab(item, prefab))
				return item;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected InventoryStorageManagerComponent GetCharacterManager()
	{
		return InventoryStorageManagerComponent.Cast(m_Character.FindComponent(SCR_InventoryStorageManagerComponent));
	}
}

//------------------------------------------------------------------------------------------------
//! An arsenal item put into a backpack in the cart: a storage without fixed slots places it itself.
class MRX_Test_ArsenalTradeNested : MRX_TestCase
{
	protected static const int STEP_MS = 500;
	protected static const ResourceName BACKPACK = "{06B68C58B72EAAC6}Prefabs/Items/Equipment/Backpacks/Backpack_ALICE_Medium.et";

	protected ref MRX_TestPossession m_Possession;
	protected IEntity m_Arsenal;
	protected int m_iPlayerId;
	//! Weak: the trade manager owns the session.
	protected MRX_ArsenalTradeSession m_Session;
	protected MRX_ArsenalTradeStorageComponent m_Cart;
	protected IEntity m_Backpack;
	protected BaseInventoryStorageComponent m_BackpackStorage;

	//------------------------------------------------------------------------------------------------
	override int GetTimeoutMs()
	{
		return 15000;
	}

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		m_Possession = MRX_TestPossession.Acquire();
		if (!m_Possession || !m_Possession.GetCharacter())
		{
			Skip("no local character");
			return;
		}

		IEntity character = m_Possession.GetCharacter();
		m_iPlayerId = m_Possession.GetController().GetPlayerId();
		string ownerId;
		MRX_IdentityService identity = MRX_Marx.GetIdentity();
		if (identity)
			ownerId = identity.GetOwnerId(m_iPlayerId);

		string skipReason = MRX_ArsenalTradeTests.GetSkipReason(ownerId);
		if (!skipReason.IsEmpty())
		{
			Skip(skipReason);
			End(false);
			return;
		}

		m_Arsenal = MRX_TestWorldUtils.SpawnPrefab(MRX_Test_ArsenalShopFlow.ARSENAL_PREFAB, character.GetOrigin() + "1.5 0 0");
		MRX_ShopComponent shopComponent;
		if (m_Arsenal)
			shopComponent = MRX_ShopComponent.Cast(m_Arsenal.FindComponent(MRX_ShopComponent));

		if (!shopComponent)
		{
			Check(false, "arsenal prefab spawns with a shop");
			End(true);
			return;
		}

		MRX_ShopCatalog catalog = new MRX_ShopCatalog();
		catalog.m_aItems = {};
		catalog.m_aItems.Insert(MRX_ShopItem.Create("backpack", BACKPACK, 300, MRX_Settings.DEFAULT_CURRENCY));
		catalog.m_aItems.Insert(MRX_ShopItem.Create("bandage", MRX_Test_ArsenalShopFlow.BANDAGE, 10, MRX_Settings.DEFAULT_CURRENCY));
		shopComponent.SetDefinition(MRX_ShopDefinition.Create("test_trade_nested", catalog, 50));

		Check(MRX_ArsenalTrades.Get().Open(m_iPlayerId, m_Arsenal, -1) == MRX_EShopStatus.OK, "open the trade");
		m_Session = MRX_ArsenalTrades.Get().Find(m_iPlayerId);
		if (m_Session)
			m_Cart = MRX_ArsenalTradeStorageComponent.Find(m_Session.GetContainer(), MRX_EArsenalTradeRole.CART);

		if (!m_Cart)
		{
			Check(false, "trade container with a cart");
			End(true);
			return;
		}

		Check(m_Session.Add("backpack", null, -1) == MRX_EShopStatus.OK, "put a backpack into the cart");
		GetGame().GetCallqueue().CallLater(AddIntoBackpack, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddIntoBackpack()
	{
		if (!m_Session || m_Session.IsClosing() || !m_Cart)
		{
			Check(false, "trade still open");
			End(true);
			return;
		}

		array<IEntity> roots = {};
		m_Cart.GetRootItems(roots);
		if (!roots.IsEmpty())
			m_Backpack = roots[0];

		if (m_Backpack)
			m_BackpackStorage = BaseInventoryStorageComponent.Cast(m_Backpack.FindComponent(SCR_UniversalInventoryStorageComponent));

		if (!m_BackpackStorage)
		{
			Check(false, "backpack with a storage in the cart");
			End(true);
			return;
		}

		MRX_EShopStatus status = m_Session.Add("bandage", m_BackpackStorage, -1);
		Check(status == MRX_EShopStatus.OK, "put a bandage into the backpack in the cart: " + typename.EnumToString(MRX_EShopStatus, status));
		GetGame().GetCallqueue().CallLater(CheckInside, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckInside()
	{
		if (!m_Backpack || !m_Cart)
		{
			Check(false, "backpack still in the cart");
			End(true);
			return;
		}

		array<IEntity> roots = {};
		CheckInt(m_Cart.GetRootItems(roots), 1, "the bandage is not in the cart's cells");
		array<IEntity> inside = {};
		m_BackpackStorage.GetAll(inside);
		bool found;
		foreach (IEntity item : inside)
		{
			if (MRX_ArsenalTradeTests.IsPrefab(item, MRX_Test_ArsenalShopFlow.BANDAGE))
				found = true;
		}

		Check(found, "the bandage is in the backpack");
		End(true);
	}

	//------------------------------------------------------------------------------------------------
	protected void End(bool finish)
	{
		if (MRX_ArsenalTrades.Get())
			MRX_ArsenalTrades.Get().Close(m_iPlayerId, "test end");

		if (m_Arsenal)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Arsenal);

		if (m_Possession)
			m_Possession.Release();

		if (finish)
			Finish();
	}
}

//------------------------------------------------------------------------------------------------
//! The inventory opened at an arsenal with a cart: the trade panel replaces the character, an inspection takes the
//! middle with its attachment points placed, an item storage (middle mouse, double click) shows below the cart, the sale
//! tab lists what the arsenal buys back and shows sale prices, X gives an item of the sale box back where it was, and
//! closing the inventory closes the trade.
class MRX_Test_ArsenalTradePanel : MRX_TestCase
{
	protected static const int STEP_MS = 500;
	protected static const int WAIT_STEPS = 16;
	protected static const int FUNDS = 5000;
	protected static const string RENDER_WIDGET = "playerRender";
	protected static const string PRICE_WIDGET = "MRX_SalePrice";
	protected static const ResourceName BINOCULARS = "{0CF54B9A85D8E0D4}Prefabs/Items/Equipment/Binoculars/Binoculars_M22/Binoculars_M22.et";

	protected ref MRX_TestPossession m_Possession;
	protected IEntity m_Character;
	protected IEntity m_Arsenal;
	protected string m_sOwnerId;
	protected int m_iPlayerId;
	protected int m_iWaits;
	//! Weak: the controller owns the trade.
	protected MRX_ArsenalTradeClient m_Trade;
	protected IEntity m_OwnItem;
	//! The player's own items put up for sale and given back with X: the bandage, a rifle and binoculars.
	protected ref array<IEntity> m_aOwnItems = {};
	protected ref array<IEntity> m_aReturnQueue = {};
	//! Entities the own items were in before the sale box, by index of m_aOwnItems.
	protected ref array<IEntity> m_aOwnParents = {};
	protected ref MRX_TxCallback m_TxCallback;

	//------------------------------------------------------------------------------------------------
	override int GetTimeoutMs()
	{
		return 30000;
	}

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		m_Possession = MRX_TestPossession.Acquire();
		if (!m_Possession || !m_Possession.GetCharacter())
		{
			Skip("no local character");
			return;
		}

		m_Character = m_Possession.GetCharacter();
		m_iPlayerId = m_Possession.GetController().GetPlayerId();
		MRX_IdentityService identity = MRX_Marx.GetIdentity();
		if (identity)
			m_sOwnerId = identity.GetOwnerId(m_iPlayerId);

		string skipReason = MRX_ArsenalTradeTests.GetSkipReason(m_sOwnerId);
		if (!skipReason.IsEmpty())
		{
			Skip(skipReason);
			End(false);
			return;
		}

		m_Arsenal = MRX_TestWorldUtils.SpawnPrefab(MRX_Test_ArsenalShopFlow.ARSENAL_PREFAB, m_Character.GetOrigin() + "1.5 0 0");
		MRX_ShopComponent shopComponent;
		if (m_Arsenal)
			shopComponent = MRX_ShopComponent.Cast(m_Arsenal.FindComponent(MRX_ShopComponent));

		MRX_ArsenalShopComponent arsenal = MRX_ArsenalShopComponent.Find(m_Arsenal);
		if (!shopComponent || !arsenal || !arsenal.UsesCart())
		{
			Check(false, "arsenal prefab spawns with a shop and a cart");
			End(true);
			return;
		}

		// Two items for sale and one the arsenal only buys back.
		MRX_ShopCatalog catalog = new MRX_ShopCatalog();
		catalog.m_aItems = {};
		catalog.m_aItems.Insert(MRX_ShopItem.Create("rifle", MRX_Test_ArsenalShopFlow.RIFLE, 1000, MRX_Settings.DEFAULT_CURRENCY));
		catalog.m_aItems.Insert(MRX_ShopItem.Create("bandage", MRX_Test_ArsenalShopFlow.BANDAGE, 10, MRX_Settings.DEFAULT_CURRENCY));
		catalog.m_aItems.Insert(MRX_ShopItem.Create("scope_buyback", MRX_Test_ArsenalShopFlow.OPTIC, 0, MRX_Settings.DEFAULT_CURRENCY, 30));
		shopComponent.SetDefinition(MRX_ShopDefinition.Create("test_trade_panel", catalog, 50));

		GetCharacterManager().TrySpawnPrefabToStorage(MRX_Test_ArsenalShopFlow.BANDAGE);
		GetCharacterManager().TrySpawnPrefabToStorage(MRX_Test_ArsenalShopFlow.RIFLE);
		GetCharacterManager().TrySpawnPrefabToStorage(BINOCULARS);
		m_TxCallback = new MRX_TxCallback();
		m_TxCallback.GetOnResult().Insert(OnFunded);
		MRX_Marx.GetEconomy().Credit(m_sOwnerId, MRX_Settings.DEFAULT_CURRENCY, FUNDS, MRX_TxContext.Create("test", "arsenal trade panel", MRX_NetworkTests.UniqueKey("tradepanel")), m_TxCallback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnFunded(MRX_TxResult result)
	{
		CheckStatus(result.m_eStatus, MRX_ETxStatus.OK, "fund the buyer");
		SCR_InventoryStorageManagerComponent manager = SCR_InventoryStorageManagerComponent.Cast(GetCharacterManager());
		manager.SetStorageToOpen(m_Arsenal);
		manager.OpenInventory();
		GetGame().GetCallqueue().CallLater(WaitForPanel, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	//! The panel comes once the trade container has replicated.
	protected void WaitForPanel()
	{
		SCR_InventoryMenuUI menu = SCR_InventoryMenuUI.GetInventoryMenu();
		MRX_ArsenalTradePanel panel;
		if (menu)
			panel = menu.MRX_GetTradePanel();

		if (!panel)
		{
			m_iWaits++;
			if (m_iWaits < WAIT_STEPS)
			{
				GetGame().GetCallqueue().CallLater(WaitForPanel, STEP_MS);
				return;
			}

			Check(false, "trade panel shows in the inventory");
			End(true);
			return;
		}

		m_Trade = MRX_ArsenalTradeClient.GetLocal();
		Check(m_Trade && m_Trade.IsReady(), "trade ready on the client");
		Check(MRX_ArsenalTrades.Get().Find(m_iPlayerId) != null, "trade open on the server");
		Check(panel.IsVisible(), "trade panel visible");
		Check(!IsRenderVisible(menu), "character preview hidden for the trade panel");
		MRX_ArsenalTradeStorageUI cartUI = panel.GetStorageUI(false);
		Check(cartUI && m_Trade && cartUI.GetStorage() == m_Trade.GetCartStorage(), "cart panel shows the cart");
		Check(panel.GetShownStorageUI() == cartUI, "the buy tab shows first");
		if (!m_Trade)
		{
			End(true);
			return;
		}

		m_Trade.Add("rifle", null);
		m_Trade.Add("bandage", null);
		GetGame().GetCallqueue().CallLater(CheckAdded, STEP_MS * 2);
	}

	//------------------------------------------------------------------------------------------------
	//! The cart panel shows the items, the arsenal the balance after them. Then an inspection.
	protected void CheckAdded()
	{
		SCR_InventoryMenuUI menu = SCR_InventoryMenuUI.GetInventoryMenu();
		MRX_ArsenalTradePanel panel;
		if (menu)
			panel = menu.MRX_GetTradePanel();

		if (!panel || !m_Trade.GetCartStorage())
		{
			Check(false, "trade panel still open");
			End(true);
			return;
		}

		array<IEntity> roots = {};
		CheckInt(m_Trade.GetCartStorage().GetRootItems(roots), 2, "two items in the cart");
		SCR_InventorySlotUI rifleSlot = FindSlot(panel.GetStorageUI(false), MRX_Test_ArsenalShopFlow.RIFLE);
		Check(rifleSlot != null, "cart panel shows the rifle");
		Check(FindSlot(panel.GetStorageUI(false), MRX_Test_ArsenalShopFlow.BANDAGE) != null, "cart panel shows the bandage");

		int balance;
		int after;
		MRX_ClientWallet wallet = MRX_ClientWallet.GetLocal();
		if (wallet && wallet.TryGetBalance(MRX_Settings.DEFAULT_CURRENCY, balance) && m_Trade.GetBalanceAfter(MRX_Settings.DEFAULT_CURRENCY, after))
			CheckInt(balance - after, 1010, "balance after the cart");
		else
			Check(false, "local balance known");

		if (!rifleSlot)
		{
			End(true);
			return;
		}

		// The player inspects weapons of the own only; any inspection uses the middle the same way.
		menu.InspectItem(rifleSlot);
		GetGame().GetCallqueue().CallLater(CheckInspected, STEP_MS);
	}

	//------------------------------------------------------------------------------------------------
	//! The inspection takes the middle with its attachment points on the weapon, and the panel comes back after it.
	//! Middle mouse and double click open the rifle's attachments below the cart; an inspection closes them.
	protected void CheckInspected()
	{
		SCR_InventoryMenuUI menu = SCR_InventoryMenuUI.GetInventoryMenu();
		MRX_ArsenalTradePanel panel;
		if (menu)
			panel = menu.MRX_GetTradePanel();

		if (!panel)
		{
			Check(false, "trade panel still open");
			End(true);
			return;
		}

		SCR_InventoryGearInspectionPointUI inspection = menu.GetGearInspectionUI();
		Check(inspection != null, "an inspection starts");
		Check(!panel.IsVisible() && IsRenderVisible(menu), "an inspection takes the middle instead of the trade panel");
		if (inspection)
		{
			int unplaced;
			foreach (SCR_InventorySlotUI point : inspection.GetUISlots())
			{
				if (!point || !point.GetWidget())
					continue;

				vector position = FrameSlot.GetPos(point.GetWidget());
				if (position[0] == 0 && position[1] == 0)
					unplaced++;
			}

			CheckInt(unplaced, 0, "attachment points left in the corner");
		}

		menu.InspectItem(null);
		Check(panel.IsVisible() && !IsRenderVisible(menu), "the trade panel comes back after the inspection");
		SCR_InventorySlotUI rifleSlot = FindSlot(panel.GetStorageUI(false), MRX_Test_ArsenalShopFlow.RIFLE);
		if (!rifleSlot)
		{
			Check(false, "cart panel shows the rifle");
			End(true);
			return;
		}

		int openedCount = menu.MRX_TestGetOpenedStorageCount();
		BaseInventoryStorageComponent rifleStorage = rifleSlot.GetAsStorage();
		MRX_ArsenalTradeStorageUI cartUI = panel.GetStorageUI(false);
		int rows = MRX_ArsenalShopComponent.Find(m_Arsenal).GetCartRows();
		menu.MRX_TestOpenAsContainer(rifleSlot);
		Check(panel.GetDetailStorageUI() && panel.GetDetailStorageUI().GetStorage() == rifleStorage, "middle mouse opens the rifle's attachments below the cart");
		CheckInt(menu.MRX_TestGetOpenedStorageCount(), openedCount, "storages opened on the left");
		Check(cartUI.MRX_GetRows() < rows, "the cart folds while the storage below is open");
		// The fold rebuilds the cart's slots, as the player's next hover finds them.
		rifleSlot = FindSlot(cartUI, MRX_Test_ArsenalShopFlow.RIFLE);
		if (!rifleSlot)
		{
			Check(false, "the folded cart shows the rifle");
			End(true);
			return;
		}

		menu.MRX_TestOpenStorage(rifleSlot);
		Check(!panel.GetDetailStorageUI(), "double click on the item shown below closes it");
		CheckInt(cartUI.MRX_GetRows(), rows, "cart rows after the storage below closes");
		Check(cartUI.GetCurrentNavigationStorage() == m_Trade.GetCartStorage(), "the cart panel stays on the cart");
		rifleSlot = FindSlot(cartUI, MRX_Test_ArsenalShopFlow.RIFLE);
		if (!rifleSlot)
		{
			Check(false, "the unfolded cart shows the rifle");
			End(true);
			return;
		}

		menu.MRX_TestOpenStorage(rifleSlot);
		Check(panel.GetDetailStorageUI() && panel.GetDetailStorageUI().GetStorage() == rifleStorage, "double click opens the rifle's attachments below the cart");
		menu.InspectItem(rifleSlot);
		Check(menu.GetGearInspectionUI() && !panel.GetDetailStorageUI(), "an inspection closes the storage below the cart");
		CheckInt(cartUI.MRX_GetRows(), rows, "cart rows after an inspection closed the storage below");
		menu.InspectItem(null);

		array<ResourceName> ownPrefabs = {MRX_Test_ArsenalShopFlow.BANDAGE, MRX_Test_ArsenalShopFlow.RIFLE, BINOCULARS};
		bool allPutUp = true;
		foreach (ResourceName prefab : ownPrefabs)
		{
			IEntity own = FindCarried(prefab);
			IEntity parent;
			if (own)
				parent = own.GetParent();
			bool putUp = own && m_Trade.PutUpForSale(own);
			Check(putUp, "own item put up for sale: " + prefab);
			if (!putUp)
				allPutUp = false;

			if (own)
			{
				m_aOwnItems.Insert(own);
				m_aOwnParents.Insert(parent);
			}
		}

		if (!m_aOwnItems.IsEmpty())
			m_OwnItem = m_aOwnItems[0];

		if (!allPutUp)
			PrintCharacterState();

		GetGame().GetCallqueue().CallLater(CheckSale, STEP_MS * 2);
	}

	//------------------------------------------------------------------------------------------------
	//! The sale tab shows the sale box with prices; the arsenal lists what it buys back.
	protected void CheckSale()
	{
		SCR_InventoryMenuUI menu = SCR_InventoryMenuUI.GetInventoryMenu();
		MRX_ArsenalTradePanel panel;
		if (menu)
			panel = menu.MRX_GetTradePanel();

		if (!panel || !m_OwnItem)
		{
			Check(false, "trade panel still open with the bandage for sale");
			End(true);
			return;
		}

		Check(m_Trade.IsSaleTab() && panel.GetShownStorageUI() == panel.GetStorageUI(true), "selling switches to the sale tab");
		Check(m_Trade.GetSaleStorage() && m_Trade.GetSaleStorage().Contains(m_OwnItem), "bandage in the sale box");
		int price;
		string currency;
		Check(m_Trade.GetSalePrice(m_OwnItem, price, currency), "sale price known on the client");
		CheckInt(price, 5, "bandage bought back at half");

		SCR_InventorySlotUI saleSlot = FindSlot(panel.GetStorageUI(true), MRX_Test_ArsenalShopFlow.BANDAGE);
		TextWidget label;
		if (saleSlot && saleSlot.GetWidget())
			label = TextWidget.Cast(saleSlot.GetWidget().FindAnyWidget(PRICE_WIDGET));

		Check(label && label.IsVisible() && !label.GetText().IsEmpty(), "sale box slot shows its price");

		BaseInventoryStorageComponent arsenalStorage = BaseInventoryStorageComponent.Cast(m_Arsenal.FindComponent(BaseInventoryStorageComponent));
		SCR_InventoryOpenedStorageArsenalUI arsenalPanel;
		if (arsenalStorage)
			arsenalPanel = SCR_InventoryOpenedStorageArsenalUI.Cast(menu.GetOpenedStorage(arsenalStorage));

		Check(arsenalPanel && arsenalPanel.MRX_IsSaleList(), "the arsenal lists what it buys back in the sale tab");
		if (arsenalPanel)
			CheckInt(arsenalPanel.MRX_GetShownCount(), 3, "buy-back list includes the item not for sale");

		m_aReturnQueue.Copy(m_aOwnItems);
		ReturnNext();
	}

	//------------------------------------------------------------------------------------------------
	//! X on the next own item in the sale box, one at a time; then the inventory closes.
	protected void ReturnNext()
	{
		SCR_InventoryMenuUI menu = SCR_InventoryMenuUI.GetInventoryMenu();
		MRX_ArsenalTradePanel panel;
		if (menu)
			panel = menu.MRX_GetTradePanel();

		if (m_aReturnQueue.IsEmpty() || !panel)
		{
			GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.Inventory20Menu);
			GetGame().GetCallqueue().CallLater(CheckClosed, STEP_MS * 2);
			return;
		}

		IEntity item = m_aReturnQueue[0];
		SCR_InventorySlotUI slot = FindSlotOf(panel.GetStorageUI(true), item);
		if (!slot)
		{
			Check(false, "sale box shows " + GetName(item));
			m_aReturnQueue.RemoveOrdered(0);
			ReturnNext();
			return;
		}

		menu.MRX_TestPressDrop(slot);
		GetGame().GetCallqueue().CallLater(CheckReturned, STEP_MS * 2);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckReturned()
	{
		IEntity item = m_aReturnQueue[0];
		m_aReturnQueue.RemoveOrdered(0);
		bool returned = item && item.GetRootParent() == m_Character && !m_Trade.GetSaleStorage().Contains(item);
		Check(returned, "X on an item of the sale box gives it back: " + GetName(item));
		int index = m_aOwnItems.Find(item);
		if (returned && m_aOwnParents.IsIndexValid(index))
			Check(item.GetParent() == m_aOwnParents[index], "the item goes back where it was: " + GetName(item));

		SCR_InventoryMenuUI menu = SCR_InventoryMenuUI.GetInventoryMenu();
		if (returned && menu && menu.MRX_GetTradePanel())
			Check(!FindSlotOf(menu.MRX_GetTradePanel().GetStorageUI(true), item), "the sale box panel no longer shows the item: " + GetName(item));
		if (!returned && item)
		{
			// Where an item picked up would go.
			InventoryStorageManagerComponent manager = GetCharacterManager();
			array<EStoragePurpose> purposes = {EStoragePurpose.PURPOSE_WEAPON_PROXY, EStoragePurpose.PURPOSE_EQUIPMENT_ATTACHMENT, EStoragePurpose.PURPOSE_DEPOSIT, EStoragePurpose.PURPOSE_ANY};
			foreach (EStoragePurpose purpose : purposes)
			{
				BaseInventoryStorageComponent target = manager.FindStorageForItem(item, purpose);
				string owner = "none";
				if (target && target.GetOwner())
					owner = GetName(target.GetOwner()) + " " + target.ClassName();

				Print(MRX_TestRunner.TAG + string.Format("  storage for %1 (%2): %3", GetName(item), typename.EnumToString(EStoragePurpose, purpose), owner));
			}

			PrintCharacterState();
		}

		ReturnNext();
	}

	//------------------------------------------------------------------------------------------------
	//! For the log when the player's items do not move: whether the character can move items at all.
	protected void PrintCharacterState()
	{
		SCR_InventoryStorageManagerComponent manager = SCR_InventoryStorageManagerComponent.Cast(GetCharacterManager());
		CharacterControllerComponent controller = CharacterControllerComponent.Cast(m_Character.FindComponent(CharacterControllerComponent));
		if (!manager || !controller)
			return;

		Print(MRX_TestRunner.TAG + string.Format("  character: animation ready %1, locked %2, controlled %3, life state %4, at %5", manager.IsAnimationReady(), manager.IsInventoryLocked(), SCR_PlayerController.GetLocalControlledEntity() == m_Character, typename.EnumToString(ECharacterLifeState, controller.GetLifeState()), m_Character.GetOrigin()));
	}

	//------------------------------------------------------------------------------------------------
	protected static string GetName(IEntity entity)
	{
		if (!entity)
			return "nothing";

		return FilePath.StripPath(SCR_ResourceNameUtils.GetPrefabName(entity));
	}

	//------------------------------------------------------------------------------------------------
	//! Slot of a storage panel showing the item, or null.
	protected static SCR_InventorySlotUI FindSlotOf(SCR_InventoryStorageBaseUI storageUI, IEntity item)
	{
		if (!storageUI || !item)
			return null;

		foreach (SCR_InventorySlotUI slot : storageUI.GetUISlots())
		{
			if (slot && slot.GetInventoryItemComponent() && slot.GetInventoryItemComponent().GetOwner() == item)
				return slot;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckClosed()
	{
		Check(!m_Trade.IsOpen(), "the trade closes with the inventory");
		Check(MRX_ArsenalTrades.Get().Find(m_iPlayerId) == null, "the server closed the trade");
		foreach (IEntity own : m_aOwnItems)
		{
			Check(own && own.GetRootParent() == m_Character, "the player keeps the own item: " + GetName(own));
		}

		End(true);
	}

	//------------------------------------------------------------------------------------------------
	protected void End(bool finish)
	{
		GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.Inventory20Menu);
		if (MRX_ArsenalTrades.Get())
			MRX_ArsenalTrades.Get().Close(m_iPlayerId, "test end");

		foreach (IEntity own : m_aOwnItems)
		{
			if (own && own.GetRootParent() == m_Character)
				SCR_EntityHelper.DeleteEntityAndChildren(own);
		}

		if (m_Arsenal)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Arsenal);

		if (m_Possession)
			m_Possession.Release();

		if (finish)
			Finish();
	}

	//------------------------------------------------------------------------------------------------
	//! Slot of a storage panel showing an item of the prefab, or null.
	protected static SCR_InventorySlotUI FindSlot(SCR_InventoryStorageBaseUI storageUI, ResourceName prefab)
	{
		if (!storageUI)
			return null;

		array<SCR_InventorySlotUI> slots = storageUI.GetUISlots();
		foreach (SCR_InventorySlotUI slot : slots)
		{
			if (slot && slot.GetInventoryItemComponent() && slot.GetInventoryItemComponent().GetOwner() && MRX_ArsenalTradeTests.IsPrefab(slot.GetInventoryItemComponent().GetOwner(), prefab))
				return slot;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsRenderVisible(notnull SCR_InventoryMenuUI menu)
	{
		Widget render = menu.GetRootWidget().FindAnyWidget(RENDER_WIDGET);
		return render && render.IsVisible();
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity FindCarried(ResourceName prefab)
	{
		array<IEntity> items = {};
		m_Possession.GetItems(items);
		foreach (IEntity item : items)
		{
			if (MRX_ArsenalTradeTests.IsPrefab(item, prefab))
				return item;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected InventoryStorageManagerComponent GetCharacterManager()
	{
		return InventoryStorageManagerComponent.Cast(m_Character.FindComponent(SCR_InventoryStorageManagerComponent));
	}
}
#endif
