// Trade container of a Marx arsenal with a cart: an entity without model that the server spawns at the arsenal while a
// player trades there (MRX_ArsenalTradeSession). One storage holds the unpaid items of the cart, the other the player's
// items put up for sale. The player's client shows the storage of the chosen tab in the middle of the vanilla inventory.

//! What a storage of the trade container holds.
enum MRX_EArsenalTradeRole
{
	//! Unpaid items the server put there; nobody takes them out before checkout.
	CART,
	//! Items of the player who trades, until they are sold or taken back.
	SALE
}

void MRX_ArsenalTradeMoveDelegate(MRX_ArsenalTradeStorageComponent storage, IEntity item, bool added);
typedef func MRX_ArsenalTradeMoveDelegate;

[ComponentEditorProps(category: "Marx", description: "Storage of the Marx arsenal trade container: the cart or the sale box")]
class MRX_ArsenalTradeStorageComponentClass : SCR_UniversalInventoryStorageComponentClass
{
}

//! Storage of the trade container. Never listed in the vicinity. The server's session decides what may go in (see
//! MRX_ArsenalTradeManagerComponent); clients answer yes and let the server refuse.
class MRX_ArsenalTradeStorageComponent : SCR_UniversalInventoryStorageComponent
{
	[Attribute("0", UIWidgets.ComboBox, "What the storage holds", enums: ParamEnumArray.FromEnum(MRX_EArsenalTradeRole))]
	protected MRX_EArsenalTradeRole m_eRole;

	protected static ref ScriptInvokerBase<MRX_ArsenalTradeMoveDelegate> s_OnItemMoved;

	//------------------------------------------------------------------------------------------------
	//! Invoked after an item was put in or taken out, on every machine that runs the move.
	static ScriptInvokerBase<MRX_ArsenalTradeMoveDelegate> GetOnItemMoved()
	{
		if (!s_OnItemMoved)
			s_OnItemMoved = new ScriptInvokerBase<MRX_ArsenalTradeMoveDelegate>();

		return s_OnItemMoved;
	}

	//------------------------------------------------------------------------------------------------
	//! Storage of the given role on a trade container, or null.
	static MRX_ArsenalTradeStorageComponent Find(IEntity container, MRX_EArsenalTradeRole role)
	{
		if (!container)
			return null;

		array<Managed> components = {};
		container.FindComponents(MRX_ArsenalTradeStorageComponent, components);
		foreach (Managed component : components)
		{
			MRX_ArsenalTradeStorageComponent storage = MRX_ArsenalTradeStorageComponent.Cast(component);
			if (storage && storage.GetRole() == role)
				return storage;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Storage of a trade container the item lies in, at any depth (e.g. a scope on a rifle in the cart), or null.
	static MRX_ArsenalTradeStorageComponent FindHolder(IEntity item)
	{
		if (!item)
			return null;

		IEntity top = item;
		while (top.GetParent() && !top.GetParent().FindComponent(MRX_ArsenalTradeManagerComponent))
		{
			top = top.GetParent();
		}

		if (!top.GetParent())
			return null;

		InventoryItemComponent itemComponent = InventoryItemComponent.Cast(top.FindComponent(InventoryItemComponent));
		if (!itemComponent || !itemComponent.GetParentSlot())
			return null;

		return MRX_ArsenalTradeStorageComponent.Cast(itemComponent.GetParentSlot().GetStorage());
	}

	//------------------------------------------------------------------------------------------------
	//! True when the item lies in the cart of a trade container, at any depth: it is not paid for.
	static bool IsInCart(IEntity item)
	{
		MRX_ArsenalTradeStorageComponent holder = FindHolder(item);
		return holder && holder.GetRole() == MRX_EArsenalTradeRole.CART;
	}

	//------------------------------------------------------------------------------------------------
	//! Cells the item takes, as laid out by the vanilla inventory (SCR_InventorySlotUI.SetSlotSize).
	static int GetCellCount(IEntity item)
	{
		InventoryItemComponent itemComponent;
		if (item)
			itemComponent = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));

		SCR_ItemAttributeCollection attributes;
		if (itemComponent)
			attributes = SCR_ItemAttributeCollection.Cast(itemComponent.GetAttributes());

		if (!attributes)
			return 1;

		switch (attributes.GetItemSize())
		{
			case ESlotSize.SLOT_2x1: return 2;
			case ESlotSize.SLOT_2x2: return 4;
			case ESlotSize.SLOT_3x3: return 9;
		}

		return 1;
	}

