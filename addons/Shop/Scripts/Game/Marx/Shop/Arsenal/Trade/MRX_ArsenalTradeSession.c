//! Server: one player's trade at a Marx arsenal with a cart (MRX_ArsenalShopComponent.UsesCart). The trade container
//! at the arsenal holds the cart and the sale box (MRX_ArsenalTradeStorageComponent):
//! - Cart: unpaid items the server spawned. Nobody takes them out; the server moves them inside the cart for the player
//!   and hands them over at checkout. Every spawned item is recorded with its catalog ID; the parts it came with (e.g. a
//!   rifle's magazine) are recorded as its default parts, which its price includes.
//! - Sale box: the player's own items put up for sale. Only the player puts items in and takes them out.
//! The session is the cart of the checkout requests (MRX_ShopCart). Closing it empties the cart and gives the sale box
//! back to the player.
class MRX_ArsenalTradeSession : MRX_ShopCart
{
	//! Weak: the manager owns its sessions.
	protected MRX_ArsenalTradeSessionManager m_Manager;
	protected int m_iPlayerId;
	//! The client's number of the opening this session answers.
	protected int m_iRequest;
	//! Weak entity handles.
	protected IEntity m_Character;
	protected IEntity m_Arsenal;
	protected IEntity m_Container;
	protected MRX_ArsenalTradeStorageComponent m_CartStorage;
	protected MRX_ArsenalTradeStorageComponent m_SaleStorage;
	protected MRX_ArsenalTradeManagerComponent m_ContainerManager;

	//! Items of the cart the server put there, by entity ID.
	protected ref map<EntityID, ref MRX_ArsenalCartEntry> m_mEntries = new map<EntityID, ref MRX_ArsenalCartEntry>();
	//! Items the server moves out of (or inside) the container right now. Weak.
	protected ref array<IEntity> m_aReleased = {};
	//! Running inventory operations, kept until they report.
	protected ref array<ref MRX_ArsenalTradeOp> m_aOps = {};
	protected int m_iPendingSpawns;
	protected int m_iPendingMoves;
	protected bool m_bCheckingOut;
	protected bool m_bClosing;
	protected bool m_bCloseAfterCheckout;
	protected string m_sCloseReason;
	protected bool m_bStateScheduled;
	//! The state last sent to the client (MRX_ArsenalTradeSessionManager.SendState).
	protected string m_sSentState;
	protected bool m_bStateSent;

	//------------------------------------------------------------------------------------------------
	void MRX_ArsenalTradeSession(MRX_ArsenalTradeSessionManager manager, int playerId, IEntity character, IEntity arsenal)
	{
		m_Manager = manager;
		m_iPlayerId = playerId;
		m_Character = character;
		m_Arsenal = arsenal;
	}

	//------------------------------------------------------------------------------------------------
	void ~MRX_ArsenalTradeSession()
	{
		GetGame().GetCallqueue().Remove(SendState);
		if (m_ContainerManager)
		{
			m_ContainerManager.m_OnItemAddedInvoker.Remove(OnContainerChanged);
			m_ContainerManager.m_OnItemRemovedInvoker.Remove(OnContainerChanged);
		}
	}

	//------------------------------------------------------------------------------------------------
	int GetPlayerId()
	{
		return m_iPlayerId;
	}

	//------------------------------------------------------------------------------------------------
	int GetRequest()
	{
		return m_iRequest;
	}

