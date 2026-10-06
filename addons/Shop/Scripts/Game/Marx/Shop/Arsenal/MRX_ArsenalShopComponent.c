[ComponentEditorProps(category: "Marx", description: "Makes a vanilla arsenal a Marx shop: it lists the catalog of the MRX_ShopComponent on the same entity and sells for Marx money instead of supplies. The entity also needs SCR_ArsenalComponent and SCR_ResourceComponent (vanilla arsenal boxes have both).")]
class MRX_ArsenalShopComponentClass : ScriptComponentClass
{
}

//! Marx arsenal (API v0). The vanilla arsenal window lists the shop's catalog (items with a purchase price), shows Marx
//! prices and buys and sells through MRX_ShopService. Server and clients build the same list from the catalog.
class MRX_ArsenalShopComponent : ScriptComponent
{
	//! Same as the vanilla arsenal requests.
	static const float MAX_DISTANCE = 30;

	protected static const int PRELOAD_CHECK_MS = 1000;
	protected static const int PRELOAD_PER_FRAME = 4;

	protected MRX_ShopComponent m_Shop;
	protected ref array<ref SCR_ArsenalItem> m_aItems;
	protected ref map<string, MRX_ShopItem> m_mItems;
	protected ref array<string> m_aCurrencies;
	protected int m_iPreloadIndex = -1;

	//------------------------------------------------------------------------------------------------
	static MRX_ArsenalShopComponent Find(IEntity entity)
	{
		if (!entity)
			return null;

		return MRX_ArsenalShopComponent.Cast(entity.FindComponent(MRX_ArsenalShopComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Marx arsenal of an arsenal's SCR_ResourceComponent replication ID, which vanilla arsenal requests carry.
	static MRX_ArsenalShopComponent FindByResourceId(RplId resourceComponentId)
	{
		if (!resourceComponentId.IsValid())
			return null;

		SCR_ResourceComponent resource = SCR_ResourceComponent.Cast(Replication.FindItem(resourceComponentId));
		if (!resource)
			return null;

		return Find(resource.GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	//! \return Null without MRX_ShopComponent or when its catalog cannot be loaded.
	MRX_ShopDefinition GetShop()
	{
		if (!m_Shop)
			return null;

		return m_Shop.GetDefinition();
	}

	//------------------------------------------------------------------------------------------------
	//! Catalog entry of a listed prefab, or null.
	MRX_ShopItem FindItem(ResourceName prefab)
	{
		Build();
		return m_mItems.Get(MRX_ShopCatalog.GetPrefabKey(prefab));
	}

	//------------------------------------------------------------------------------------------------
	//! Items listed in the arsenal window, in catalog order. \return Count.
	int GetArsenalItems(notnull array<SCR_ArsenalItem> outItems)
	{
		Build();
		foreach (SCR_ArsenalItem item : m_aItems)
		{
			outItems.Insert(item);
		}

		return m_aItems.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Currencies of the listed items, in catalog order.
	array<string> GetCurrencies()
	{
		Build();
		return m_aCurrencies;
	}

	//------------------------------------------------------------------------------------------------
	protected void Build()
	{
		if (m_aItems)
			return;

		m_aItems = {};
		m_mItems = new map<string, MRX_ShopItem>();
		m_aCurrencies = {};
		MRX_ShopDefinition shop = GetShop();
		if (!shop)
			return;

		foreach (MRX_ShopItem item : shop.m_Catalog.m_aItems)
		{
			string key = MRX_ShopCatalog.GetPrefabKey(item.m_sPrefab);
			if (item.m_iPrice <= 0 || m_mItems.Contains(key))
				continue;

			MRX_ArsenalItem arsenalItem = new MRX_ArsenalItem();
			arsenalItem.MRX_SetPrefab(item.m_sPrefab);
			m_aItems.Insert(arsenalItem);
			m_mItems.Set(key, item);
			if (!m_aCurrencies.Contains(item.m_sCurrency))
				m_aCurrencies.Insert(item.m_sCurrency);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnDefinitionChanged(MRX_ShopComponent shop)
	{
		m_aItems = null;
		m_mItems = null;
		m_aCurrencies = null;
		SCR_ArsenalComponent arsenal = SCR_ArsenalComponent.Cast(GetOwner().FindComponent(SCR_ArsenalComponent));
		if (arsenal)
			arsenal.RefreshArsenal();

		if (System.IsConsoleApp())
			return;

		m_iPreloadIndex = -1;
		GetGame().GetCallqueue().Remove(PreloadStep);
		GetGame().GetCallqueue().Remove(CheckPreload);
		GetGame().GetCallqueue().CallLater(CheckPreload, PRELOAD_CHECK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnPostInit(IEntity owner)
	{
		if (SCR_Global.IsEditMode())
			return;

		m_Shop = MRX_ShopComponent.Cast(owner.FindComponent(MRX_ShopComponent));
		if (!m_Shop)
		{
			Print(string.Format("[MRX] %1: MRX_ArsenalShopComponent needs MRX_ShopComponent on the same entity", owner), LogLevel.ERROR);
			return;
		}

		m_Shop.GetOnDefinitionChanged().Insert(OnDefinitionChanged);

		// Players only: the first opening of a large arsenal creates a preview entity per item.
		if (!System.IsConsoleApp())
			GetGame().GetCallqueue().CallLater(CheckPreload, PRELOAD_CHECK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(CheckPreload);
		GetGame().GetCallqueue().Remove(PreloadStep);
		if (m_Shop)
			m_Shop.GetOnDefinitionChanged().Remove(OnDefinitionChanged);
	}

	//------------------------------------------------------------------------------------------------
	//! Creates the preview entities of the listed items a few per frame once the local player comes near.
	protected void CheckPreload()
	{
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		if (!player || vector.DistanceSq(player.GetOrigin(), GetOwner().GetOrigin()) > MAX_DISTANCE * MAX_DISTANCE)
			return;

		GetGame().GetCallqueue().Remove(CheckPreload);
		m_iPreloadIndex = 0;
		GetGame().GetCallqueue().CallLater(PreloadStep);
	}

	//------------------------------------------------------------------------------------------------
	protected void PreloadStep()
	{
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!world || !world.GetItemPreviewManager() || m_iPreloadIndex < 0)
			return;

		Build();
		ItemPreviewManagerEntity previews = world.GetItemPreviewManager();
		int end = Math.Min(m_iPreloadIndex + PRELOAD_PER_FRAME, m_aItems.Count());
		for (; m_iPreloadIndex < end; m_iPreloadIndex++)
		{
			previews.ResolvePreviewEntityForPrefab(m_aItems[m_iPreloadIndex].GetItemResourceName());
		}

		if (m_iPreloadIndex < m_aItems.Count())
			GetGame().GetCallqueue().CallLater(PreloadStep);
	}
}

//------------------------------------------------------------------------------------------------
//! Arsenal list entry of a Marx arsenal. Internal.
class MRX_ArsenalItem : SCR_ArsenalItemStandalone
{
	//------------------------------------------------------------------------------------------------
	void MRX_SetPrefab(ResourceName prefab)
	{
		m_ItemResourceName = prefab;
	}
}
