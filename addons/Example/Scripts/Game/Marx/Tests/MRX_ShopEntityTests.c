#ifdef WORKBENCH
// End-to-end shop test with the real prefab, UI and RPCs on the local (host) player.

//------------------------------------------------------------------------------------------------
class MRX_ShopEntityTests
{
	//------------------------------------------------------------------------------------------------
	static void Register(notnull MRX_TestRunner runner)
	{
		runner.Add(new MRX_Test_ShopEntityFlow());
	}
}

//------------------------------------------------------------------------------------------------
//! Spawns the shop table next to the local player, opens and closes the shop menu, then buys and sells an item
//! through the player controller RPCs. Without a character or identity, only the expected refusal is checked.
class MRX_Test_ShopEntityFlow : MRX_TestCase
{
	static const ResourceName SHOP_PREFAB = "{10C12BB88B37C571}Prefabs/Marx/Shop/MRX_ShopTable.et";
	static const string ITEM_ID = "field_dressing";
	protected static const int SAMPLE_ITEM_COUNT = 7;
	//! Longer than the controller's request interval.
	protected static const int REQUEST_GAP_MS = 400;

	protected IEntity m_ShopEntity;
	protected ref MRX_TestPossession m_Possession;
	protected SCR_PlayerController m_Controller;
	protected IEntity m_Character;
	protected bool m_bSelling;
	protected MRX_EShopStatus m_eExpected;
	protected int m_iCountBefore;

	//------------------------------------------------------------------------------------------------
	override int GetTimeoutMs()
	{
		return 15000;
	}

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		// Game Master sessions start without a character: possess a spawned one for the duration of the test.
		m_Possession = MRX_TestPossession.Acquire();
		if (!m_Possession)
		{
			Skip("no local player");
			return;
		}

		m_Controller = m_Possession.GetController();
		m_Character = m_Possession.GetCharacter();

		vector position = MRX_TestWorldUtils.GetCameraPosition();
		if (m_Character)
			position = m_Character.GetOrigin() + "1 0 0";

		m_ShopEntity = MRX_TestWorldUtils.SpawnPrefab(SHOP_PREFAB, position);
		if (!m_ShopEntity)
		{
			Check(false, "shop prefab spawns");
			Finish();
			return;
		}

		MRX_ShopComponent shop = MRX_ShopComponent.Cast(m_ShopEntity.FindComponent(MRX_ShopComponent));
		Check(shop != null, "shop prefab has MRX_ShopComponent");
		Check(SCR_EntityHelper.EntityToRplId(m_ShopEntity) != RplId.Invalid(), "shop entity is replicated");
		CheckOpenAction();
		if (!shop)
		{
			End();
			return;
		}

		MRX_ShopDefinition definition = shop.GetDefinition();
		Check(definition != null, "sample catalog loads");
		if (definition)
			CheckInt(definition.m_Catalog.m_aItems.Count(), SAMPLE_ITEM_COUNT, "sample catalog items");

		MRX_ShopMenu menu = MRX_ShopMenu.Open(shop);
		Check(menu != null, "shop menu opens");
		if (menu)
			menu.Close();

		m_Controller.MRX_GetOnShopResult().Insert(OnShopResult);

		MRX_IdentityService identity = MRX_Marx.GetIdentity();
		string ownerId;
		if (identity)
			ownerId = identity.GetOwnerId(m_Controller.GetPlayerId());

		if (!m_Character)
		{
			Print("[MRX_TEST]   no controlled character: expecting TOO_FAR");
			m_eExpected = MRX_EShopStatus.TOO_FAR;
			GetGame().GetCallqueue().CallLater(RequestBuy, REQUEST_GAP_MS);
			return;
		}

		if (ownerId.IsEmpty())
		{
			Print("[MRX_TEST]   local player has no owner: expecting OWNER_NOT_READY");
			m_eExpected = MRX_EShopStatus.OWNER_NOT_READY;
			GetGame().GetCallqueue().CallLater(RequestBuy, REQUEST_GAP_MS);
			return;
		}