	//------------------------------------------------------------------------------------------------
	//! A new opening (also of the same trade) gets the state sent again.
	void SetRequest(int request)
	{
		m_iRequest = request;
		m_sSentState = string.Empty;
		m_bStateSent = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Internal: remembers the state sent to the client. \return False when it is the one sent last.
	bool SetSentState(string state)
	{
		if (m_bStateSent && state == m_sSentState)
			return false;

		m_sSentState = state;
		m_bStateSent = true;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	IEntity GetCharacter()
	{
		return m_Character;
	}

	//------------------------------------------------------------------------------------------------
	IEntity GetArsenal()
	{
		return m_Arsenal;
	}

	//------------------------------------------------------------------------------------------------
	IEntity GetContainer()
	{
		return m_Container;
	}

	//------------------------------------------------------------------------------------------------
	bool IsClosing()
	{
		return m_bClosing;
	}

	//------------------------------------------------------------------------------------------------
	bool IsCheckingOut()
	{
		return m_bCheckingOut;
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns the trade container at the arsenal. \return False when it cannot be created.
	bool Start(ResourceName containerPrefab)
	{
		Resource resource = Resource.Load(containerPrefab);
		if (!resource || !resource.IsValid())
		{
			Print(string.Format("[MRX] Arsenal trade container %1 cannot be loaded", containerPrefab), LogLevel.ERROR);
			return false;
		}

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = m_Arsenal.GetOrigin();
		m_Container = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
		if (!m_Container)
			return false;

		m_CartStorage = MRX_ArsenalTradeStorageComponent.Find(m_Container, MRX_EArsenalTradeRole.CART);
		m_SaleStorage = MRX_ArsenalTradeStorageComponent.Find(m_Container, MRX_EArsenalTradeRole.SALE);
		m_ContainerManager = MRX_ArsenalTradeManagerComponent.Cast(m_Container.FindComponent(MRX_ArsenalTradeManagerComponent));
		if (!m_CartStorage || !m_SaleStorage || !m_ContainerManager)
		{
			Print(string.Format("[MRX] Arsenal trade container %1 needs a cart and a sale storage (MRX_ArsenalTradeStorageComponent) and MRX_ArsenalTradeManagerComponent", containerPrefab), LogLevel.ERROR);
			SCR_EntityHelper.DeleteEntityAndChildren(m_Container);
			m_Container = null;
			return false;
		}

		m_ContainerManager.SetSession(this);
		m_ContainerManager.m_OnItemAddedInvoker.Insert(OnContainerChanged);
		m_ContainerManager.m_OnItemRemovedInvoker.Insert(OnContainerChanged);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Empties the cart, gives the sale box back to the player (at the player's feet when the inventory is full) and
	//! deletes the container. A running checkout finishes first.
	void Close(string reason)
	{
		if (m_bClosing)
			return;

		if (m_bCheckingOut)
		{
			m_bCloseAfterCheckout = true;
			m_sCloseReason = reason;
			return;
		}

		Print(string.Format("[MRX] Arsenal trade of player %1 closing: %2", m_iPlayerId, reason), LogLevel.NORMAL);
		m_bClosing = true;
		int dropped;
		array<IEntity> items = {};
		if (m_SaleStorage)
			m_SaleStorage.GetRootItems(items);

		// Items of the player's own that got into the cart go back too; the cart's items go with the container.
		array<IEntity> foreign = {};
		CollectForeign(foreign);
		items.InsertAll(foreign);
		foreach (IEntity item : items)
		{
			if (!ReturnToCharacter(item))
				dropped++;
		}

		m_Manager.OnSessionClosing(this, dropped);
		if (m_ContainerManager)
			m_ContainerManager.SetSession(null);

		if (m_Container)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Container);

		Print(string.Format("[MRX] Arsenal trade of player %1 closed", m_iPlayerId), LogLevel.NORMAL);
		m_Manager.OnSessionClosed(this);
	}

	//------------------------------------------------------------------------------------------------
	//! Puts a new item of the arsenal into the cart: into the cart's cells, or into a storage of an item in the cart
	//! (e.g. a rifle's attachment slots). An item already in that slot moves to the cells.
	//! \param storage Null or the cart: its cells. \param slotId Slot of storage, -1: the first empty one that takes it.
	MRX_EShopStatus Add(string itemId, BaseInventoryStorageComponent storage, int slotId)
	{
		if (!CanChange())
			return MRX_EShopStatus.BUSY;

		MRX_ArsenalShopComponent arsenal = MRX_ArsenalShopComponent.Find(m_Arsenal);
		MRX_ShopDefinition shop = GetShop();
		if (!arsenal || !shop)
			return MRX_EShopStatus.UNKNOWN_SHOP;

		// Only what the arsenal lists for sale.
		MRX_ShopItem item = shop.m_Catalog.FindItem(itemId);
		if (!item || arsenal.FindItem(item.m_sPrefab) != item)
			return MRX_EShopStatus.UNKNOWN_ITEM;

		if (!storage || storage == m_CartStorage)
		{
			Spawn(item, m_CartStorage, -1);
			return MRX_EShopStatus.OK;
		}

		if (!IsCartStorage(storage))
			return MRX_EShopStatus.REJECTED;

		if (slotId < 0 || slotId >= storage.GetSlotsCount())
			slotId = FindEmptySlot(item.m_sPrefab, storage);

		// Storages without fixed slots (e.g. a backpack's: its slots are the items it holds) place the item themselves.
		if (slotId < 0)
		{
			if (!m_ContainerManager.CanInsertResourceInStorage(item.m_sPrefab, storage, -1))
				return MRX_EShopStatus.NO_SPACE;

			Spawn(item, storage, -1);
			return MRX_EShopStatus.OK;
		}

		IEntity occupant = storage.Get(slotId);
		if (!occupant)
		{
			if (!m_ContainerManager.CanInsertResourceInStorage(item.m_sPrefab, storage, slotId))
				return MRX_EShopStatus.NO_SPACE;

			Spawn(item, storage, slotId);
			return MRX_EShopStatus.OK;
		}

		// The part in the slot moves to the cells first; the new item then goes into the slot.
		if (!HasRoomFor(occupant))
			return MRX_EShopStatus.NO_SPACE;

		MRX_ArsenalTradeOp op = CreateOp(MRX_EArsenalTradeStep.VACATE, occupant);
		op.m_Item = item;
		op.m_Storage = storage;
		op.m_iSlotId = slotId;
		Move(op, m_CartStorage, -1);
		return MRX_EShopStatus.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes an item out of the cart: it is deleted with what it holds and with the default parts it came with that lie
	//! in the cells. Items of the player's own inside it go back to the player.
	MRX_EShopStatus RemoveFromCart(IEntity item)
	{
		if (!CanChange())
			return MRX_EShopStatus.BUSY;

		if (!item || !IsInCart(item))
			return MRX_EShopStatus.NOT_IN_INVENTORY;

		array<IEntity> inside = {};
		CollectNested(item, inside);
		foreach (IEntity nested : inside)
		{
			if (!m_mEntries.Contains(nested.GetID()))
				ReturnToCharacter(nested);
		}

		array<IEntity> roots = {};
		m_CartStorage.GetRootItems(roots);
		foreach (IEntity root : roots)
		{
			MRX_ArsenalCartEntry entry = m_mEntries.Get(root.GetID());
			if (entry && entry.m_bDefault && entry.m_Origin == item)
				Delete(root);
		}

		Delete(item);
		ScheduleState();
		return MRX_EShopStatus.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Moves an item of the cart within the cart for the player: to the cells, or into a storage of another item in the
	//! cart (an item already in that slot moves to the cells). A default part goes into another item only when the arsenal
	//! sells it: there it is paid for.
	MRX_EShopStatus MoveInCart(IEntity item, BaseInventoryStorageComponent storage, int slotId)
	{
		if (!CanChange())
			return MRX_EShopStatus.BUSY;

		if (!item || !IsInCart(item))
			return MRX_EShopStatus.NOT_IN_INVENTORY;

		if (!storage || storage == m_CartStorage)
		{
			if (item.GetParent() == m_Container)
				return MRX_EShopStatus.OK;

			if (!HasRoomFor(item))
				return MRX_EShopStatus.NO_SPACE;

			Move(CreateOp(MRX_EArsenalTradeStep.MOVE, item), m_CartStorage, -1);
			return MRX_EShopStatus.OK;
		}

		IEntity targetOwner = storage.GetOwner();
		if (!IsCartStorage(storage) || targetOwner == item || IsInside(targetOwner, item))
			return MRX_EShopStatus.REJECTED;

		MRX_ArsenalCartEntry entry = m_mEntries.Get(item.GetID());
		bool home = entry && entry.m_Origin && (targetOwner == entry.m_Origin || IsInside(targetOwner, entry.m_Origin));
		if (entry && entry.m_bDefault && !home && entry.m_sItemId.IsEmpty())
			return MRX_EShopStatus.NOT_FOR_SALE;

		if (slotId < 0 || slotId >= storage.GetSlotsCount())
		{
			Move(CreateOp(MRX_EArsenalTradeStep.MOVE, item), storage, -1);
			return MRX_EShopStatus.OK;
		}

		IEntity occupant = storage.Get(slotId);
		if (!occupant)
		{
			Move(CreateOp(MRX_EArsenalTradeStep.MOVE, item), storage, slotId);
			return MRX_EShopStatus.OK;
		}

		if (occupant == item)
			return MRX_EShopStatus.OK;

		if (!HasRoomFor(occupant))
			return MRX_EShopStatus.NO_SPACE;

		MRX_ArsenalTradeOp op = CreateOp(MRX_EArsenalTradeStep.VACATE, occupant);
		op.m_Follower = item;
		op.m_Storage = storage;
		op.m_iSlotId = slotId;
		Move(op, m_CartStorage, -1);
		return MRX_EShopStatus.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Starts the checkout of the cart (purchase) or of the sale box (sale) through the shop service.
	MRX_EShopStatus Checkout(bool sell)
	{
		if (!CanChange() || m_iPendingSpawns > 0 || m_iPendingMoves > 0)
			return MRX_EShopStatus.BUSY;

		MRX_ShopService service = MRX_Shop.GetService();
		MRX_ShopDefinition shop = GetShop();
		if (!service || !shop)
			return MRX_EShopStatus.UNKNOWN_SHOP;

		m_bCheckingOut = true;
		MRX_ArsenalTradeCheckoutReply reply = new MRX_ArsenalTradeCheckoutReply(this, sell);
		if (sell)
			service.CheckoutSell(m_iPlayerId, shop, this, reply);
		else
			service.CheckoutBuy(m_iPlayerId, shop, this, reply);

		return MRX_EShopStatus.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by MRX_ArsenalTradeCheckoutReply.
	void OnCheckoutDone(bool sell, notnull MRX_ShopResult result)
	{
		m_bCheckingOut = false;
		m_Manager.SendCheckoutResult(this, sell, result);
		ScheduleState();
		if (m_bCloseAfterCheckout)
			Close(m_sCloseReason);
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by MRX_ArsenalTradeStorageComponent through the container's manager.
	bool CanStore(notnull MRX_ArsenalTradeStorageComponent storage, notnull IEntity item)
	{
		if (m_bClosing)
			return false;

		MRX_ArsenalTradeStorageComponent holder = MRX_ArsenalTradeStorageComponent.FindHolder(item);
		if (storage == m_CartStorage)
		{
			// Moves of the server inside the cart, or a spawned item. Never anything from a character.
			if (holder)
				return holder == m_CartStorage && IsReleased(item);

			return m_iPendingSpawns > 0 && !ChimeraCharacter.Cast(item.GetRootParent());
		}

		if (storage != m_SaleStorage || m_bCheckingOut)
			return false;

		if (holder == m_SaleStorage)
			return item.GetParent() == m_Container || HasRoomFor(item, m_SaleStorage);

		return item.GetRootParent() == m_Character && CanSell(item) && HasRoomFor(item, m_SaleStorage);
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by MRX_ArsenalTradeStorageComponent through the container's manager.
	bool CanSpawnInto(notnull MRX_ArsenalTradeStorageComponent storage)
	{
		return !m_bClosing && storage == m_CartStorage && m_iPendingSpawns > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by MRX_ArsenalTradeManagerComponent: items of the cart leave only when the server moves them,
	//! items of the sale box only by the player's own hand.
	bool CanRemove(InventoryStorageManagerComponent instigatorManager, IEntity item)
	{
		if (!item || IsReleased(item))
			return true;

		MRX_ArsenalTradeStorageComponent holder = MRX_ArsenalTradeStorageComponent.FindHolder(item);
		if (!holder)
			return true;

		if (holder != m_SaleStorage || m_bCheckingOut || m_bClosing)
			return false;

		return instigatorManager && instigatorManager.GetOwner() == m_Character;
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by MRX_ArsenalTradeOp.
	void OnOpDone(notnull MRX_ArsenalTradeOp op, bool success)
	{
		m_aOps.RemoveItem(op);
		switch (op.m_eStep)
		{
			case MRX_EArsenalTradeStep.SPAWN:
				m_iPendingSpawns--;
				OnSpawned(op, success);
				break;

			case MRX_EArsenalTradeStep.MOVE:
				m_iPendingMoves--;
				m_aReleased.RemoveItem(op.m_Target);
				if (!success)
					m_Manager.ReportFailure(this, MRX_EShopStatus.NO_SPACE);

				break;

			case MRX_EArsenalTradeStep.VACATE:
				m_iPendingMoves--;
				m_aReleased.RemoveItem(op.m_Target);
				OnVacated(op, success);
				break;

			case MRX_EArsenalTradeStep.DELIVER:
				m_iPendingMoves--;
				m_aReleased.RemoveItem(op.m_Target);
				OnDelivered(op, success);
				break;
		}

		ScheduleState();
	}

	//------------------------------------------------------------------------------------------------
	//! Cart lines: one per item in the cells, with the paid items it holds. A default part in the cells goes with the item
	//! it came with while that item is in the cart. Sale lines: one per item in the sale box.
	override int GetLines(bool selling, notnull array<ref MRX_ShopCartLine> outLines)
	{
		array<IEntity> roots = {};
		if (selling)
		{
			if (m_SaleStorage)
				m_SaleStorage.GetRootItems(roots);

			foreach (IEntity saleRoot : roots)
			{
				MRX_ShopCartLine saleLine = new MRX_ShopCartLine();
				saleLine.m_Item = saleRoot;
				outLines.Insert(saleLine);
			}

			return outLines.Count();
		}

		if (!m_CartStorage)
			return 0;

		m_CartStorage.GetRootItems(roots);
		foreach (IEntity root : roots)
		{
			MRX_ArsenalCartEntry entry = m_mEntries.Get(root.GetID());
			if (entry && entry.m_bDefault && entry.m_Origin && IsInCart(entry.m_Origin))
				continue;

			MRX_ShopCartLine line = new MRX_ShopCartLine();
			line.m_Item = root;
			array<IEntity> items = {root};
			CollectNested(root, items);
			foreach (IEntity item : items)
			{
				MRX_ShopItem paid = GetPaidItem(item);
				if (paid)
					line.m_aItemIds.Insert(paid.m_sId);
			}

			outLines.Insert(line);
		}

		return outLines.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Moves a paid line into the player's inventory, then the default parts it came with that lie in the cells.
	override void Deliver(int playerId, notnull MRX_ShopCartLine line, notnull MRX_ShopDeliveryCallback callback)
	{
		IEntity root = IEntity.Cast(line.m_Item);
		InventoryStorageManagerComponent manager = GetCharacterManager();
		BaseInventoryStorageComponent target;
		if (manager && root && root.GetParent() == m_Container && IsInCart(root))
			target = manager.FindStorageForItem(root);

		if (!target)
		{
			callback.OnResult(false);
			return;
		}

		MRX_ArsenalTradeOp op = CreateOp(MRX_EArsenalTradeStep.DELIVER, root);
		op.m_Delivery = callback;
		Release(op);
		m_iPendingMoves++;
		if (!manager.TryMoveItemToStorage(root, target, -1, op))
			op.Fail();
	}

	//------------------------------------------------------------------------------------------------
	override MRX_EShopStatus InspectForSale(notnull MRX_ShopCartLine line, out ResourceName prefab, notnull array<ResourceName> outContents)
	{
		IEntity item = IEntity.Cast(line.m_Item);
		if (!item || MRX_ArsenalTradeStorageComponent.FindHolder(item) != m_SaleStorage)
			return MRX_EShopStatus.NOT_IN_INVENTORY;

		if (MRX_IssuedItems.IsIssued(item))
			return MRX_EShopStatus.ISSUED;

		InventoryItemComponent itemComponent = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
		if (!itemComponent || !MRX_EntityShopInventory.IsRefundable(itemComponent) || !MRX_EntityShopInventory.CollectContents(item, outContents))
			return MRX_EShopStatus.NOT_BUYABLE;

		prefab = SCR_ResourceNameUtils.GetPrefabName(item);
		return MRX_EShopStatus.OK;
	}

	//------------------------------------------------------------------------------------------------
	override void Return(int playerId, notnull MRX_ShopCartLine line)
	{
		ReturnToCharacter(IEntity.Cast(line.m_Item));
	}

	//------------------------------------------------------------------------------------------------
	override bool Remove(notnull MRX_ShopCartLine line)
	{
		IEntity item = IEntity.Cast(line.m_Item);
		if (!item || MRX_ArsenalTradeStorageComponent.FindHolder(item) != m_SaleStorage)
			return false;

		Delete(item);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! What the player's client shows: the cart's total by currency and the sale price of each item in the sale box.
	void GetState(notnull array<string> outCartCurrencies, notnull array<int> outCartTotals, notnull array<string> outSaleKeys, notnull array<int> outSalePrices, notnull array<string> outSaleCurrencies)
	{
		array<IEntity> items = {};
		if (m_CartStorage)
			m_CartStorage.GetRootItems(items);

		array<IEntity> roots = {};
		roots.Copy(items);
		foreach (IEntity root : roots)
		{
			CollectNested(root, items);
		}

		foreach (IEntity item : items)
		{
			MRX_ShopItem paid = GetPaidItem(item);
			if (!paid)
				continue;

			int index = outCartCurrencies.Find(paid.m_sCurrency);
			if (index < 0)
			{
				outCartCurrencies.Insert(paid.m_sCurrency);
				outCartTotals.Insert(paid.m_iPrice);
			}
			else
			{
				outCartTotals[index] = MRX_ShopCheckoutOp.AddCapped(outCartTotals[index], paid.m_iPrice);
			}
		}

		MRX_ShopDefinition shop = GetShop();
		array<IEntity> saleItems = {};
		if (m_SaleStorage)
			m_SaleStorage.GetRootItems(saleItems);

		foreach (IEntity saleItem : saleItems)
		{
			outSaleKeys.Insert(SCR_EntityHelper.EntityToRplId(saleItem).AsString());
			string currency;
			outSalePrices.Insert(GetSalePrice(shop, saleItem, currency));
			outSaleCurrencies.Insert(currency);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Sends the state to the client on the next frame (several changes in one frame send it once).
	void ScheduleState()
	{
		if (m_bStateScheduled || m_bClosing)
			return;

		m_bStateScheduled = true;
		GetGame().GetCallqueue().CallLater(SendState);
	}

	//------------------------------------------------------------------------------------------------
	protected void SendState()
	{
		m_bStateScheduled = false;
		if (!m_bClosing)
			m_Manager.SendState(this);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnContainerChanged(IEntity item, BaseInventoryStorageComponent storage)
	{
		ScheduleState();
	}

	//------------------------------------------------------------------------------------------------
	protected bool CanChange()
	{
		return !m_bClosing && !m_bCheckingOut && m_Container;
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_ShopDefinition GetShop()
	{
		MRX_ArsenalShopComponent arsenal = MRX_ArsenalShopComponent.Find(m_Arsenal);
		if (!arsenal)
			return null;

		return arsenal.GetShop();
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_ArsenalTradeOp CreateOp(MRX_EArsenalTradeStep step, IEntity target)
	{
		MRX_ArsenalTradeOp op = new MRX_ArsenalTradeOp(this, step);
		op.m_Target = target;
		m_aOps.Insert(op);
		return op;
	}

	//------------------------------------------------------------------------------------------------
	protected void Spawn(notnull MRX_ShopItem item, notnull BaseInventoryStorageComponent storage, int slotId)
	{
		MRX_ArsenalTradeOp op = CreateOp(MRX_EArsenalTradeStep.SPAWN, null);
		op.m_Item = item;
		m_iPendingSpawns++;
		if (!m_ContainerManager.TrySpawnPrefabToStorage(item.m_sPrefab, storage, slotId, EStoragePurpose.PURPOSE_ANY, op))
			op.Fail();
	}

	//------------------------------------------------------------------------------------------------
	//! Moves an item of the cart with the container's manager, which may take it out (op.m_Target).
	protected void Move(notnull MRX_ArsenalTradeOp op, notnull BaseInventoryStorageComponent storage, int slotId)
	{
		Release(op);
		m_iPendingMoves++;
		if (!m_ContainerManager.TryMoveItemToStorage(op.m_Target, storage, slotId, op))
			op.Fail();
	}

	//------------------------------------------------------------------------------------------------
	protected void Release(notnull MRX_ArsenalTradeOp op)
	{
		if (op.m_Target && !m_aReleased.Contains(op.m_Target))
			m_aReleased.Insert(op.m_Target);
	}

	//------------------------------------------------------------------------------------------------
	//! The item or one of the items it lies in is being moved by the server.
	protected bool IsReleased(notnull IEntity item)
	{
		IEntity current = item;
		while (current && current != m_Container)
		{
			if (m_aReleased.Contains(current))
				return true;

			current = current.GetParent();
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSpawned(notnull MRX_ArsenalTradeOp op, bool success)
	{
		IEntity item = op.m_Spawned;
		if (!success || !item)
		{
			m_Manager.ReportFailure(this, MRX_EShopStatus.NO_SPACE);
			return;
		}

		if (m_bClosing)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(item);
			return;
		}

		MRX_ArsenalCartEntry entry = new MRX_ArsenalCartEntry();
		entry.m_sItemId = op.m_Item.m_sId;
		m_mEntries.Set(item.GetID(), entry);

		// What the item came with is part of it and paid with its price.
		MRX_ArsenalShopComponent arsenal = MRX_ArsenalShopComponent.Find(m_Arsenal);
		array<IEntity> parts = {};
		CollectNested(item, parts);
		foreach (IEntity part : parts)
		{
			MRX_ArsenalCartEntry partEntry = new MRX_ArsenalCartEntry();
			partEntry.m_bDefault = true;
			partEntry.m_Origin = item;
			MRX_ShopItem partItem;
			if (arsenal)
				partItem = arsenal.FindItem(SCR_ResourceNameUtils.GetPrefabName(part));

			if (partItem)
				partEntry.m_sItemId = partItem.m_sId;

			m_mEntries.Set(part.GetID(), partEntry);
		}

		// The cart is one page: an item that does not fit any more goes again.
		if (item.GetParent() == m_Container && m_CartStorage.GetUsedCells() > GetMaxCells())
		{
			Delete(item);
			m_Manager.ReportFailure(this, MRX_EShopStatus.NO_SPACE);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The slot's old part went to the cells: the new item (a spawned one or a moved one) goes into the slot. When that
	//! fails, the old part goes back.
	protected void OnVacated(notnull MRX_ArsenalTradeOp op, bool success)
	{
		if (!success || !op.m_Storage || m_bClosing)
		{
			m_Manager.ReportFailure(this, MRX_EShopStatus.NO_SPACE);
			return;
		}

		if (op.m_Follower)
		{
			MRX_ArsenalTradeOp follow = CreateOp(MRX_EArsenalTradeStep.MOVE, op.m_Follower);
			Release(follow);
			m_iPendingMoves++;
			if (m_ContainerManager.TryMoveItemToStorage(op.m_Follower, op.m_Storage, op.m_iSlotId, follow))
				return;

			// Reports the failure.
			follow.Fail();
		}
		else if (op.m_Item && m_ContainerManager.CanInsertResourceInStorage(op.m_Item.m_sPrefab, op.m_Storage, op.m_iSlotId))
		{
			Spawn(op.m_Item, op.m_Storage, op.m_iSlotId);
			return;
		}
		else
		{
			m_Manager.ReportFailure(this, MRX_EShopStatus.NO_SPACE);
		}

		// Does not fit: the old part goes back into its slot.
		Move(CreateOp(MRX_EArsenalTradeStep.MOVE, op.m_Target), op.m_Storage, op.m_iSlotId);
	}

	//------------------------------------------------------------------------------------------------
	//! A delivered line is paid: it leaves the records. The default parts it came with that lie in the cells follow it.
	protected void OnDelivered(notnull MRX_ArsenalTradeOp op, bool success)
	{
		IEntity root = op.m_Target;
		if (success && root)
		{
			array<IEntity> delivered = {root};
			CollectNested(root, delivered);
			foreach (IEntity item : delivered)
			{
				m_mEntries.Remove(item.GetID());
			}

			InventoryStorageManagerComponent manager = GetCharacterManager();
			array<IEntity> roots = {};
			m_CartStorage.GetRootItems(roots);
			foreach (IEntity part : roots)
			{
				MRX_ArsenalCartEntry entry = m_mEntries.Get(part.GetID());
				if (!manager || !entry || !entry.m_bDefault || entry.m_Origin != root)
					continue;

				BaseInventoryStorageComponent target = manager.FindStorageForItem(part);
				if (!target)
					continue;

				// Left in the cart when it does not fit: delivered with the next checkout for nothing.
				MRX_ArsenalTradeOp partOp = CreateOp(MRX_EArsenalTradeStep.DELIVER, part);
				Release(partOp);
				m_iPendingMoves++;
				if (!manager.TryMoveItemToStorage(part, target, -1, partOp))
					partOp.Fail();
			}
		}

		if (op.m_Delivery)
			op.m_Delivery.OnResult(success);
	}

	//------------------------------------------------------------------------------------------------
	//! Moves the item into the character's inventory, or puts it on the ground.
	//! \return True when it went into the inventory.
	protected bool ReturnToCharacter(IEntity item)
	{
		if (!item)
			return false;

		if (!m_aReleased.Contains(item))
			m_aReleased.Insert(item);

		InventoryStorageManagerComponent manager = GetCharacterManager();
		BaseInventoryStorageComponent target;
		if (manager)
			target = manager.FindStorageForItem(item);

		if (target && manager.TryMoveItemToStorage(item, target))
			return true;

		InventoryItemComponent itemComponent = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
		BaseInventoryStorageComponent from;
		if (itemComponent && itemComponent.GetParentSlot())
			from = itemComponent.GetParentSlot().GetStorage();

		// Dropped by the character, at its feet; without one, next to the arsenal.
		if (manager && from && manager.TryRemoveItemFromStorage(item, from))
			return false;

		if (m_ContainerManager && from)
			m_ContainerManager.TryRemoveItemFromStorage(item, from);

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Deletes an item of the container with what it holds and forgets the records of the cart's items among them.
	protected void Delete(notnull IEntity item)
	{
		array<IEntity> items = {item};
		CollectNested(item, items);
		foreach (IEntity deleted : items)
		{
			m_mEntries.Remove(deleted.GetID());
		}

		if (!m_aReleased.Contains(item))
			m_aReleased.Insert(item);

		if (!m_ContainerManager || !m_ContainerManager.TryDeleteItem(item))
			SCR_EntityHelper.DeleteEntityAndChildren(item);
	}

	//------------------------------------------------------------------------------------------------
	//! Items in the cart that the server did not put there (the player's own), the outermost ones only.
	protected void CollectForeign(notnull array<IEntity> outItems)
	{
		if (!m_CartStorage)
			return;

		array<IEntity> items = {};
		m_CartStorage.GetRootItems(items);
		array<IEntity> roots = {};
		roots.Copy(items);
		foreach (IEntity root : roots)
		{
			CollectNested(root, items);
		}

		foreach (IEntity item : items)
		{
			if (m_mEntries.Contains(item.GetID()))
				continue;

			// Inside another foreign item: it goes with that one.
			bool outer = true;
			foreach (IEntity other : outItems)
			{
				if (IsInside(item, other))
					outer = false;
			}

			if (outer)
				outItems.Insert(item);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The catalog item a cart item is paid as, or null: items the server put there are paid with their catalog price,
	//! except default parts that lie in their item or in the cells.
	protected MRX_ShopItem GetPaidItem(notnull IEntity item)
	{
		MRX_ArsenalCartEntry entry = m_mEntries.Get(item.GetID());
		if (!entry || entry.m_sItemId.IsEmpty())
			return null;

		MRX_ShopDefinition shop = GetShop();
		if (!shop)
			return null;

		MRX_ShopItem paid = shop.m_Catalog.FindItem(entry.m_sItemId);
		if (!paid || paid.m_iPrice <= 0)
			return null;

		if (entry.m_bDefault && (item.GetParent() == m_Container || (entry.m_Origin && IsInside(item, entry.m_Origin))))
			return null;

		return paid;
	}

	//------------------------------------------------------------------------------------------------
	//! Sale price of an item of the sale box with what it holds; 0 for issued items and items the shop does not buy.
	protected int GetSalePrice(MRX_ShopDefinition shop, notnull IEntity item, out string currency)
	{
		currency = string.Empty;
		if (!shop || MRX_IssuedItems.IsIssued(item))
			return 0;

		ResourceName prefab = SCR_ResourceNameUtils.GetPrefabName(item);
		MRX_ShopItem shopItem = shop.m_Catalog.FindByPrefab(prefab);
		if (!shopItem)
			return 0;

		currency = shopItem.m_sCurrency;
		array<ResourceName> contents = {};
		if (!MRX_EntityShopInventory.CollectContents(item, contents))
			return 0;

		return MRX_ShopService.GetSellPriceWithContents(shop, prefab, contents);
	}

	//------------------------------------------------------------------------------------------------
	//! The shop buys items back and the item holds no mission item (vanilla refund rule).
	protected bool CanSell(notnull IEntity item)
	{
		MRX_ShopDefinition shop = GetShop();
		if (!shop || !shop.m_bAllowSell)
			return false;

		SCR_ArsenalComponent vanillaArsenal = SCR_ArsenalComponent.FindArsenalComponent(m_Arsenal, false);
		if (vanillaArsenal && !vanillaArsenal.IsRefundEnabled())
			return false;

		return IsRefundableWithContents(item);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsRefundableWithContents(notnull IEntity item)
	{
		InventoryItemComponent itemComponent = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
		if (itemComponent && !MRX_EntityShopInventory.IsRefundable(itemComponent))
			return false;

		array<IEntity> contents = {};
		CollectNested(item, contents);
		foreach (IEntity content : contents)
		{
			InventoryItemComponent contentComponent = InventoryItemComponent.Cast(content.FindComponent(InventoryItemComponent));
			if (contentComponent && !MRX_EntityShopInventory.IsRefundable(contentComponent))
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Room in the storage's page for the item (the cart and the sale box are one page each).
	protected bool HasRoomFor(notnull IEntity item, MRX_ArsenalTradeStorageComponent storage = null)
	{
		if (!storage)
			storage = m_CartStorage;

		return storage.GetUsedCells() + MRX_ArsenalTradeStorageComponent.GetCellCount(item) <= GetMaxCells();
	}

	//------------------------------------------------------------------------------------------------
	protected int GetMaxCells()
	{
		MRX_ArsenalShopComponent arsenal = MRX_ArsenalShopComponent.Find(m_Arsenal);
		if (!arsenal)
			return 0;

		return arsenal.GetCartColumns() * arsenal.GetCartRows();
	}

	//------------------------------------------------------------------------------------------------
	//! The storage belongs to an item in the cart (e.g. a rifle's attachment slots).
	protected bool IsCartStorage(notnull BaseInventoryStorageComponent storage)
	{
		IEntity owner = storage.GetOwner();
		return owner && owner != m_Container && IsInCart(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsInCart(IEntity item)
	{
		return item && m_CartStorage && MRX_ArsenalTradeStorageComponent.FindHolder(item) == m_CartStorage;
	}

	//------------------------------------------------------------------------------------------------
	//! First empty slot of the storage that takes the prefab, or -1.
	protected int FindEmptySlot(ResourceName prefab, notnull BaseInventoryStorageComponent storage)
	{
		int count = storage.GetSlotsCount();
		for (int i = 0; i < count; i++)
		{
			if (!storage.Get(i) && m_ContainerManager.CanInsertResourceInStorage(prefab, storage, i))
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	protected InventoryStorageManagerComponent GetCharacterManager()
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(m_Character);
		if (!character || !character.GetCharacterController())
			return null;

		return character.GetCharacterController().GetInventoryStorageManager();
	}

	//------------------------------------------------------------------------------------------------
	//! Adds everything the entity holds, at any depth (attachments, magazines, stored items).
	protected static void CollectNested(notnull IEntity entity, notnull array<IEntity> outItems)
	{
		set<BaseInventoryStorageComponent> storages = new set<BaseInventoryStorageComponent>();
		MRX_EntitySnapshots.FindStorages(entity, storages);
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			array<InventoryItemComponent> itemComponents = {};
			storage.GetOwnedItems(itemComponents, false);
			foreach (InventoryItemComponent itemComponent : itemComponents)
			{
				IEntity child = itemComponent.GetOwner();
				if (!child || child == entity || outItems.Contains(child))
					continue;

				outItems.Insert(child);
				CollectNested(child, outItems);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsInside(notnull IEntity item, IEntity ancestor)
	{
		if (!ancestor)
			return false;

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
//! Record of an item the server put into the cart. Internal.
class MRX_ArsenalCartEntry : Managed
{
	//! Catalog ID; empty for a default part the arsenal does not sell.
	string m_sItemId;
	//! The item came with another one (e.g. a rifle's magazine), whose price includes it.
	bool m_bDefault;
	//! Default part: the item it came with. Weak.
	IEntity m_Origin;
}

//------------------------------------------------------------------------------------------------
enum MRX_EArsenalTradeStep
{
	//! A new item goes into the cart.
	SPAWN,
	//! An item of the cart moves within the cart.
	MOVE,
	//! An item of the cart moves to the cells to free its slot for m_Item (spawned) or m_Follower (moved).
	VACATE,
	//! A paid item goes into the player's inventory.
	DELIVER
}

//------------------------------------------------------------------------------------------------
//! One inventory operation of a trade session, reported once. Internal.
class MRX_ArsenalTradeOp : ScriptedInventoryOperationCallback
{
	//! Weak: the session keeps its running operations.
	protected MRX_ArsenalTradeSession m_Session;
	MRX_EArsenalTradeStep m_eStep;
	//! Weak entities: the moved item, and the item that goes into the vacated slot.
	IEntity m_Target;
	IEntity m_Follower;
	//! Item to spawn (into the vacated slot for VACATE). Owned by the catalog.
	MRX_ShopItem m_Item;
	//! Vacated slot.
	BaseInventoryStorageComponent m_Storage;
	int m_iSlotId = -1;
	//! Weak: the spawned item.
	IEntity m_Spawned;
	ref MRX_ShopDeliveryCallback m_Delivery;
	protected bool m_bDone;

	//------------------------------------------------------------------------------------------------
	void MRX_ArsenalTradeOp(MRX_ArsenalTradeSession session, MRX_EArsenalTradeStep step)
	{
		m_Session = session;
		m_eStep = step;
	}

	//------------------------------------------------------------------------------------------------
	//! Reports failure when the request was not accepted.
	void Fail()
	{
		Report(false);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnComplete()
	{
		if (m_eStep == MRX_EArsenalTradeStep.SPAWN)
		{
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(GetItem()));
			if (rpl)
				m_Spawned = rpl.GetEntity();
		}

		Report(true);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnFailed()
	{
		Report(false);
	}

	//------------------------------------------------------------------------------------------------
	protected void Report(bool success)
	{
		if (m_bDone)
			return;

		m_bDone = true;
		if (m_Session)
			m_Session.OnOpDone(this, success);
	}
}

//------------------------------------------------------------------------------------------------
//! Passes the result of a checkout to its session. Internal.
class MRX_ArsenalTradeCheckoutReply : MRX_ShopCallback
{
	protected ref MRX_ArsenalTradeSession m_Session;
	protected bool m_bSell;

	//------------------------------------------------------------------------------------------------
	void MRX_ArsenalTradeCheckoutReply(MRX_ArsenalTradeSession session, bool sell)
	{
		m_Session = session;
		m_bSell = sell;
	}

	//------------------------------------------------------------------------------------------------
	override void OnResult(MRX_ShopResult result)
	{
		super.OnResult(result);
		m_Session.OnCheckoutDone(m_bSell, result);
	}
}