	//------------------------------------------------------------------------------------------------
	MRX_EArsenalTradeRole GetRole()
	{
		return m_eRole;
	}

	//------------------------------------------------------------------------------------------------
	//! Items lying directly in the storage (not the ones inside them).
	int GetRootItems(notnull array<IEntity> outItems)
	{
		outItems.Clear();
		array<InventoryItemComponent> itemComponents = {};
		GetOwnedItems(itemComponents, false);
		foreach (InventoryItemComponent itemComponent : itemComponents)
		{
			if (itemComponent && itemComponent.GetOwner())
				outItems.Insert(itemComponent.GetOwner());
		}

		return outItems.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Cells taken by the items lying in the storage.
	int GetUsedCells()
	{
		array<IEntity> items = {};
		GetRootItems(items);
		int cells;
		foreach (IEntity item : items)
		{
			cells += GetCellCount(item);
		}

		return cells;
	}

	//------------------------------------------------------------------------------------------------
	override protected bool ShouldHideInVicinity()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanStoreItem(IEntity item, int slotID)
	{
		if (!super.CanStoreItem(item, slotID))
			return false;

		// Clients do not know the session: answering no there would stop the request (or a replicated move).
		MRX_ArsenalTradeManagerComponent manager = GetTradeManager();
		if (!Replication.IsServer() || !manager)
			return true;

		return manager.CanStore(this, item);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanStoreResource(ResourceName resourceName, int slotID)
	{
		if (!super.CanStoreResource(resourceName, slotID))
			return false;

		MRX_ArsenalTradeManagerComponent manager = GetTradeManager();
		if (!Replication.IsServer() || !manager)
			return true;

		return manager.CanSpawnInto(this);
	}

	//------------------------------------------------------------------------------------------------
	protected override void OnAddedToSlot(IEntity item, int slotID)
	{
		super.OnAddedToSlot(item, slotID);
		if (s_OnItemMoved)
			s_OnItemMoved.Invoke(this, item, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnRemovedFromSlot(IEntity item, int slotID)
	{
		super.OnRemovedFromSlot(item, slotID);
		if (s_OnItemMoved)
			s_OnItemMoved.Invoke(this, item, false);
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_ArsenalTradeManagerComponent GetTradeManager()
	{
		return MRX_ArsenalTradeManagerComponent.Cast(GetOwner().FindComponent(MRX_ArsenalTradeManagerComponent));
	}
}

[ComponentEditorProps(category: "Marx", description: "Inventory manager of the Marx arsenal trade container")]
class MRX_ArsenalTradeManagerComponentClass : ScriptedInventoryStorageManagerComponentClass
{
}

//! Inventory manager of the trade container. Server: asks the session that owns the container what may go in and come
//! out. Without a session (closing) nothing goes in or out.
class MRX_ArsenalTradeManagerComponent : ScriptedInventoryStorageManagerComponent
{
	//! Weak: server, the session that spawned the container.
	protected MRX_ArsenalTradeSession m_Session;

	//------------------------------------------------------------------------------------------------
	void SetSession(MRX_ArsenalTradeSession session)
	{
		m_Session = session;
	}

	//------------------------------------------------------------------------------------------------
	MRX_ArsenalTradeSession GetSession()
	{
		return m_Session;
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by MRX_ArsenalTradeStorageComponent on the server.
	bool CanStore(notnull MRX_ArsenalTradeStorageComponent storage, IEntity item)
	{
		return m_Session && item && m_Session.CanStore(storage, item);
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by MRX_ArsenalTradeStorageComponent on the server.
	bool CanSpawnInto(notnull MRX_ArsenalTradeStorageComponent storage)
	{
		return m_Session && m_Session.CanSpawnInto(storage);
	}

	//------------------------------------------------------------------------------------------------
	override protected bool ShouldForbidRemoveByInstigator(InventoryStorageManagerComponent instigatorManager, BaseInventoryStorageComponent fromStorage, IEntity item)
	{
		// Clients do not know the session; the server refuses (and undoes) the move.
		if (!Replication.IsServer())
			return false;

		return !m_Session || !m_Session.CanRemove(instigatorManager, item);
	}
}
