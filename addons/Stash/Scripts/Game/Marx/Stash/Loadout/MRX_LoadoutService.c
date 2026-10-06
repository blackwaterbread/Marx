//! Outcome of a loadout request (API v0).
enum MRX_ELoadoutStatus
{
	OK,
	OWNER_NOT_READY,
	//! Loadouts are off (no slots) or no price list is set (MRX_Marx.SetPriceList).
	NOT_AVAILABLE,
	//! The player has no open stash: loadouts are saved and put on at a stash point.
	NO_STASH,
	UNKNOWN_SLOT,
	EMPTY_SLOT,
	//! The player has no alive character.
	NO_CHARACTER,
	//! Another loadout request of the player is running.
	BUSY,
	//! The player's gear changed while paying; the payment was refunded.
	CHANGED,
	//! See MRX_LoadoutResult.m_eTxStatus.
	PAYMENT_FAILED,
	//! Part of the loadout could not be put on; its price was refunded.
	INCOMPLETE,
	STORAGE_ERROR
}

//------------------------------------------------------------------------------------------------
//! Result of a loadout request (API v0).
class MRX_LoadoutResult : Managed
{
	MRX_ELoadoutStatus m_eStatus;
	MRX_ETxStatus m_eTxStatus;
	int m_iSlot;
	string m_sCurrency;
	//! Paid (positive) or received (negative) for putting the loadout on.
	int m_iNet;
	int m_iUnavailableCount;

	//------------------------------------------------------------------------------------------------
	static MRX_LoadoutResult Create(MRX_ELoadoutStatus status, int slot)
	{
		MRX_LoadoutResult result = new MRX_LoadoutResult();
		result.m_eStatus = status;
		result.m_iSlot = slot;
		return result;
	}
}

//------------------------------------------------------------------------------------------------
//! Override OnResult.
class MRX_LoadoutCallback : Managed
{
	//------------------------------------------------------------------------------------------------
	void OnResult(MRX_LoadoutResult result)
	{
	}
}

//------------------------------------------------------------------------------------------------
//! The slots of a player as the stash panel shows them (API v0), parallel arrays by slot.
class MRX_LoadoutInfo : Managed
{
	string m_sCurrency;
	//! Main weapon (or first item) of each saved loadout, empty for an empty slot.
	ref array<string> m_aMainItems = {};
	ref array<int> m_aItemCounts = {};
	//! What putting it on costs now (negative: the player receives money).
	ref array<int> m_aNets = {};
	ref array<int> m_aUnavailable = {};
}

//------------------------------------------------------------------------------------------------
//! Override OnResult.
class MRX_LoadoutInfoCallback : Managed
{
	//------------------------------------------------------------------------------------------------
	void OnResult(MRX_ELoadoutStatus status, MRX_LoadoutInfo info)
	{
	}
}

//------------------------------------------------------------------------------------------------
//! A saved loadout as kept in the stash property. Internal.
class MRX_SavedLoadout : Managed
{
	//! Main weapon (or first item), for the slot's title.
	ResourceName m_sMainItem;
	int m_iSavedAt;
	ref MRX_ItemSnapshot m_Loadout;
}

//------------------------------------------------------------------------------------------------
//! Saved loadouts (API v0): at an open stash, a player saves what they wear and carry into a slot and later puts it on
//! again, using the items they have, buying the missing ones and selling the rest (MRX_LoadoutMath with
//! MRX_Marx.GetPriceList()). Issued items (MRX_IssuedItems) neither count nor sell. Loadouts are kept as stash
//! properties; the number of slots is MRX_Settings.m_iLoadoutSlots. Server; use MRX_Loadouts.Get().
class MRX_LoadoutService : Managed
{
	static const string PROPERTY_PREFIX = "marx.loadout.";
	static const string LEDGER_SOURCE = "marx_loadout";

	protected ref set<int> m_aBusyPlayerIds = new set<int>();
	protected ref array<ref MRX_LoadoutOp> m_aOps = {};

	//------------------------------------------------------------------------------------------------
	int GetSlotCount()
	{
		MRX_MarxSystem system = MRX_MarxSystem.GetInstance();
		if (!system || !system.GetSettings())
			return 0;

		return Math.Max(0, system.GetSettings().m_iLoadoutSlots);
	}