		// Enough cash for the item, then the full buy and sell round trip.
		m_eExpected = MRX_EShopStatus.OK;
		m_iCountBefore = CountItems();
		MRX_TxCallback callback = new MRX_TxCallback();
		callback.GetOnResult().Insert(OnFunded);
		MRX_Marx.GetEconomy().Credit(ownerId, MRX_Settings.DEFAULT_CURRENCY, 100, MRX_TxContext.Create("test", "shop entity flow", MRX_NetworkTests.UniqueKey("shop-entity")), callback);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckOpenAction()
	{
		ActionsManagerComponent actions = ActionsManagerComponent.Cast(m_ShopEntity.FindComponent(ActionsManagerComponent));
		Check(actions != null, "shop prefab has ActionsManagerComponent");
		if (!actions)
			return;

		array<BaseUserAction> list = {};
		actions.GetActionsList(list);
		bool found;
		foreach (BaseUserAction action : list)
		{
			if (MRX_OpenShopAction.Cast(action))
				found = true;
		}

		Check(found, "shop prefab has MRX_OpenShopAction");
	}

	//------------------------------------------------------------------------------------------------
	protected void OnFunded(MRX_TxResult result)
	{
		CheckStatus(result.m_eStatus, MRX_ETxStatus.OK, "fund the buyer");
		GetGame().GetCallqueue().CallLater(RequestBuy, REQUEST_GAP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void RequestBuy()
	{
		if (m_ShopEntity)
			m_Controller.MRX_RequestBuy(m_ShopEntity, ITEM_ID);
	}

	//------------------------------------------------------------------------------------------------
	protected void RequestSell()
	{
		IEntity item = FindItem();
		Check(item != null, "bought item is in the inventory");
		if (!item || !m_ShopEntity)
		{
			End();
			return;
		}

		m_bSelling = true;
		m_Controller.MRX_RequestSell(m_ShopEntity, item);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnShopResult(MRX_EShopStatus status, MRX_ETxStatus txStatus, string itemId, int price, string currency)
	{
		string step = "buy";
		if (m_bSelling)
			step = "sell";

		Print(string.Format("[MRX_TEST]   shop %1 result %2 (tx %3, item %4, price %5 %6)", step, typename.EnumToString(MRX_EShopStatus, status), typename.EnumToString(MRX_ETxStatus, txStatus), itemId, price, currency));
		if (status != m_eExpected)
			Check(false, string.Format("%1: expected %2, got %3", step, typename.EnumToString(MRX_EShopStatus, m_eExpected), typename.EnumToString(MRX_EShopStatus, status)));

		if (m_eExpected != MRX_EShopStatus.OK || status != MRX_EShopStatus.OK)
		{
			End();
			return;
		}

		if (!m_bSelling)
		{
			CheckInt(CountItems(), m_iCountBefore + 1, "items after buy");
			GetGame().GetCallqueue().CallLater(RequestSell, REQUEST_GAP_MS);
			return;
		}

		// The sold item is deleted through replication; give it a frame.
		GetGame().GetCallqueue().CallLater(CheckSold, REQUEST_GAP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckSold()
	{
		CheckInt(CountItems(), m_iCountBefore, "items after sell");
		End();
	}

	//------------------------------------------------------------------------------------------------
	protected void End()
	{
		if (m_Controller)
			m_Controller.MRX_GetOnShopResult().Remove(OnShopResult);

		if (m_ShopEntity)
			SCR_EntityHelper.DeleteEntityAndChildren(m_ShopEntity);

		if (m_Possession)
			m_Possession.Release();

		Finish();
	}


	//------------------------------------------------------------------------------------------------
	protected int CountItems()
	{
		array<IEntity> items = {};
		GetItems(items);
		int count;
		foreach (IEntity item : items)
		{
			if (IsSampleItem(item))
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity FindItem()
	{
		array<IEntity> items = {};
		GetItems(items);
		foreach (IEntity item : items)
		{
			if (IsSampleItem(item))
				return item;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsSampleItem(IEntity item)
	{
		MRX_ShopComponent shop = MRX_ShopComponent.Cast(m_ShopEntity.FindComponent(MRX_ShopComponent));
		MRX_ShopItem catalogItem = shop.GetDefinition().m_Catalog.FindByPrefab(SCR_ResourceNameUtils.GetPrefabName(item));
		return catalogItem && catalogItem.m_sId == ITEM_ID;
	}

	//------------------------------------------------------------------------------------------------
	protected void GetItems(notnull array<IEntity> items)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(m_Character);
		if (!character || !character.GetCharacterController())
			return;

		InventoryStorageManagerComponent manager = character.GetCharacterController().GetInventoryStorageManager();
		if (manager)
			manager.GetItems(items);
	}
}
#endif