	//------------------------------------------------------------------------------------------------
	//! Saves the player's gear into the slot (0-based), replacing what it held.
	void Save(int playerId, int slot, MRX_LoadoutCallback callback = null)
	{
		StartOp(new MRX_LoadoutSaveOp(this, playerId, slot, callback));
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the loadout of the slot (0-based) on the player's character and settles the price.
	void Load(int playerId, int slot, MRX_LoadoutCallback callback = null)
	{
		StartOp(new MRX_LoadoutLoadOp(this, playerId, slot, callback));
	}

	//------------------------------------------------------------------------------------------------
	//! The player's slots with what putting each on would cost now.
	void Describe(int playerId, notnull MRX_LoadoutInfoCallback callback)
	{
		MRX_LoadoutDescribeOp op = new MRX_LoadoutDescribeOp(this, playerId, callback);
		Track(op);
		op.Start();
	}

	//------------------------------------------------------------------------------------------------
	static string GetPropertyKey(int slot)
	{
		return PROPERTY_PREFIX + (slot + 1).ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! \return Null when the text holds no loadout.
	static MRX_SavedLoadout Parse(string json)
	{
		if (json.IsEmpty())
			return null;

		JsonLoadContext context = new JsonLoadContext();
		if (!context.LoadFromString(json))
			return null;

		context.EnableTypeDiscriminator(false);
		MRX_SavedLoadout saved = new MRX_SavedLoadout();
		if (!context.ReadValue("", saved) || !saved.m_Loadout)
			return null;

		RepairSnapshot(saved.m_Loadout);
		return saved;
	}

	//------------------------------------------------------------------------------------------------
	static string ToJson(notnull MRX_SavedLoadout saved)
	{
		JsonSaveContext context = new JsonSaveContext();
		context.EnableTypeDiscriminator(false);
		context.WriteValue("", saved);
		return context.SaveToString();
	}

	//------------------------------------------------------------------------------------------------
	//! The player's alive character, or null.
	static ChimeraCharacter GetCharacter(int playerId)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId));
		if (!character || !character.GetCharacterController() || character.GetCharacterController().GetLifeState() != ECharacterLifeState.ALIVE)
			return null;

		return character;
	}

	//------------------------------------------------------------------------------------------------
	//! Prefabs of the character's items that count as the player's own for putting on a loadout: not issued, and
	//! magazines only when full.
	static void GetOwnedPrefabs(notnull IEntity character, notnull array<ResourceName> outPrefabs)
	{
		array<IEntity> items = {};
		MRX_EntitySnapshots.GetLoadoutItems(character, items);
		foreach (IEntity item : items)
		{
			if (MRX_IssuedItems.IsIssued(item))
				continue;

			BaseMagazineComponent magazine = BaseMagazineComponent.Cast(item.FindComponent(BaseMagazineComponent));
			if (magazine && magazine.GetAmmoCount() < magazine.GetMaxAmmoCount())
				continue;

			outPrefabs.Insert(SCR_ResourceNameUtils.GetPrefabName(item));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Prefab of the character's main weapon (primary first), else of its first loadout item.
	static ResourceName GetMainItem(notnull IEntity character, notnull MRX_ItemSnapshot loadout)
	{
		BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(character.FindComponent(BaseWeaponManagerComponent));
		if (weapons)
		{
			array<WeaponSlotComponent> slots = {};
			weapons.GetWeaponsSlots(slots);
			IEntity best;
			int bestIndex = int.MAX;
			foreach (WeaponSlotComponent slot : slots)
			{
				if (slot.GetWeaponEntity() && slot.GetWeaponSlotIndex() < bestIndex)
				{
					best = slot.GetWeaponEntity();
					bestIndex = slot.GetWeaponSlotIndex();
				}
			}

			if (best)
				return SCR_ResourceNameUtils.GetPrefabName(best);
		}

		if (!loadout.m_aChildren.IsEmpty())
			return loadout.m_aChildren[0].m_sPrefab;

		return ResourceName.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Internal: OK when the player can use loadouts at an open stash now.
	MRX_ELoadoutStatus CheckPlayer(int playerId, int slot, bool needPrices)
	{
		if (slot < 0 || slot >= GetSlotCount())
			return MRX_ELoadoutStatus.UNKNOWN_SLOT;

		if (needPrices && !MRX_Marx.GetPriceList())
			return MRX_ELoadoutStatus.NOT_AVAILABLE;

		if (MRX_Marx.GetOwnerId(playerId).IsEmpty())
			return MRX_ELoadoutStatus.OWNER_NOT_READY;

		if (!GetCharacter(playerId))
			return MRX_ELoadoutStatus.NO_CHARACTER;

		MRX_StashSessionManager sessions = MRX_StashSessions.Get();
		MRX_StashSession session;
		if (sessions)
			session = sessions.Find(playerId);

		if (!session || !session.IsReady() || session.IsClosing() || session.GetCharacter() != GetCharacter(playerId))
			return MRX_ELoadoutStatus.NO_STASH;

		return MRX_ELoadoutStatus.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by MRX_LoadoutOp once with its result.
	void OnOpFinished(notnull MRX_LoadoutOp op, bool releasePlayer)
	{
		if (releasePlayer)
			m_aBusyPlayerIds.RemoveItem(op.GetPlayerId());
	}

	//------------------------------------------------------------------------------------------------
	protected void StartOp(notnull MRX_LoadoutOp op)
	{
		Track(op);
		int playerId = op.GetPlayerId();
		if (m_aBusyPlayerIds.Contains(playerId))
		{
			op.FinishEarly(MRX_ELoadoutStatus.BUSY);
			return;
		}

		m_aBusyPlayerIds.Insert(playerId);
		op.Start();
	}

	//------------------------------------------------------------------------------------------------
	protected void Track(notnull MRX_LoadoutOp op)
	{
		for (int i = m_aOps.Count() - 1; i >= 0; i--)
		{
			if (m_aOps[i].IsDone())
				m_aOps.Remove(i);
		}

		m_aOps.Insert(op);
	}

	//------------------------------------------------------------------------------------------------
	protected static void RepairSnapshot(notnull MRX_ItemSnapshot snapshot)
	{
		if (!snapshot.m_aHitZones)
			snapshot.m_aHitZones = {};

		if (!snapshot.m_aFuel)
			snapshot.m_aFuel = {};

		if (!snapshot.m_aChildren)
			snapshot.m_aChildren = {};

		foreach (MRX_ItemSnapshot child : snapshot.m_aChildren)
		{
			RepairSnapshot(child);
		}
	}
}

//------------------------------------------------------------------------------------------------
//! One running loadout request. Internal.
class MRX_LoadoutOp : Managed
{
	//! Weak: the service owns its ops.
	protected MRX_LoadoutService m_Service;
	protected int m_iPlayerId;
	protected int m_iSlot;
	protected string m_sOwnerId;
	protected ref MRX_LoadoutCallback m_Callback;
	protected ref MRX_LoadoutResult m_Result;
	protected bool m_bDone;

	//------------------------------------------------------------------------------------------------
	protected void Init(MRX_LoadoutService service, int playerId, int slot, MRX_LoadoutCallback callback)
	{
		m_Service = service;
		m_iPlayerId = playerId;
		m_iSlot = slot;
		m_Callback = callback;
		m_Result = MRX_LoadoutResult.Create(MRX_ELoadoutStatus.OK, slot);
	}

	//------------------------------------------------------------------------------------------------
	void Start()
	{
	}

	//------------------------------------------------------------------------------------------------
	int GetPlayerId()
	{
		return m_iPlayerId;
	}

	//------------------------------------------------------------------------------------------------
	bool IsDone()
	{
		return m_bDone;
	}

	//------------------------------------------------------------------------------------------------
	//! Answers BUSY without releasing the player, whose other request still runs.
	void FinishEarly(MRX_ELoadoutStatus status)
	{
		m_Result.m_eStatus = status;
		m_bDone = true;
		if (m_Service)
			m_Service.OnOpFinished(this, false);

		if (m_Callback)
			m_Callback.OnResult(m_Result);
	}

	//------------------------------------------------------------------------------------------------
	protected void Finish(MRX_ELoadoutStatus status)
	{
		if (m_bDone)
			return;

		m_Result.m_eStatus = status;
		m_bDone = true;
		if (m_Service)
			m_Service.OnOpFinished(this, true);

		if (m_Callback)
			m_Callback.OnResult(m_Result);
	}

	//------------------------------------------------------------------------------------------------
	protected void ListStash()
	{
		MRX_StashCallback callback = new MRX_StashCallback();
		callback.GetOnResult().Insert(OnListed);
		MRX_Marx.GetStash().List(m_sOwnerId, callback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnListed(MRX_EStashStatus status, MRX_StashRecord record)
	{
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_TxContext CreateContext(string action)
	{
		string reason = string.Format("loadout:%1:%2", action, m_iSlot + 1);
		return MRX_TxContext.Create(MRX_LoadoutService.LEDGER_SOURCE, reason, action + ":" + MRX_Marx.NewId());
	}
}

//------------------------------------------------------------------------------------------------
class MRX_LoadoutSaveOp : MRX_LoadoutOp
{
	//------------------------------------------------------------------------------------------------
	void MRX_LoadoutSaveOp(MRX_LoadoutService service, int playerId, int slot, MRX_LoadoutCallback callback)
	{
		Init(service, playerId, slot, callback);
	}

	//------------------------------------------------------------------------------------------------
	override void Start()
	{
		MRX_ELoadoutStatus status = m_Service.CheckPlayer(m_iPlayerId, m_iSlot, false);
		if (status != MRX_ELoadoutStatus.OK)
		{
			Finish(status);
			return;
		}

		m_sOwnerId = MRX_Marx.GetOwnerId(m_iPlayerId);
		ChimeraCharacter character = MRX_LoadoutService.GetCharacter(m_iPlayerId);
		MRX_SavedLoadout saved = new MRX_SavedLoadout();
		saved.m_Loadout = MRX_EntitySnapshots.CaptureLoadout(character);
		saved.m_sMainItem = MRX_LoadoutService.GetMainItem(character, saved.m_Loadout);
		saved.m_iSavedAt = System.GetUnixTime();

		MRX_PropertyChange change = MRX_PropertyChange.Create(MRX_LoadoutService.GetPropertyKey(m_iSlot), MRX_LoadoutService.ToJson(saved));
		MRX_StashResultCallback callback = new MRX_StashResultCallback();
		callback.GetOnResult().Insert(OnSaved);
		MRX_Marx.GetStash().SetProperty(m_sOwnerId, change, CreateContext("save"), callback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSaved(MRX_StashResult result)
	{
		if (result.m_eStatus != MRX_EStashStatus.OK)
		{
			Finish(MRX_ELoadoutStatus.STORAGE_ERROR);
			return;
		}

		Finish(MRX_ELoadoutStatus.OK);
	}
}

//------------------------------------------------------------------------------------------------
//! Plan -> payment -> plan again (the gear must not have changed) -> put on -> refund of what could not be put on, or
//! payout of the sale.
class MRX_LoadoutLoadOp : MRX_LoadoutOp
{
	protected ref MRX_SavedLoadout m_Saved;
	protected ref MRX_LoadoutPlan m_Plan;
	protected int m_iCharged;

	//------------------------------------------------------------------------------------------------
	void MRX_LoadoutLoadOp(MRX_LoadoutService service, int playerId, int slot, MRX_LoadoutCallback callback)
	{
		Init(service, playerId, slot, callback);
	}

	//------------------------------------------------------------------------------------------------
	override void Start()
	{
		MRX_ELoadoutStatus status = m_Service.CheckPlayer(m_iPlayerId, m_iSlot, true);
		if (status != MRX_ELoadoutStatus.OK)
		{
			Finish(status);
			return;
		}

		m_sOwnerId = MRX_Marx.GetOwnerId(m_iPlayerId);
		ListStash();
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnListed(MRX_EStashStatus status, MRX_StashRecord record)
	{
		if (status != MRX_EStashStatus.OK || !record)
		{
			Finish(MRX_ELoadoutStatus.STORAGE_ERROR);
			return;
		}

		m_Saved = MRX_LoadoutService.Parse(record.GetProperty(MRX_LoadoutService.GetPropertyKey(m_iSlot)));
		if (!m_Saved)
		{
			Finish(MRX_ELoadoutStatus.EMPTY_SLOT);
			return;
		}

		m_Plan = CreatePlan();
		if (!m_Plan)
		{
			Finish(MRX_ELoadoutStatus.NO_STASH);
			return;
		}

		m_Result.m_sCurrency = m_Plan.m_sCurrency;
		m_Result.m_iUnavailableCount = m_Plan.m_iUnavailableCount;
		int net = m_Plan.GetNet();
		if (net <= 0)
		{
			PutOn();
			return;
		}

		MRX_TxCallback callback = new MRX_TxCallback();
		callback.GetOnResult().Insert(OnPaid);
		MRX_Marx.GetEconomy().Debit(m_sOwnerId, m_Plan.m_sCurrency, net, CreateContext("buy"), callback);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Null when the player cannot use the stash any more.
	protected MRX_LoadoutPlan CreatePlan()
	{
		if (m_Service.CheckPlayer(m_iPlayerId, m_iSlot, true) != MRX_ELoadoutStatus.OK)
			return null;

		array<ResourceName> ownItems = {};
		MRX_LoadoutService.GetOwnedPrefabs(MRX_LoadoutService.GetCharacter(m_iPlayerId), ownItems);
		return MRX_LoadoutMath.Plan(m_Saved.m_Loadout, ownItems, MRX_Marx.GetPriceList());
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPaid(MRX_TxResult result)
	{
		if (result.m_eStatus != MRX_ETxStatus.OK)
		{
			m_Result.m_eTxStatus = result.m_eStatus;
			Finish(MRX_ELoadoutStatus.PAYMENT_FAILED);
			return;
		}

		m_iCharged = m_Plan.GetNet();
		// The payment took at least a frame: the gear (or the stash) may have changed meanwhile.
		MRX_LoadoutPlan again = CreatePlan();
		if (!again || !again.IsSameAs(m_Plan))
		{
			Settle(m_iCharged, MRX_ELoadoutStatus.CHANGED);
			return;
		}

		PutOn();
	}

	//------------------------------------------------------------------------------------------------
	protected void PutOn()
	{
		ChimeraCharacter character = MRX_LoadoutService.GetCharacter(m_iPlayerId);
		if (!character)
		{
			Settle(m_iCharged, MRX_ELoadoutStatus.NO_CHARACTER);
			return;
		}

		InventoryStorageManagerComponent manager = character.GetCharacterController().GetInventoryStorageManager();
		if (!MRX_EntitySnapshots.ApplyLoadout(character, m_Plan.m_Loadout, manager))
			Print(string.Format("[MRX] Loadout %1 of player %2 was put on incompletely", m_iSlot + 1, m_iPlayerId), LogLevel.WARNING);

		// Everything the character wears now is paid for or the player's own, issued items it kept included.
		array<IEntity> items = {};
		MRX_EntitySnapshots.GetLoadoutItems(character, items);
		map<string, int> received = new map<string, int>();
		foreach (IEntity item : items)
		{
			MRX_IssuedItems.Unmark(item, false);
			string key = MRX_LoadoutMath.GetPrefabKey(SCR_ResourceNameUtils.GetPrefabName(item));
			received.Set(key, received.Get(key) + 1);
		}

		map<string, int> needed = new map<string, int>();
		MRX_LoadoutMath.CountItems(m_Plan.m_Loadout, needed);
		int shortfall = m_Plan.GetShortfallPrice(received, needed);

		// Owed: the price of what was not received, and the sale when it was worth more than the purchase.
		int owed = MRX_LoadoutMath.AddCapped(shortfall, Math.Max(0, -m_Plan.GetNet()));
		m_Result.m_iNet = m_Plan.GetNet() - shortfall;
		MRX_ELoadoutStatus status = MRX_ELoadoutStatus.OK;
		if (shortfall > 0)
			status = MRX_ELoadoutStatus.INCOMPLETE;

		Settle(owed, status);
	}

	//------------------------------------------------------------------------------------------------
	//! Pays the player the amount (refund or sale) and finishes.
	protected void Settle(int amount, MRX_ELoadoutStatus status)
	{
		m_Result.m_eStatus = status;
		if (status != MRX_ELoadoutStatus.OK && status != MRX_ELoadoutStatus.INCOMPLETE)
			m_Result.m_iNet = 0;

		if (amount <= 0)
		{
			Finish(status);
			return;
		}

		MRX_TxCallback callback = new MRX_TxCallback();
		callback.GetOnResult().Insert(OnSettled);
		MRX_Marx.GetEconomy().Credit(m_sOwnerId, m_Plan.m_sCurrency, amount, CreateContext("settle"), callback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSettled(MRX_TxResult result)
	{
		if (result.m_eStatus != MRX_ETxStatus.OK)
			Print(string.Format("[MRX] Loadout payout for player %1 failed: %2", m_iPlayerId, typename.EnumToString(MRX_ETxStatus, result.m_eStatus)), LogLevel.ERROR);

		Finish(m_Result.m_eStatus);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_LoadoutDescribeOp : MRX_LoadoutOp
{
	protected ref MRX_LoadoutInfoCallback m_InfoCallback;

	//------------------------------------------------------------------------------------------------
	void MRX_LoadoutDescribeOp(MRX_LoadoutService service, int playerId, MRX_LoadoutInfoCallback callback)
	{
		Init(service, playerId, 0, null);
		m_InfoCallback = callback;
	}

	//------------------------------------------------------------------------------------------------
	override void Start()
	{
		m_sOwnerId = MRX_Marx.GetOwnerId(m_iPlayerId);
		if (m_Service.GetSlotCount() == 0)
		{
			Answer(MRX_ELoadoutStatus.NOT_AVAILABLE, null);
			return;
		}

		if (m_sOwnerId.IsEmpty())
		{
			Answer(MRX_ELoadoutStatus.OWNER_NOT_READY, null);
			return;
		}

		ListStash();
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnListed(MRX_EStashStatus status, MRX_StashRecord record)
	{
		if (status != MRX_EStashStatus.OK || !record)
		{
			Answer(MRX_ELoadoutStatus.STORAGE_ERROR, null);
			return;
		}

		MRX_LoadoutInfo info = new MRX_LoadoutInfo();
		MRX_PriceList prices = MRX_Marx.GetPriceList();
		array<ResourceName> ownItems = {};
		ChimeraCharacter character = MRX_LoadoutService.GetCharacter(m_iPlayerId);
		if (character)
			MRX_LoadoutService.GetOwnedPrefabs(character, ownItems);

		if (prices)
			info.m_sCurrency = prices.GetCurrency();

		for (int slot = 0, count = m_Service.GetSlotCount(); slot < count; slot++)
		{
			MRX_SavedLoadout saved = MRX_LoadoutService.Parse(record.GetProperty(MRX_LoadoutService.GetPropertyKey(slot)));
			if (!saved)
			{
				info.m_aMainItems.Insert(string.Empty);
				info.m_aItemCounts.Insert(0);
				info.m_aNets.Insert(0);
				info.m_aUnavailable.Insert(0);
				continue;
			}

			info.m_aMainItems.Insert(saved.m_sMainItem);
			info.m_aItemCounts.Insert(saved.m_Loadout.CountItems() - 1);
			if (!prices)
			{
				info.m_aNets.Insert(0);
				info.m_aUnavailable.Insert(0);
				continue;
			}

			MRX_LoadoutPlan plan = MRX_LoadoutMath.Plan(saved.m_Loadout, ownItems, prices);
			info.m_aNets.Insert(plan.GetNet());
			info.m_aUnavailable.Insert(plan.m_iUnavailableCount);
		}

		Answer(MRX_ELoadoutStatus.OK, info);
	}

	//------------------------------------------------------------------------------------------------
	protected void Answer(MRX_ELoadoutStatus status, MRX_LoadoutInfo info)
	{
		m_bDone = true;
		m_InfoCallback.OnResult(status, info);
	}
}
